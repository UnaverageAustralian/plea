#pragma once

#include <stdint.h>

#include "lexer.h"

typedef enum {
    OP_CONST,
    OP_INC, OP_DEC,
    OP_SET_VAR, OP_CHG_VAR,
    OP_REASSIGN, OP_PUSH,
    OP_POP, OP_CALL, OP_RET,
    OP_FNCTN, OP_HLT, OP_BEG
} Op_Code;

typedef struct {
    char *name;
    int location;
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
    char *name;
    union {
        int integer;
        float real;
    } as;
} Var;

typedef struct {
    Code *code;
    Token_List *tokens;
    Var vars[256];
    int vars_count;
    int pos;
    int is_in_function;
} Compiler;

Code *compile(Token_List *tokens);
