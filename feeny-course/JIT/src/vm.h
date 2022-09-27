#ifndef VM2_H
#define VM2_H

#include <stdint.h>
#include "quicken.h"

//#define DEBUG

#define MB (1024 * 1024)

typedef struct {
    Vector* stack;
    int fp;
} StackFrame;

typedef struct {
    int size;
    char* memory;
    char* sp;
    char* head;
    char* free;
} Heap;

typedef enum {
    VM_INT,
    VM_NULL,
    VM_ARRAY,
} VM_TAG;

typedef struct {
    long tag;
    long scratch;
} VMNull;


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
    intptr_t slots[];
} VMObj;

typedef struct {
    long tag;
    long length;
    intptr_t items[];
} VMArray;


typedef struct {
    Vector* classes;
    Code* code_buffer;
    StackFrame* fstack;
    intptr_t null;
    Vector* stack;
    Heap* heap;
    char* ip;
    int genv_size;
    intptr_t* genv;
} VM;

typedef struct {
    long tag;
    intptr_t forwarding;
} BHeart;


void interpret_bc (Program* p);

#endif