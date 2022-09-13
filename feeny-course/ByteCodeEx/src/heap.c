#include "heap.h"

Heap* init_heap() {
    Heap* heap = malloc(sizeof(Heap));
    heap->size = MB * 1000;
    heap->memory = malloc(heap->size);
    heap->head = heap->memory + heap->size;
    heap->sp = heap->memory;
    return heap;
}

void* halloc (Heap* heap, long tag, int sz) {
  if(heap->sp + sz > heap->head){
      printf("Out of Memory.\n");
      exit(-1);
    }
  long* obj = (long*)heap->sp;
  obj[0] = tag;
  heap->sp += sz;
  return obj;
}