
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
    heap->size = MB;
    heap->memory = malloc(heap->size);
    heap->free = malloc(heap->size);
    heap->head = heap->memory + heap->size;
    heap->sp = heap->memory;
    return heap;
}

void switch_heap(Heap* heap) {
    char* current_heap = heap->memory;
    heap->memory = heap->free;
    heap->free = current_heap;
    heap->sp = heap->memory;
    heap->head = heap->memory + heap->size;
}

void free_heap(Heap* heap) {
    free(heap->memory);
    free(heap);
}

void* halloc (VM* vm, long tag, int sz) {
  if(vm->heap->sp + sz > vm->heap->head){
      printf("Calling GC.\n");
      run_gc(vm);
      if(vm->heap->sp + sz > vm->heap->head) {
        printf("Out of memory.");
        exit(-1);
      }
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
long BHEART = -1;

int get_obj_size(VM* vm, VMValue* obj) {
    int sz;
    switch(obj->tag) {
        case VM_NULL: 
            sz = sizeof(VMNull);
            break;
        case VM_INT:
            sz = sizeof(VMInt);
            break;
        case VM_ARRAY:
            VMArray* array = (VMArray*) obj;
            sz = sizeof(VMArray) + sizeof(void*) * array->length;
            break;
        default:
            CClass* class = vector_get(vm->classes, obj->tag);
            sz = sizeof(VMObj) + sizeof(void*) * class->nvars;
            break;
    }
    return sz;
}

void* forward_pointer(void* src, void* dst) {
    BHeart* bheart = (BHeart*) src;
    bheart->tag = BHEART;
    bheart->forwarding = dst;
}

void* copy_to_free(VM* vm, void* obj) {
    void* dst = vm->heap->sp;
    int sz = get_obj_size(vm, (VMValue*) obj);
    memcpy(dst, obj, sz);
    vm->heap->sp += sz;
    forward_pointer(obj, dst);
    return dst;
}

void* get_post_gc_ptr(VM* vm, void* obj) {
    long tag = ((long*)obj)[0];
    if (tag == BHEART) {
        BHeart* bheart = (BHeart*) obj;
        return bheart->forwarding;
    } else {
        return copy_to_free(vm, obj);
    }
}

void scan_stack(VM* vm){
    for (int i = 0; i < vm->stack->size; i++) {
        void* new_obj = get_post_gc_ptr(vm, vector_get(vm->stack, i));
        vector_set(vm->stack, i, new_obj);
    }
}

void scan_stack_frame(VM* vm) {
    int frame_start = vm->fstack->fp + 2;
    int frame_end = vm->fstack->stack->size;
    while (frame_end > 0) {
        for (int i = frame_start; i < frame_end; i++) {
            void* new_obj = get_post_gc_ptr(vm, vector_get(vm->fstack->stack, i));
            vector_set(vm->fstack->stack, i, new_obj);
        }
        frame_end = frame_start - 2;
        frame_start = ((int) vector_get(vm->fstack->stack, frame_start - 2)) + 2;
    }
}

void scan_globals(VM* vm) {
    int globals_len = sizeof(vm->genv) / sizeof(vm->genv[0]);
    for (int i = 0; i < globals_len; i++) {
        vm->genv[i] = get_post_gc_ptr(vm, vm->genv[i]);
    }
}

void scan_root_set(VM* vm) {
    scan_stack(vm);
    scan_stack_frame(vm);
    scan_globals(vm);
}

void scan_array(VM* vm, VMArray* array) {
    for (int i = 0; i < array->length; i++) {
        array->items[i] = get_post_gc_ptr(vm, array->items[i]);
    }
}

void scan_object(VM* vm, VMObj* obj) {
    CClass* class = vector_get(vm->classes, obj->tag);
    obj->parent = get_post_gc_ptr(vm, obj->parent);
    for (int i = 0; i < class->nvars; i++) {
        obj->slots[i] = get_post_gc_ptr(vm, obj->slots[i]);
    }
}

void scan_heap(VM* vm) {
    char* obj = vm->heap->memory;
    while (obj < vm->heap->sp) {// only scan till the items we have just added
        VMValue* value = (VMValue*) obj;
        switch(value->tag) {
            case VM_INT: // do nothing for int and null as they do not contain subitems
            case VM_NULL:
                break;
            case VM_ARRAY:
                scan_array(vm, (VMArray*) value);
                break;
            default:
                scan_object(vm, (VMObj*) value);
                break;
        }
        obj += get_obj_size(vm, value);
    }
}


void run_gc(VM* vm) {
    switch_heap(vm->heap);
    scan_root_set(vm);
    vm->null = get_post_gc_ptr(vm, vm->null);
    scan_heap(vm);
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
  VMInt* value = (VMInt*) args[2];
  VMArray* array = args[0];
  array->items[pos]= value;
  return create_null_or_int(vm, 0);
}

VMValue* fe_get(VM* vm, void** args) {
  int pos = ((VMInt*) args[1])->value;
  VMArray* array = args[0];
  VMInt* value = array->items[pos];
  return (VMValue*) value;
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

