#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "jit1.h"

extern char return_ins[];
extern char return_ins_end[];
extern char inc_counter_ins[];
extern char inc_counter_ins_end[];
long counter = 0;

int main (int argc, char** argvs) {
    long value = 102;
    run_code(return_ins, return_ins_end, value);
    run_code(inc_counter_ins, inc_counter_ins_end, (long) &counter);
}

void run_code(char* start, char* end, long value) {
    char* code = mmap(0, 1024,
                     PROT_READ|PROT_WRITE|PROT_EXEC,
                    MAP_PRIVATE|MAP_ANON, -1, 0);
    
    int code_size = end - start;
    memcpy(code, start, code_size);
    change_placeholder(code, code_size, value);
    long (*f)() = (void*) code;
    printf("Returned %ld\n", f());
}

void change_placeholder(char* code, int code_size, long value) {
    long place_holder = 0xcafebabecafebabe;
    long l;
    for (int i = 0; i < code_size; i++) {
        int long_end = i + sizeof(long);
        if (((unsigned char) code[i]) == 0xbe & (long_end <= code_size)) {
            memcpy(&l, &code[i], sizeof(long));
            if (l == 0xcafebabecafebabe) {
                memcpy(&code[i], &value, sizeof(long));
            }
        }
    }

}