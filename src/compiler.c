#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include "compiler.h"

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
        .location = location,
        .arity = 0,
        .vars = malloc(256 * sizeof(char*)),
        .vars_count = 0
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

void error(char *message, int line) {
#ifdef PLEA_DEBUG
    fprintf(stderr, "%d: %s", line, message);
#else
    fprintf(stderr, "%s\n", message);
#endif
    exit(1);
}

void expect_token(Compiler *compiler, Token_Kind token_kind) {
    compiler->pos++;
    if (compiler->tokens->toks[compiler->pos].kind != token_kind) {
#ifdef PLEA_DEBUG
        fprintf(stderr, "Expected %s, got %s\n", token_to_string(token_kind), token_to_string(compiler->tokens->toks[compiler->pos].kind));
#else
        fprintf(stderr, "%d\n", token_kind);
#endif
        exit(1);
    }
}

Token get_and_expect_token(Compiler *compiler, Token_Kind token_kind) {
    compiler->pos++;
    if (compiler->tokens->toks[compiler->pos].kind != token_kind) {
#ifdef PLEA_DEBUG
        fprintf(stderr, "Expected %s, got %s\n", token_to_string(token_kind), token_to_string(compiler->tokens->toks[compiler->pos].kind));
#else
        fprintf(stderr, "%d\n", token_kind);
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

int check_for_when(Compiler* compiler) {
    for (int i = compiler->pos; i < compiler->tokens->count; i++) {
        Token token = compiler->tokens->toks[i];
        if (token.kind == WHEN) return 1;
        if (token.kind == THEN || token.kind == SEMICOLON) break;
    }
    return 0;
}

void compile_when_condition(Compiler* compiler, int cur_byte_pos) {
    consume_token(compiler);
    int cond;

    da_append(compiler->code, OP_RET, bytes);
    Token lhs = consume_token(compiler);

    expect_token(compiler, IS);
    if (peek_token(compiler).kind == NOT) {
        consume_token(compiler);
        cond = 0;
    }
    else {
        cond = 1;
    }

    int instruc_start = (int)compiler->code->count;
    Token rhs = consume_token(compiler);

    int mode = 0;
    switch (lhs.kind) {
    case IDENT:
        for (int i = 0; i < compiler->cur_function->vars_count; i++) {
            mode |= 1;
            if (strcmp(lhs.ident_name, compiler->cur_function->vars[i]) == 0) {
                add_bytes(compiler->code, 2, OP_PUSHI, i);
                break;
            }
            if (i == compiler->cur_function->vars_count - 1) {
                error("Variable not found", __LINE__);
            }
        }
        break;
    case REAL:
    case INTEGER: add_bytes(compiler->code, 2, OP_PUSHI, lhs.int_val); break;
    default: error("MALFORMED TOKEN", __LINE__);
    }

    switch (rhs.kind) {
    case IDENT:
        for (int i = 0; i < compiler->cur_function->vars_count; i++) {
            mode |= 2;
            if (strcmp(rhs.ident_name, compiler->cur_function->vars[i]) == 0) {
                add_bytes(compiler->code, 2, OP_PUSHI, i);
                break;
            }
            if (i == compiler->cur_function->vars_count - 1) {
                error("Variable not found", __LINE__);
            }
        }
        break;
    case REAL:
    case INTEGER: add_bytes(compiler->code, 2, OP_PUSHI, rhs.int_val); break;
    default: error("MALFORMED TOKEN", __LINE__);
    }
    add_bytes(compiler->code, 2, OP_PUSHI, mode);
    da_append(compiler->code, cond ? OP_WHEN : OP_WHEN_NOT, bytes);
    int instruc_end = (int)compiler->code->count;

    if (cur_byte_pos > 256) {
        add_bytes(compiler->code, 2, OP_CONST, compiler->code->constant_list->count);
        da_append(compiler->code->constant_list, cur_byte_pos, constants);
    }
    else {
        add_bytes(compiler->code, 2, OP_PUSHI, cur_byte_pos);
    }

    for (int i = instruc_start; i < instruc_end; i++) {
        da_append(compiler->code, compiler->code->bytes[i], bytes);
    }
    da_append(compiler->code, OP_JMPB, bytes);
}

int compile_function_declaration(Compiler *compiler) {
    da_append(compiler->code, OP_FNCTN, bytes);
    expect_token(compiler, RETURNS);
    int ret_val = get_and_expect_token(compiler, INTEGER).int_val;

    expect_token(compiler, NM);
    char *name = compiler->tokens->toks[compiler->pos+1].ident_name;
    add_string(compiler->code, name);

    add_function(compiler->code->function_list, name, (int)compiler->code->count);
    compiler->cur_function = &compiler->code->function_list->functions[compiler->code->function_list->count - 1];
    consume_token(compiler);

    expect_token(compiler, ARGS);
    while (peek_token(compiler).kind != CALLS) {
        expect_token(compiler, LET);

        char *parameter_name = strdup(consume_token(compiler).ident_name);
        compiler->cur_function->arity++;

        expect_token(compiler, IN);

        Token_Kind type = consume_token(compiler).kind;
        if (type != INT && type != FLOAT && type != VOID && type != CHAR) {
            error("Unknown type", __LINE__);
        }
        if (type != VOID) {
            compiler->cur_function->vars[compiler->cur_function->vars_count] = parameter_name;
            add_bytes(compiler->code, 2, OP_POP, compiler->cur_function->vars_count++);
        }
    }
    consume_token(compiler);
    return ret_val;
}

void compile_chg_expr(Compiler *compiler, int var_id) {
    Token cur_token = consume_token(compiler);

    if (cur_token.kind == STAR) {
        add_bytes(compiler->code, 2, OP_PUSH, var_id);
    }
    else if (cur_token.kind == IDENT) {
        for (int i = 0; i < compiler->cur_function->vars_count; i++) {
            if (strcmp(cur_token.ident_name, compiler->cur_function->vars[i]) == 0) {
                add_bytes(compiler->code, 2, OP_PUSH, i);
                break;
            }
            if (i == compiler->cur_function->vars_count - 1) {
                error("Could not find variable", __LINE__);
            }
        }
    }
    else {
        error("MALFORMED TOKEN", __LINE__);
    }

    while (peek_token(compiler).kind == PLUS || peek_token(compiler).kind == MINUS || peek_token(compiler).kind == TIMES) {
        switch (peek_token(compiler).kind) {
        case PLUS:  da_append(compiler->code, OP_INC, bytes); break;
        case MINUS: da_append(compiler->code, OP_DEC, bytes); break;
        case TIMES:
            consume_token(compiler);
            if (peek_token(compiler).kind != INTEGER) error("MALFORMED TOKEN", __LINE__);
            int num = peek_token(compiler).int_val;

            uint8_t cur_instruction = compiler->code->bytes[compiler->code->count-1];
            if (cur_instruction != OP_INC && cur_instruction != OP_DEC) error("MALFORMED TOKEN", __LINE__);

            for (int i = 1; i < num; i++) {
                da_append(compiler->code, cur_instruction, bytes);
            }
            break;
        default: error("MALFORMED TOKEN", __LINE__);
        }
        consume_token(compiler);
    }
    add_bytes(compiler->code, 2, OP_POP, var_id);
}

void compile_let(Compiler *compiler) {
    compiler->cur_function->vars[compiler->cur_function->vars_count] = strdup(consume_token(compiler).ident_name);

    int cur_byte_pos;
    if (check_for_when(compiler)) {
        add_bytes(compiler->code, 3, OP_SET_VAR, compiler->cur_function->vars_count, 0);
        add_bytes(compiler->code, 4, OP_PUSHI, compiler->code->line_positions->count-1, OP_INC, OP_JMP);
        cur_byte_pos = (int)compiler->code->count;
    }

    expect_token(compiler, EQUALS);
    int val = get_and_expect_token(compiler, INTEGER).int_val;

    if (val < 256 && val >= 0) {
        add_bytes(compiler->code, 3, OP_SET_VAR, compiler->cur_function->vars_count, val);
    }
    else {
        add_bytes(compiler->code, 4, OP_CONST, compiler->code->constant_list->count, OP_POP, compiler->cur_function->vars_count);
        da_append(compiler->code->constant_list, val, constants);
    }
    compiler->cur_function->vars_count++;
    if (peek_token(compiler).kind == WHEN) {
        da_append(compiler->code->line_positions, compiler->code->count+1, positions);
        compile_when_condition(compiler, cur_byte_pos);
    }
}

void compile_chg(Compiler *compiler) {
    int cur_byte_pos;
    if (check_for_when(compiler)) {
        add_bytes(compiler->code, 4, OP_PUSHI, compiler->code->line_positions->count-1, OP_INC, OP_JMP);
        cur_byte_pos = (int)compiler->code->count;
    }

    for (int i = 0; i < compiler->cur_function->vars_count; i++) {
        if (strcmp(peek_token(compiler).ident_name, compiler->cur_function->vars[i]) == 0) {
            consume_token(compiler);
            expect_token(compiler, COMMA);
            if (peek_token(compiler).kind == INTEGER || peek_token(compiler).kind == REAL) {
                int val = get_and_expect_token(compiler, INTEGER).int_val;
                if (val < 256) {
                    add_bytes(compiler->code, 3, OP_SET_VAR, i, val);
                }
                else {
                    add_bytes(compiler->code, 4, OP_CONST, compiler->code->constant_list->count, OP_POP, i);
                    da_append(compiler->code->constant_list, val, constants);
                }
            }
            else {
                compile_chg_expr(compiler, i);
            }
            break;
        }
        if (i == compiler->cur_function->vars_count - 1) error("Variable not found", __LINE__);
    }

    if (peek_token(compiler).kind == WHEN) {
        da_append(compiler->code->line_positions, compiler->code->count+1, positions);
        compile_when_condition(compiler, cur_byte_pos);
    }
}

void compile_function_call(Compiler *compiler) {
    char *function_name = strdup(consume_token(compiler).ident_name);
    expect_token(compiler, IN);

    int cur_byte_pos;
    if (check_for_when(compiler)) {
        add_bytes(compiler->code, 4, OP_PUSHI, compiler->code->line_positions->count-1, OP_INC, OP_JMP);
        cur_byte_pos = (int)compiler->code->count;
    }

    int arguments = 0;
    for (; ;) {
        arguments++;
        if (strcmp(peek_token(compiler).ident_name, "input") == 0) {
            da_append(compiler->code, OP_INPUT, bytes);
        }
        else {
            for (int i = 0; i < compiler->cur_function->vars_count; i++) {
                if (strcmp(peek_token(compiler).ident_name, compiler->cur_function->vars[i]) == 0) {
                    add_bytes(compiler->code, 2, OP_PUSH, i);
                    break;
                }
                if (i == compiler->cur_function->vars_count - 1) {
                    error("Variable not found", __LINE__);
                }
            }
        }
        consume_token(compiler);
        if (peek_token(compiler).kind != COMMA) {
            da_append(compiler->code, OP_CALL, bytes);
            add_string(compiler->code, function_name);
            expect_token(compiler, ENDIN);
            break;
        }
        consume_token(compiler);
    }
    for (int i = 0; i < compiler->code->function_list->count; i++) {
        if (strcmp(function_name, compiler->code->function_list->functions[i].name) == 0) {
            if (compiler->code->function_list->functions[i].arity != arguments)
                error("Function call has wrong amount of arguments", __LINE__);
            break;
        }
        if (i == compiler->code->function_list->count - 1 && strcmp(function_name, "print") == 0) {
            if (arguments != 1) error("Function call has wrong amount of arguments", __LINE__);
        }
        else if (i == compiler->code->function_list->count - 1) {
            error("Function not found", __LINE__);
        }
    }

    if (peek_token(compiler).kind == WHEN) {
        da_append(compiler->code->line_positions, compiler->code->count+1, positions);
        compile_when_condition(compiler, cur_byte_pos);
    }
}

void compile_jump(Compiler *compiler) {
    int cur_line = (int)compiler->code->line_positions->count-1;

    int cur_byte_pos;
    if (check_for_when(compiler)) {
        add_bytes(compiler->code, 4, OP_PUSHI, compiler->code->line_positions->count-1, OP_INC, OP_JMP);
        cur_byte_pos = (int)compiler->code->count;
    }

    expect_token(compiler, UNDER);
    if (cur_line >= 256 || cur_line < 0) {
        add_bytes(compiler->code, 2, OP_CONST, compiler->code->constant_list->count);
        da_append(compiler->code->constant_list, cur_line, constants);
    }
    else {
        add_bytes(compiler->code, 2, OP_PUSHI, cur_line);
    }

    while (peek_token(compiler).kind == PLUS || peek_token(compiler).kind == MINUS) {
        if (peek_token(compiler).kind == PLUS) {
            da_append(compiler->code, OP_INC, bytes);
        }
        else if (peek_token(compiler).kind == MINUS) {
            da_append(compiler->code, OP_DEC, bytes);
        }
        else {
            error("MALFORMED TOKEN", __LINE__);
        }
        consume_token(compiler);
    }

    if (peek_token(compiler).kind == WHEN) {
        add_bytes(compiler->code, 3, OP_POPR, OP_POPR, OP_JMP);
        da_append(compiler->code->line_positions, compiler->code->count+1, positions);
        compile_when_condition(compiler, cur_byte_pos);
    }
    else {
        da_append(compiler->code, OP_JMP, bytes);
    }
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

    code->line_positions = malloc(sizeof(Line_Pos_List));
    code->line_positions->count = 0;
    code->line_positions->capacity = 4;
    code->line_positions->positions = malloc(4 * sizeof(int));

    *compiler = (Compiler){
        .code = code,
        .tokens = tokens,
        .pos = 0,
        .is_in_function = 0,
        .cur_function = NULL,
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

    da_append(compiler.code->line_positions, compiler.code->count, positions);

    int cur_ret_val = 0;
    Token token = compiler.tokens->toks[compiler.pos];
    while (token.kind != T_EOF) {
        if (compiler.is_in_function) {
            switch (token.kind) {
            case SEMICOLON:
                add_bytes(compiler.code, 3, OP_PUSHI, cur_ret_val, OP_RET);
                compiler.is_in_function = 0;
                free(compiler.cur_function->vars);
                break;
            case FNCTN:
                cur_ret_val = compile_function_declaration(&compiler);
                break;
            case LET:
                compile_let(&compiler);
                if (peek_token(&compiler).kind != SEMICOLON) expect_token(&compiler, THEN);
                break;
            case CHG:
                compile_chg(&compiler);
                if (peek_token(&compiler).kind != SEMICOLON) expect_token(&compiler, THEN);
                break;
            case CALL:
                compile_function_call(&compiler);
                if (peek_token(&compiler).kind != SEMICOLON) expect_token(&compiler, THEN);
                break;
            case JMP:
                compile_jump(&compiler);
                if (peek_token(&compiler).kind != SEMICOLON) expect_token(&compiler, THEN);
                break;
            default: error("MALFORMED TOKEN", __LINE__);
            }
            da_append(compiler.code->line_positions, compiler.code->count, positions);
            token = consume_token(&compiler);
        }
        else if (compiler.tokens->toks[compiler.pos].kind == FNCTN) {
            compiler.is_in_function = 1;
        }
        else {
            token = consume_token(&compiler);
        }
    }
    compiler.code->line_positions->count--;
    return compiler.code;
}
