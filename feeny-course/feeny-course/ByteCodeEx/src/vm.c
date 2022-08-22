#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/mman.h>

#include "vm.h"

void add_globals(ht* hm, Vector* const_pool, Vector* globals);
void interpret(ht*, int entry_point);
void run(VM* vm);
void arthrimetic(char* string, Vector* stack);
void add_labels(ht* hm, Vector* const_pool);
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
Value* format_print(void** args);

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
  return vm;
}

void free_vm(VM* vm) {
  ht_destroy(vm->hm);
  destroy_frame(vm->current_frame);
  vector_free(vm->stack);
  free(vm);
}

void interpret_bc (Program* p) {
  printf("Interpreting Bytecode Program:\n");
  print_prog(p);
  VM* vm = init_vm(p);
  run(vm);
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

void run(VM* vm) {
  while(vm->IP != NULL) {
    ByteIns* ins = (ByteIns*) *vm->IP;
    printf("\n");
    switch(ins->tag) {
      case LABEL_OP: {
        LabelIns* i = (LabelIns*)ins;
        printf("label #%d", i->name);
        vm->IP++;
        break;
      }
      case LIT_OP: {
        LitIns* i = (LitIns*)ins;
        printf("   lit #%d", i->idx);
        vector_add(vm->stack, vector_get(vm->const_pool, i->idx));
        vm->IP++;
        break;
      }
      case PRINTF_OP: {
        PrintfIns* i = (PrintfIns*)ins;
        printf("   printf #%d %d", i->format, i->arity);
        void** args = malloc(sizeof(void*) * i->arity + 1);
        args[0] = (StringValue*) vector_get(vm->const_pool, i->format);
        for (int j = 0; j < i->arity; j++) {
          args[i->arity - j] = vector_pop(vm->stack);
        }
        Value* null = format_print(args);
        vector_add(vm->stack, null);
        vm->IP++;
        break;
      }
      case ARRAY_OP: {
        printf("   array");
        IntValue* initial = (IntValue*) vector_pop(vm->stack);
        IntValue* len = (IntValue*) vector_pop(vm->stack);
        int* array = (int*) malloc(sizeof(int) * len->value);
        for (int i = 0; i < len->value; i++) {
          array[i] = initial->value;
        }
        ArrayValue* value = malloc(sizeof(ArrayValue));
        value->tag = ARRAY_VAL;
        value->value = array;
        value->len = len->value;
        vector_add(vm->stack, (void *) value);
        ArrayValue* arrayVal = (ArrayValue* ) vector_peek(vm->stack);
        vm->IP++;
        break;
      }
      case OBJECT_OP: {
        ObjectIns* i = (ObjectIns*)ins;
        printf("   object #%d", i->class);
        break;
      }
      case SLOT_OP: {
        SlotIns* i = (SlotIns*)ins;
        printf("   slot #%d", i->name);
        break;
      }
      case SET_SLOT_OP: {
        SetSlotIns* i = (SetSlotIns*)ins;
        printf("   set-slot #%d", i->name);
        break;
      }
      case CALL_SLOT_OP: {
        CallSlotIns* i = (CallSlotIns*)ins;
        printf("   call-slot #%d %d", i->name, i->arity);
        char* method_name = ((StringValue*) vector_get(vm->const_pool, i->name))->value;
        void** args = malloc(sizeof(void*) * i->arity);
        for (int j = 0; j < i->arity; j++) {
           args[i->arity - j - 1] = vector_pop(vm->stack);
        }
        printf("\nmethod: ");
        print_string(method_name);
        for (int k = 0; k < i->arity; k++) {
            printf("arg %d is: %d\n", k, ((IntValue*) args[k])->value);
        }
        Value* reciever = ((Value*)args[0]); 
        switch (reciever->tag) {
          case (ARRAY_VAL):
          case (INT_VAL): {
            Value* (*func)(void**) = ht_get(vm->inbuilt, method_name);
            Value* return_value = (*func)(args); 
            vector_add(vm->stack, (void*) return_value);
            vm->IP++;
            break;
          }
          default:
            printf("Unknown value");
            exit(-1);
        }
        break;
      }
      case CALL_OP: {
        CallIns* i = (CallIns*)ins;
        printf("   call #%d %d", i->name, i->arity);
        MethodValue* method = (MethodValue*) ht_get(vm->hm, ((StringValue*) vector_get(vm->const_pool, i->name))->value);
        vm->current_frame = make_frame(method->nargs + method->nlocals, vm->current_frame, vm->IP+1);
        for (int j = 0; j < i->arity; j++) {
          vm->current_frame->variables[i->arity - j - 1] = vector_pop(vm->stack);
        }
        vm->IP = &method->code->array[0];
        break;
      }
      case SET_LOCAL_OP: {
        SetLocalIns* i = (SetLocalIns*)ins;
        printf("   set local %d", i->idx);
        vm->current_frame->variables[i->idx] = (IntValue*) vector_peek(vm->stack);
        vm->IP++;
        break;
      }
      case GET_LOCAL_OP: {
        GetLocalIns* i = (GetLocalIns*)ins;
        vector_add(vm->stack, vm->current_frame->variables[i->idx]);
        printf("   get local %d", i->idx);
        vm->IP++;
        break;
      }
      case SET_GLOBAL_OP: {
        SetGlobalIns* i = (SetGlobalIns*)ins;
        printf("   set global #%d", i->name);
        StringValue* string = (StringValue*) vector_get(vm->const_pool, i->name);
        ht_set(vm->hm, string->value, vector_peek(vm->stack));
        vm->IP++;
        break;
      }
      case GET_GLOBAL_OP: {
        GetGlobalIns* i = (GetGlobalIns*)ins;
        printf("   get global #%d", i->name);
        StringValue* string = (StringValue*) vector_get(vm->const_pool, i->name);
        vector_add(vm->stack, ht_get(vm->hm, string->value));
        vm->IP++;
        break;
      }
      case BRANCH_OP: {
        BranchIns* i = (BranchIns*)ins;
        printf("   branch #%d", i->name);
        Value* value = (Value*) vector_pop(vm->stack);
        if (value->tag != NULL_VAL) {
          StringValue* value = (StringValue*) vector_get(vm->const_pool, i->name);
          vm->IP = ht_get(vm->labels, value->value);
          continue;
        }
        vm->IP++;
        break;
      }
      case GOTO_OP: {
        GotoIns* i = (GotoIns*)ins;
        printf("   goto #%d", i->name);
        StringValue* value = (StringValue*) vector_get(vm->const_pool, i->name);
        vm->IP = ht_get(vm->labels, value->value);
        ByteIns* ins = (ByteIns*) *vm->IP;
        break;
      }
      case RETURN_OP: {
        printf("   return");
        if (vm->current_frame->parent != NULL) {
          vm->IP = vm->current_frame->return_address;
          vm->current_frame = vm->current_frame->parent;
        } else {
          exit(1);
        }
        break;
      }
      case DROP_OP: {
        printf("   drop");
        vector_pop(vm->stack);
        vm->IP++;
        break;
      }
      default: {
        printf("Unknown instruction with tag: %u\n", ins->tag);
        exit(-1);
      }
    }
  }
  free_vm(vm);
}


//===================== BUILTINS ================================


ht* init_builtins() {
  ht* inbuilt_hash = ht_create();
  ht_set(inbuilt_hash, "set", &fe_set);
  ht_set(inbuilt_hash, "get", &fe_get);
  ht_set(inbuilt_hash, "length", &fe_len);
  ht_set(inbuilt_hash, "add", &fe_add);
  ht_set(inbuilt_hash, "sub", &fe_sub);
  ht_set(inbuilt_hash, "mult", &fe_mult);
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
  IntValue* pos = ((IntValue*) args[1]);
  IntValue* value = ((IntValue*) args[2]);
  ((ArrayValue*) args[0])->value[((IntValue*) args[1])->value] = value->value;
  printf("array value at position: %d is: %d\n", pos->value, ((ArrayValue*) args[0])->value[pos->value]);
  return create_null_or_int(0);
}

Value* fe_get(void** args) {
  printf("hello");
  int value = ((ArrayValue*) args[0])->value[((IntValue*) args[1])->value];
  return create_null_or_int(value);
}

Value* fe_len(void** args) {
  return create_int(((ArrayValue* ) args[0])->len);
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

Value* format_print(void **args) {
    printf("\n");
    char *string = ((StringValue*)args[0])->value;
    int i = 1;
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
