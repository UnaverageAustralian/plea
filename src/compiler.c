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

void add_constant(Constant_List *constant_list, int val) {
    if (constant_list->count == constant_list->capacity) {
        constant_list->capacity *= 2;
        constant_list->constants = realloc(constant_list->constants, constant_list->capacity * sizeof(Value));
        assert(constant_list->constants != NULL);
    }
    constant_list->constants[constant_list->count].as.integer = val;
    constant_list->count++;
}

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
        .vars = malloc(256 * sizeof(Var)),
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

Token consume_token(Compiler *compiler) {
    compiler->pos++;
    return compiler->tokens->toks[compiler->pos];
}

Token peek_token(Compiler *compiler) {
    return compiler->tokens->toks[compiler->pos+1];
}

Token cur_token(Compiler *compiler) {
    return compiler->tokens->toks[compiler->pos];
}

void error(Compiler *compiler, char *message, int line) {
    int pos = compiler->pos-1;
    while (compiler->tokens->toks[pos].kind != THEN
        && compiler->tokens->toks[pos].kind != CALLS
        && compiler->tokens->toks[pos].kind != FNCTN
        && compiler->tokens->toks[pos].kind != BEG) {
        pos--;
    }
    pos++;

    while (compiler->tokens->toks[pos].kind != THEN && compiler->tokens->toks[pos].kind != SEMICOLON && compiler->tokens->toks[pos].kind != CALLS) {
        if (compiler->tokens->toks[pos].kind == CATCH && compiler->tokens->toks[pos+1].kind == ERROR) return;
        pos++;
    }

#ifdef PLEA_DEBUG
    fprintf(stderr, "%d: %s\n", line, message);
#else
    fprintf(stderr, "%s\n", message);
#endif
    exit(1);
}

void expect_token(Compiler *compiler, Token_Kind token_kind) {
    compiler->pos++;
    if (cur_token(compiler).kind != token_kind) {
        int pos = compiler->pos-1;
        while (compiler->tokens->toks[pos].kind != THEN
            && compiler->tokens->toks[pos].kind != CALLS
            && compiler->tokens->toks[pos].kind != FNCTN
            && compiler->tokens->toks[pos].kind != BEG) {
            pos--;
        }
        pos++;

        while (compiler->tokens->toks[pos].kind != THEN && compiler->tokens->toks[pos].kind != SEMICOLON && compiler->tokens->toks[pos].kind != CALLS) {
            if (compiler->tokens->toks[pos].kind == CATCH && compiler->tokens->toks[pos+1].kind == ERROR) return;
            pos++;
        }

#ifdef PLEA_DEBUG
        fprintf(stderr, "Expected %s, got %s\n", token_to_string(token_kind), token_to_string(cur_token(compiler).kind));
#else
        fprintf(stderr, "%d\n", token_kind);
#endif
        exit(1);
    }
}

Token get_and_expect_token(Compiler *compiler, Token_Kind token_kind) {
    compiler->pos++;
    if (cur_token(compiler).kind != token_kind) {
        int pos = compiler->pos-1;
        while (compiler->tokens->toks[pos].kind != THEN
            && compiler->tokens->toks[pos].kind != CALLS
            && compiler->tokens->toks[pos].kind != FNCTN
            && compiler->tokens->toks[pos].kind != BEG) {
            pos--;
        }
        pos++;

        while (compiler->tokens->toks[pos].kind != THEN && compiler->tokens->toks[pos].kind != SEMICOLON && compiler->tokens->toks[pos].kind != CALLS) {
            if (compiler->tokens->toks[pos].kind == CATCH && compiler->tokens->toks[pos+1].kind == ERROR) return (Token){ .kind = NONE, .int_val = 0 };
            pos++;
        }

#ifdef PLEA_DEBUG
        fprintf(stderr, "Expected %s, got %s\n", token_to_string(token_kind), token_to_string(cur_token(compiler).kind));
#else
        fprintf(stderr, "%d\n", token_kind);
#endif
        exit(1);
    }
    return cur_token(compiler);
}

