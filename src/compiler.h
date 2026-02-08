#pragma once

#include <stdint.h>

#include "lexer.h"

typedef enum {
    OP_CONST,
    OP_INC, OP_DEC,
    OP_SET_VAR, OP_REASSIGN, OP_PUSH,
    OP_POP, OP_CALL, OP_RET,
    OP_FNCTN, OP_HLT, OP_BEG,
    OP_PUSHI, OP_INPUT,
    OP_JMP, OP_JMPB, OP_WHEN,
    OP_WHEN_NOT, OP_POPR, OP_JMPBS,
    OP_JMPS, OP_ADD, OP_SUB
} Op_Code;

typedef struct {
    char *name;
    int location;
    int arity;
    char **vars;
    int vars_count;
} Function;

typedef struct {
    size_t count;
    size_t capacity;
    Function *functions;
} Function_List;

typedef struct {
    int8_t cond;
    union {
        uint8_t var1;
        int val1;
    };
    union {
        uint8_t var2;
        int val2;
    };
    int loc;
    uint8_t mode;
} When;

typedef struct {
    size_t count;
    size_t capacity;
    When *whens;
} When_Queue;

typedef struct {
    size_t count;
    size_t capacity;
    int *constants;
} Constant_List;

typedef struct {
    size_t count;
    size_t capacity;
    int *positions;
} Line_Pos_List;

typedef struct {
    size_t count;
    size_t capacity;
    uint8_t *bytes;
    Function_List *function_list;
    Constant_List *constant_list;
    Line_Pos_List *line_positions;
} Code;

typedef struct {
    Code *code;
    Token_List *tokens;
    Function *cur_function;
    int pos;
    int is_in_function;
} Compiler;

Code *compile(Token_List *tokens);
