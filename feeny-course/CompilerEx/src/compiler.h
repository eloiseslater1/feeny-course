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
    MethodValue* global_scope;
    MethodValue* local_scope;
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
