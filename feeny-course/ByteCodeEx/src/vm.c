#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/mman.h>

#include "vm.h"

void add_globals(ht* hm, Vector* const_pool, Vector* globals);
void interpret(ht*, int entry_point);
void run(VM* vm);
void add_labels(ht* hm, Vector* const_pool);

void op_return(VM* vm);
void op_drop(VM* vm);
void op_goto(VM* vm, GotoIns* i);
void op_branch(VM* vm, BranchIns* i);
void op_get_global(VM* vm, GetGlobalIns* i);
void op_set_global(VM* vm, SetGlobalIns* i);
void op_get_local(VM* vm, GetLocalIns* i);
void op_set_local(VM* vm, SetLocalIns* i);
void op_call(VM* vm, CallIns* i);
void op_call_slot(VM* vm, CallSlotIns* i);
void op_set_slot(VM* vm, SetSlotIns* i);
void op_slot(VM* vm, SlotIns* i);
void op_object(VM* vm, ObjectIns* i);
void op_array(VM* vm);
void op_print(VM* vm, PrintfIns* i);
void op_lit(VM* vm, LitIns* i);

MethodValue* search_class_for_method(VM* vm, VMObj* class, int method_name);
Value* fe_set(void** args);
Value* fe_get(void** args);
Value* fe_len(void** args);
Value* fe_add(void** args);
Value* fe_sub(void** args);
Value* fe_mult(void** args);
Value* fe_div(void** args);
Value* fe_mod(void** args);
Value* fe_lt(void** args);
Value* fe_le(void** args);
Value* fe_gt(void** args);
Value* fe_ge(void** args);
Value* fe_eq(void** args);
Value* create_int(int a);
Value* create_null_or_int(int a);
ht* init_builtins();
Value* format_print(StringValue* format_string, void** args);

VM* init_vm(Program* p) {
  VM* vm = malloc(sizeof(VM));
  vm->stack = make_vector();
  vm->hm = ht_create();
  vm->labels = ht_create();
  vm->inbuilt = init_builtins();
  add_globals(vm->hm, p->values, p->slots);
  add_labels(vm->labels, p->values);
  MethodValue* entry_func = (MethodValue*) vector_get(p->values, p->entry);
  vm->IP = &entry_func->code->array[0];
  vm->current_frame =  make_frame(entry_func->nargs + entry_func->nlocals, 
                                  NULL, 
                                  vm->IP);
  vm->const_pool = p->values;
  print_prog(p);
  return vm;
}

void free_vm(VM* vm) {
  ht_destroy(vm->hm);
  ht_destroy(vm->inbuilt);
  ht_destroy(vm->labels);
  destroy_frame(vm->current_frame);
  vector_free(vm->stack);
  vector_free(vm->const_pool);
  free(vm->IP);
  free(vm);
}

void interpret_bc (Program* p) {
  #ifdef DEBUG
    printf("Interpreting Bytecode Program:\n");
    print_prog(p);
  #endif
  VM* vm = init_vm(p);
  run(vm);
  free(vm);
}

void add_labels(ht* hm, Vector* const_pool) {
  for (int i = 0; i < const_pool->size; i++) {
    Value* value = (Value*) vector_get(const_pool, i);
    if (value->tag == METHOD_VAL) {
      MethodValue* method = (MethodValue*) value;
      for (int j = 0; j < method->code->size; j++) {
        ByteIns* ins = vector_get(method->code, j);
        if (ins->tag == LABEL_OP) {
          char* key =  ((StringValue*)(vector_get(const_pool, ((LabelIns*)ins)->name)))->value;
          ByteIns* ins = (ByteIns*) method->code->array[j];
          ht_set(hm, key, &method->code->array[j]);
        }
      }
    }
  }
}

void add_globals(ht* hm, Vector* const_pool, Vector* globals) {
  for (int i = 0; i < globals->size; i++) {
    void* value_idx = vector_get(const_pool, (int)vector_get(globals, i));
    int name_idx = ((Value*) value_idx)->tag == SLOT_VAL ? ((SlotValue*) value_idx)->name : ((MethodValue*) value_idx)->name;
    char* name = ((StringValue*)vector_get(const_pool, name_idx))->value;
    ht_set(hm, name, value_idx);
  }
}

void op_return(VM* vm) {
  if (vm->current_frame->parent != NULL) {
    Frame* old_frame = vm->current_frame;
    vm->IP = vm->current_frame->return_address;
    vm->current_frame = vm->current_frame->parent;
    destroy_frame(old_frame);
  } else {
    exit(1);
  }
}

void op_drop(VM* vm) {
  vector_pop(vm->stack);
  vm->IP++;
}

void op_goto(VM* vm, GotoIns* i) {
  StringValue* value = (StringValue*) vector_get(vm->const_pool, i->name);
  vm->IP = ht_get(vm->labels, value->value);
}