int find_var(Compiler *compiler, char *name) {
    if (compiler->cur_function->vars_count == 0) return -1;
    for (int i = 0; i < compiler->cur_function->vars_count; i++) {
        if (strcmp(name, compiler->cur_function->vars[i].name) == 0) return i;
    }
    return -1;
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

    Token lhs = consume_token(compiler);
    expect_token(compiler, IS);
    if (peek_token(compiler).kind == NOT) {
        consume_token(compiler);
        cond = 0;
    }
    else {
        cond = 1;
    }

    da_append(compiler->code, OP_RETS, bytes);

    Token rhs = consume_token(compiler);

    int mode = 0;
    switch (lhs.kind) {
    case IDENT:
        mode |= 1;
        int var_index = find_var(compiler, lhs.ident_name);
        if (var_index != -1) {
            add_bytes(compiler->code, 2, OP_PUSHI, var_index);
        }
        else {
            error(compiler, "Variable not found", __LINE__);
        }
        break;
    case REAL:
    case INTEGER: add_bytes(compiler->code, 2, OP_PUSHI, lhs.int_val); break;
    default: error(compiler, "MALFORMED TOKEN", __LINE__);
    }

    switch (rhs.kind) {
    case IDENT:
        mode |= 2;
        int var_index = find_var(compiler, rhs.ident_name);
        if (var_index != -1) {
            add_bytes(compiler->code, 2, OP_PUSHI, var_index);
        }
        else {
            error(compiler, "Variable not found", __LINE__);
        }
        break;
    case REAL:
    case INTEGER: add_bytes(compiler->code, 2, OP_PUSHI, rhs.int_val); break;
    default: error(compiler, "MALFORMED TOKEN", __LINE__);
    }
    add_bytes(compiler->code, 2, OP_PUSHI, mode);

    if (peek_token(compiler).kind == CATCH && compiler->tokens->toks[compiler->pos+2].kind == ERROR) {
        da_append(compiler->code, cond ? OP_WHEN : OP_WHEN_NOT, bytes);
        compiler->pos += 2;
    }
    else {
        da_append(compiler->code, cond ? OP_PROMISE : OP_PROMISE_NOT, bytes);
    }

    if (cur_byte_pos > 256) {
        add_bytes(compiler->code, 2, OP_JMPBSC, compiler->code->constant_list->count);
        add_constant(compiler->code->constant_list, cur_byte_pos);
    }
    else {
        add_bytes(compiler->code, 2, OP_JMPBSI, cur_byte_pos);
    }
}

int compile_function_declaration(Compiler *compiler) {
    expect_token(compiler, RETURNS);
    consume_token(compiler);
    int ret_val_pos = compiler->pos;

    while (peek_token(compiler).kind != NM) consume_token(compiler);

    expect_token(compiler, NM);
    char *name = compiler->tokens->toks[compiler->pos+1].ident_name;

    da_append(compiler->code, OP_FNCTN, bytes);
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
            error(compiler, "Unknown type", __LINE__);
        }
        if (type != VOID) {
            compiler->cur_function->vars[compiler->cur_function->vars_count] = (Var){ .name = parameter_name, .type = (type - 17) >> 1 };
            if (peek_token(compiler).kind == L_BRACKET) {
                consume_token(compiler);
                compiler->cur_function->vars[compiler->cur_function->vars_count].type += 2;
                expect_token(compiler, R_BRACKET);
            }
            add_bytes(compiler->code, 2, OP_POP, compiler->cur_function->vars_count++);
        }
        else {
            compiler->cur_function->vars[compiler->cur_function->vars_count] = (Var){ .name = parameter_name, .type = 4 };
            compiler->cur_function->vars_count++;
        }
    }
    consume_token(compiler);
    if (peek_token(compiler).kind == CATCH) compiler->pos += 3;
    return ret_val_pos;
}

int compile_expr(Compiler *compiler);

