#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stddef.h>
#include "utils.h"
#include "ast.h"
#include "compiler.h"
#include "bytecode.h"

void add_ins(Compiler* compiler, ByteIns* ins);
ByteIns* make_ins(int tag);

//----------------------------------------------------------
//------------------  CONSTANT POOL ------------------------
//----------------------------------------------------------

Value* make_value(int tag) {
  Value* value = malloc(sizeof(Value));
  value->tag = tag;
  return value;
}

StringValue* make_string(char *value) {
  StringValue* s = malloc(sizeof(StringValue));
  s->tag = STRING_VAL;
  s->value = value;
  return s;
}

IntValue* make_int(int value) {
  IntValue* i = malloc(sizeof(IntValue));
  i->tag = INT_VAL;
  i->value = value;
  return i;
}

LitIns* make_lit(int idx) {
  LitIns* i = malloc(sizeof(LitIns));
  i->tag = LIT_OP;
  i->idx = idx;
  return i;
}

GetLocalIns* make_get_local(int idx) {
  GetLocalIns* i = malloc(sizeof(GetLocalIns));
  i->tag = GET_LOCAL_OP;
  i->idx = idx;
  return i;
}

MethodValue* make_methodv(int name, int nargs, int nlocals) {
  MethodValue* method = malloc(sizeof(MethodValue));
  method->tag = METHOD_VAL;
  method->name = name;
  method->nargs = nargs;
  method->nlocals = nlocals;
  method->code = make_vector();
  return method;
}

int int_to_idx(int i, Compiler* compiler) {
  char int_char = '0' + i; 
  void* idx = ht_get(compiler->int_idx, &int_char);
  if (!idx) {
    vector_add(compiler->programe->values, make_int(i));
    idx = compiler->programe->values->size;
    ht_set(compiler->int_idx, &int_char, (void*) idx);
  } else {
    idx = (int) idx;
  }
  return idx - 1;
}

int str_to_idx(char* str, Compiler* compiler) {
  void* idx = ht_get(compiler->strings_idx, str);
  if (!idx) {
    if (str != "NULL") {
      vector_add(compiler->programe->values, make_string(str));
    } else {
      vector_add(compiler->programe->values, make_value(NULL_VAL)); 
    }
    idx = compiler->programe->values->size; 
    ht_set(compiler->strings_idx, str, (void*) idx);
  } else {
    idx = (int) idx;
  }
  return idx - 1;
}

//----------------------------------------------------------
//------------------  ENTRY + START-UP ------------------------
//----------------------------------------------------------

Program* compile (ScopeStmt* stmt) {
  printf("Compiling Program:\n");
  print_scopestmt(stmt);
  
  Compiler* compiler = init_compiler();
  parse_scope(compiler, stmt);

  compiler->global_scope->name = str_to_idx("entry123", compiler);
  vector_add(compiler->programe->values, compiler->global_scope);
  compiler->programe->entry = compiler->programe->values->size - 1;
  add_ins(compiler, make_ins(DROP_OP));
  add_ins(compiler, (ByteIns*) make_lit(str_to_idx("NULL", compiler)));
  add_ins(compiler, make_ins(RETURN_OP));
  return compiler->programe;
}

Compiler* init_compiler() {
  Compiler* compiler = malloc(sizeof(Compiler));
  compiler->strings_idx = ht_create();
  compiler->int_idx = ht_create();
  compiler->global_scope = make_methodv(0, 0, 0);
  compiler->local_scope = NULL;
  compiler->programe = init_programe();
  return compiler;
}

void free_compiler(Compiler* compiler) {
  destroy_programe(compiler->programe);
  ht_destroy(compiler->strings_idx);
  ht_destroy(compiler->int_idx);
  free(compiler);
}


//----------------------------------------------------------
//------------------  COMPILE ------------------------
//----------------------------------------------------------


ByteIns* make_ins(int tag) {
  ByteIns* ins = malloc(sizeof(ByteIns));
  ins->tag = tag;
  return ins;
}

void add_ins(Compiler* compiler, ByteIns* ins) {
  if (compiler->local_scope) {
    vector_add(compiler->local_scope->code, ins);
  } else {
    vector_add(compiler->global_scope->code, ins);
  }
}

