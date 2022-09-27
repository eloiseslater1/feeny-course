
#include "vm.h"

//---------------------------------------------------------------------------
//--------------------------------init_vm------------------------------------
//---------------------------------------------------------------------------
Vector* fstack;
int fp;

int heap_size;
char* heap_memory;
char* heap_free;
char* heap_head;
char* heap_sp;

intptr_t* genv;
int genv_size;

Vector* classes;
Code* code_buffer;
Vector* stack;
intptr_t null;
char* ip;

void init_frame();
void init_genv(int globals_size);
void runvm();
void init_heap();
void free_heap();
intptr_t create_null();
void int_function_call(char* name);
void array_function_call(char* name);

void init_vm(VMInfo* vm_info) {
    classes = vm_info->classes;
    code_buffer = vm_info->code_buffer;
    stack = make_vector();
    init_heap();
    init_frame();
    null = create_null();
    ip = vm_info->ip;
    init_genv(vm_info->globals_size);
}

void init_genv(int globals_size) {
    genv = malloc(sizeof(intptr_t) * globals_size);
    for (int i = 0; i < globals_size; i++) {
        genv[i] = null;
    }
    genv_size = globals_size;
}

void free_vm() {
    vector_free(classes);
    free_code_buffer(code_buffer);
    vector_free(fstack);
    vector_free(stack);
    free_heap();
    free(genv);
}

void interpret_bc(Program* program) {
    VMInfo* info = quicken_vm(program);
    init_vm(info);
    runvm();
    free_vm();
}



//---------------------------------------------------------------------------
//===================== Classes ==================================================
//---------------------------------------------------------------------------

CSlot get_slot(VMObj* obj, char* name) {
    CClass* class = vector_get(classes, obj->tag);
    for (int i = 0; i < class->nslots; i++) {
        CSlot slot = class->slots[i];
        if (strcmp(name, slot.name) == 0) return slot;
    }
    return get_slot(obj->parent, name);
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

unsigned char next_char () {
  unsigned char c = ip[0];
  ip++;
  return c;
}

int next_short () {
  ip = (char*)(((long)ip + 1)&(-2));
  int s = ((unsigned short*)ip)[0];  
  ip += 2;
  return s;
}

int next_int () {
  ip = (char*)(((long)ip + 3)&(-4));
  int s = ((int*)ip)[0];  
  ip += 4;
  return s;
}

void* next_ptr () {
  ip = (char*)(((long)ip + 7)&(-8));
  void* s = ((void**)ip)[0];  
  ip += 8;
  return s;
}

void* format_print(char* string, int nargs) {
    int i = 0;
    while (*string != '\0') {
        if (*string == '~') {
            printf("%d", get_int((intptr_t) vector_get(stack, stack->size - nargs + i)));
            string++;
            i++;
        }
        printf("%c", *string);
        string++;
    }
    for (int i = 0; i < nargs; i++) {
        vector_pop(stack);
    }
}

//---------------------------------------------------------------------------
//--------------------------------StackFrame------------------------------------
//---------------------------------------------------------------------------

void init_frame() {
    fstack = make_vector();
    fp = 0;
}

void add_frame() {
    int nargs = next_int();
    int nlocals = next_int();
    fp = (fstack->size > 0) ? fstack->size - 2 : 0; 
    vector_set_length(fstack, fstack->size + nargs + nlocals, (void*) null);
    for (int i = nargs; i > 0; i--) {
        vector_set(fstack, fp + 1 + i, vector_pop(stack));
    }
}

//---------------------------------------------------------------------------
//--------------------------------heap------------------------------------
//---------------------------------------------------------------------------
void run_gc();
void init_heap() {
    heap_size = MB;
    heap_memory = malloc(heap_size);
    heap_free = malloc(heap_size);
    heap_head = heap_memory + heap_size;
    heap_sp = heap_memory;
}

void switch_heap() {
    char* current_heap = heap_memory;
    heap_memory = heap_free;
    heap_free = current_heap;
    heap_sp = heap_memory;
    heap_head = heap_memory + heap_size;
}

void free_heap() {
    free(heap_memory);
    free(heap_free);
}

void* halloc (long tag, int sz) {
  run_gc();
  if(heap_sp + sz > heap_head){
      printf("Calling GC.\n");
      if(heap_sp + sz > heap_head) {
        printf("Out of memory.");
        exit(-1);
      }
    }
  long* obj = (long*)heap_sp;
  obj[0] = tag;
  heap_sp += sz;
  return obj;
}

VMArray* create_array(intptr_t length) {
    int int_length = get_int(length);
    VMArray* array = (VMArray*) halloc(VM_ARRAY, sizeof(VMArray) + sizeof(void*) * int_length);
    array->length = int_length;
    return array;
}

VMObj* create_object(int class, int arity) {
  VMObj* obj = halloc(class, sizeof(VMObj) + sizeof(void*) * arity);
}
//---------------------------------------------------------------------------
//--------------------------------run gc------------------------------------
//---------------------------------------------------------------------------
long BHEART = -1;

int get_obj_size(VMValue* obj) {
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
            CClass* class = vector_get(classes, obj->tag);
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

intptr_t copy_to_free(void* obj) {
    void* dst = heap_sp;
    int sz = get_obj_size((VMValue*) obj);
    memcpy(dst, obj, sz);
    heap_sp += sz;
    return forward_pointer(obj, dst);
}

intptr_t get_post_gc_ptr(intptr_t obj_ptr) {
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
                return copy_to_free(obj);
            }
    }
}

