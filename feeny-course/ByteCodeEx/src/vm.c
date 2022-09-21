
#include "vm.h"

//---------------------------------------------------------------------------
//--------------------------------init_vm------------------------------------
//---------------------------------------------------------------------------
StackFrame* init_frame();
void free_frame(StackFrame* stack_frame);
void init_genv(VM* vm, int globals_size);
void runvm (VM* vm);
Heap* init_heap();
void free_heap(Heap* heap);
//VMValue* create_null(VM* vm);
intptr_t create_null();
void int_function_call(VM* vm, char* name);
void array_function_call(VM* vm, char* name);

VM* init_vm(VMInfo* vm_info) {
    VM* vm = malloc(sizeof(VM));
    vm->classes = vm_info->classes;
    vm->code_buffer = vm_info->code_buffer;
    vm->fstack = init_frame();
    vm->stack = make_vector();
    vm->heap = init_heap();
    //vm->null = NULL;
    //vm->null = create_null(vm);
    vm->null = create_null();
    vm->ip = vm_info->ip;
    init_genv(vm, vm_info->globals_size);
    return vm;
}

void init_genv(VM* vm, int globals_size) {
    vm->genv = malloc(sizeof(void*) * globals_size);
    for (int i = 0; i < globals_size; i++) {
        vm->genv[i] = vm->null;
    }
    vm->genv_size = globals_size;
}

