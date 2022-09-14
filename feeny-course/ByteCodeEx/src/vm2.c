#include "vm2.h"

void process_programe(VM* vm);
void runvm(VM* vm);
void* format_print(char* string, void **args);
StackFrame* init_frame();
ht* init_builtins();

void init_genv(VM* vm) {
    vm->genv = malloc(sizeof(void*) * vm->globals->size);
    for (int i = 0; i < vm->globals->size; i++) {
        vm->genv[i] = vm->null;
    }
}

void init_classes(VM* vm) {
    vm->classes = make_vector();
    for (int i = 0; i < 3; i++) {
        vector_add(vm->classes, (void*) 0);
    }
}

VM* init_vm(Program* program) {
    VM* vm = malloc(sizeof(VM));
    vm->heap = init_heap(); // need to running
    vm->globals = make_vector(); // can free
    vm->fstack = init_frame(); // need
    vm->patch_buffer = make_vector(); // can free
    vm->hm = ht_create(); // can free
    vm->code_buffer = init_code_buffer(); // need
    vm->null = (VMNull*) create_null(vm->heap); // need
    vm->program = program; // can free
    vm->stack = make_vector(); // need
    vm->inbuilt = init_builtins(); // need
    init_classes(vm);
    init_genv(vm); // need
    return vm;
}


void interpret_bc (Program* p) {
  #ifdef DEBUG
    printf("Interpreting Bytecode Program:\n");
    print_prog(p);
  #endif
  VM* vm = init_vm(p);
  process_programe(vm);
  runvm(vm);
}

//---------------------------------------------------------------------------
//--------------------------------Entries------------------------------------
//---------------------------------------------------------------------------
void add_entry(VM* vm, char* key, int tag) {
    Entry* entry = malloc(sizeof(Entry));
    entry->code_idx = get_code_idx(vm->code_buffer);
    entry->type = tag;
    ht_set(vm->hm, key, entry);
}


void add_class_entry(VM* vm, int const_pool_idx, int class_idx) {
    char int_char[11];
    sprintf(int_char,"%d", const_pool_idx);
    Entry* entry = malloc(sizeof(Entry));
    entry->type = CLASS_ENTRY;
    entry->class_idx = class_idx;
    ht_set(vm->hm, int_char, entry);
}

void add_int_entry(VM* vm, int const_pool_idx, int tag) {
    char int_char[11];
    sprintf(int_char,"%d", const_pool_idx);
    add_entry(vm, int_char, tag);
}

Entry* get_entry(VM* vm, char* key) {
    return (Entry*) ht_get(vm->hm, key);
}

Entry* get_entry_by_int(VM* vm, int const_pool_idx) {
    char int_char[11];
    sprintf(int_char,"%d", const_pool_idx);
    return get_entry(vm, int_char);
}

Entry* link_entries_with_int(VM* vm, Entry* entry, int name) {
    char int_char[11];
    sprintf(int_char,"%d", name);
    ht_set(vm->hm, int_char, entry);
}
//---------------------------------------------------------------------------
//--------------------------------classes------------------------------------
//---------------------------------------------------------------------------

char* idx_to_str(VM* vm, int idx) {
    StringValue* value = vector_get(vm->program->values, idx);
    return value->value;
}

int make_class(VM* vm, ClassValue* class) {
    CClass* cclass = malloc(sizeof(CClass));
    cclass->nvars = 0;
    cclass->nslots = class->slots->size;
    cclass->slots = malloc(sizeof(CSlot) * cclass->nslots); 
    for (int i = 0; i < cclass->nslots; i++) {
        int const_pool_idx = (int) vector_get(class->slots, i);
        Value* value = vector_get(vm->program->values, const_pool_idx);
        switch(value->tag) {
            case (SLOT_VAL): {
                SlotValue* sval = (SlotValue*) value;
                cclass->slots[i].tag = VAR_SLOT;
                cclass->slots[i].name = idx_to_str(vm, sval->name);
                cclass->slots[i].idx = cclass->nvars;
                cclass->nvars++;
                break;
            } case (METHOD_VAL): {
                MethodValue* mval = (MethodValue*)value;
                cclass->slots[i].tag = CODE_SLOT;
                cclass->slots[i].name = idx_to_str(vm, mval->name);
                Entry* entry = get_entry_by_int(vm, const_pool_idx);
                cclass->slots[i].code = vm->code_buffer->code + entry->code_idx;
                break;
            }
        }
    }
    vector_add(vm->classes, cclass);
    return vm->classes->size - 1;
}