void op_branch(VM* vm, BranchIns* i) {
  Value* value = (Value*) vector_pop(vm->stack);
  if (value->tag == NULL_VAL) {
    vm->IP++;
    return;
  }
  StringValue* label = (StringValue*) vector_get(vm->const_pool, i->name);
  vm->IP = ht_get(vm->labels, label->value);
}

void op_get_global(VM* vm, GetGlobalIns* i) {
  StringValue* string = (StringValue*) vector_get(vm->const_pool, i->name);
  vector_add(vm->stack, ht_get(vm->hm, string->value));
  vm->IP++;
}

void op_set_global(VM* vm, SetGlobalIns* i) {
  StringValue* string = (StringValue*) vector_get(vm->const_pool, i->name);
  ht_set(vm->hm, string->value, vector_peek(vm->stack));
  vm->IP++;
}

void op_get_local(VM* vm, GetLocalIns* i) {
  vector_add(vm->stack, vm->current_frame->variables[i->idx]);
  vm->IP++;
}

void op_set_local(VM* vm, SetLocalIns* i) {
  vm->current_frame->variables[i->idx] = vector_peek(vm->stack);
  vm->IP++;
}

void op_call(VM* vm, CallIns* i) {
  MethodValue* method = (MethodValue*) ht_get(vm->hm, ((StringValue*) vector_get(vm->const_pool, i->name))->value);
  vm->current_frame = make_frame(method->nargs + method->nlocals, vm->current_frame, vm->IP+1);
  for (int j = 0; j < i->arity; j++) {
    vm->current_frame->variables[i->arity - j - 1] = vector_pop(vm->stack);
  }
  vm->IP = &method->code->array[0];
} 

void op_call_slot(VM* vm, CallSlotIns* i) {
  char* method_name = ((StringValue*) vector_get(vm->const_pool, i->name))->value;
  void** args = malloc(sizeof(void*) * i->arity);
  for (int j = 0; j < i->arity; j++) {
      args[i->arity - j - 1] = vector_pop(vm->stack);
  }
  switch (((Value*)args[0])->tag) {
    case (ARRAY_VAL):
    case (INT_VAL): {
      Value* (*func)(void**) = ht_get(vm->inbuilt, method_name);
      Value* return_value = (*func)(args); 
      vector_add(vm->stack, (void*) return_value);
      vm->IP++;
      free(args);
      break;
    }
    default: {
      VMObj* object = args[0];
      MethodValue* method = search_class_for_method(vm, object, i->name);
      Frame* newFrame = make_frame(method->nargs + method->nlocals, vm->current_frame, vm->IP+1);
      for (int j = 0; j < i->arity; j++) {
        newFrame->variables[j] = args[j];
      }
      vm->current_frame = newFrame;
      vm->IP = &method->code->array[0];
      free(args);
      break;
    }
  }
}

void op_set_slot(VM* vm, SetSlotIns* i) {
  void* valToStore = vector_pop(vm->stack);
  VMObj* object = vector_pop(vm->stack);
  int varIdx = search_for_slot_idx(vm, object, i->name);
  object->slots[varIdx] = valToStore;
  vector_add(vm->stack, valToStore);
  vm->IP++;
}

void op_slot(VM* vm, SlotIns* i) {
  VMObj* object = vector_pop(vm->stack);
  int varIdx = search_for_slot_idx(vm, object, i->name);
  vector_add(vm->stack, object->slots[varIdx]);
  vm->IP++;
}

void op_object(VM* vm, ObjectIns* ins) {
  ClassValue* class = (ClassValue* ) vector_get(vm->const_pool, ins->class);
  VMObj* obj = NULL;
  for (int i = 0; i< class->slots->size; i++) {
    int j = 1;
    int pool_idx = (int) vector_get(class->slots, class->slots->size - i - 1);
    Value* slot = (Value* ) vector_get(vm->const_pool, pool_idx);
    if (slot->tag == METHOD_VAL) {
    } else {
      if (obj == NULL) {
        obj = malloc(sizeof(VMObj) + (sizeof(void*) * (class->slots->size - i)));
      }
      obj->slots[class->slots->size - i - j] = vector_pop(vm->stack);
      j++;
    }
  }
  if (obj == NULL) obj = malloc(sizeof(VMObj));
  obj->parent = vector_pop(vm->stack);
  obj->tag = ins->class;
  vector_add(vm->stack, obj);
  vm->IP++;
}

void op_array(VM* vm) {
  IntValue* initial = vector_pop(vm->stack);
  IntValue* len = vector_pop(vm->stack);
  VMArray* array = malloc(sizeof(VMArray) + (sizeof(void*) * len->value));
  for (int i = 0; i < len->value; i++) {
    array->items[i] = initial->value;
  }
  array->tag = ARRAY_VAL;
  array->length = len->value;
  vector_add(vm->stack, (void *) array);
  vm->IP++;
}