void free_vm(VM* vm) {
    vector_free(vm->classes);
    free_code_buffer(vm->code_buffer);
    free_frame(vm->fstack);
    vector_free(vm->stack);
    free_heap(vm->heap);
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
//--------------------------------Tagging------------------------------------
//---------------------------------------------------------------------------

typedef enum {
    INT_PTAG,
    OBJ_PTAG,
    NULL_PTAG,
} PTAG;

static const intptr_t tagMask = 7;
static const intptr_t pointerMask = -tagMask;

intptr_t create_int(int value) {
    return (intptr_t) value << 3;
}

int get_int(intptr_t value) {
    return (int) value >> 3;
}

intptr_t create_null() {
    return (intptr_t) NULL_PTAG; 
}

intptr_t set_obj_bit(VMValue* ptr) {
    return ((intptr_t) ptr) | 1;
}

VMValue* get_obj(intptr_t ptr) {
    return (VMValue*) (ptr & ~1);
}

int get_tag_value(intptr_t value) {
    return value & tagMask;
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

void* format_print(VM* vm, char* string, int nargs) {
    int i = 0;
    while (*string != '\0') {
        if (*string == '~') {
            printf("%d", get_int((intptr_t) vector_get(vm->stack, vm->stack->size - nargs + i)));
            string++;
            i++;
        }
        printf("%c", *string);
        string++;
    }
    for (int i = 0; i < nargs; i++) {
        vector_pop(vm->stack);
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
    vector_set_length(vm->fstack->stack, vm->fstack->stack->size + nargs + nlocals, (void*) vm->null);
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
    heap->size = MB / 1000;
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

VMArray* create_array(VM* vm, intptr_t length) {
    int int_length = get_int(length);
    VMArray* array = (VMArray*) halloc(vm, VM_ARRAY, sizeof(VMArray) + sizeof(void*) * int_length);
    array->length = int_length;
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

intptr_t forward_pointer(void* src, void* dst) {
    BHeart* bheart = (BHeart*) src; 
    bheart->tag = BHEART;
    bheart->forwarding = set_obj_bit(dst);
    return bheart->forwarding;
}

intptr_t copy_to_free(VM* vm, void* obj) {
    void* dst = vm->heap->sp;
    int sz = get_obj_size(vm, (VMValue*) obj);
    memcpy(dst, obj, sz);
    vm->heap->sp += sz;
    return forward_pointer(obj, dst);
}

intptr_t get_post_gc_ptr(VM* vm, intptr_t obj_ptr) {
    switch(get_tag_value(obj_ptr)) {
        case INT_PTAG:
        case NULL_PTAG:
            return obj_ptr;
        case OBJ_PTAG:
            void* obj = (void*) get_obj(obj_ptr);
            long tag = ((long*)obj)[0];
            if (tag == BHEART) {
                BHeart* bheart = (BHeart*) obj;
                return bheart->forwarding;
            } else {
                return copy_to_free(vm, obj);
            }
    }
}

void scan_stack(VM* vm) {
    for (int i = 0; i < vm->stack->size; i++) {
        intptr_t new_obj = get_post_gc_ptr(vm, (intptr_t) vector_get(vm->stack, i));
        vector_set(vm->stack, i, (void*) new_obj);
    }
}

void scan_stack_frame(VM* vm) {
    int frame_start = vm->fstack->fp + 2;
    int frame_end = vm->fstack->stack->size;
    while (frame_end > 0) {
        for (int i = frame_start; i < frame_end; i++) {
            intptr_t new_obj = get_post_gc_ptr(vm, (intptr_t) vector_get(vm->fstack->stack, i));
            vector_set(vm->fstack->stack, i, (void*) new_obj);
        }
        frame_end = frame_start - 2;
        frame_start = ((int) vector_get(vm->fstack->stack, frame_start - 2)) + 2;
    }
}

void scan_globals(VM* vm) {
    for (int i = 0; i < vm->genv_size; i++) {
        vm->genv[i] = get_post_gc_ptr(vm, (intptr_t) vm->genv[i]);
    }
}

void scan_root_set(VM* vm) {
    scan_stack(vm);
    scan_stack_frame(vm);
    scan_globals(vm);
}

void scan_array(VM* vm, VMArray* array) {
    for (int i = 0; i < array->length; i++) {
        array->items[i] = get_post_gc_ptr(vm, (intptr_t) array->items[i]);
    }
}

void scan_object(VM* vm, VMObj* obj) {
    CClass* class = vector_get(vm->classes, obj->tag);
    obj->parent = get_obj(get_post_gc_ptr(vm, set_obj_bit((VMValue*) obj->parent)));
    for (int i = 0; i < class->nvars; i++) {
        obj->slots[i] = get_post_gc_ptr(vm, (intptr_t) obj->slots[i]);
    }
}

void scan_heap(VM* vm) {
    char* obj = vm->heap->memory;
    while (obj < vm->heap->sp) {
        VMValue* value = (VMValue*) obj;
        switch(value->tag) {
            case VM_INT: 
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
            intptr_t value = (intptr_t) vector_get(vm->stack, i);
            switch(get_tag_value(value)) {
                case INT_PTAG: 
                    printf("int: %d, ", get_int(value));
                    break;
                case NULL_PTAG:
                    printf("null, ");
                    break;
                case OBJ_PTAG:
                    VMValue* value2 = get_obj(value);
                    switch(value2->tag) {
                        case VM_ARRAY:
                            VMArray* array = (VMArray*) value2;
                            printf("(");
                            for(int i = 0; i < array->length; i++) {
                                printf("%d, ", get_int(array->items[i]));
                            }
                            printf(")");
                            break;
                        default :{
                            VMObj* obj = (VMObj*) value2;
                            printf("Class #%ld", obj->tag);
                        }
                    }
                    break;
                }
        }
        printf("\n");
    #endif
    switch (tag) {
        case INT_INS : {
            int i = next_int(vm);
            intptr_t value = create_int(i);
            #ifdef DEBUG
                printf("int ins val: %d\n", i);
            #endif
            vector_add(vm->stack, (void*) value);
            break;
        }
        case NULL_INS : {
            #ifdef DEBUG
                printf("null ins\n");
            #endif
            intptr_t value = vm->null;
            vector_add(vm->stack, (void*) value);
            break;
        }
        case PRINTF_INS : {
            int nargs = next_int(vm);
            char* str = next_ptr(vm);
            #ifdef DEBUG
                printf("print: %d and str: %s\n", nargs, str);
            #endif
            format_print(vm, str, nargs);
            vector_add(vm->stack, (void*) vm->null);
            break;
        }
        case ARRAY_INS : {
            #ifdef DEBUG
                printf("array\n");
            #endif
            intptr_t length = (intptr_t) vector_get(vm->stack, vm->stack->size - 2);
            VMArray* array = create_array(vm, length);
            intptr_t initial = (intptr_t) vector_pop(vm->stack);
            vector_pop(vm->stack);
            for (int i = 0; i < array->length; i++) {
                array->items[i] = initial;
            }
            vector_add(vm->stack, (void*) set_obj_bit((VMValue*) array));
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
            intptr_t parent_ptr = vector_pop(vm->stack);
            obj->parent = (VMObj*) get_obj(parent_ptr);
            vector_add(vm->stack, (void*) set_obj_bit((VMValue*) obj));
            break;
        }
        case SLOT_INS: {
            char* name = next_ptr(vm);
            intptr_t obj = (intptr_t) vector_pop(vm->stack);
            VMObj* vm_obj = (VMObj*) get_obj(obj);
            CSlot slot = get_slot(vm, vm_obj, name);
            vector_add(vm->stack, vm_obj->slots[slot.idx]);
            break;
        }
        case SET_SLOT_INS: {
            char* name = next_ptr(vm);
            intptr_t value = (intptr_t) vector_pop(vm->stack);
            intptr_t obj =  (intptr_t) vector_pop(vm->stack);
            VMObj* vm_obj = get_obj(obj);
            CSlot slot = get_slot(vm, vm_obj, name);
            vm_obj->slots[slot.idx] = value;
            vector_add(vm->stack, (void*) value);
            break;
        }
        case CALL_SLOT_INS: {
            int arity = next_int(vm);
            char* name = next_ptr(vm);
            #ifdef DEBUG
                printf("call-op #%d and str: %s\n", arity, name);
            #endif
            intptr_t ptr = (intptr_t) vector_get(vm->stack, vm->stack->size - arity);
            switch(get_tag_value(ptr)) {
                case INT_PTAG: {
                    int_function_call(vm, name);
                    break;
                } case NULL_PTAG: {
                    printf("No slots can be called on null value");
                    exit(-1); 
                } case OBJ_PTAG: {
                    VMValue* value = get_obj(ptr);
                    switch(value->tag) {
                        case VM_ARRAY: {
                            array_function_call(vm, name);
                            break;
                        }
                        default : {
                            VMObj* obj = (VMObj*) value;
                            CSlot slot = get_slot(vm, obj, name);
                            vector_add(vm->fstack->stack, (void*) vm->fstack->fp);
                            vector_add(vm->fstack->stack, vm->ip);
                            vm->ip = slot.code;
                            break;
                        }
                    }   
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
            intptr_t value = (intptr_t) vector_pop(vm->stack);
            #ifdef DEBUG
                printf("branch tag: %d, ptr: %p\n", get_tag_value(value), new_ptr);
            #endif
            if(get_tag_value(value) != NULL_PTAG) vm->ip = new_ptr;
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
            vector_set_length(vm->fstack->stack, vm->fstack->fp, (void*) vm->null);
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

intptr_t create_null_or_int(VM* vm, int a) {
  if (a) return create_int(a);
  else return vm->null;
}

void int_function_call(VM* vm, char* name) {
    intptr_t y = vector_pop(vm->stack);
    intptr_t x = vector_pop(vm->stack);
    intptr_t value;
    if(strcmp(name, "eq") == 0)
        value = create_null_or_int(vm, x == y);
    else if(strcmp(name, "lt") == 0)
        value = create_null_or_int(vm, x < y);
    else if(strcmp(name, "le") == 0)
        value = create_null_or_int(vm, x <= y);
    else if(strcmp(name, "gt") == 0)
        value =  create_null_or_int(vm, x > y);
    else if(strcmp(name, "ge") == 0)
        value = create_null_or_int(vm, x >= y);
    else if(strcmp(name, "add") == 0)
        value = x + y;
    else if(strcmp(name, "sub") == 0)
        value = x - y;
    else if(strcmp(name, "mul") == 0)
        value = create_int(get_int(x) * get_int(y));
    else if(strcmp(name, "div") == 0)
        value = create_int(x / y);
    else if(strcmp(name, "mod") == 0)
        value = x % y;
    else {
        printf("No slot named %s for Int.\n", name);
        exit(-1);
    }
    vector_add(vm->stack, (void*) value);
}

void array_function_call(VM* vm, char* name) {
    intptr_t value;
    if(strcmp(name, "get") == 0) {
        intptr_t i = vector_pop(vm->stack);
        intptr_t array_ptr = vector_pop(vm->stack);
        VMArray* array = (VMArray*) get_obj(array_ptr);
        value = array->items[get_int(i)];
    } else if(strcmp(name, "set") == 0) {
        intptr_t value = vector_pop(vm->stack);
        intptr_t pos = vector_pop(vm->stack);
        intptr_t array_ptr = vector_pop(vm->stack);
        VMArray* array = (VMArray*) get_obj(array_ptr);
        array->items[get_int(pos)] = value;
        value = vm->null;
    } else if(strcmp(name, "length") == 0) {
        intptr_t array_ptr = vector_pop(vm->stack);
        VMArray* array = (VMArray*) get_obj(array_ptr);
        value = create_int(array->length);
    } else {
        printf("No slot named %s for Int.\n", name);
        exit(-1);
    }
    vector_add(vm->stack, (void*) value); 
}
