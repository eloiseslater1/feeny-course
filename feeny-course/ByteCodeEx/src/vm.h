#ifndef VM2_H
#define VM2_H

#include "quicken.h"

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
    void* slots[];
} VMObj;

typedef struct {
    long tag;
    int length;
    void* items[];
} VMArray;


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