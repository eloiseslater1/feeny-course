#ifndef VM2_H
#define VM2_H

#include "utils.h"
#include "ht.h"
#include "codebuffer.h"
#include "heap.h"

//#define DEBUG

typedef struct {
    Vector* stack;
    int fp;
} StackFrame;

typedef struct {
    long tag;
    long scratch;
} VMNull;

typedef struct {
    Vector* patch_buffer;
    Vector* globals;
    Program* program;
    ht* hm;
    Code* code_buffer;
    StackFrame* fstack;
    VMNull* null;
    Vector* stack;
    Heap* heap;
    ht* inbuilt;
    char* ip;
    void** genv;
} VM;

typedef enum {
    METHOD_ENTRY,
    LABEL_ENTRY,
} TYPE_ENTRY;

typedef struct {
    TYPE_ENTRY type;
    int code_idx;
} Entry;

typedef enum {
    FUNCTION_PATCH,
    LABEL_PATCH,
    INT_PATCH
} PATCH_TYPE;

typedef struct {
    PATCH_TYPE type;
    int code_pos;
    int name;
} Patch;

typedef enum {
    VM_INT,
    VM_NULL,
    VM_ARRAY,
} VM_TAG;

typedef struct {
    long tag;
} VMValue;

typedef struct {
    long tag;
    long value;
} VMInt;

typedef struct {
    long tag;
    void* parent;
    void* slots[];
} VMObj;

typedef struct {
    long tag;
    int length;
    void* items[];
} VMArray;

typedef enum {
  INT_INS,        //0
  NULL_INS,       //1
  PRINTF_INS,     //2
  ARRAY_INS,      //3
  OBJECT_INS,     //4
  SLOT_INS,       //5
  SET_SLOT_INS,   //6
  CALL_SLOT_INS,  //7
  CALL_INS,       //8
  SET_LOCAL_INS,  //9
  GET_LOCAL_INS,  //a
  SET_GLOBAL_INS, //b
  GET_GLOBAL_INS, //c
  BRANCH_INS,     //d
  GOTO_INS,       //e
  RETURN_INS,     //f
  DROP_INS,       //10
  FRAME_INS       //11
} OpTag;



void interpret_bc();

#endif
