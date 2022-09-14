#include "heap.h"

Heap* init_heap() {
    Heap* heap = malloc(sizeof(Heap));
    heap->size = MB * 100;
    heap->memory = malloc(heap->size);
    heap->head = heap->memory + heap->size;
    heap->sp = heap->memory;
    return heap;
}

void free_heap(Heap* heap) {
    free(heap->memory);
    free(heap);
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

VMValue* create_null(Heap* heap) {
  VMNull* null = (VMNull*) halloc(heap, VM_NULL, sizeof(VMNull));
  return (VMValue*) null;
}

VMInt* create_int(Heap* heap, int a) {
  VMInt* value = (VMInt*) halloc(heap, VM_INT, sizeof(VMInt));
  value->value = a;
  return value;
}

VMArray* create_array(Heap* heap, int length, int initial) {
    VMArray* array = (VMArray*) halloc(heap, VM_ARRAY, sizeof(VMArray) + sizeof(void*) * length);
    for (int i = 0; i < length; i++) {
        array->items[i] = initial;
    }
    array->length = length;
    return array;
}

VMObj* create_object(Heap* heap, int class, int arity) {
  VMObj* obj = halloc(heap, class, sizeof(VMObj) + sizeof(void*) * arity);
}
