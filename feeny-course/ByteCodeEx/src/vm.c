
#include "vm.h"

//---------------------------------------------------------------------------
//--------------------------------init_vm------------------------------------
//---------------------------------------------------------------------------
StackFrame* init_frame();
void free_frame(StackFrame* stack_frame);
ht* init_builtins();
void init_genv(VM* vm, int globals_size);
void runvm (VM* vm);
Heap* init_heap();
void free_heap(Heap* heap);
VMValue* create_null(VM* vm);

VM* init_vm(VMInfo* vm_info) {
    VM* vm = malloc(sizeof(VM));
    vm->classes = vm_info->classes;
    vm->code_buffer = vm_info->code_buffer;
    vm->fstack = init_frame();
    vm->stack = make_vector();
    vm->heap = init_heap();
    vm->null = create_null(vm);
    vm->inbuilt = init_builtins();
    vm->ip = vm_info->ip;
    init_genv(vm, vm_info->globals_size);
    return vm;
}

void init_genv(VM* vm, int globals_size) {
    vm->genv = malloc(sizeof(void*) * globals_size);
    for (int i = 0; i < globals_size; i++) {
        vm->genv[i] = vm->null;
    }
}

void free_vm(VM* vm) {
    vector_free(vm->classes);
    free_code_buffer(vm->code_buffer);
    free_frame(vm->fstack);
    vector_free(vm->stack);
    free_heap(vm->heap);
    ht_destroy(vm->inbuilt);
    free(vm->genv);
    free(vm);
}

void interpret_bc(Program* program) {
    VMInfo* info = quicken_vm(program);
    VM* vm = init_vm(info);
    runvm(vm);
    free_vm(vm);
}



//---------------------------------------------------------------------------
//===================== Classes ==================================================
//---------------------------------------------------------------------------

CSlot get_slot(VM* vm, VMObj* obj, char* name) {
    CClass* class = vector_get(vm->classes, obj->tag);
    for (int i = 0; i < class->nslots; i++) {
        CSlot slot = class->slots[i];
        if (strcmp(name, slot.name) == 0) return slot;
    }
    return get_slot(vm, obj->parent, name);
}

//---------------------------------------------------------------------------
//===================== VM ==================================================
//---------------------------------------------------------------------------

unsigned char next_char (VM* vm) {
  unsigned char c = vm->ip[0];
  vm->ip++;
  return c;
}

int next_short (VM* vm) {
  vm->ip = (char*)(((long)vm->ip + 1)&(-2));
  int s = ((unsigned short*)vm->ip)[0];  
  vm->ip += 2;
  return s;
}

int next_int (VM* vm) {
  vm->ip = (char*)(((long)vm->ip + 3)&(-4));
  int s = ((int*)vm->ip)[0];  
  vm->ip += 4;
  return s;
}

void* next_ptr (VM* vm) {
  vm->ip = (char*)(((long)vm->ip + 7)&(-8));
  void* s = ((void**)vm->ip)[0];  
  vm->ip += 8;
  return s;
}

void* format_print(char* string, void **args) {
    int i = 0;
    while (*string != '\0') {
        if (*string == '~') {
            printf("%ld", ((VMInt*) args[i])->value);
            string++;
            i++;
        }
        printf("%c", *string);
        string++;
    }
}
//---------------------------------------------------------------------------
//--------------------------------StackFrame------------------------------------
//---------------------------------------------------------------------------


StackFrame* init_frame() {
    StackFrame* frame = malloc(sizeof(StackFrame));
    frame->stack = make_vector();
    frame->fp = 0;
}

void free_frame(StackFrame* stack_frame) {
    vector_free(stack_frame->stack);
    free(stack_frame);
}

void add_frame(VM* vm) {
    int nargs = next_int(vm);
    int nlocals = next_int(vm);
    vm->fstack->fp = (vm->fstack->stack->size > 0) ? vm->fstack->stack->size - 2 : 0; //size = 9, fp = 7
    vector_set_length(vm->fstack->stack, vm->fstack->stack->size + nargs + nlocals, vm->null);
    for (int i = nargs; i > 0; i--) {
        vector_set(vm->fstack->stack, vm->fstack->fp + 1 + i, vector_pop(vm->stack));
    }
}

