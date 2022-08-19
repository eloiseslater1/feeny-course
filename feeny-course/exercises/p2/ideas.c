

// stack
#define STACK_MAX 256

typedef double Value;

typedef struct {
    int capacity;
    Value stack[STACK_MAX];
    Value *stackTop;
} Stack;
