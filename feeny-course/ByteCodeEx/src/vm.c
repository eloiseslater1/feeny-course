#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/mman.h>

#include "vm.h"

void add_globals(ht* hm, Vector* const_pool, Vector* globals);
void interpret(ht*, int entry_point);
void run(VM* vm);
void arthrimetic(char* string, Vector* stack);

VM* init_vm(Program* p) {
  VM* vm = malloc(sizeof(VM));
  vm->stack = make_vector();
  vm->hm = ht_create();
  add_globals(vm->hm, p->values, p->slots);
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
        break;
      }
      case LIT_OP: {
        LitIns* i = (LitIns*)ins;
        vector_add(vm->stack, vector_get(vm->const_pool, i->idx));
        printf("   lit #%d", i->idx);
        vm->IP++;
        break;
      }
      case PRINTF_OP: {
        PrintfIns* i = (PrintfIns*)ins;
        printf("   printf #%d %d", i->format, i->arity);
        StringValue* string_format = (StringValue*) vector_get(vm->const_pool, i->format);
        format_print(string_format, vm->stack, i->arity);
        vm->IP++;
        break;
      }
      case ARRAY_OP: {
        printf("   array");
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
        void** args = malloc(sizeof(void*) * i->arity);
        for (int j = 0; j < i->arity; j++) {
           args[i->arity - j - 1] = vector_pop(vm->stack);
        }
        char* method_name = ((StringValue*) vector_get(vm->const_pool, i->name))->value;
        do_function(method_name, vm->stack, args);
        IntValue* value  = (IntValue*) vector_peek(vm->stack);
        vm->IP++;
        break;
      }
      case CALL_OP: {
        CallIns* i = (CallIns*)ins;
        printf("   call #%d %d", i->name, i->arity);
        MethodValue* method = (MethodValue*) ht_get(vm->hm, ((StringValue*) vector_get(vm->const_pool, i->name))->value);
        Frame* current_frame = make_frame(method->nargs + method->nlocals, current_frame, vm->IP);
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
        break;
      }
      case GET_GLOBAL_OP: {
        GetGlobalIns* i = (GetGlobalIns*)ins;
        printf("   get global #%d", i->name);
        break;
      }
      case BRANCH_OP: {
        BranchIns* i = (BranchIns*)ins;
        printf("   branch #%d", i->name);
        break;
      }
      case GOTO_OP: {
        GotoIns* i = (GotoIns*)ins;
        printf("   goto #%d", i->name);
        break;
      }
      case RETURN_OP: {
        printf("   return");
        if (vm->current_frame->parent != NULL) {
          vm->IP = vm->current_frame->return_address++;
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
