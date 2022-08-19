#ifndef INBUILT_H
#define INBUILT_H

#include "vm.h"
#include "bytecode.h"
#include "utils.h"

void do_function(char* string, Vector* stack, void** args);
void format_print(StringValue* format_string, Vector* stack, int nargs);

typedef enum {
  ADD,
  SUB,
  PRINT
} Function;

#endif 