void compile_chg_expr(Compiler *compiler, int var_id) {
    Token cur_token = consume_token(compiler);

    if (cur_token.kind == STAR) {
        add_bytes(compiler->code, 2, OP_PUSH, var_id);
    }
    else if (cur_token.kind == IDENT) {
        int var_index = find_var(compiler, cur_token.ident_name);
        if (var_index != -1) {
            if (peek_token(compiler).kind == AT) {
                if (compiler->cur_function->vars[var_id].type != compiler->cur_function->vars[var_index].type-2) error(compiler, "Incompatible type", __LINE__);
                compiler->pos += 2;
                add_bytes(compiler->code, 2, OP_PUSHI, var_index);
                compile_expr(compiler);
                da_append(compiler->code, OP_PUSH_INDEX, bytes);
            }
            else if (compiler->cur_function->vars[var_index].type > 1 && peek_token(compiler).kind == L_BRACKET) {
                if (compiler->cur_function->vars[var_id].type != compiler->cur_function->vars[var_index].type) error(compiler, "Incompatible type", __LINE__);
                consume_token(compiler);
                expect_token(compiler, R_BRACKET);
                add_bytes(compiler->code, 2, OP_PUSH, var_index);
            }
            else if (compiler->cur_function->vars[var_index].type > 1) {
                if (compiler->cur_function->vars[var_id].type != compiler->cur_function->vars[var_index].type-2) error(compiler, "Incompatible type", __LINE__);
                add_bytes(compiler->code, 5, OP_PUSHI, var_index, OP_PUSHI, 0, OP_PUSH_INDEX);
            }
            else {
                if (compiler->cur_function->vars[var_id].type != compiler->cur_function->vars[var_index].type) error(compiler, "Incompatible type", __LINE__);
                add_bytes(compiler->code, 2, OP_PUSH, var_index);
            }
        }
        else {
            error(compiler, "Variable not found", __LINE__);
        }
    }
    else {
        if (compiler->cur_function->vars[var_id].type != compile_expr(compiler)) {
            error(compiler, "Incompatible type", __LINE__);
        }
        add_bytes(compiler->code, 2, OP_POP, var_id);
        return;
    }

    while (peek_token(compiler).kind == PLUS || peek_token(compiler).kind == MINUS || peek_token(compiler).kind == TIMES) {
        switch (peek_token(compiler).kind) {
        case PLUS:  da_append(compiler->code, OP_INC, bytes); break;
        case MINUS: da_append(compiler->code, OP_DEC, bytes); break;
        case TIMES:
            consume_token(compiler);

            uint8_t cur_instruction = compiler->code->bytes[compiler->code->count-1];
            if (cur_instruction != OP_INC && cur_instruction != OP_DEC) error(compiler, "MALFORMED TOKEN", __LINE__);

            if (peek_token(compiler).kind == INTEGER || peek_token(compiler).kind == REAL) {
                int val = peek_token(compiler).int_val;
                if (val > 256 || val < 0) {
                    add_bytes(compiler->code, 3, OP_CONST, compiler->code->constant_list->count, cur_instruction == OP_INC ? OP_ADD : OP_SUB);
                    add_constant(compiler->code->constant_list, val-1);
                }
                else {
                    add_bytes(compiler->code, 3, OP_PUSHI, val-1, cur_instruction == OP_INC ? OP_ADD : OP_SUB);
                }
            }
            else if (peek_token(compiler).kind == IDENT) {
                int var_index = find_var(compiler, peek_token(compiler).ident_name);
                if (var_index != -1) {
                    add_bytes(compiler->code, 4, OP_PUSH, var_index, cur_instruction == OP_INC ? OP_DEC : OP_INC, cur_instruction == OP_INC ? OP_ADD : OP_SUB);
                }
                else {
                    error(compiler, "Variable not found", __LINE__);
                }
            }
            else {
                error(compiler, "MALFORMED TOKEN", __LINE__);
            }
            break;
        default: error(compiler, "MALFORMED TOKEN", __LINE__);
        }
        consume_token(compiler);
    }
    add_bytes(compiler->code, 2, OP_POP, var_id);
}

