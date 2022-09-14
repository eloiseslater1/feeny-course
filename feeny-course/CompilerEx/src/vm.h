#ifndef VM_H
#define VM_H

#include "bytecode.h"
#include "ht.h"
#include "utils.h"
#include "frame.h"

#define DEBUG

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

#endif
