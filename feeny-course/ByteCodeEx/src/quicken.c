#include "quicken.h"

//---------------------------------------------------------------------------
//--------------------------------init_quicken-------------------------------
//---------------------------------------------------------------------------
char* process_programe(Quicken* q);

void init_classes(Quicken* q) {
    q->classes = make_vector();
    for (int i = 0; i < 3; i++) {
        vector_add(q->classes, (void*) 0);
    }
}

Quicken* init_quicken(Program* program){
    Quicken* q = malloc(sizeof(Quicken));
    q->patch_buffer = make_vector();
    q->globals = make_vector();
    q->program = program;
    q->hm = ht_create();
    q->code_buffer = init_code_buffer();
    init_classes(q);
    return q;
}

VMInfo* create_vm_info(Quicken* q, char* ip) {
    VMInfo* vm_info = malloc(sizeof(VMInfo));
    vm_info->classes = q->classes;
    vm_info->code_buffer = q->code_buffer;
    vm_info->const_pool = q->program->values;
    vm_info->ip = ip;
    vm_info->globals_size = q->globals->size;
    return vm_info;
}

void free_quicken(Quicken* q) {
    vector_free(q->patch_buffer);
    vector_free(q->globals);
    ht_destroy(q->hm);
    vector_free(q->program->slots);
    free(q->program);
    free(q);
}

//---------------------------------------------------------------------------
//--------------------------------Entries------------------------------------
//---------------------------------------------------------------------------
void add_entry(Quicken* q, char* key, int tag) {
    Entry* entry = malloc(sizeof(Entry));
    entry->code_idx = get_code_idx(q->code_buffer);
    entry->type = tag;
    ht_set(q->hm, key, entry);
}


void add_class_entry(Quicken* q, int const_pool_idx, int class_idx) {
    char int_char[11];
    sprintf(int_char,"%d", const_pool_idx);
    Entry* entry = malloc(sizeof(Entry));
    entry->type = CLASS_ENTRY;
    entry->class_idx = class_idx;
    ht_set(q->hm, int_char, entry);
}

void add_int_entry(Quicken* q, int const_pool_idx, int tag) {
    char int_char[11];
    sprintf(int_char,"%d", const_pool_idx);
    add_entry(q, int_char, tag);
}

Entry* get_entry(Quicken* q, char* key) {
    return (Entry*) ht_get(q->hm, key);
}

Entry* get_entry_by_int(Quicken* q, int const_pool_idx) {
    char int_char[11];
    sprintf(int_char,"%d", const_pool_idx);
    return get_entry(q, int_char);
}

Entry* link_entries_with_int(Quicken* q, Entry* entry, int name) {
    char int_char[11];
    sprintf(int_char,"%d", name);
    ht_set(q->hm, int_char, entry);
}

//---------------------------------------------------------------------------
//--------------------------------globals------------------------------------
//---------------------------------------------------------------------------

int get_global_idx(Quicken* q, int name) {
    for (int i = 0; i < q->globals->size; i++) {
        int gname = (int) vector_get(q->globals, i);
        if (name == gname) return i;
    }
    printf("No global with name %d.\n", name);
    exit(-1);
}

//---------------------------------------------------------------------------
//--------------------------------Patches------------------------------------
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//--------------------------------Patches------------------------------------
//---------------------------------------------------------------------------

void make_patch(Quicken* q, int name, int tag) {
    Patch* patch = malloc(sizeof(Patch));
    patch->type = tag;
    patch->code_pos = get_code_idx(q->code_buffer);
    patch->name = name;
    vector_add(q->patch_buffer, patch);
}

void write_patch_pointer(Quicken* q, int name, int tag) {
    align_ptr(q->code_buffer);
    make_patch(q, name, tag);
    write_ptr(q->code_buffer, 0);
}

void write_patch_int(Quicken* q, int name, int tag) {
    align_int(q->code_buffer);
    make_patch(q, name, tag);
    write_int(q->code_buffer, 0);
}

//---------------------------------------------------------------------------
//--------------------------------classes------------------------------------
//---------------------------------------------------------------------------

char* idx_to_str(Quicken* q, int idx) {
    StringValue* value = vector_get(q->program->values, idx);
    return value->value;
}

int make_class(Quicken* q, ClassValue* class) {
    CClass* cclass = malloc(sizeof(CClass));
    cclass->nvars = 0;
    cclass->nslots = class->slots->size;
    cclass->slots = malloc(sizeof(CSlot) * cclass->nslots); 
    for (int i = 0; i < cclass->nslots; i++) {
        int const_pool_idx = (int) vector_get(class->slots, i);
        Value* value = vector_get(q->program->values, const_pool_idx);
        switch(value->tag) {
            case (SLOT_VAL): {
                SlotValue* sval = (SlotValue*) value;
                cclass->slots[i].tag = VAR_SLOT;
                cclass->slots[i].name = idx_to_str(q, sval->name);
                cclass->slots[i].idx = cclass->nvars;
                cclass->nvars++;
                break;
            } case (METHOD_VAL): {
                MethodValue* mval = (MethodValue*)value;
                cclass->slots[i].tag = CODE_SLOT;
                cclass->slots[i].name = idx_to_str(q, mval->name);
                Entry* entry = get_entry_by_int(q, const_pool_idx);
                cclass->slots[i].code = q->code_buffer->code + entry->code_idx;
                break;
            }
        }
    }
    vector_add(q->classes, cclass);
    return q->classes->size - 1;
}
//---------------------------------------------------------------------------
//--------------------------------parse_ops----------------------------------
//---------------------------------------------------------------------------

