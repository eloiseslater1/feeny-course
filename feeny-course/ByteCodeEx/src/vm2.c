#include "vm2.h"

VMValue* create_null();
void process_programe(VM* vm);
void runvm(VM* vm);
void* format_print(char* string, void **args);
StackFrame* init_frame();

VM* init_vm(Program* program) {
    VM* vm = malloc(sizeof(VM));
    vm->heap = init_heap();
    vm->global = make_vector();
    vm->patch_buffer = make_vector();
    vm->hm = ht_create();
    vm->code_buffer = init_code_buffer();
    vm->fstack = init_frame();
    vm->null = (VMNull*) create_null(vm);
    vm->program = program;
    vm->stack = make_vector();
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
void add_entry(VM* vm, char* key) {
    Entry* entry = malloc(sizeof(Entry));
    entry->code_idx = get_code_idx(vm->code_buffer);
    entry->type = METHOD_ENTRY;
    ht_set(vm->hm, key, entry);
}

void add_int_entry(VM* vm, int const_pool_idx) {
    char int_char[11];
    sprintf(int_char,"%d", const_pool_idx);
    add_entry(vm, int_char);
    Entry* entry = ht_get(vm->hm, int_char);
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
//--------------------------------Patches------------------------------------
//---------------------------------------------------------------------------


void patch_function_ptr(VM* vm, int name) {
    align_ptr(vm->code_buffer);
    Patch* patch = malloc(sizeof(Patch));
    patch->type = FUNCTION_PATCH;
    patch->code_pos = get_code_idx(vm->code_buffer);
    patch->name = name;
    vector_add(vm->patch_buffer, patch);
    write_ptr(vm->code_buffer, 0);
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
        break;
        }
        case OBJECT_OP: {
        ObjectIns* i = (ObjectIns*)ins;
        #ifdef DEBUG
            printf("   object #%d", i->class);
        #endif
        break;
        }
        case SLOT_OP: {
        SlotIns* i = (SlotIns*)ins;
        #ifdef DEBUG
            printf("   slot #%d", i->name);
        #endif
        break;
        }
        case SET_SLOT_OP: {
        SetSlotIns* i = (SetSlotIns*)ins;
        #ifdef DEBUG
            printf("   set-slot #%d", i->name);
        #endif
        break;
        }
        case CALL_SLOT_OP: {
        CallSlotIns* i = (CallSlotIns*)ins;
        #ifdef DEBUG
            printf("   call-slot #%d %d", i->name, i->arity);
        #endif
        break;
        }
        case CALL_OP: {
        CallIns* i = (CallIns*)ins;
            #ifdef DEBUG
                printf("   call #%d %d", i->name, i->arity);
            #endif
            write_int(vm->code_buffer, CALL_INS);
            write_int(vm->code_buffer, i->arity);
            patch_function_ptr(vm, i->name);
        break;
        }
        case SET_LOCAL_OP: {
        SetLocalIns* i = (SetLocalIns*)ins;
        #ifdef DEBUG
            printf("   set local %d", i->idx);
        #endif
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
        break;
        }
        case GET_GLOBAL_OP: {
        GetGlobalIns* i = (GetGlobalIns*)ins;
        #ifdef DEBUG
            printf("   get global #%d", i->name);
        #endif
        break;
        }
        case BRANCH_OP: {
        BranchIns* i = (BranchIns*)ins;
        #ifdef DEBUG
            printf("   branch #%d", i->name);
        #endif
        break;
        }
        case GOTO_OP: {
        GotoIns* i = (GotoIns*)ins;
        #ifdef DEBUG
            printf("   goto #%d", i->name);
        #endif
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
        switch(value->tag) {
            case (METHOD_VAL): {
                MethodValue* method = (MethodValue*) value;
                add_int_entry(vm, i);
                write_frame(method, vm->code_buffer);
                for (int j = 0; j < method->code->size; j++) {
                    parse_ins(vm, (ByteIns*) vector_get(method->code, j));
                }
            }
            case (CLASS_VAL): {
                ClassValue* class = (ClassValue* ) value;
                // parse class;
            }
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
            default: {
                printf("don't handle tag: %d for global\n", value->tag);
            }
        }
    }

    for (int i = 0; i < vm->patch_buffer->size; i++) {
        Patch* patch = vector_get(vm->patch_buffer, i);
        switch(patch->type) {
            case (FUNCTION_PATCH): {
                Entry* entry = get_entry_by_int(vm, patch->name);
                ((void**)(vm->code_buffer->code + patch->code_pos))[0] = vm->code_buffer->code + entry->code_idx;
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


VMValue* create_null(VM* vm) {
  VMNull* null = (VMNull*) halloc(vm->heap, VM_NULL, sizeof(VMNull));
  null->tag = VM_NULL;
  return (VMValue*) null;
}

VMInt* create_int(VM* vm, int a) {
  VMInt* value = (VMInt*) halloc(vm->heap, VM_INT, sizeof(VMInt));
  value->value = a;
  return value;
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
    vm->fstack->fp = vm->fstack->stack->size - 2;
    vector_set_length(vm->fstack->stack, vm->fstack->stack->size + nargs + nlocals, vm->null);
    for (int i = 0; i < nargs; i++) {
        vector_set(vm->fstack->stack, vm->fstack->stack->size - i - 1, vector_pop(vm->stack));
    }
    
}

void runvm (VM* vm) {  
  while (vm->ip) {
    int tag = next_int(vm);
    #ifdef DEBUG
        printf("tag is: %d\n", tag);
    #endif
    switch (tag) {
        case INT_INS : {
            #ifdef DEBUG
                printf("int ins\n");
            #endif
            VMInt* value = create_int(vm, next_int(vm));
            vector_add(vm->stack, value);
            break;
        }
        case NULL_INS : {
            VMValue* value = create_null(vm);
            vector_add(vm->stack, value);
            break;
        }
        case PRINTF_INS : {
            int nargs = next_int(vm);
            char* str = next_ptr(vm);
            #ifdef DEBUG
                printf("print ins with args: %d and string: %s\n", nargs, str);
            #endif
            void** args = malloc(sizeof(void*) * nargs);
            for (int i = 0; i< nargs; i++) {
                args[nargs - i- 1] =  vector_pop(vm->stack);
            }
            format_print(str, args);
            vector_add(vm->stack, vm->null);
            break;
        }
        case ARRAY_INS : {
            break;
        }
        case OBJECT_INS: {
            break;
        }
        case SLOT_INS: {
            break;
        }
        case SET_SLOT_INS: {
            break;
        }
        case CALL_SLOT_INS: {
            break;
        }
        case CALL_INS : {
            int arity = next_int(vm);
            void* new_code = next_ptr(vm);
            #ifdef DEBUG
                printf("calls ins with arity: %d and code: %p\n", arity, new_code);
            #endif
            vector_add(vm->fstack->stack, (void*) vm->fstack->fp);
            vector_add(vm->fstack->stack, vm->ip);
            vm->ip = new_code;
        }
        case SET_LOCAL_INS : {
            break;
        }
        case GET_LOCAL_INS : {
            int idx = next_int(vm);
            #ifdef DEBUG
                printf("get local ins at idx: %d\n", idx);
            #endif
            VMInt* value = vector_get(vm->fstack->stack, vm->fstack->fp + 2 + idx);
            vector_add(vm->stack, vector_get(vm->fstack->stack, vm->fstack->fp + 2 + idx));
            break;
        }
        case SET_GLOBAL_INS : {
            break;
        }
        case GET_GLOBAL_INS : {
            break;
        }
        case BRANCH_INS : {
            break;
        }
        case GOTO_INS : {
            break;
        }
        case RETURN_INS : {
            #ifdef DEBUG
                printf("return ins\n");
            #endif
            if (vm->fstack->fp == 0) {
                return;
            }
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

