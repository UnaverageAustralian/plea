#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

#define NUM_KEYWORDS 37

static const char *keywords[] = {
    "is", "not",
    "i", "c", "f", "void",
    "int", "char", "float",
    "identifier", "\"string\"", "integer number", "real number",
    "fnctn", "when", "returns", "return", "calls", "call",
    "let", "chg", "args", "nm", "beg", "in", "out", "endin", "endout",
    "then", "lng", "of", "jmp", "catch", "error", "defl", "", "\0",
};

void add_token(Token_List *token_list, Token_Kind token_kind) {
    Token token = {
        .kind = token_kind,
        .int_val = 0
    };
    if (token_list->count == token_list->capacity) {
        token_list->capacity *= 2;
        token_list->toks = realloc(token_list->toks, token_list->capacity * sizeof(token));
        assert(token_list->toks != NULL);
    }
    token_list->toks[token_list->count] = token;
    token_list->count++;
}

char consume(Lexer *lexer) {
    return *(lexer->src + lexer->pos++);
}

char peek(Lexer *lexer) {
    return *(lexer->src + lexer->pos);
}

void lex_number(Lexer *lexer) {
    add_token(lexer->tokens, NONE);
    Token_Kind *token_kind = &lexer->tokens->toks[lexer->tokens->count-1].kind;
    char number[48];

    char c = *(lexer->src + lexer->pos - 1);
    for (int i = 0; c != '\n'; i++) {
        if (i == 0 & c == '-') {
            number[0] = '-';
            c = consume(lexer);
            continue;
        }
        if (c == '.') *token_kind = REAL;

        if (i >= 47) {
            fprintf(stderr, "Number is too big");
            exit(1);
        }

        number[i] = c;
        number[i+1] = '\0';

        if (peek(lexer) == '\0' || (!isdigit(peek(lexer)) && peek(lexer) != '.')) break;
        c = consume(lexer);
    }

    if (*token_kind == REAL) {
        lexer->tokens->toks[lexer->tokens->count-1].real_val = strtof(number, NULL);
    }
    else {
        *token_kind = INTEGER;
        lexer->tokens->toks[lexer->tokens->count-1].int_val = (int)strtol(number, NULL, 10);
    }
}

void lex_ident_or_keyword(Lexer *lexer) {
    add_token(lexer->tokens, NONE);
    char *ident_name = lexer->tokens->toks[lexer->tokens->count-1].ident_name;
    Token_Kind *token_kind = &lexer->tokens->toks[lexer->tokens->count-1].kind;

    char c = *(lexer->src + lexer->pos - 1);
    for (int i = 0; c != '\n'; i++) {
        if (i > 255) {
            fprintf(stderr, "Identifier is too long");
            exit(1);
        }

        ident_name[i] = c;
        ident_name[i+1] = '\0';

        if (peek(lexer) == '\0' || !isalnum(peek(lexer))) break;
        c = consume(lexer);
    }

    for (int i = 0; i < NUM_KEYWORDS; i++) {
        if (strcmp(ident_name, keywords[i]) == 0) {
            *token_kind = i+11;
            break;
        }
    }

    if (*token_kind == NONE) {
        *token_kind = IDENT;
    }
}

void lex_string(Lexer *lexer) {
    add_token(lexer->tokens, STRING);
    char *ident_name = lexer->tokens->toks[lexer->tokens->count-1].ident_name;

    int i = 0;
    for (char c; (c = consume(lexer)) != '\"'; i++) {
        if (c == '\n') {
            fprintf(stderr, "Premature end of line");
            exit(1);
        }
        if (c == '\0') {
            fprintf(stderr, "Premature end of file");
            exit(1);
        }

        if (i >= 255) {
            fprintf(stderr, "I'm not reading all that");
            exit(1);
        }

        ident_name[i] = c;
    }
    ident_name[i+1] = '\0';
}