CSlot get_slot(VM* vm, VMObj* obj, char* name) {
    CClass* class = vector_get(vm->classes, obj->tag);
    for (int i = 0; i < class->nslots; i++) {
        CSlot slot = class->slots[i];
        if (strcmp(name, slot.name) == 0) return slot;
    }
    return get_slot(vm, obj->parent, name);
}

//---------------------------------------------------------------------------
//--------------------------------globals------------------------------------
//---------------------------------------------------------------------------

int get_global_idx(VM* vm, int name) {
    for (int i = 0; i < vm->globals->size; i++) {
        int gname = (int) vector_get(vm->globals, i);
        if (name == gname) return i;
    }
    printf("No global with name %d.\n", name);
    exit(-1);
}

//---------------------------------------------------------------------------
//--------------------------------Patches------------------------------------
//---------------------------------------------------------------------------

void make_patch(VM* vm, int name, int tag) {
    Patch* patch = malloc(sizeof(Patch));
    patch->type = tag;
    patch->code_pos = get_code_idx(vm->code_buffer);
    patch->name = name;
    vector_add(vm->patch_buffer, patch);
}

void write_patch_pointer(VM* vm, int name, int tag) {
    align_ptr(vm->code_buffer);
    make_patch(vm, name, tag);
    write_ptr(vm->code_buffer, 0);
}

void write_patch_int(VM* vm, int name, int tag) {
    align_int(vm->code_buffer);
    make_patch(vm, name, tag);
    write_int(vm->code_buffer, 0);
}

//---------------------------------------------------------------------------
//--------------------------------parse_ins----------------------------------
//---------------------------------------------------------------------------

