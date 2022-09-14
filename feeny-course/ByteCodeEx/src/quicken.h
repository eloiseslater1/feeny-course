#ifndef QUICKEN_H
#define QUICKEN_H

#include "utils.h"
#include "ht.h"
#include "codebuffer.h"
#include "heap.h"

typedef struct {
    Vector* patch_buffer;
    Vector* globals;
    Program* program;
    ht* hm;
    Code* code_buffer;
    Vector* classes;
    Vector* const_pool;
} Quicken;

typedef struct {
    Code* code_buffer;
    Vector* classes;
    Vector* const_pool;
    int globals_size;
    char* ip;
} VMInfo;

typedef enum {
  INT_INS,        
  NULL_INS,       
  PRINTF_INS,     
  ARRAY_INS,      
  OBJECT_INS,     
  SLOT_INS,       
  SET_SLOT_INS,   
  CALL_SLOT_INS,  
  CALL_INS,       
  SET_LOCAL_INS,  
  GET_LOCAL_INS,  
  SET_GLOBAL_INS, 
  GET_GLOBAL_INS, 
  BRANCH_INS,     
  GOTO_INS,       
  RETURN_INS,     
  DROP_INS,       
  FRAME_INS       
} OpTag;

typedef enum {
    METHOD_ENTRY,
    LABEL_ENTRY,
    CLASS_ENTRY,
} TYPE_ENTRY;

typedef struct {
    TYPE_ENTRY type;
    union {
        int code_idx;
        int class_idx;
    };
} Entry;

typedef enum {
    FUNCTION_PATCH,
    LABEL_PATCH,
    INT_PATCH,
    CLASS_ARITY_PATCH,
    CLASS_TAG_PATCH
} PATCH_TYPE;

typedef struct {
    PATCH_TYPE type;
    int code_pos;
    int name;
} Patch;

typedef enum {
  VAR_SLOT,
  CODE_SLOT
} SlotTag;

typedef struct {
  SlotTag tag;
  char* name;
  union {
    int idx;
    void* code;
  };
} CSlot;

typedef struct {
  int nvars;
  int nslots;
  CSlot* slots;
} CClass;

VMInfo* quicken_vm(Program* program);

#endif