#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "vm.h"

void sb_append(String_Builder *sb, char *str) {
    sb->count += strlen(str);
    while (sb->count > sb->capacity) {
        sb->capacity *= 2;
        sb->string = realloc(sb->string, sb->capacity * sizeof(char));
        assert(sb->string != NULL);
    }
    strcat(sb->string, str);
}

void sb_appendf(String_Builder *sb, char *format, ...) {
    va_list args;
    va_start(args, format);

    int needed_size = vsnprintf(NULL, 0, format, args);
    va_end(args);

    while (sb->count + needed_size + 1 > sb->capacity) {
        sb->capacity *= 2;
        sb->string = realloc(sb->string, sb->capacity * sizeof(char));
        assert(sb->string != NULL);
    }
    va_start(args, format);
    vsnprintf(sb->string + sb->count, needed_size + 1, format, args);
    sb->count += needed_size;
    va_end(args);
}

void push(int **stack_ptr, int val) {
    **stack_ptr = val;
    (*stack_ptr)++;
}

int pop(int **stack_ptr) {
    (*stack_ptr)--;
    int val = **stack_ptr;
    **stack_ptr = 0;
    return val;
}

uint8_t consume_byte(Code *code, int *cur_byte) {
    (*cur_byte)++;
    return code->bytes[*cur_byte];
}

void check_beg_text(char *beg_text);

void run_bytecode(Code *code) {
    int stack[1024];
    Var vars[256];

    int *stack_ptr = stack;

    int cur_byte = 0;
    if (code->bytes[cur_byte] != OP_BEG) {
        fprintf(stderr, "Programmer has insufficiently begged");
        exit(1);
    }
    while (code->bytes[cur_byte] != OP_HLT) {
        switch (code->bytes[cur_byte]) {
        case OP_CONST:
            push(&stack_ptr, code->constant_list->constants[consume_byte(code, &cur_byte)]);
            consume_byte(code, &cur_byte);
            break;
        case OP_INC:
            push(&stack_ptr, pop(&stack_ptr)+1);
            consume_byte(code, &cur_byte);
            break;
        case OP_DEC:
            push(&stack_ptr, pop(&stack_ptr)-1);
            consume_byte(code, &cur_byte);
            break;
        case OP_SET_VAR:
        case OP_CHG_VAR:
            int index = consume_byte(code, &cur_byte);
            int val = consume_byte(code, &cur_byte);
            vars[index].as.integer = val;
            consume_byte(code, &cur_byte);
            break;
        case OP_PUSH:
            push(&stack_ptr, vars[consume_byte(code, &cur_byte)].as.integer);
            consume_byte(code, &cur_byte);
            break;
        case OP_PUSHI:
            push(&stack_ptr, consume_byte(code, &cur_byte));
            consume_byte(code, &cur_byte);
            break;
        case OP_POP:
            vars[consume_byte(code, &cur_byte)].as.integer = pop(&stack_ptr);
            consume_byte(code, &cur_byte);
            break;
        case OP_CALL:
            consume_byte(code, &cur_byte);
            char *func_name = (char *)&code->bytes[cur_byte];
            for (int i = 0; i < code->function_list->count; i++) {
                if (strcmp(func_name, code->function_list->functions[i].name) == 0) {
                    while (code->bytes[cur_byte] != 0) {
                        consume_byte(code, &cur_byte);
                    }
                    consume_byte(code, &cur_byte);

                    push(&stack_ptr, cur_byte);
                    cur_byte = code->function_list->functions[i].location;
                    break;
                }
                if (i == code->function_list->count - 1) {
                    if (strcmp(func_name, "print") == 0) {
                        printf("%c", pop(&stack_ptr));
                        while (code->bytes[cur_byte] != 0) {
                            consume_byte(code, &cur_byte);
                        }
                        consume_byte(code, &cur_byte);
                    }
                }
            }
            break;
        case OP_RET:
            pop(&stack_ptr);
            cur_byte = pop(&stack_ptr);
            break;
        case OP_BEG:
            check_beg_text((char *)&code->bytes[cur_byte+1]);
            while (code->bytes[cur_byte] != 0) {
                consume_byte(code, &cur_byte);
            }
            consume_byte(code, &cur_byte);
            break;
        case OP_FNCTN:
            while (code->bytes[cur_byte] != 0) {
                consume_byte(code, &cur_byte);
            }
            consume_byte(code, &cur_byte);
            break;
        default: break;
        }
    }
}