void op_print(VM* vm, PrintfIns* i) {
  StringValue* string_format = (StringValue*) vector_get(vm->const_pool, i->format);
  void** args = malloc(sizeof(void*) * i->arity);
  for (int j = 0; j < i->arity; j++) {
    args[i->arity - j - 1] = vector_pop(vm->stack);
  }
  Value* null = format_print(string_format, args);
  vector_add(vm->stack, null);
  vm->IP++;
}

void op_lit(VM* vm, LitIns* i) {
  vector_add(vm->stack, vector_get(vm->const_pool, i->idx));
  vm->IP++;
}

void run(VM* vm) {
  while(vm->IP != NULL) {
    ByteIns* ins = (ByteIns*) *vm->IP;
    #ifdef DEBUG
      printf("\n");
    #endif
    switch(ins->tag) {
      case LABEL_OP: {
        LabelIns* i = (LabelIns*)ins;
        #ifdef DEBUG
          printf("label #%d", i->name);
        #endif
        vm->IP++;
        break;
      }
      case LIT_OP: {
        LitIns* i = (LitIns*)ins;
        #ifdef DEBUG
          printf("   lit #%d", i->idx);
        #endif
        op_lit(vm, i);
        break;
      }
      case PRINTF_OP: {
        PrintfIns* i = (PrintfIns*)ins;
        #ifdef DEBUG
          printf("   printf #%d %d", i->format, i->arity);
        #endif
        op_print(vm, i);
        break;
      }
      case ARRAY_OP: {
        #ifdef DEBUG
          printf("   array");
        #endif
        op_array(vm);
        break;
      }
      case OBJECT_OP: {
        ObjectIns* i = (ObjectIns*)ins;
        #ifdef DEBUG
          printf("   object #%d", i->class);
        #endif
        op_object(vm, i);
        break;
      }
      case SLOT_OP: {
        SlotIns* i = (SlotIns*)ins;
        #ifdef DEBUG
          printf("   slot #%d", i->name);
        #endif
        op_slot(vm, i);
        break;
      }
      case SET_SLOT_OP: {
        SetSlotIns* i = (SetSlotIns*)ins;
        #ifdef DEBUG
          printf("   set-slot #%d", i->name);
        #endif
        op_set_slot(vm, i);
        break;
      }
      case CALL_SLOT_OP: {
        CallSlotIns* i = (CallSlotIns*)ins;
        #ifdef DEBUG
          printf("   call-slot #%d %d", i->name, i->arity);
        #endif
        op_call_slot(vm, i);
        break;
      }
      case CALL_OP: {
        CallIns* i = (CallIns*)ins;
        #ifdef DEBUG
          printf("   call #%d %d", i->name, i->arity);
        #endif
        op_call(vm, i);
        break;
      }
      case SET_LOCAL_OP: {
        SetLocalIns* i = (SetLocalIns*)ins;
        #ifdef DEBUG
          printf("   set local %d", i->idx);
        #endif
        op_set_local(vm, i);
        break;
      }
      case GET_LOCAL_OP: {
        GetLocalIns* i = (GetLocalIns*)ins;
        #ifdef DEBUG
          printf("   get local %d", i->idx);
        #endif
        op_get_local(vm, i);
        break;
      }
      case SET_GLOBAL_OP: {
        SetGlobalIns* i = (SetGlobalIns*)ins;
        #ifdef DEBUG
          printf("   set global #%d", i->name);
        #endif
        op_set_global(vm, i);
        break;
      }
      case GET_GLOBAL_OP: {
        GetGlobalIns* i = (GetGlobalIns*)ins;
        #ifdef DEBUG
          printf("   get global #%d", i->name);
        #endif
        op_get_global(vm, i);
        break;
      }
      case BRANCH_OP: {
        BranchIns* i = (BranchIns*)ins;
        #ifdef DEBUG
          printf("   branch #%d", i->name);
        #endif
        op_branch(vm, i);
        break;
      }
      case GOTO_OP: {
        GotoIns* i = (GotoIns*)ins;
        #ifdef DEBUG
          printf("   goto #%d", i->name);
        #endif
        op_goto(vm, i);
        break;
      }
      case RETURN_OP: {
        #ifdef DEBUG
          printf("   return");
        #endif
        op_return(vm);
        break;
      }
      case DROP_OP: {
        #ifdef DEBUG
          printf("   drop");
        #endif
        op_drop(vm);
        break;
      }
      default: {
        printf("Unknown instruction with tag: %u\n", ins->tag);
        exit(-1);
      }
    }
  }
}

//===================== UTILS ================================

