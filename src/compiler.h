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
    OP_JMPS, OP_ADD, OP_SUB,
    OP_PROMISE, OP_PROMISE_NOT,
    OP_PUSH_INDEX, OP_SET_INDEX,
    OP_SET_LEN, OP_SET_ARRAY,
    OP_RETS, OP_JMPBSI, OP_JMPBSC
} Op_Code;

typedef struct {
    int type;
    union {
        int integer;
        float real;
        uintptr_t pointer;
    } as;
} Value;

typedef union Value32 {
    int integer;
    float real;
} Value32;

typedef struct {
    char *name;
    int type;
} Var;

typedef struct {
    size_t len;
    Value32 *items;
} Array;

typedef struct {
    char *name;
    int location;
    int arity;
    Var *vars;
    int vars_count;
    int return_type;
} Function;

typedef struct {
    size_t count;
    size_t capacity;
    Function *functions;
} Function_List;

typedef struct {
    size_t count;
    size_t capacity;
    Value *constants;
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