void compile_let(Compiler *compiler) {
    int cur_byte_pos;
    int var_index = find_var(compiler, peek_token(compiler).ident_name);

    if (peek_token(compiler).kind == LNG) {
        if (check_for_when(compiler)) {
            add_bytes(compiler->code, 4, OP_PUSHI, compiler->code->line_positions->count-1, OP_INC, OP_JMP);
            cur_byte_pos = (int)compiler->code->count;
        }
        consume_token(compiler);
        expect_token(compiler, OF);
        var_index = find_var(compiler, consume_token(compiler).ident_name);
        expect_token(compiler, L_BRACKET);
        expect_token(compiler, R_BRACKET);
        expect_token(compiler, EQUALS);

        consume_token(compiler);
        compile_expr(compiler);
        add_bytes(compiler->code, 3, OP_PUSHI, var_index, OP_SET_LEN);
    }
    else if (compiler->tokens->toks[compiler->pos+2].kind == AT) {
        if (check_for_when(compiler)) {
            add_bytes(compiler->code, 4, OP_PUSHI, compiler->code->line_positions->count-1, OP_INC, OP_JMP);
            cur_byte_pos = (int)compiler->code->count;
        }

        consume_token(compiler);
        if (var_index != -1) {
            add_bytes(compiler->code, 2, OP_PUSHI, var_index);
        }
        else {
            error(compiler, "Variable not found", __LINE__);
        }
        compiler->pos += 2;
        compile_expr(compiler);

        expect_token(compiler, EQUALS);

        consume_token(compiler);
        if (compiler->cur_function->vars[var_index].type-2 != compile_expr(compiler)) error(compiler, "Incompatible type", __LINE__);

        da_append(compiler->code, OP_SET_INDEX, bytes);
    }
    else if (var_index != -1 && compiler->cur_function->vars[var_index].type > 1) {
        if (check_for_when(compiler)) {
            add_bytes(compiler->code, 4, OP_PUSHI, compiler->code->line_positions->count-1, OP_INC, OP_JMP);
            cur_byte_pos = (int)compiler->code->count;
        }
        consume_token(compiler);
        expect_token(compiler, EQUALS);
        add_bytes(compiler->code, 4, OP_PUSHI, var_index, OP_PUSHI, 0);

        consume_token(compiler);
        compile_expr(compiler);
        if (compiler->cur_function->vars[var_index].type-2 != compile_expr(compiler)) error(compiler, "Incompatible type", __LINE__);

        da_append(compiler->code, OP_SET_INDEX, bytes);
    }
    else {
        if (check_for_when(compiler)) {
            add_bytes(compiler->code, 3, OP_SET_VAR, compiler->cur_function->vars_count, 0);
            add_bytes(compiler->code, 4, OP_PUSHI, compiler->code->line_positions->count-1, OP_INC, OP_JMP);
            cur_byte_pos = (int)compiler->code->count;
        }
        compiler->cur_function->vars[compiler->cur_function->vars_count] = (Var){ .name = strdup(consume_token(compiler).ident_name), .type = 0 };
        expect_token(compiler, EQUALS);

        int var_id = compiler->cur_function->vars_count;
        int type = peek_token(compiler).kind;
        if (type == INTEGER || type == REAL) {
            int val = consume_token(compiler).int_val;

            if (val < 256 && val >= 0) {
                add_bytes(compiler->code, 3, OP_SET_VAR, compiler->cur_function->vars_count, val);
            }
            else {
                add_bytes(compiler->code, 4, OP_CONST, compiler->code->constant_list->count, OP_POP, compiler->cur_function->vars_count);
                add_constant(compiler->code->constant_list, val);
            }
            compiler->cur_function->vars[compiler->cur_function->vars_count].type = type - 22;
            compiler->cur_function->vars_count++;
        }
        else if (type == SH_INT || type == SH_FLOAT || type == SH_CHAR) {
            consume_token(compiler);
            if (peek_token(compiler).kind == L_BRACKET) {
                consume_token(compiler);
                expect_token(compiler, R_BRACKET);
                add_bytes(compiler->code, 2, OP_SET_ARRAY, compiler->cur_function->vars_count);
                compiler->cur_function->vars[compiler->cur_function->vars_count].type = ((type - 13) >> 1) + 2;
            }
            else {
                add_bytes(compiler->code, 3, OP_SET_VAR, compiler->cur_function->vars_count, 0);
                compiler->cur_function->vars[compiler->cur_function->vars_count].type = (type - 13) >> 1;
            }
            compiler->cur_function->vars_count++;
        }
        else {
            compiler->cur_function->vars_count++;
            consume_token(compiler);
            compiler->cur_function->vars[compiler->cur_function->vars_count-1].type = compile_expr(compiler);
            add_bytes(compiler->code, 2, OP_POP, var_id);
        }
    }

    if (peek_token(compiler).kind == WHEN) {
        da_append(compiler->code->line_positions, compiler->code->count+1, positions);
        compile_when_condition(compiler, cur_byte_pos);
    }
    if (peek_token(compiler).kind == CATCH) compiler->pos += 2;
}

