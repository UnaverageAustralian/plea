#pragma once

#include "compiler.h"

typedef struct {
    size_t count;
    size_t capacity;
    char *string;
} String_Builder;

typedef struct {
    int val1;
    int val2;
    int loc;
    uint8_t mode;
    uint8_t is_promise;
    int8_t cond;
} When;

typedef struct {
    size_t count;
    size_t capacity;
    When *whens;
} When_Queue;

void run_bytecode(Code *code);
char *disassemble(Code *code);