void parse_ins(VM* vm, ByteIns* ins) {
    switch(ins->tag) { 
        case LABEL_OP: {
            LabelIns* i = (LabelIns*)ins;
            #ifdef DEBUG
                printf("label #%d", i->name);
            #endif
            add_int_entry(vm, i->name, LABEL_ENTRY);
            break;
        }
        case LIT_OP: {
            LitIns* i = (LitIns*)ins;
            #ifdef DEBUG
                printf("   lit #%d", i->idx);
            #endif
            Value* value = vector_get(vm->program->values, i->idx);
            if (value->tag == INT_VAL) {
                write_int(vm->code_buffer, INT_INS);
                write_int(vm->code_buffer, ((IntValue*)value)->value);
            } else if (value->tag == NULL_VAL) {
                write_int(vm->code_buffer, NULL_INS);
            }
            break;
        }
        case PRINTF_OP: {
            PrintfIns* i = (PrintfIns*)ins;
            #ifdef DEBUG
                printf("   printf #%d %d", i->format, i->arity);
            #endif
            write_int(vm->code_buffer, PRINTF_INS);
            write_int(vm->code_buffer, i->arity);
            StringValue* str = vector_get(vm->program->values, i->format);
            write_ptr(vm->code_buffer, str->value);
            break;
        }
        case ARRAY_OP: {
            #ifdef DEBUG
                printf("   array");
            #endif
            write_int(vm->code_buffer, ARRAY_INS);
            break;
        }
        case OBJECT_OP: {
            ObjectIns* i = (ObjectIns*)ins;
            #ifdef DEBUG
                printf("   object #%d", i->class);
            #endif
            write_int(vm->code_buffer, OBJECT_INS);
            write_patch_int(vm, i->class, CLASS_ARITY_PATCH);
            write_patch_int(vm, i->class, CLASS_TAG_PATCH);
            break;
        }
        case SLOT_OP: {
            SlotIns* i = (SlotIns*)ins;
            #ifdef DEBUG
                printf("   slot #%d", i->name);
            #endif
            write_int(vm->code_buffer, SLOT_INS);
            write_ptr(vm->code_buffer, idx_to_str(vm, i->name));
            break;
        }
        case SET_SLOT_OP: {
            SetSlotIns* i = (SetSlotIns*)ins;
            #ifdef DEBUG
                printf("   set-slot #%d", i->name);
            #endif
            write_int(vm->code_buffer, SET_SLOT_INS);
            write_ptr(vm->code_buffer, idx_to_str(vm, i->name));
            break;
        }
        case CALL_SLOT_OP: {
            CallSlotIns* i = (CallSlotIns*)ins;
            #ifdef DEBUG
                printf("   call-slot #%d %d", i->name, i->arity);
            #endif
            write_int(vm->code_buffer, CALL_SLOT_INS);
            write_int(vm->code_buffer, i->arity);
            StringValue* str = vector_get(vm->program->values, i->name);
            write_ptr(vm->code_buffer, str->value);
            break;
        }
        case CALL_OP: {
        CallIns* i = (CallIns*)ins;
            #ifdef DEBUG
                printf("   call #%d %d", i->name, i->arity);
            #endif
            write_int(vm->code_buffer, CALL_INS);
            write_int(vm->code_buffer, i->arity);
            write_patch_pointer(vm, i->name, FUNCTION_PATCH);
        break;
        }
        case SET_LOCAL_OP: {
        SetLocalIns* i = (SetLocalIns*)ins;
            #ifdef DEBUG
                printf("   set local %d", i->idx);
            #endif
            write_int(vm->code_buffer, SET_LOCAL_INS);
            write_int(vm->code_buffer, i->idx);
            break;
        }
        case GET_LOCAL_OP: {
            GetLocalIns* i = (GetLocalIns*)ins;
            #ifdef DEBUG
                printf("   get local %d", i->idx);
            #endif
            write_int(vm->code_buffer, GET_LOCAL_INS);
            write_int(vm->code_buffer, i->idx);
            break;
        }
        case SET_GLOBAL_OP: {
            SetGlobalIns* i = (SetGlobalIns*)ins;
            #ifdef DEBUG
                printf("   set global #%d", i->name);
            #endif
            write_int(vm->code_buffer, SET_GLOBAL_INS);
            write_patch_int(vm, i->name, INT_PATCH);
            break;
        }
        case GET_GLOBAL_OP: {
            GetGlobalIns* i = (GetGlobalIns*)ins;
            #ifdef DEBUG
                printf("   get global #%d", i->name);
            #endif
            write_int(vm->code_buffer, GET_GLOBAL_INS);
            write_patch_int(vm, i->name, INT_PATCH);
            break;
        }
        case BRANCH_OP: {
            BranchIns* i = (BranchIns*)ins;
            #ifdef DEBUG
                printf("   branch #%d", i->name);
            #endif
            write_int(vm->code_buffer, BRANCH_INS);
            write_patch_pointer(vm, i->name, LABEL_PATCH);
            break;
        }
        case GOTO_OP: {
            GotoIns* i = (GotoIns*)ins;
            #ifdef DEBUG
                printf("   goto #%d", i->name);
            #endif
            write_int(vm->code_buffer, GOTO_INS);
            write_patch_pointer(vm, i->name, LABEL_PATCH);
            break;
        }
        case RETURN_OP: {
            #ifdef DEBUG
                printf("   return");
            #endif
                write_int(vm->code_buffer, RETURN_INS);
            break;
        }
        case DROP_OP: {
            #ifdef DEBUG
                printf("   drop");
            #endif
            write_int(vm->code_buffer, DROP_INS);
            break;
        }
        default: {
        printf("Unknown instruction with tag: %u\n", ins->tag);
        exit(-1);
        }
}
}

//---------------------------------------------------------------------------
//--------------------------------process_propgramme-------------------------
//---------------------------------------------------------------------------

void write_frame (MethodValue* method, Code* code_buffer) {
  check_size(code_buffer);
  write_int(code_buffer, FRAME_INS);
  write_int(code_buffer, method->nargs);
  write_int(code_buffer, method->nlocals);
}