void parse_scope(Compiler* compiler, ScopeStmt* s) {
  switch(s->tag){
  case VAR_STMT:{
    ScopeVar* s2 = (ScopeVar*)s;
    printf("var %s = ", s2->name);
    print_exp(s2->exp);
    break;
  }
  case FN_STMT:{
    ScopeFn* s2 = (ScopeFn*)s;
    int name = str_to_idx(s2->name, compiler);
    MethodValue* method = make_methodv(name, s2->nargs, 0);
    compiler->local_scope = method;

    for (int i = 0; i < s2->nargs; i++) {
      add_ins(compiler, (ByteIns*) make_get_local(i));
    }

    parse_scope(compiler, s2->body);
    add_ins(compiler, make_ins(RETURN_OP));
    compiler->local_scope = NULL;
    vector_add(compiler->programe->values, method);
    vector_add(compiler->programe->slots, (void*) compiler->programe->values->size - 1);
    break;
  }
  case SEQ_STMT:{
    ScopeSeq* s2 = (ScopeSeq*)s;
    parse_scope(compiler, s2->a);
    printf(" ");
    if (s2->a->tag == EXP_STMT && s2->b->tag == EXP_STMT) {
      add_ins(compiler, make_ins(DROP_OP));
    }
    parse_scope(compiler, s2->b);
    break;
  }
  case EXP_STMT:{
    ScopeExp* s2 = (ScopeExp*)s;
    add_exp(s2->exp, compiler);
    break;
  }
  default:
    printf("Unrecognized scope statement with tag %d\n", s->tag);
    exit(-1);
  }
}

void add_exp(Exp* e, Compiler* compiler) {
  switch(e->tag){
  case INT_EXP:{
    IntExp* e2 = (IntExp*)e;
    add_ins(compiler, (ByteIns*) make_lit(int_to_idx(e2->value, compiler)));
    break;
  }
  case NULL_EXP:{
    printf("null");
    break;
  }
  case PRINTF_EXP:{
    PrintfExp* e2 = (PrintfExp*)e;
    PrintfIns* print_ins = malloc(sizeof(PrintfIns));
    print_ins->format = str_to_idx(e2->format, compiler);
    print_ins->tag = PRINTF_OP;
    print_ins->arity = e2->nexps;
    add_ins(compiler, (ByteIns*) print_ins);
    str_to_idx("NULL", compiler);
    break;
  }
  case ARRAY_EXP:{
    ArrayExp* e2 = (ArrayExp*)e;
    printf("array(");
    print_exp(e2->length);
    printf(", ");
    print_exp(e2->init);
    printf(")");
    break;
  }
  case OBJECT_EXP:{
    ObjectExp* e2 = (ObjectExp*)e;
    printf("object : (");
    for(int i=0; i<e2->nslots; i++){
      if(i > 0) printf(" ");
      print_slotstmt(e2->slots[i]);
    }
    printf(")");
    break;
  }
  case SLOT_EXP:{
    SlotExp* e2 = (SlotExp*)e;
    print_exp(e2->exp);
    printf(".%s", e2->name);
    break;
  }
  case SET_SLOT_EXP:{
    SetSlotExp* e2 = (SetSlotExp*)e;
    print_exp(e2->exp);
    printf(".%s = ", e2->name);
    print_exp(e2->value);
    break;
  }
  case CALL_SLOT_EXP:{
    CallSlotExp* e2 = (CallSlotExp*)e;
    print_exp(e2->exp);
    printf(".%s(", e2->name);
    for(int i=0; i<e2->nargs; i++){
      if(i > 0) printf(", ");
      print_exp(e2->args[i]);
    }
    printf(")");
    break;
  }
  case CALL_EXP:{
    CallExp* e2 = (CallExp*)e;
    for (int i = 0; i < e2->nargs; i++) {
      add_exp(e2->args[i], compiler);
    }
    CallIns* call_ins = malloc(sizeof(CallIns));
    call_ins->tag = CALL_OP;
    call_ins->name = str_to_idx(e2->name, compiler); 
    call_ins->arity = e2->nargs;
    add_ins(compiler, (ByteIns*) call_ins);
    break;
  }
  case SET_EXP:{
    SetExp* e2 = (SetExp*)e;
    printf("%s = ", e2->name);
    print_exp(e2->exp);
    break;
  }
  case IF_EXP:{
    IfExp* e2 = (IfExp*)e;
    printf("if ");
    print_exp(e2->pred);
    printf(" : (");
    print_scopestmt(e2->conseq);
    printf(") else : (");
    print_scopestmt(e2->alt);
    printf(")");
    break;
  }
  case WHILE_EXP:{
    WhileExp* e2 = (WhileExp*)e;
    printf("while ");
    print_exp(e2->pred);
    printf(" : (");
    print_scopestmt(e2->body);
    printf(")");
    break;
  }
  case REF_EXP:{
    RefExp* e2 = (RefExp*)e;
    printf("%s", e2->name);
    break;
  }
  default:
    printf("Unrecognized Expression with tag %d\n", e->tag);
    exit(-1);
  }
}

