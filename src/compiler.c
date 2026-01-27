#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include "compiler.h"

#define PLEA_DEBUG

#define da_append(a,i,n)                                                    \
    do {                                                                    \
        if ((a)->count == (a)->capacity) {                                  \
            (a)->capacity *= 2;                                             \
            (a)->n = realloc((a)->n, (a)->capacity * sizeof(*((a)->n)));    \
            assert((a)->n != NULL);                                         \
        }                                                                   \
        (a)->n[(a)->count] = i;                                             \
        (a)->count++;                                                       \
    } while (0)

void add_function(Function_List *function_list, char *name, int location) {
    if (function_list->count == function_list->capacity) {
        function_list->capacity *= 2;
        function_list->functions = realloc(function_list->functions, function_list->capacity * sizeof(Function));
        assert(function_list->functions != NULL);
    }
    function_list->functions[function_list->count] = (Function){
        .name = strdup(name),
        .location = location
    };
    function_list->count++;
}

void add_bytes(Code *code, int num_bytes, ...) {
    va_list args;
    va_start(args, num_bytes);
    for (int i = 0; i < num_bytes; i++) {
        da_append(code, va_arg(args, int), bytes);
    }
    va_end(args);
}

void add_string(Code *code, char *str) {
    size_t len = strlen(str);
    for (int i = 0; i < len; i++) {
        da_append(code, str[i], bytes);
    }
    da_append(code, '\0', bytes);
}

void error(char *message) {
    fprintf(stderr, "%s", message);
    exit(1);
}

void expect_token(Compiler *compiler, Token_Kind token_kind) {
    compiler->pos++;
    if (compiler->tokens->toks[compiler->pos].kind != token_kind) {
#ifdef PLEA_DEBUG
        fprintf(stderr, "Expected %s, got %s", token_to_string(token_kind), token_to_string(compiler->tokens->toks[compiler->pos].kind));
#else
        fprintf(stderr, "%d", token_kind);
#endif
        exit(1);
    }
}

Token get_and_expect_token(Compiler *compiler, Token_Kind token_kind) {
    compiler->pos++;
    if (compiler->tokens->toks[compiler->pos].kind != token_kind) {
#ifdef PLEA_DEBUG
        fprintf(stderr, "Expected %s, got %s", token_to_string(token_kind), token_to_string(compiler->tokens->toks[compiler->pos].kind));
#else
        fprintf(stderr, "%d", token_kind);
#endif
        exit(1);
    }
    return compiler->tokens->toks[compiler->pos];
}

Token consume_token(Compiler *compiler) {
    compiler->pos++;
    return compiler->tokens->toks[compiler->pos];
}

Token peek_token(Compiler *compiler) {
    return compiler->tokens->toks[compiler->pos+1];
}

int compile_function_declaration(Compiler *compiler) {
    da_append(compiler->code, OP_FNCTN, bytes);
    expect_token(compiler, RETURNS);
    int ret_val = get_and_expect_token(compiler, INTEGER).int_val;

    expect_token(compiler, NM);
    char *name = compiler->tokens->toks[compiler->pos+1].ident_name;
    add_string(compiler->code, name);

    add_function(compiler->code->function_list, name, (int)compiler->code->count);
    consume_token(compiler);

    expect_token(compiler, ARGS);
    while (compiler->tokens->toks[compiler->pos].kind != CALLS) {
        consume_token(compiler);
    }
    return ret_val;
}

void compile_chg_expr(Compiler *compiler, int var_id) {
    Token cur_token = consume_token(compiler);

    if (cur_token.kind == STAR) {
        add_bytes(compiler->code, 2, OP_PUSH, var_id);
    }
    else if (cur_token.kind == IDENT) {
        for (int i = 0; i < compiler->vars_count; i++) {
            if (strcmp(cur_token.ident_name, compiler->vars[i].name) == 0) {
                add_bytes(compiler->code, 2, OP_PUSH, i);
                break;
            }
            if (i == compiler->vars_count - 1) {
                error("Could not find variable");
            }
        }
    }
    else {
        error("MALFORMED TOKEN");
    }

    while (peek_token(compiler).kind == PLUS || peek_token(compiler).kind == MINUS || peek_token(compiler).kind == TIMES) {
        switch (peek_token(compiler).kind) {
        case PLUS:  da_append(compiler->code, OP_INC, bytes); break;
        case MINUS: da_append(compiler->code, OP_DEC, bytes); break;
        case TIMES:
            consume_token(compiler);
            if (peek_token(compiler).kind != INTEGER) error("MALFORMED TOKEN");
            int num = peek_token(compiler).int_val;

            uint8_t cur_instruction = compiler->code->bytes[compiler->code->count-1];
            if (cur_instruction != OP_INC && cur_instruction != OP_DEC) error("MALFORMED TOKEN");

            for (int i = 1; i < num; i++) {
                da_append(compiler->code, cur_instruction, bytes);
            }
            break;
        default: error("MALFORMED TOKEN");
        }
        consume_token(compiler);
    }
    add_bytes(compiler->code, 2, OP_POP, var_id);
}

