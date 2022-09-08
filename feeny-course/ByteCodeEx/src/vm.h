#ifndef VM_H
#define VM_H

#include "bytecode.h"
#include "ht.h"
#include "utils.h"
#include "frame.h"

//#define DEBUG

typedef struct {
    Vector* stack;
    ht* hm;
    ht* labels;
    ht* inbuilt;
    Frame* current_frame;
    Vector* const_pool;
    void** IP;
} VM;

void interpret_bc (Program* prog);
VM* init_vm(Program* p);

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
    long null;
} VMNull;

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

#endif