//---------------------------------------------------------------------------
//--------------------------------heap------------------------------------
//---------------------------------------------------------------------------
void run_gc(VM* vm);
Heap* init_heap() {
    Heap* heap = malloc(sizeof(Heap));
    heap->size = MB * 10;
    heap->memory = malloc(heap->size);
    heap->head = heap->memory + heap->size;
    heap->sp = heap->memory;
    return heap;
}

void free_heap(Heap* heap) {
    free(heap->memory);
    free(heap);
}

void* halloc (VM* vm, long tag, int sz) {
  if(vm->heap->sp + sz > vm->heap->head){
      printf("Out of Memory.\n");
      run_gc(vm);
      exit(-1);
    }
  long* obj = (long*)vm->heap->sp;
  obj[0] = tag;
  vm->heap->sp += sz;
  return obj;
}

VMValue* create_null(VM* vm) {
  VMNull* null = (VMNull*) halloc(vm, VM_NULL, sizeof(VMNull));
  return (VMValue*) null;
}

VMInt* create_int(VM* vm, int a) {
  VMInt* value = (VMInt*) halloc(vm, VM_INT, sizeof(VMInt));
  value->value = a;
  return value;
}

VMArray* create_array(VM* vm, int length, int initial) {
    VMArray* array = (VMArray*) halloc(vm, VM_ARRAY, sizeof(VMArray) + sizeof(void*) * length);
    for (int i = 0; i < length; i++) {
        array->items[i] = initial;
    }
    array->length = length;
    return array;
}

VMObj* create_object(VM* vm, int class, int arity) {
  VMObj* obj = halloc(vm, class, sizeof(VMObj) + sizeof(void*) * arity);
}
//---------------------------------------------------------------------------
//--------------------------------run gc------------------------------------
//---------------------------------------------------------------------------
void scan_stack(VM* vm){
    for (int i = 0; i < vm->stack->size; i++) {
        void* ptr = vector_get(vm->stack, i);
        printf("stack pointer at %i is: %p and in heap: %d\n", i, ptr, check_within_heap(vm->heap, ptr));
    }
}

void scan_stack_frame(VM* vm) {
    for (int i = 0; i < vm->fstack->stack->size; i++) {
        void* ptr = vector_get(vm->fstack->stack, i);
        printf("frame pointer at %i is: %p and in heap: %d\n", i, ptr, check_within_heap(vm->heap, ptr)); 
    }
}

void scan_root_set(VM* vm) {
    printf("start of heap is: %p\n", vm->heap->memory);
    printf("end of heap is: %p\n", vm->heap->head);
    scan_stack(vm);
    scan_stack_frame(vm);
}

int check_within_heap(Heap* heap, void* pointer) {
    if (pointer >= heap->memory && pointer <= heap->head) return 1;
    return 0;
}

void run_gc(VM* vm) {
    scan_root_set(vm);
}

//---------------------------------------------------------------------------
//--------------------------------run vm------------------------------------
//---------------------------------------------------------------------------

