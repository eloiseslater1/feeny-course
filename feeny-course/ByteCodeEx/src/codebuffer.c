#include <string.h>
#include <stdio.h>

#include "codebuffer.h"

#define MB (1024 * 1024)

Code* init_code_buffer() {
    Code* code_buffer = malloc(sizeof(Code));
    code_buffer->code = malloc(MB * 24);
    code_buffer->capacity = MB * 24;
    code_buffer->sp = code_buffer->code;
    return code_buffer;
}

void free_code_buffer(Code* code_buffer) {
  free(code_buffer->code);
  free(code_buffer);
}

void check_size(Code* code_buffer) {
  int size = code_buffer->sp - code_buffer->code;
  if (size + 2 * sizeof(long) > code_buffer->capacity) {
    int new_cap = code_buffer->capacity * 2;
    char* buf = malloc(new_cap);
    memcpy(buf, code_buffer->code, size);
    free(code_buffer->code);
    code_buffer->code = buf;
    code_buffer->capacity = new_cap;
    code_buffer->sp = code_buffer->code + size;
  }
}

int get_code_idx(Code* code_buffer) {
    return code_buffer->sp - code_buffer->code;
}

void align_short (Code* code_buffer) {
  code_buffer->sp = (char*)(((long)code_buffer->sp + 1)&(-2));
}
void align_int (Code* code_buffer) {
  code_buffer->sp = (char*)(((long)code_buffer->sp + 3)&(-4));
}
void align_ptr (Code* code_buffer) {
  code_buffer->sp = (char*)(((long)code_buffer->sp + 7)&(-8));
}

void write_char (Code* code_buffer, char c) {
  check_size(code_buffer);  
  code_buffer->sp[0] = c;
  code_buffer->sp++;
}

void write_short (Code* code_buffer, short s) {
  check_size(code_buffer);
  align_short(code_buffer);
  ((short*)code_buffer->sp)[0] = s;
  code_buffer->sp += sizeof(short);
}

void write_int (Code* code_buffer, int i) {
  check_size(code_buffer);
  align_int(code_buffer);
  ((int*)code_buffer->sp)[0] = i;
  code_buffer->sp += sizeof(int);
}

void write_ptr (Code* code_buffer, void* ptr) {
  check_size(code_buffer);
  align_ptr(code_buffer);
  ((void**)code_buffer->sp)[0] = ptr;
  code_buffer->sp += sizeof(void*);
}