void process_programe(VM* vm) {
    for (int i = 0; i < vm->program->values->size; i++) {
        Value* value = vector_get(vm->program->values, i);
        if (value->tag == METHOD_VAL) {
            MethodValue* method = (MethodValue*) value;
            add_int_entry(vm, i, METHOD_ENTRY);
            write_frame(method, vm->code_buffer);
            for (int j = 0; j < method->code->size; j++) {
                parse_ins(vm, (ByteIns*) vector_get(method->code, j));
            }
        }
    }

    // maybe just add idx to a vector, avoid a second loop.
    for (int i = 0; i < vm->program->values->size; i++) {
        Value* value = vector_get(vm->program->values, i);
        if (value->tag == CLASS_VAL) {
            ClassValue* class = (ClassValue* ) value;
            int classes_idx = make_class(vm, class);
            add_class_entry(vm, i, classes_idx); //maybe just add straight to had table??
        }
    }

    for (int i = 0; i < vm->program->slots->size; i++) {
        int idx = (int) vector_get(vm->program->slots, i);
        Value* value = vector_get(vm->program->values, idx);
        switch(value->tag) {
            case (METHOD_VAL): {
                MethodValue* method = (MethodValue*) value;
                Entry* entry = get_entry_by_int(vm, idx);
                link_entries_with_int(vm, entry, method->name);
                break;
            }
            case (SLOT_VAL): {
                SlotValue* svalue = (SlotValue*) value;
                vector_add(vm->globals, (void*) svalue->name);
                break;
            }
            default: {
                printf("don't handle tag: %d for global\n", value->tag);
                exit(-1);
            }
        }
    }

    for (int i = 0; i < vm->patch_buffer->size; i++) {
        Patch* patch = vector_get(vm->patch_buffer, i);
        switch(patch->type) {
            case (LABEL_PATCH):
            case (FUNCTION_PATCH): {
                Entry* entry = get_entry_by_int(vm, patch->name);
                ((void**)(vm->code_buffer->code + patch->code_pos))[0] = vm->code_buffer->code + entry->code_idx;
                break;
            } case(INT_PATCH): {
                int idx = get_global_idx(vm, patch->name);
                ((int*)(vm->code_buffer->code + patch->code_pos))[0] = idx;
                break;
            } case (CLASS_ARITY_PATCH): {
                Entry* entry = get_entry_by_int(vm, patch->name);
                CClass* class = vector_get(vm->classes, entry->class_idx);
                ((short*)(vm->code_buffer->code + patch->code_pos))[0] = class->nvars;
                break;
            } case (CLASS_TAG_PATCH): {
                Entry* entry = get_entry_by_int(vm, patch->name);
                ((short*)(vm->code_buffer->code + patch->code_pos))[0] = entry->class_idx;
                break;
            }
            default: {
                printf("patch type not recorgnise: %d\n", patch->type);
            }
        }
    }

    Entry* entry = get_entry_by_int(vm, vm->program->entry);
    vm->ip = vm->code_buffer->code + entry->code_idx;
}

//---------------------------------------------------------------------------
//--------------------------------vm-----------------------------------------
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


//---------------------------------------------------------------------------
//--------------------------------StackFrame------------------------------------
//---------------------------------------------------------------------------


StackFrame* init_frame() {
    StackFrame* frame = malloc(sizeof(StackFrame));
    frame->stack = make_vector();
    frame->fp = 0;
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
            VMInt* value = create_int(vm->heap, next_int(vm));
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
            VMValue* value = create_null(vm->heap);
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
            vector_add(vm->stack, create_array(vm->heap, length->value, intial->value));
            break;
        }
        case OBJECT_INS: {
            int arity = next_int(vm);
            int class = next_int(vm);
            #ifdef DEBUG
                printf("object \n");
            #endif
            VMObj* obj = create_object(vm->heap, class, arity);
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

//===================== BUILTINS ================================

VMValue* create_null_or_int(VM* vm, long a) {
  if (a) {
    return (VMValue*) create_int(vm->heap, a);
  }
  else {
    return create_null(vm->heap);
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
  return (VMValue*) create_int(vm->heap, value);
}

VMValue* fe_len(VM* vm, void** args) {
  return (VMValue*) create_int(vm->heap, ((VMArray* ) args[0])->length);
}

VMValue* fe_add(VM* vm, void** args) {
  return (VMValue*) create_int(vm->heap, ((VMInt*) args[0])->value + ((VMInt*) args[1])->value);
}

VMValue* fe_sub(VM* vm, void** args) {
  return (VMValue*) create_int(vm->heap, ((VMInt*) args[0])->value - ((VMInt*) args[1])->value);
}

VMValue* fe_mult(VM* vm, void** args) {
  return (VMValue*) create_int(vm->heap, ((VMInt*) args[0])->value * ((VMInt*) args[1])->value);
}

VMValue* fe_div(VM*vm, void** args) {
  return (VMValue*) create_int(vm->heap, ((VMInt*) args[0])->value / ((VMInt*) args[1])->value);
}

VMValue* fe_mod(VM* vm, void** args) {
  return (VMValue*) create_int(vm->heap, ((VMInt*) args[0])->value % ((VMInt*) args[1])->value);
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