char *disassemble(Code *code) {
    String_Builder disasm = (String_Builder){
        .count = 0,
        .capacity = 4,
        .string = malloc(4 * sizeof(char))
    };

    int i = 0;
    while (i < code->count) {
        switch (code->bytes[i]) {
        case OP_CONST:
            sb_appendf(&disasm, "\tCONST %d\n", consume_byte(code, &i));
            consume_byte(code, &i);
            break;
        case OP_INC:
            sb_append(&disasm, "\tINC\n");
            consume_byte(code, &i);
            break;
        case OP_DEC:
            sb_append(&disasm, "\tDEC\n");
            consume_byte(code, &i);
            break;
        case OP_SET_VAR:
            int index = consume_byte(code, &i);
            int val = consume_byte(code, &i);
            sb_appendf(&disasm, "\tSET_VAR %d %d\n", index, val);
            consume_byte(code, &i);
            break;
        case OP_CHG_VAR:
            index = consume_byte(code, &i);
            val = consume_byte(code, &i);
            sb_appendf(&disasm, "\tCHG_VAR %d %d\n", index, val);
            consume_byte(code, &i);
            break;
        case OP_PUSH:
            sb_appendf(&disasm, "\tPUSH %d\n", consume_byte(code, &i));
            consume_byte(code, &i);
            break;
        case OP_PUSHI:
            sb_appendf(&disasm, "\tPUSHI %d\n", consume_byte(code, &i));
            consume_byte(code, &i);
            break;
        case OP_POP:
            sb_appendf(&disasm, "\tPOP %d\n", consume_byte(code, &i));
            consume_byte(code, &i);
            break;
        case OP_CALL:
            consume_byte(code, &i);
            sb_appendf(&disasm, "\tCALL %s\n", (char *)&code->bytes[i]);
            while (code->bytes[i] != 0) {
                consume_byte(code, &i);
            }
            consume_byte(code, &i);
            break;
        case OP_RET:
            sb_append(&disasm, "\tRET\n");
            consume_byte(code, &i);
            break;
        case OP_BEG:
            consume_byte(code, &i);
            sb_appendf(&disasm, "\tBEG \"%s\"\n", (char *)&code->bytes[i]);
            while (code->bytes[i] != 0) {
                consume_byte(code, &i);
            }
            consume_byte(code, &i);
            break;
        case OP_FNCTN:
            consume_byte(code, &i);
            sb_appendf(&disasm, "\nFNCTN %s\n", (char *)&code->bytes[i]);
            while (code->bytes[i] != 0) {
                consume_byte(code, &i);
            }
            consume_byte(code, &i);
            break;
        case OP_HLT:
            sb_append(&disasm, "\tHLT\n");
            consume_byte(code, &i);
            break;
        default: fprintf(stderr, "Unknown instruction: %d", code->bytes[i]); exit(1);
        }
    }
    return disasm.string;
}

void check_beg_text(char *beg_text) {
    srand(time(NULL));
    int probability = 0;

    int num_chars = (int)strlen(beg_text);
    int num_spaces = 0;
    int num_excl = 0;
    for (int i = 0; i < num_chars; i++) {
        if (beg_text[i] == ' ') num_spaces++;
        else if (beg_text[i] == '!') num_excl++;
    }
    if (num_spaces == 0) {
        fprintf(stderr, "Programmer has insufficiently begged");
        exit(1);
    }
    if (num_chars/num_spaces > 10) {
        fprintf(stderr, "Programmer has insufficiently begged");
        exit(1);
    }

    for (int i = 0; i < num_chars; i++) {
        beg_text[i] = (char)tolower(beg_text[i]);
    }

    if (strstr(beg_text, "please") != NULL) {
        probability += 25;
    }
    if (strstr(beg_text, "family") != NULL) {
        probability += 15;
    }
    if (strstr(beg_text, "great") != NULL) {
        probability += 20;
    }
    if (strstr(beg_text, "almighty program") != NULL) {
        probability += 20;
    }
    probability += num_excl*2;

    time_t t = time(NULL);
    struct tm *local_time = localtime(&t);
    if (local_time->tm_hour < 9) {
        probability -= 20;
    }

    if (rand()%100 >= probability) {
        fprintf(stderr, "Programmer has insufficiently begged");
        exit(1);
    }
}
