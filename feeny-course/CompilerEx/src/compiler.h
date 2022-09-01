#ifndef COMPILER_H
#define COMPILER_H
#include "ast.h"
#include "bytecode.h"
#include "utils.h"
#include "ht.h"

typedef struct {
    Program* programe;
    ht* strings_idx;
    ht* int_idx;
    MethodValue* global_frame;
    MethodValue* local_frame;
    ht* local_scope;
    ht* global_scope;
    int label_num;
} Compiler;

Program* compile (ScopeStmt* stmt);
Compiler* init_compiler();
void free_compiler(Compiler* compiler);
void parse_scope(Compiler* compiler, ScopeStmt* s);
void add_exp(Exp* e, Compiler* compiler);

typedef struct {
    AstTag tag;
    int len;
    char* value;
} String;

#endif
