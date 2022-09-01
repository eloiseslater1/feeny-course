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

GetGlobalIns* make_get_global(int name) {
  GetGlobalIns* i = malloc(sizeof(GetGlobalIns));
  i->tag = GET_LOCAL_OP;
  i->name = name;
  return i;
}

SetLocalIns* make_set_local(int idx) {
  SetLocalIns* i = malloc(sizeof(SetLocalIns));
  i->tag = SET_LOCAL_OP;
  i->idx = idx;
}

SetGlobalIns* make_set_global(int name) {
  SetGlobalIns* i = malloc(sizeof(SetGlobalIns));
  i->tag = SET_GLOBAL_OP;
  i->name = name;
}

BranchIns* make_branch(int name) {
  BranchIns* i = malloc(sizeof(BranchIns));
  i->tag = BRANCH_OP;
  i->name = name;
}

LabelIns* make_label(int name) {
  LabelIns* i = malloc(sizeof(LabelIns));
  i->tag = LABEL_OP;
  i->name = name;
}

GotoIns* make_goto(int name) {
  GotoIns* i = malloc(sizeof(GotoIns));
  i->tag = GOTO_OP;
  i->name = name;
}

CallSlotIns* make_call_slot(int name, int arity) {
  CallSlotIns* i = malloc(sizeof(CallSlotIns));
  i->name = name;
  i->arity = arity;
  i->tag = CALL_SLOT_OP;
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
  char int_char[11];
  sprintf(int_char,"%d", i);
  IntValue* idx = (IntValue*) ht_get(compiler->int_idx, int_char);
  if (!idx) {
    vector_add(compiler->programe->values, make_int(i));
    idx = make_int(compiler->programe->values->size - 1);
    ht_set(compiler->int_idx, int_char, idx);
  } 
  return idx->value;
}

int str_to_idx(char* str, Compiler* compiler) {
  IntValue* idx = (IntValue*) ht_get(compiler->strings_idx, str);
  if (!idx) {
    if (str != "NULL") {
      vector_add(compiler->programe->values, make_string(str));
    } else {
      vector_add(compiler->programe->values, make_value(NULL_VAL)); 
    }
    idx = make_int(compiler->programe->values->size - 1); 
    ht_set(compiler->strings_idx, str, idx);
  }
  return idx->value;
}

#define LABEL_START 35

typedef enum {
  ENTRY_TAG,
  CONSEQ_TAG,
  END_TAG,
  TEST_TAG,
  LOOP_TAG
} LabelTag;

int add_label(Compiler* compiler, LabelTag tag) {
  char* label = malloc(sizeof(char) * 10);
  switch(tag) {
    case (ENTRY_TAG): {
      sprintf(label, "entry%2d", LABEL_START);
      break;
    }
    case (CONSEQ_TAG): {
      sprintf(label, "conseq%2d", LABEL_START+compiler->label_num);
      break;
    }
    case (END_TAG): {
      sprintf(label, "end%2d", LABEL_START+compiler->label_num);
      break;
    }
    case (TEST_TAG): {
      sprintf(label, "test%2d", LABEL_START+compiler->label_num);
      break;
    }
    case (LOOP_TAG): {
      sprintf(label, "loop%2d", LABEL_START+compiler->label_num);
      break;
    }
  }
  compiler->label_num++;
  return str_to_idx(label, compiler);
}

//----------------------------------------------------------
//------------------  ENTRY + START-UP ------------------------
//----------------------------------------------------------

Program* compile (ScopeStmt* stmt) {
  printf("Compiling Program:\n");
  print_scopestmt(stmt);
  
  Compiler* compiler = init_compiler();
  parse_scope(compiler, stmt);

  compiler->global_frame->name = add_label(compiler, ENTRY_TAG);
  vector_add(compiler->programe->values, compiler->global_frame);
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
  compiler->global_frame = make_methodv(0, 0, 0);
  compiler->local_frame = NULL;
  compiler->local_scope = ht_create();
  compiler->programe = init_programe();
  compiler->global_scope = ht_create();
  compiler->label_num = 0;
  return compiler;
}

void free_compiler(Compiler* compiler) {
  destroy_programe(compiler->programe);
  ht_destroy(compiler->strings_idx);
  ht_destroy(compiler->int_idx);
  ht_destroy(compiler->local_scope);
  ht_destroy(compiler->local_scope);
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
  if (compiler->local_frame) {
    vector_add(compiler->local_frame->code, ins);
  } else {
    vector_add(compiler->global_frame->code, ins);
  }
}