void runvm (VM* vm) {  
  while (vm->ip) {
    int tag = next_int(vm);
    //printf("tag is: %d", tag);
    #ifdef DEBUG
        printf("STACK: ");
        for (int i = 0; i < vm->stack->size; i++) {
            VMValue* value = vector_get(vm->stack, i);
            switch(value->tag) {
                case (VM_INT): {
                    printf("int: %ld, ", ((VMInt*) value)->value);
                    break;
                } case (NULL_VAL): {
                    printf("null, ", ((VMInt*) value)->value);
                    break;
                } case (VM_ARRAY): {
                    VMArray* array = (VMArray*) value;
                    printf("(");
                    for(int i = 0; i < array->length; i++) {
                        printf("%d, ", (int) array->items[i]);
                    }
                    printf(")");
                } default :{
                    VMObj* obj = (VMObj*) value;
                    printf("Class #%ld", obj->tag);
                }
            }
            }
        printf("\n");
    #endif
    switch (tag) {
        case INT_INS : {
            VMInt* value = create_int(vm, next_int(vm));
            #ifdef DEBUG
                printf("int ins: tag: %ld, val: %ld\n", value->tag, value->value);
            #endif
            vector_add(vm->stack, value);
            break;
        }
        case NULL_INS : {
            #ifdef DEBUG
                printf("null ins\n");
            #endif
            VMValue* value = create_null(vm);
            vector_add(vm->stack, value);
            break;
        }
        case PRINTF_INS : {
            int nargs = next_int(vm);
            char* str = next_ptr(vm);
            #ifdef DEBUG
                printf("print: %d and str: %s\n", nargs, str);
            #endif
            void** args = malloc(sizeof(void*) * nargs);
            for (int i = 0; i< nargs; i++) {
                args[nargs - i- 1] =  vector_pop(vm->stack);
            }
            format_print(str, args);
            free(args);
            vector_add(vm->stack, vm->null);
            break;
        }
        case ARRAY_INS : {
            #ifdef DEBUG
                printf("array\n");
            #endif
            VMInt* intial = vector_pop(vm->stack);
            VMInt* length = vector_pop(vm->stack);
            vector_add(vm->stack, create_array(vm, length->value, intial->value));
            break;
        }
        case OBJECT_INS: {
            int arity = next_int(vm);
            int class = next_int(vm);
            #ifdef DEBUG
                printf("object \n");
            #endif
            VMObj* obj = create_object(vm, class, arity);
            for (int i = arity - 1; i >= 0; i--) {
                obj->slots[i] = vector_pop(vm->stack);
            }
            obj->parent = vector_pop(vm->stack);
            vector_add(vm->stack, obj);
            break;
        }
        case SLOT_INS: {
            char* name = next_ptr(vm);
            VMObj* obj = vector_pop(vm->stack);
            CSlot slot = get_slot(vm, obj, name);
            vector_add(vm->stack, obj->slots[slot.idx]);
            break;
        }
        case SET_SLOT_INS: {
            char* name = next_ptr(vm);
            VMValue* value = vector_pop(vm->stack);
            VMObj* obj = vector_pop(vm->stack);
            CSlot slot = get_slot(vm, obj, name);
            obj->slots[slot.idx] = value;
            vector_add(vm->stack, value);
            break;
        }
        case CALL_SLOT_INS: {
            int arity = next_int(vm);
            char* name = next_ptr(vm);
            #ifdef DEBUG
                printf("call-op #%d and str: %s\n", arity, name);
            #endif
            VMValue* value = vector_get(vm->stack, vm->stack->size - arity);
            switch (value->tag) {
                case (VM_ARRAY):
                case (VM_INT): {
                    void** args = malloc(sizeof(void*) * arity);
                    for (int i = 0; i < arity; i++) {
                        args[arity - i - 1] = vector_pop(vm->stack);
                    }
                    VMValue* (*func)(VM*, void**) = ht_get(vm->inbuilt, name);
                    VMValue* return_value = (*func)(vm, args); 
                    vector_add(vm->stack, return_value);
                    free(args);
                    break;
                }
                case (VM_NULL): {
                    printf("No slots can be called on null value");
                    exit(-1);
                } default: {
                    VMObj* obj = (VMObj*) value;
                    CSlot slot = get_slot(vm, obj, name);
                    vector_add(vm->fstack->stack, (void*) vm->fstack->fp);
                    vector_add(vm->fstack->stack, vm->ip);
                    vm->ip = slot.code;
                }
            }
            break;
        }
        case CALL_INS : {
            int arity = next_int(vm);
            void* new_code = next_ptr(vm);
            #ifdef DEBUG
                printf("calls #%d and ptr: %p\n", arity, new_code);
            #endif
            vector_add(vm->fstack->stack, (void*) vm->fstack->fp);
            vector_add(vm->fstack->stack, vm->ip);
            vm->ip = new_code;
            break;
        }
        case SET_LOCAL_INS : {
            int idx = next_int(vm);
            #ifdef DEBUG
                printf("set local : %d at: %d\n", idx, vm->fstack->fp + 2 + idx);
            #endif
            vector_set(vm->fstack->stack, vm->fstack->fp + 2 + idx, vector_peek(vm->stack));
            break;
        }
        case GET_LOCAL_INS : {
            int idx = next_int(vm);
            #ifdef DEBUG
                printf("get local : %d\n", idx);
            #endif
            VMInt* value = vector_get(vm->fstack->stack, vm->fstack->fp + 2 + idx);
            vector_add(vm->stack, vector_get(vm->fstack->stack, vm->fstack->fp + 2 + idx));
            break;
        }
        case SET_GLOBAL_INS : {
            int idx = next_int(vm);
            #ifdef DEBUG
                printf("set global : %d\n", idx);
            #endif
            vm->genv[idx] = vector_peek(vm->stack);
            break;
        }
        case GET_GLOBAL_INS : {
            int idx = next_int(vm);
            #ifdef DEBUG
                printf("get global : %d\n", idx);
            #endif
            vector_add(vm->stack, vm->genv[idx]);
            break;
        }
        case BRANCH_INS : {
            void* new_ptr = next_ptr(vm);
            VMValue* value = vector_pop(vm->stack);
            #ifdef DEBUG
                printf("branch tag: %ld, ptr: %p\n", value->tag, new_ptr);
            #endif
            if(value->tag != VM_NULL) vm->ip = new_ptr;
            break;
        }
        case GOTO_INS : {
            void* ptr = next_ptr(vm);
            #ifdef DEBUG
                printf("goto ins, ptr: %p\n", ptr);
            #endif
            vm->ip = ptr;
            break;
        }
        case RETURN_INS : {
            #ifdef DEBUG
                printf("return ins\n");
            #endif
            if (vm->fstack->stack->size == 0) return;
            int old_fp = (int) vector_get(vm->fstack->stack, vm->fstack->fp);
            vm->ip = vector_get(vm->fstack->stack, vm->fstack->fp + 1); 
            vector_set_length(vm->fstack->stack, vm->fstack->fp, vm->null);
            vm->fstack->fp = old_fp;
            break;
        }
        case DROP_INS : {
            #ifdef DEBUG
                printf("drop ins\n");
            #endif
            vector_pop(vm->stack);
            break;
        }
        case FRAME_INS : {
            #ifdef DEBUG
                printf("frame ins\n");
            #endif
            add_frame(vm);
            break;
        }
        default: {
            printf("Unknown tag: %d\n", tag);
            exit(-1);
        }
    }  
  }
}



