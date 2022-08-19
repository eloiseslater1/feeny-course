#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "inbuilt.h"

Function getFunc(char* string);
void new_int(int value, Vector* stack);

Function getFunc(char* string) {
  if(strcmp(string, "add") == 0) return ADD;
  if(strcmp(string, "sub") == 0) return SUB;
  if(strcmp(string, "print") == 0) return PRINT;
}

void new_int(int value, Vector* stack) {
    IntValue* new_value = malloc(sizeof(IntValue));
    new_value->tag = INT_VAL;
    new_value->value = value;
    vector_add(stack, (void*) new_value);
}

void do_function(char* string, Vector* stack, void** args) {
  switch (getFunc(string)) {
    case ADD: {
      int value = ((IntValue*) args[0])->value + ((IntValue*) args[1])->value; 
      new_int(value, stack);
      break;
    }
    case SUB: {
      int value = ((IntValue*) args[0])->value - ((IntValue*) args[1])->value;  
      new_int(value, stack);
      break;
    }
  }
}

void format_print(StringValue* format_string, Vector* stack, int nargs) {
    printf("\n");
    char *string = format_string->value;
    while (*string != '\0') {
        if (*string == '~') {
            printf("%d", ((IntValue*) vector_pop(stack))->value);
        }
        printf("%c", *string);
        string++;
    }
    Value* null = (Value*) malloc(sizeof(NULL_VAL));
    null->tag = NULL_VAL;
    vector_add(stack, (void*) null);
}