void init_compiler(Token_List *tokens, Compiler *compiler) {
    Code *code = malloc(sizeof(Code));
    code->count = 0;
    code->capacity = 4;
    code->bytes = malloc(4 * sizeof(uint8_t));

    code->function_list = malloc(sizeof(Function_List));
    code->function_list->count = 0;
    code->function_list->capacity = 4;
    code->function_list->functions = malloc(4 * sizeof(Function));

    code->constant_list = malloc(sizeof(Constant_List));
    code->constant_list->count = 0;
    code->constant_list->capacity = 4;
    code->constant_list->constants = malloc(4 * sizeof(int));

    *compiler = (Compiler){
        .code = code,
        .tokens = tokens,
        .pos = 0,
        .is_in_function = 0,
        .vars = {0},
        .vars_count = 0
    };
}

Code *compile(Token_List *tokens) {
    Compiler compiler;
    init_compiler(tokens, &compiler);

    if (tokens->toks[0].kind == BEG) {
        da_append(compiler.code, OP_BEG, bytes);
        consume_token(&compiler);
        add_string(compiler.code, tokens->toks[1].ident_name);
        expect_token(&compiler, SEMICOLON);
    }

    da_append(compiler.code, OP_CALL, bytes);
    add_string(compiler.code, "main");
    da_append(compiler.code, OP_HLT, bytes);

    int cur_ret_val = 0;
    Token token = compiler.tokens->toks[compiler.pos];
    while (token.kind != T_EOF) {
        if (compiler.is_in_function) {
            switch (token.kind) {
            case SEMICOLON:
                add_bytes(compiler.code, 2, OP_RET, cur_ret_val);
                compiler.is_in_function = 0;
                // Will change later
                memset(compiler.vars, 0, sizeof(compiler.vars));
                break;
            case FNCTN:
                cur_ret_val = compile_function_declaration(&compiler);
                break;
            case LET:
                compiler.vars[compiler.vars_count] = (Var){
                    .name = strdup(consume_token(&compiler).ident_name),
                    .as = 0
                };

                expect_token(&compiler, EQUALS);
                int val = get_and_expect_token(&compiler, INTEGER).int_val;
                compiler.vars[compiler.vars_count].as.integer = val;

                if (val < 256) {
                    add_bytes(compiler.code, 3, OP_SET_VAR, compiler.vars_count, val);
                }
                else {
                    add_bytes(compiler.code, 4, OP_CONST, compiler.code->constant_list->count, OP_POP, compiler.vars_count);
                    da_append(compiler.code->constant_list, val, constants);
                }
                compiler.vars_count++;

                if (peek_token(&compiler).kind != SEMICOLON) expect_token(&compiler, THEN);
                break;
            case CHG:
                for (int i = 0; i < compiler.vars_count; i++) {
                    if (strcmp(peek_token(&compiler).ident_name, compiler.vars[i].name) == 0) {
                        consume_token(&compiler);
                        expect_token(&compiler, COMMA);
                        if (peek_token(&compiler).kind == INTEGER || peek_token(&compiler).kind == REAL) {
                            val = get_and_expect_token(&compiler, INTEGER).int_val;
                            if (val < 256) {
                                add_bytes(compiler.code, 3, OP_CHG_VAR, i, val);
                            }
                            else {
                                add_bytes(compiler.code, 4, OP_CONST, compiler.code->constant_list->count, OP_POP, i);
                                da_append(compiler.code->constant_list, val, constants);
                            }
                        }
                        else {
                            compile_chg_expr(&compiler, i);
                        }
                        break;
                    }
                    if (i == compiler.vars_count - 1) {
                        error("Variable not found");
                    }
                }
                if (peek_token(&compiler).kind != SEMICOLON) expect_token(&compiler, THEN);
                break;
            case CALL:
                char *function_name = strdup(consume_token(&compiler).ident_name);
                expect_token(&compiler, IN);
                for (int i = 0; i < compiler.vars_count; i++) {
                    if (strcmp(peek_token(&compiler).ident_name, compiler.vars[i].name) == 0) {
                        add_bytes(compiler.code, 2, OP_PUSH, i);
                        break;
                    }
                    if (i == compiler.vars_count - 1 && strcmp(peek_token(&compiler).ident_name, "input") != 0) {
                        error("Variable not found");
                    }
                    else if (i == compiler.vars_count - 1) {
                        //todo
                    }
                }
                da_append(compiler.code, OP_CALL, bytes);
                add_string(compiler.code, function_name);

                consume_token(&compiler);
                expect_token(&compiler, ENDIN);
                if (peek_token(&compiler).kind != SEMICOLON) expect_token(&compiler, THEN);
                break;
            default: error("MALFORMED TOKEN");
            }
        }
        else if (peek_token(&compiler).kind == FNCTN) {
            compiler.is_in_function = 1;
        }
        token = consume_token(&compiler);
    }
    return compiler.code;
}
