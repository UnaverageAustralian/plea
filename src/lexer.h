#pragma once

typedef enum {
    L_BRACKET, R_BRACKET, COMMA, MINUS, PLUS, SEMICOLON, STAR, UNDER, TIMES, AT, EQUALS,
    IS, NOT,
    SH_INT, SH_CHAR, SH_FLOAT, VOID,
    INT, CHAR, FLOAT,
    IDENT, STRING, INTEGER, REAL,
    FNCTN, WHEN, RETURNS, RETURN, CALLS, CALL, LET, CHG, ARGS, NM, BEG, IN, OUT, ENDIN, ENDOUT, THEN, LNG, OF, JMP, CATCH, ERROR, DEFL, DOT,
    NONE, T_EOF
} Token_Kind;

typedef struct {
    Token_Kind kind;
    union {
        char ident_name[256];
        int int_val;
        float real_val;
    };
} Token;

typedef struct {
    size_t count;
    size_t capacity;
    Token *toks;
} Token_List;

typedef struct {
    Token_List *tokens;
    char *src;
    int pos;
} Lexer;

Token_List lex(char *src);
char *token_to_string(Token_Kind type);
