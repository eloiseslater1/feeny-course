#ifndef JIT1_H
#define JIT1_H

void change_placeholder(char* code, int code_size, long value);
void run_code(char* start, char* end, long value);

#endif