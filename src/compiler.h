#pragma once

#include <stdint.h>

#include "lexer.h"

typedef enum {
    OP_CONST,
    OP_INC, OP_DEC,
    OP_SET_VAR, OP_CHG_VAR,
    OP_REASSIGN, OP_PUSH,
    OP_POP, OP_CALL, OP_RET,
    OP_FNCTN, OP_HLT, OP_BEG,
    OP_PUSHI, OP_INPUT
} Op_Code;

typedef struct {
    char *name;
    union {
        int integer;
        float real;
    } as;
} Var;

typedef struct {
    char *name;
    int location;
    int arity;
    Var *vars;
    int vars_count;
} Function;

typedef struct {
    size_t count;
    size_t capacity;
    Function *functions;
} Function_List;

typedef struct {
    size_t count;
    size_t capacity;
    int *constants;
} Constant_List;

typedef struct {
    size_t count;
    size_t capacity;
    uint8_t *bytes;
    Function_List *function_list;
    Constant_List *constant_list;
} Code;

typedef struct {
    Code *code;
    Token_List *tokens;
    int pos;
    int is_in_function;
    Function *cur_function;
} Compiler;

Code *compile(Token_List *tokens);
