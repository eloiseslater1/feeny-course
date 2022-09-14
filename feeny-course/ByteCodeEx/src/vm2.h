#ifndef VM2_H
#define VM2_H

#include "quicken.h"
#include "heap.h"

typedef struct {
    Vector* stack;
    int fp;
} StackFrame;

typedef struct {
    Vector* classes;
    Code* code_buffer;
    StackFrame* fstack;
    VMNull* null;
    Vector* stack;
    Heap* heap;
    ht* inbuilt;
    char* ip;
    void** genv;
} VM;

void interpret_bc (Program* p);

#endif