void parse_scope(Compiler* compiler, ScopeStmt* s) {
  switch(s->tag){
  case VAR_STMT:{
    ScopeVar* s2 = (ScopeVar*)s;
    add_exp(s2->exp, compiler);
    IntValue* local = (IntValue*) ht_get(compiler->local_scope, s2->name);
    if (compiler->local_frame) {
      local = make_int(compiler->local_frame->nargs + compiler->local_frame->nlocals); 
      ht_set(compiler->local_scope, s2->name, local);
      compiler->local_frame->nlocals++;
      add_ins(compiler, (ByteIns*) make_set_local(local->value));
    } else {
      IntValue* global = make_int(compiler->programe->values->size);
      ht_set(compiler->global_scope, s2->name, global);
      add_ins(compiler, (ByteIns*) make_set_global(global->value));
    }
    break;
  }
  case FN_STMT:{
    ScopeFn* s2 = (ScopeFn*)s;
    int name = str_to_idx(s2->name, compiler);
    MethodValue* method = make_methodv(name, s2->nargs, 0);
    compiler->local_frame = method;
    compiler->local_scope = ht_create();

    for (int i = 0; i < s2->nargs; i++) {
      ht_set(compiler->local_scope, s2->args[i], make_int(i));
    }

    parse_scope(compiler, s2->body);
    add_ins(compiler, make_ins(RETURN_OP));
    compiler->local_frame = NULL;

    ht_destroy(compiler->local_scope);
    vector_add(compiler->programe->values, method);
    vector_add(compiler->programe->slots, (void*) compiler->programe->values->size - 1);
    break;
  }
  case SEQ_STMT:{
    ScopeSeq* s2 = (ScopeSeq*)s;
    parse_scope(compiler, s2->a);
    printf(" ");
    if (compiler->local_frame && (s2->a->tag != SEQ_STMT)) {
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
    add_ins(compiler, (ByteIns*) make_lit(str_to_idx("NULL", compiler)));
    break;
  }
  case PRINTF_EXP:{
    PrintfExp* e2 = (PrintfExp*)e;
    PrintfIns* print_ins = malloc(sizeof(PrintfIns));
    print_ins->format = str_to_idx(e2->format, compiler);
    print_ins->tag = PRINTF_OP;
    print_ins->arity = e2->nexps;
    for(int i = 0; i < e2->nexps; i++) {
      add_exp(e2->exps[i], compiler);
    }
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
    add_exp(e2->exp, compiler);
    for(int i=0; i<e2->nargs; i++){
      add_exp(e2->args[i], compiler);
    }
    int idx = str_to_idx(e2->name, compiler);
    add_ins(compiler, (ByteIns*) make_call_slot(idx, e2->nargs + 1));
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
    add_exp(e2->exp, compiler);
    IntValue* local = (IntValue*) ht_get(compiler->local_scope, e2->name);
    if (local) {
      add_ins(compiler, (ByteIns*) make_set_local(local->value));
    } else {
      IntValue* global = (IntValue*) ht_get(compiler->global_scope, e2->name);
      add_ins(compiler, (ByteIns*) make_set_global(local->value));
    }
    break;
  }
  case IF_EXP:{
    IfExp* e2 = (IfExp*)e;
    int conseq = add_label(compiler, CONSEQ_TAG);
    int end = add_label(compiler, END_TAG);
    add_exp(e2->pred, compiler);
    add_ins(compiler, (ByteIns*) make_branch(conseq));
    parse_scope(compiler, e2->alt);
    add_ins(compiler, (ByteIns*) make_goto(end));
    add_ins(compiler, (ByteIns*) make_label(conseq));
    parse_scope(compiler, e2->conseq);
    add_ins(compiler, (ByteIns*) make_label(end));
    break;
  }
  case WHILE_EXP:{
    WhileExp* e2 = (WhileExp*)e;
    int test = add_label(compiler, TEST_TAG);
    int loop = add_label(compiler, LOOP_TAG);
    add_ins(compiler, (ByteIns*) make_goto(test));
    add_ins(compiler, (ByteIns*) make_label(loop));
    parse_scope(compiler, e2->body);
    add_ins(compiler, make_ins(DROP_OP));
    add_ins(compiler, (ByteIns*) make_label(test));
    add_exp(e2->pred, compiler);
    add_ins(compiler, (ByteIns*) make_branch(loop));
    add_ins(compiler, (ByteIns*) make_lit(str_to_idx("NULL", compiler)));
    break;
  }
  case REF_EXP:{
    RefExp* e2 = (RefExp*)e;
    printf("%s", e2->name);
    IntValue* local = ht_get(compiler->local_scope, e2->name);
    if (local) {
      add_ins(compiler, (ByteIns* ) make_get_local(local->value));
    } else {
      IntValue* global = ht_get(compiler->local_scope, e2->name);
      add_ins(compiler, (ByteIns*) make_get_global(global->value));
    }
    break;
  }
  default:
    printf("Unrecognized Expression with tag %d\n", e->tag);
    exit(-1);
  }
}