void parse_ops(Quicken* q, ByteIns* ins) {
    switch(ins->tag) { 
        case LABEL_OP: {
            LabelIns* i = (LabelIns*)ins;
            #ifdef DEBUG
                printf("label #%d", i->name);
            #endif
            add_int_entry(q, i->name, LABEL_ENTRY);
            break;
        }
        case LIT_OP: {
            LitIns* i = (LitIns*)ins;
            #ifdef DEBUG
                printf("   lit #%d", i->idx);
            #endif
            Value* value = vector_get(q->program->values, i->idx);
            if (value->tag == INT_VAL) {
                write_int(q->code_buffer, INT_INS);
                write_int(q->code_buffer, ((IntValue*)value)->value);
            } else if (value->tag == NULL_VAL) {
                write_int(q->code_buffer, NULL_INS);
            }
            break;
        }
        case PRINTF_OP: {
            PrintfIns* i = (PrintfIns*)ins;
            #ifdef DEBUG
                printf("   printf #%d %d", i->format, i->arity);
            #endif
            write_int(q->code_buffer, PRINTF_INS);
            write_int(q->code_buffer, i->arity);
            StringValue* str = vector_get(q->program->values, i->format);
            write_ptr(q->code_buffer, str->value);
            break;
        }
        case ARRAY_OP: {
            #ifdef DEBUG
                printf("   array");
            #endif
            write_int(q->code_buffer, ARRAY_INS);
            break;
        }
        case OBJECT_OP: {
            ObjectIns* i = (ObjectIns*)ins;
            #ifdef DEBUG
                printf("   object #%d", i->class);
            #endif
            write_int(q->code_buffer, OBJECT_INS);
            write_patch_int(q, i->class, CLASS_ARITY_PATCH);
            write_patch_int(q, i->class, CLASS_TAG_PATCH);
            break;
        }
        case SLOT_OP: {
            SlotIns* i = (SlotIns*)ins;
            #ifdef DEBUG
                printf("   slot #%d", i->name);
            #endif
            write_int(q->code_buffer, SLOT_INS);
            write_ptr(q->code_buffer, idx_to_str(q, i->name));
            break;
        }
        case SET_SLOT_OP: {
            SetSlotIns* i = (SetSlotIns*)ins;
            #ifdef DEBUG
                printf("   set-slot #%d", i->name);
            #endif
            write_int(q->code_buffer, SET_SLOT_INS);
            write_ptr(q->code_buffer, idx_to_str(q, i->name));
            break;
        }
        case CALL_SLOT_OP: {
            CallSlotIns* i = (CallSlotIns*)ins;
            #ifdef DEBUG
                printf("   call-slot #%d %d", i->name, i->arity);
            #endif
            write_int(q->code_buffer, CALL_SLOT_INS);
            write_int(q->code_buffer, i->arity);
            StringValue* str = vector_get(q->program->values, i->name);
            write_ptr(q->code_buffer, str->value);
            break;
        }
        case CALL_OP: {
        CallIns* i = (CallIns*)ins;
            #ifdef DEBUG
                printf("   call #%d %d", i->name, i->arity);
            #endif
            write_int(q->code_buffer, CALL_INS);
            write_int(q->code_buffer, i->arity);
            write_patch_pointer(q, i->name, FUNCTION_PATCH);
        break;
        }
        case SET_LOCAL_OP: {
        SetLocalIns* i = (SetLocalIns*)ins;
            #ifdef DEBUG
                printf("   set local %d", i->idx);
            #endif
            write_int(q->code_buffer, SET_LOCAL_INS);
            write_int(q->code_buffer, i->idx);
            break;
        }
        case GET_LOCAL_OP: {
            GetLocalIns* i = (GetLocalIns*)ins;
            #ifdef DEBUG
                printf("   get local %d", i->idx);
            #endif
            write_int(q->code_buffer, GET_LOCAL_INS);
            write_int(q->code_buffer, i->idx);
            break;
        }
        case SET_GLOBAL_OP: {
            SetGlobalIns* i = (SetGlobalIns*)ins;
            #ifdef DEBUG
                printf("   set global #%d", i->name);
            #endif
            write_int(q->code_buffer, SET_GLOBAL_INS);
            write_patch_int(q, i->name, INT_PATCH);
            break;
        }
        case GET_GLOBAL_OP: {
            GetGlobalIns* i = (GetGlobalIns*)ins;
            #ifdef DEBUG
                printf("   get global #%d", i->name);
            #endif
            write_int(q->code_buffer, GET_GLOBAL_INS);
            write_patch_int(q, i->name, INT_PATCH);
            break;
        }
        case BRANCH_OP: {
            BranchIns* i = (BranchIns*)ins;
            #ifdef DEBUG
                printf("   branch #%d", i->name);
            #endif
            write_int(q->code_buffer, BRANCH_INS);
            write_patch_pointer(q, i->name, LABEL_PATCH);
            break;
        }
        case GOTO_OP: {
            GotoIns* i = (GotoIns*)ins;
            #ifdef DEBUG
                printf("   goto #%d", i->name);
            #endif
            write_int(q->code_buffer, GOTO_INS);
            write_patch_pointer(q, i->name, LABEL_PATCH);
            break;
        }
        case RETURN_OP: {
            #ifdef DEBUG
                printf("   return");
            #endif
                write_int(q->code_buffer, RETURN_INS);
            break;
        }
        case DROP_OP: {
            #ifdef DEBUG
                printf("   drop");
            #endif
            write_int(q->code_buffer, DROP_INS);
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

VMInfo* quicken_vm(Program* p) {
    Quicken* q = init_quicken(p);
    char* ip = process_programe(q);
    VMInfo* vm_info = create_vm_info(q, ip);
    free_quicken(q);
    return vm_info;
}

void write_frame (MethodValue* method, Code* code_buffer) {
  check_size(code_buffer);
  write_int(code_buffer, FRAME_INS);
  write_int(code_buffer, method->nargs);
  write_int(code_buffer, method->nlocals);
}

void* process_methods(Quicken* q) {
    for (int i = 0; i < q->program->values->size; i++) {
        Value* value = vector_get(q->program->values, i);
        if (value->tag == METHOD_VAL) {
            MethodValue* method = (MethodValue*) value;
            add_int_entry(q, i, METHOD_ENTRY);
            write_frame(method, q->code_buffer);
            for (int j = 0; j < method->code->size; j++) {
                parse_ops(q, (ByteIns*) vector_get(method->code, j));
            }
        }
    }
}

void* process_classes(Quicken* q) {
    for (int i = 0; i < q->program->values->size; i++) {
        Value* value = vector_get(q->program->values, i);
        if (value->tag == CLASS_VAL) {
            ClassValue* class = (ClassValue* ) value;
            int classes_idx = make_class(q, class);
            add_class_entry(q, i, classes_idx);
        }
    }
}

void* process_globals(Quicken* q) {
    for (int i = 0; i < q->program->slots->size; i++) {
        int idx = (int) vector_get(q->program->slots, i);
        Value* value = vector_get(q->program->values, idx);
        switch(value->tag) {
            case (METHOD_VAL): {
                MethodValue* method = (MethodValue*) value;
                Entry* entry = get_entry_by_int(q, idx);
                link_entries_with_int(q, entry, method->name);
                break;
            }
            case (SLOT_VAL): {
                SlotValue* svalue = (SlotValue*) value;
                vector_add(q->globals, (void*) svalue->name);
                break;
            }
            default: {
                printf("don't handle tag: %d for global\n", value->tag);
                exit(-1);
            }
        }
    }
}

void* process_patches(Quicken* q) {
    for (int i = 0; i < q->patch_buffer->size; i++) {
        Patch* patch = vector_get(q->patch_buffer, i);
        switch(patch->type) {
            case (LABEL_PATCH):
            case (FUNCTION_PATCH): {
                Entry* entry = get_entry_by_int(q, patch->name);
                ((void**)(q->code_buffer->code + patch->code_pos))[0] = q->code_buffer->code + entry->code_idx;
                break;
            } case(INT_PATCH): {
                int idx = get_global_idx(q, patch->name);
                ((int*)(q->code_buffer->code + patch->code_pos))[0] = idx;
                break;
            } case (CLASS_ARITY_PATCH): {
                Entry* entry = get_entry_by_int(q, patch->name);
                CClass* class = vector_get(q->classes, entry->class_idx);
                ((short*)(q->code_buffer->code + patch->code_pos))[0] = class->nvars;
                break;
            } case (CLASS_TAG_PATCH): {
                Entry* entry = get_entry_by_int(q, patch->name);
                ((short*)(q->code_buffer->code + patch->code_pos))[0] = entry->class_idx;
                break;
            }
            default: {
                printf("patch type not recorgnise: %d\n", patch->type);
            }
        }
    }
}

char* process_programe(Quicken* q) {
    process_methods(q);
    // maybe just add idx to a vector, avoid a second loop.
    process_classes(q);
    process_globals(q);
    process_patches(q);
    Entry* entry = get_entry_by_int(q, q->program->entry);
    return q->code_buffer->code + entry->code_idx;
}
