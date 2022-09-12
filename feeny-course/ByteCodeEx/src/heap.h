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

Heap* init_heap();
void* halloc (Heap* heap, long tag, int sz);

#endif
