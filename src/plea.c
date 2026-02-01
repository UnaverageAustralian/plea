#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

void display_token(Token token) {
    if (token.kind == IDENT || token.kind == STRING) {
        printf("%s, %s\n", token_to_string(token.kind), token.ident_name);
    }
    else if (token.kind == INTEGER) {
        printf("%s, %d\n", token_to_string(token.kind), token.int_val);
    }
    else if (token.kind == FLOAT) {
        printf("%s, %f\n", token_to_string(token.kind), token.real_val);
    }
    else {
        printf("%s\n", token_to_string(token.kind));
    }
}

void free_code(Code *code) {
    free(code->function_list->functions);
    free(code->function_list);
    free(code->constant_list->constants);
    free(code->constant_list);
    free(code->bytes);
    free(code);
}

void run(char *src) {
    Token_List tokens = lex(src);
    Code *code = compile(&tokens);

#ifdef PLEA_LEXER_DEBUG
    for (int i = 0; i < tokens.count; i++) {
        display_token(tokens.toks[i]);
    }
#endif

#ifdef PLEA_DEBUG
    printf("%s", disassemble(code));
#endif

#ifndef PLEA_DEBUG
    run_bytecode(code);
#endif

    free_code(code);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: plea <file>\n");
        exit(1);
    }

    FILE *f = fopen(argv[1], "rb");

    if (!f) {
        fprintf(stderr, "Could not find the file \"%s\"", argv[1]);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    rewind(f);

    char *buffer = malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Could not read the file \"%s\"", argv[1]);
        exit(1);
    }

    fread(buffer, 1, length, f);
    fclose(f);

    buffer[length] = '\0';
    run(buffer);

    free(buffer);

    return 0;
}
