#pragma once

#include "compiler.h"

typedef struct {
    size_t count;
    size_t capacity;
    char *string;
} String_Builder;

void run_bytecode(Code *code);
char *disassemble(Code *code);