int compile_chg(Compiler *compiler) {
    int cur_byte_pos;
    if (check_for_when(compiler)) {
        add_bytes(compiler->code, 4, OP_PUSHI, compiler->code->line_positions->count-1, OP_INC, OP_JMP);
        cur_byte_pos = (int)compiler->code->count;
    }

    int var_index = find_var(compiler, peek_token(compiler).ident_name);
    if (var_index != -1) {
        consume_token(compiler);
        expect_token(compiler, COMMA);
        if (peek_token(compiler).kind == INTEGER || peek_token(compiler).kind == REAL) {
            if (compiler->cur_function->vars[var_index].type != peek_token(compiler).kind - 22) {
                error(compiler, "Incompatible type", __LINE__);
            }

            int val = consume_token(compiler).int_val;
            if (val < 256 && val >= 0) {
                add_bytes(compiler->code, 3, OP_SET_VAR, var_index, val);
            }
            else {
                add_bytes(compiler->code, 4, OP_CONST, compiler->code->constant_list->count, OP_POP, var_index);
                add_constant(compiler->code->constant_list, val);
            }
        }
        else {
            compile_chg_expr(compiler, var_index);
        }
    }
    else {
        error(compiler, "Variable not found", __LINE__);
    }

    if (peek_token(compiler).kind == WHEN) {
        da_append(compiler->code->line_positions, compiler->code->count+1, positions);
        compile_when_condition(compiler, cur_byte_pos);
    }
    if (peek_token(compiler).kind == CATCH) compiler->pos += 2;
    return var_index;
}

int compile_function_call(Compiler *compiler) {
    int type = 0;
    char *function_name = strdup(consume_token(compiler).ident_name);
    expect_token(compiler, IN);

    int cur_byte_pos;
    if (check_for_when(compiler)) {
        add_bytes(compiler->code, 4, OP_PUSHI, compiler->code->line_positions->count-1, OP_INC, OP_JMP);
        cur_byte_pos = (int)compiler->code->count;
    }

    int function;
    for (function = 0; function < compiler->code->function_list->count; function++) {
        if (strcmp(function_name, compiler->code->function_list->functions[function].name) == 0) break;
        if (function == compiler->code->function_list->count - 1 && strcmp(function_name, "print") != 0) {
            error(compiler, "Function not found", __LINE__);
        }
    }

    int arguments = 0;
    for (; ;) {
        arguments++;
        if (peek_token(compiler).kind == IDENT && strcmp(peek_token(compiler).ident_name, "input") == 0) {
            if (strcmp(function_name, "print") != 0 && compiler->code->function_list->functions[function].vars[arguments-1].type != 2) {
                error(compiler, "Argument has wrong type", __LINE__);
            }
            da_append(compiler->code, OP_INPUT, bytes);
            consume_token(compiler);
        }
        else if (peek_token(compiler).kind != VOID) {
            consume_token(compiler);
            type = compile_expr(compiler);
            if (strcmp(function_name, "print") == 0) {
                if (type != 2 && type != 0) error(compiler, "Argument has wrong type", __LINE__);
            }
            else if (compiler->code->function_list->functions[function].vars[arguments-1].type != type) {
                error(compiler, "Argument has wrong type", __LINE__);
            }
        }
        else {
            if (strcmp(function_name, "print") == 0) {
                error(compiler, "Argument has wrong type", __LINE__);
            }
            else if (compiler->code->function_list->functions[function].vars[arguments-1].type != 4) {
                error(compiler, "Argument has wrong type", __LINE__);
            }
            consume_token(compiler);
        }
        if (peek_token(compiler).kind != COMMA) {
            da_append(compiler->code, OP_CALL, bytes);
            add_string(compiler->code, function_name);
            if (cur_token(compiler).kind == ENDIN) break;
            expect_token(compiler, ENDIN);
            break;
        }
        consume_token(compiler);
    }
    if (strcmp(function_name, "print") != 0) {
        type = compiler->code->function_list->functions[function].return_type;
    }

    if (strcmp(function_name, "print") == 0) {
        if (arguments != 1) error(compiler, "Function call has wrong amount of arguments", __LINE__);
    }
    else if (compiler->code->function_list->functions[function].arity != arguments) {
        error(compiler, "Function call has wrong amount of arguments", __LINE__);
    }

    if (peek_token(compiler).kind == WHEN) {
        da_append(compiler->code->line_positions, compiler->code->count+1, positions);
        compile_when_condition(compiler, cur_byte_pos);
    }
    if (peek_token(compiler).kind == CATCH) compiler->pos += 2;

    return type;
}