void scan_stack() {
    for (int i = 0; i < stack->size; i++) {
        intptr_t new_obj = get_post_gc_ptr((intptr_t) vector_get(stack, i));
        vector_set(stack, i, (void*) new_obj);
    }
}

void scan_stack_frame() {
    int frame_start = fp + 2;
    int frame_end = fstack->size;
    while (frame_end > 0) {
        for (int i = frame_start; i < frame_end; i++) {
            intptr_t new_obj = get_post_gc_ptr((intptr_t) vector_get(fstack, i));
            vector_set(fstack, i, (void*) new_obj);
        }
        frame_end = frame_start - 2;
        frame_start = ((int) vector_get(fstack, frame_start - 2)) + 2;
    }
}

void scan_globals() {
    for (int i = 0; i < genv_size; i++) {
        genv[i] = get_post_gc_ptr((intptr_t) genv[i]);
    }
}

void scan_root_set() {
    scan_stack();
    scan_stack_frame();
    scan_globals();
}

void scan_array(VMArray* array) {
    for (int i = 0; i < array->length; i++) {
        array->items[i] = get_post_gc_ptr((intptr_t) array->items[i]);
    }
}

void scan_object(VMObj* obj) {
    CClass* class = vector_get(classes, obj->tag);
    obj->parent = get_obj(get_post_gc_ptr(set_obj_bit((VMValue*) obj->parent)));
    for (int i = 0; i < class->nvars; i++) {
        obj->slots[i] = get_post_gc_ptr((intptr_t) obj->slots[i]);
    }
}

void scan_heap() {
    char* obj = heap_memory;
    while (obj < heap_sp) {
        VMValue* value = (VMValue*) obj;
        switch(value->tag) {
            case VM_INT: 
            case VM_NULL:
                break;
            case VM_ARRAY:
                scan_array((VMArray*) value);
                break;
            default:
                scan_object((VMObj*) value);
                break;
        }
        obj += get_obj_size(value);
    }
}


void run_gc() {
    switch_heap();
    scan_root_set();
    scan_heap();
}

//---------------------------------------------------------------------------
//--------------------------------run vm------------------------------------
//---------------------------------------------------------------------------