//---------------------------------------------------------------------------
//===================== BUILTINS ============================================
//---------------------------------------------------------------------------

VMValue* create_null_or_int(VM* vm, long a) {
  if (a) {
    return (VMValue*) create_int(vm, a);
  }
  else {
    return create_null(vm);
  }
}

VMValue* fe_set(VM* vm, void** args) {
  int pos = ((VMInt*) args[1])->value;
  int value = ((VMInt*) args[2])->value;
  VMArray* array = args[0];
  array->items[pos]= value;
  return create_null_or_int(vm, 0);
}

VMValue* fe_get(VM* vm, void** args) {
  int pos = ((VMInt*) args[1])->value;
  VMArray* array = args[0];
  int value = array->items[pos];
  return (VMValue*) create_int(vm, value);
}

VMValue* fe_len(VM* vm, void** args) {
  return (VMValue*) create_int(vm, ((VMArray* ) args[0])->length);
}

VMValue* fe_add(VM* vm, void** args) {
  return (VMValue*) create_int(vm, ((VMInt*) args[0])->value + ((VMInt*) args[1])->value);
}

VMValue* fe_sub(VM* vm, void** args) {
  return (VMValue*) create_int(vm, ((VMInt*) args[0])->value - ((VMInt*) args[1])->value);
}

VMValue* fe_mult(VM* vm, void** args) {
  return (VMValue*) create_int(vm, ((VMInt*) args[0])->value * ((VMInt*) args[1])->value);
}

VMValue* fe_div(VM*vm, void** args) {
  return (VMValue*) create_int(vm, ((VMInt*) args[0])->value / ((VMInt*) args[1])->value);
}

VMValue* fe_mod(VM* vm, void** args) {
  return (VMValue*) create_int(vm, ((VMInt*) args[0])->value % ((VMInt*) args[1])->value);
}

VMValue* fe_lt(VM* vm, void** args) {
  return create_null_or_int(vm, ((VMInt*) args[0])->value < ((VMInt*) args[1])->value);
}

VMValue* fe_le(VM* vm, void** args) {
  return create_null_or_int(vm, ((VMInt*) args[0])->value <= ((VMInt*) args[1])->value);
}

VMValue* fe_gt(VM* vm, void** args) {
  return create_null_or_int(vm, ((VMInt*) args[0])->value > ((VMInt*) args[1])->value);
}

VMValue* fe_ge(VM* vm, void** args) {
  return create_null_or_int(vm, ((VMInt*) args[0])->value >= ((VMInt*) args[1])->value);
}

VMValue* fe_eq(VM* vm, void** args) {
  return create_null_or_int(vm, ((VMInt*) args[0])->value == ((VMInt*) args[1])->value);
}

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