int compile_expr(Compiler *compiler) {
    int type = 0;
    switch (cur_token(compiler).kind) {
    case REAL:
        type = 1;
        if (cur_token(compiler).int_val < 256 && cur_token(compiler).int_val >= 0) {
            add_bytes(compiler->code, 2, OP_PUSHI, cur_token(compiler).int_val);
        }
        else {
            add_bytes(compiler->code, 2, OP_CONST, compiler->code->constant_list->count);
            add_constant(compiler->code->constant_list, cur_token(compiler).int_val);
        }
        break;
    case INTEGER:
        type = 0;
        if (cur_token(compiler).int_val < 256 && cur_token(compiler).int_val >= 0) {
            add_bytes(compiler->code, 2, OP_PUSHI, cur_token(compiler).int_val);
        }
        else {
            add_bytes(compiler->code, 2, OP_CONST, compiler->code->constant_list->count);
            add_constant(compiler->code->constant_list, cur_token(compiler).int_val);
        }
        break;
    case IDENT: {
        int var_index = find_var(compiler, cur_token(compiler).ident_name);
        if (var_index == -1) error(compiler, "Variable not found", __LINE__);

        if (peek_token(compiler).kind == AT) {
            type = compiler->cur_function->vars[var_index].type - 2;
            compiler->pos += 2;
            add_bytes(compiler->code, 2, OP_PUSHI, var_index);
            compile_expr(compiler);
            da_append(compiler->code, OP_PUSH_INDEX, bytes);
        }
        else if (compiler->cur_function->vars[var_index].type > 1 && peek_token(compiler).kind == L_BRACKET) {
            type = compiler->cur_function->vars[var_index].type;
            consume_token(compiler);
            expect_token(compiler, R_BRACKET);
            add_bytes(compiler->code, 2, OP_PUSH, var_index);
        }
        else if (compiler->cur_function->vars[var_index].type > 1) {
            type = compiler->cur_function->vars[var_index].type - 2;
            add_bytes(compiler->code, 5, OP_PUSHI, var_index, OP_PUSHI, 0, OP_PUSH_INDEX);
        }
        else {
            type = compiler->cur_function->vars[var_index].type;
            add_bytes(compiler->code, 2, OP_PUSH, var_index);
        }
        break;
    }
    case LET:
        compile_let(compiler);
        type = compiler->cur_function->vars[compiler->cur_function->vars_count-1].type;
        add_bytes(compiler->code, 2, OP_PUSH, compiler->cur_function->vars_count-1);
        break;
    case CHG: {
        int index = compile_chg(compiler);
        type = compiler->cur_function->vars[index].type;
        add_bytes(compiler->code, 2, OP_PUSH, index);
        break;
    }
    case CALL:
        type = compile_function_call(compiler);
        break;
    default: error(compiler, "Invalid expression", __LINE__);
    }
    return type;
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
        add_constant(compiler->code->constant_list, cur_line);
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
            error(compiler, "MALFORMED TOKEN", __LINE__);
        }
        consume_token(compiler);
    }

    if (peek_token(compiler).kind == WHEN) {
        add_bytes(compiler->code, 2, OP_POPR, OP_JMP);
        da_append(compiler->code->line_positions, compiler->code->count+1, positions);
        compile_when_condition(compiler, cur_byte_pos);
    }
    else {
        da_append(compiler->code, OP_JMP, bytes);
    }
    if (peek_token(compiler).kind == CATCH) compiler->pos += 2;
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
    code->constant_list->constants = malloc(4 * sizeof(Value));

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

    int ret_val_pos = 0;
    Token token = compiler.tokens->toks[compiler.pos];
    while (token.kind != T_EOF) {
        if (compiler.is_in_function) {
            switch (token.kind) {
            case SEMICOLON: {
                int cur_position = compiler.pos;

                compiler.pos = ret_val_pos;
                compiler.cur_function->return_type = compile_expr(&compiler);
                da_append(compiler.code, OP_RET, bytes);
                compiler.pos = cur_position;

                compiler.is_in_function = 0;
                break;
            }
            case FNCTN:
                ret_val_pos = compile_function_declaration(&compiler);
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
            default: error(&compiler, "MALFORMED TOKEN", __LINE__);
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
    for (int i = 0; i < compiler.code->function_list->count; i++) {
        free(compiler.code->function_list->functions[i].vars);
    }
    compiler.code->line_positions->count--;
    return compiler.code;
}
