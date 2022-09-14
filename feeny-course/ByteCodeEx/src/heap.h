#ifndef HEAP_H
#define HEAP_H

#include <stdio.h>
#include <stdlib.h>

#define MB (1024 * 1024)

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

Heap* init_heap();
void* halloc (Heap* heap, long tag, int sz);
VMValue* create_null(Heap* heap);
VMInt* create_int(Heap* heap, int a);
VMArray* create_array(Heap* heap, int length, int initial);
VMObj* create_object(Heap* heap, int class, int arity);
#endif
