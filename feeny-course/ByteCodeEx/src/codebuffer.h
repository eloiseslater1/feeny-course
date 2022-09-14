#ifndef CODEBUFFER_H
#define CODEBUFFER_H

#include <stdlib.h>

typedef struct {
    char* code;
    int capacity;
    char* sp;
} Code;

Code* init_code_buffer();
void check_size(Code* code_buffer);
int get_code_idx(Code* code_buffer);
void align_short (Code* code_buffer);
void align_int (Code* code_buffer);
void align_ptr (Code* code_buffer);
void write_char (Code* code_buffer, char c);
void write_short (Code* code_buffer, short s);
void write_int (Code* code_buffer, int i);
void write_ptr (Code* code_buffer, void* ptr);
void free_code_buffer(Code* code_buffer);

#endif