void runvm () {  
  while (ip) {
    int tag = next_int();
    #ifdef DEBUG
        printf("STACK: ");
        for (int i = 0; i < stack->size; i++) {
            intptr_t value = (intptr_t) vector_get(stack, i);
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
            int i = next_int();
            intptr_t value = create_int(i);
            #ifdef DEBUG
                printf("int ins val: %d\n", i);
            #endif
            vector_add(stack, (void*) value);
            break;
        }
        case NULL_INS : {
            #ifdef DEBUG
                printf("null ins\n");
            #endif
            intptr_t value = null;
            vector_add(stack, (void*) value);
            break;
        }
        case PRINTF_INS : {
            int nargs = next_int();
            char* str = next_ptr();
            #ifdef DEBUG
                printf("print: %d and str: %s\n", nargs, str);
            #endif
            format_print(str, nargs);
            vector_add(stack, (void*) null);
            break;
        }
        case ARRAY_INS : {
            #ifdef DEBUG
                printf("array\n");
            #endif
            intptr_t length = (intptr_t) vector_get(stack, stack->size - 2);
            VMArray* array = create_array(length);
            intptr_t initial = (intptr_t) vector_pop(stack);
            vector_pop(stack);
            for (int i = 0; i < array->length; i++) {
                array->items[i] = initial;
            }
            vector_add(stack, (void*) set_obj_bit((VMValue*) array));
            break;
        }
        case OBJECT_INS: {
            int arity = next_int();
            int class = next_int();
            #ifdef DEBUG
                printf("object \n");
            #endif
            VMObj* obj = create_object(class, arity);
            for (int i = arity - 1; i >= 0; i--) {
                obj->slots[i] = vector_pop(stack);
            }
            intptr_t parent_ptr = (intptr_t) vector_pop(stack);
            obj->parent = (VMObj*) get_obj(parent_ptr);
            vector_add(stack, (void*) set_obj_bit((VMValue*) obj));
            break;
        }
        case SLOT_INS: {
            char* name = next_ptr();
            intptr_t obj = (intptr_t) vector_pop(stack);
            VMObj* vm_obj = (VMObj*) get_obj(obj);
            CSlot slot = get_slot(vm_obj, name);
            vector_add(stack, (void*) vm_obj->slots[slot.idx]);
            break;
        }
        case SET_SLOT_INS: {
            char* name = next_ptr();
            intptr_t value = (intptr_t) vector_pop(stack);
            intptr_t obj =  (intptr_t) vector_pop(stack);
            VMObj* vm_obj = (VMObj*) get_obj(obj);
            CSlot slot = get_slot(vm_obj, name);
            vm_obj->slots[slot.idx] = value;
            vector_add(stack, (void*) value);
            break;
        }
        case CALL_SLOT_INS: {
            int arity = next_int();
            char* name = next_ptr();
            #ifdef DEBUG
                printf("call-op #%d and str: %s\n", arity, name);
            #endif
            intptr_t ptr = (intptr_t) vector_get(stack, stack->size - arity);
            switch(get_tag_value(ptr)) {
                case INT_PTAG: {
                    int_function_call(name);
                    break;
                } case NULL_PTAG: {
                    printf("No slots can be called on null value");
                    exit(-1); 
                } case OBJ_PTAG: {
                    VMValue* value = get_obj(ptr);
                    switch(value->tag) {
                        case VM_ARRAY: {
                            array_function_call(name);
                            break;
                        }
                        default : {
                            VMObj* obj = (VMObj*) value;
                            CSlot slot = get_slot(obj, name);
                            vector_add(fstack, (void*) fp);
                            vector_add(fstack, ip);
                            ip = slot.code;
                            break;
                        }
                    }   
                }
            }
            break;
        }
        case CALL_INS : {
            int arity = next_int();
            void* new_code = next_ptr();
            #ifdef DEBUG
                printf("calls #%d and ptr: %p\n", arity, new_code);
            #endif
            vector_add(fstack, (void*) fp);
            vector_add(fstack, ip);
            ip = new_code;
            break;
        }
        case SET_LOCAL_INS : {
            int idx = next_int();
            #ifdef DEBUG
                printf("set local : %d at: %d\n", idx, fp + 2 + idx);
            #endif
            vector_set(fstack, fp + 2 + idx, vector_peek(stack));
            break;
        }
        case GET_LOCAL_INS : {
            int idx = next_int();
            #ifdef DEBUG
                printf("get local : %d\n", idx);
            #endif
            vector_add(stack, vector_get(fstack, fp + 2 + idx));
            break;
        }
        case SET_GLOBAL_INS : {
            int idx = next_int();
            #ifdef DEBUG
                printf("set global : %d\n", idx);
            #endif
            genv[idx] = vector_peek(stack);
            break;
        }
        case GET_GLOBAL_INS : {
            int idx = next_int();
            #ifdef DEBUG
                printf("get global : %d\n", idx);
            #endif
            vector_add(stack, (void*) genv[idx]);
            break;
        }
        case BRANCH_INS : {
            void* new_ptr = next_ptr();
            intptr_t value = (intptr_t) vector_pop(stack);
            #ifdef DEBUG
                printf("branch tag: %d, ptr: %p\n", get_tag_value(value), new_ptr);
            #endif
            if(get_tag_value(value) != NULL_PTAG) ip = new_ptr;
            break;
        }
        case GOTO_INS : {
            void* ptr = next_ptr();
            #ifdef DEBUG
                printf("goto ins, ptr: %p\n", ptr);
            #endif
            ip = ptr;
            break;
        }
        case RETURN_INS : {
            #ifdef DEBUG
                printf("return ins\n");
            #endif
            if (fstack->size == 0) return;
            int old_fp = (int) vector_get(fstack, fp);
            ip = vector_get(fstack, fp + 1); 
            vector_set_length(fstack, fp, (void*) null);
            fp = old_fp;
            break;
        }
        case DROP_INS : {
            #ifdef DEBUG
                printf("drop ins\n");
            #endif
            vector_pop(stack);
            break;
        }
        case FRAME_INS : {
            #ifdef DEBUG
                printf("frame ins\n");
            #endif
            add_frame();
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

intptr_t create_null_or_int(int a) {
  if (a) return create_int(a);
  else return null;
}

void int_function_call(char* name) {
    intptr_t y = (intptr_t) vector_pop(stack);
    intptr_t x = (intptr_t) vector_pop(stack);
    intptr_t value;
    if(strcmp(name, "eq") == 0)
        value = create_null_or_int(x == y);
    else if(strcmp(name, "lt") == 0)
        value = create_null_or_int(x < y);
    else if(strcmp(name, "le") == 0)
        value = create_null_or_int(x <= y);
    else if(strcmp(name, "gt") == 0)
        value =  create_null_or_int(x > y);
    else if(strcmp(name, "ge") == 0)
        value = create_null_or_int(x >= y);
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
    vector_add(stack, (void*) value);
}

void array_function_call(char* name) {
    intptr_t value;
    if(strcmp(name, "get") == 0) {
        intptr_t i = (intptr_t) vector_pop(stack);
        intptr_t array_ptr = (intptr_t) vector_pop(stack);
        VMArray* array = (VMArray*) get_obj(array_ptr);
        value = array->items[get_int(i)];
    } else if(strcmp(name, "set") == 0) {
        intptr_t value = (intptr_t) vector_pop(stack);
        intptr_t pos = (intptr_t) vector_pop(stack);
        intptr_t array_ptr = (intptr_t) vector_pop(stack);
        VMArray* array = (VMArray*) get_obj(array_ptr);
        array->items[get_int(pos)] = value;
        value = null;
    } else if(strcmp(name, "length") == 0) {
        intptr_t array_ptr = (intptr_t) vector_pop(stack);
        VMArray* array = (VMArray*) get_obj(array_ptr);
        value = create_int(array->length);
    } else {
        printf("No slot named %s for Int.\n", name);
        exit(-1);
    }
    vector_add(stack, (void*) value); 
}