Token_List lex(char *src) {
    Token_List tokens = {
        .count = 0,
        .capacity = 4,
        .toks = malloc(4 * sizeof(Token))
    };

    Lexer lexer = {
        .pos = 0,
        .src = src,
        .tokens = &tokens
    };

    for (char c; (c = consume(&lexer)) != '\0';) {
        switch (c) {
        case '[': add_token(&tokens, L_BRACKET); break;
        case ']': add_token(&tokens, R_BRACKET); break;
        case ',': add_token(&tokens, COMMA);     break;
        case '+': add_token(&tokens, PLUS);      break;
        case ';': add_token(&tokens, SEMICOLON); break;
        case '*': add_token(&tokens, STAR);      break;
        case '@': add_token(&tokens, AT);        break;
        case '=': add_token(&tokens, EQUALS);    break;
        case '-':
            if (!isdigit(peek(&lexer))) {
                add_token(&tokens, MINUS);
            }
            else {
                lex_number(&lexer);
            }
            break;
        case '_':
            if (peek(&lexer) == '+' || peek(&lexer) == '-') {
                add_token(&tokens, UNDER);
            }
            break;
        case ' ':
        case '\r':
        case '\t':
        case '\n': break;
        case '.':
            if (peek(&lexer) == 'x') {
                add_token(&tokens, TIMES);
                consume(&lexer);
            }
            else {
                add_token(&tokens, DOT);
            }
            break;
        case '\"':
            lex_string(&lexer);
            break;
        default:
            if (isalpha(c)) {
                lex_ident_or_keyword(&lexer);
            }
            else if (isdigit(c)) {
                lex_number(&lexer);
            }
            else {
                fprintf(stderr, "Invalid token");
                exit(1);
            }
            break;
        }
    }

    add_token(&tokens, T_EOF);
    return tokens;
}

char *token_to_string(Token_Kind type) {
    switch (type) {
    case L_BRACKET: return "L_BRACKET";
    case R_BRACKET: return "R_BRACKET";
    case COMMA:     return "COMMA";
    case MINUS:     return "MINUS";
    case PLUS:      return "PLUS";
    case SEMICOLON: return "SEMICOLON";
    case STAR:      return "STAR";
    case UNDER:     return "UNDERSCORE";
    case TIMES:     return "TIMES";
    case AT:        return "AT";
    case EQUALS:    return "EQUALS";
    case IS:        return "IS";
    case NOT:       return "NOT";
    case INT:       return "INT";
    case CHAR:      return "CHAR";
    case FLOAT:     return "FLOAT";
    case VOID:      return "VOID";
    case IDENT:     return "IDENTIFIER";
    case STRING:    return "STRING";
    case INTEGER:   return "INTEGER";
    case REAL:      return "REAL";
    case FNCTN:     return "FNCTN";
    case WHEN:      return "WHEN";
    case RETURNS:   return "RETURNS";
    case RETURN:    return "RETURN";
    case CALLS:     return "CALLS";
    case CALL:      return "CALL";
    case LET:       return "LET";
    case CHG:       return "CHG";
    case ARGS:      return "ARGS";
    case NM:        return "NM";
    case BEG:       return "BEG";
    case IN:        return "IN";
    case OUT:       return "OUT";
    case ENDIN:     return "ENDIN";
    case ENDOUT:    return "ENDOUT";
    case THEN:      return "THEN";
    case LNG:       return "LNG";
    case OF:        return "OF";
    case JMP:       return "JMP";
    case CATCH:     return "CATCH";
    case ERROR:     return "ERROR";
    case DEFL:      return "DEFL";
    case DOT:       return "DOT";
    case SH_INT:    return "SH_INT";
    case SH_CHAR:   return "SH_CHAR";
    case SH_FLOAT:  return "SH_FLOAT";
    case NONE:      return "NONE";
    case T_EOF:     return "EOF";
    default:
        fprintf(stderr, "Could not convert token to string");
        exit(1);
    }
}