MethodValue* search_class_for_method(VM* vm, VMObj* object, int method_name) {
    ClassValue* class = vector_get(vm->const_pool, object->tag);
    for (int j = 0; j < class->slots->size; j++) {
      int idx = (int) vector_get(class->slots, class->slots->size - j - 1);
      Value* value = vector_get(vm->const_pool, idx);
      if (value->tag == METHOD_VAL) {
        MethodValue* methodVal = (MethodValue* ) value;
        if (methodVal->name == method_name) {
          return methodVal;
        }
      }
    }
    search_class_for_method(vm, (VMObj*) vector_peek(object->parent), method_name);
}

int search_for_slot_idx(VM* vm, VMObj* object, int name) {
  ClassValue* class = (ClassValue*) vector_get(vm->const_pool, object->tag);
  for (int i = 0; i < class->slots->size; i++) {
    int parIdx = (int) vector_get(class->slots, i);
    SlotValue* parName = (SlotValue*) vector_get(vm->const_pool, parIdx);
    if (parName->name == name) {
      return i;
    }
  }
}

//===================== BUILTINS ================================


ht* init_builtins() {
  ht* inbuilt_hash = ht_create();
  ht_set(inbuilt_hash, "set", &fe_set);
  ht_set(inbuilt_hash, "get", &fe_get);
  ht_set(inbuilt_hash, "length", &fe_len);
  ht_set(inbuilt_hash, "add", &fe_add);
  ht_set(inbuilt_hash, "sub", &fe_sub);
  ht_set(inbuilt_hash, "mul", &fe_mult);
  ht_set(inbuilt_hash, "div", &fe_div);
  ht_set(inbuilt_hash, "mod", &fe_mod);
  ht_set(inbuilt_hash, "lt", &fe_lt);
  ht_set(inbuilt_hash, "le", &fe_le);
  ht_set(inbuilt_hash, "gt", &fe_gt);
  ht_set(inbuilt_hash, "ge", &fe_ge);
  ht_set(inbuilt_hash, "eq", &fe_eq);
  return inbuilt_hash;
}

Value* fe_set(void** args) {
  int pos = ((IntValue*) args[1])->value;
  int value = ((IntValue*) args[2])->value;
  VMArray* array = args[0];
  array->items[pos]= value;
  return create_null_or_int(0);
}

Value* fe_get(void** args) {
  int pos = ((IntValue*) args[1])->value;
  VMArray* array = args[0];
  int value = array->items[pos];
  return create_int(value);
}

Value* fe_len(void** args) {
  return create_int(((VMArray* ) args[0])->length);
}

Value* fe_add(void** args) {
  return create_int(((IntValue*) args[0])->value + ((IntValue*) args[1])->value);
}

Value* fe_sub(void** args) {
  return create_int(((IntValue*) args[0])->value - ((IntValue*) args[1])->value);
}

Value* fe_mult(void** args) {
  return create_int(((IntValue*) args[0])->value * ((IntValue*) args[1])->value);
}

Value* fe_div(void** args) {
  return create_int(((IntValue*) args[0])->value / ((IntValue*) args[1])->value);
}

Value* fe_mod(void** args) {
  return create_int(((IntValue*) args[0])->value % ((IntValue*) args[1])->value);
}

Value* fe_lt(void** args) {
  return create_null_or_int(((IntValue*) args[0])->value < ((IntValue*) args[1])->value);
}

Value* fe_le(void** args) {
  return create_null_or_int(((IntValue*) args[0])->value <= ((IntValue*) args[1])->value);
}

Value* fe_gt(void** args) {
  return create_null_or_int(((IntValue*) args[0])->value > ((IntValue*) args[1])->value);
}

Value* fe_ge(void** args) {
  return create_null_or_int(((IntValue*) args[0])->value >= ((IntValue*) args[1])->value);
}

Value* fe_eq(void** args) {
  return create_null_or_int(((IntValue*) args[0])->value == ((IntValue*) args[1])->value);
}

Value* create_null_or_int(int a) {
  if (a) {
    return (Value*) create_int(a);
  }
  else {
    Value* value = malloc(sizeof(Value));
    value->tag = NULL_VAL;
    return value;
  }
}

Value* create_int(int a) {
  IntValue* value = malloc(sizeof(IntValue));
  value->value = a;
  value->tag = INT_VAL;
  return (Value*) value;
}

Value* format_print(StringValue* format_string, void **args) {
    char *string = format_string->value;
    int i = 0;
    while (*string != '\0') {
        if (*string == '~') {
            printf("%d", ((IntValue*) args[i])->value);
            string++;
            i++;
        }
        printf("%c", *string);
        string++;
    }
    Value* null = (Value*) malloc(sizeof(NULL_VAL));
    null->tag = NULL_VAL;
    return null;
}
