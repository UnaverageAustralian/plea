#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "vm.h"

#define MAX_SCOPE 16

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

int *allocate_scope(int *vars, int scope) {
    vars = realloc(vars, (scope+1)*256*sizeof(int));
    assert(vars != NULL);
    return vars;
}

void when_queue_add(When_Queue *when_queue, uint8_t cond, int val1, int val2, int loc, uint8_t mode) {
    if (when_queue->count == when_queue->capacity) {
        when_queue->capacity *= 2;
        when_queue->whens = realloc(when_queue->whens, when_queue->capacity * sizeof(When));
        assert(when_queue->whens != NULL);
    }
    when_queue->whens[when_queue->count] = (When){
        .cond = cond,
        .val1 = val1,
        .val2 = val2,
        .loc = loc,
        .mode = mode
    };
    when_queue->count++;
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

void skip_instruction(Code *code, int *cur_byte) {
    switch (code->bytes[*cur_byte]) {
    case OP_REASSIGN:
    case OP_SET_VAR: *cur_byte += 3; break;
    case OP_PUSH:
    case OP_PUSHI:
    case OP_CONST: *cur_byte += 2;   break;
    case OP_INC:
    case OP_POP:
    case OP_POPR:
    case OP_JMPBS:
    case OP_JMPS:
    case OP_HLT:
    case OP_INPUT:
    case OP_JMP:
    case OP_JMPB:
    case OP_WHEN:
    case OP_WHEN_NOT:
    case OP_RET:
    case OP_DEC: (*cur_byte)++;      break;
    case OP_FNCTN:
    case OP_BEG:
    case OP_CALL:
        while (code->bytes[*cur_byte] != '\0') (*cur_byte)++;
        break;
    default:                         break;
    }
}

void run_bytecode(Code *code) {
    int return_stack[MAX_SCOPE];
    int stack[1024];
    int *vars = malloc(256 * sizeof(int));

    When_Queue when_queue = (When_Queue){
        .count = 0,
        .capacity = 4,
        .whens = malloc(4 * sizeof(When))
    };

    int *stack_ptr = stack;
    int *return_stack_ptr = return_stack;

    int scope = -1;
    int cur_byte = 0;
    if (code->bytes[cur_byte] != OP_BEG) {
        fprintf(stderr, "Programmer has insufficiently begged\n");
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
            int index = consume_byte(code, &cur_byte) + scope*256;
            int val = consume_byte(code, &cur_byte);
            vars[index] = val;
            consume_byte(code, &cur_byte);
            break;
        case OP_PUSH:
            push(&stack_ptr, vars[consume_byte(code, &cur_byte) + scope*256]);
            consume_byte(code, &cur_byte);
            break;
        case OP_PUSHI:
            push(&stack_ptr, consume_byte(code, &cur_byte));
            consume_byte(code, &cur_byte);
            break;
        case OP_POP:
            vars[consume_byte(code, &cur_byte) + scope*256] = pop(&stack_ptr);
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

                    push(&return_stack_ptr, cur_byte);
                    cur_byte = code->function_list->functions[i].location;

                    scope++;
                    vars = allocate_scope(vars, scope);
                    if (scope >= MAX_SCOPE) {
                        fprintf(stderr, "The scope is too deep\n");
                        exit(1);
                    }
                    break;
                }
                if (i == code->function_list->count - 1) {
                    if (strcmp(func_name, "print") == 0) {
                        char c = pop(&stack_ptr);
                        printf("%c", c);
                        while (code->bytes[cur_byte] != 0) {
                            consume_byte(code, &cur_byte);
                        }
                        push(&stack_ptr, c);
                        consume_byte(code, &cur_byte);
                    }
                }
            }
            break;
        case OP_RET:
            pop(&stack_ptr);
            cur_byte = pop(&return_stack_ptr);
            scope--;
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
        case OP_INPUT:
            char c;
            printf("\n");
            scanf(" %c", &c);
            push(&stack_ptr, c);
            consume_byte(code, &cur_byte);
            break;
        case OP_JMP:
            cur_byte = code->line_positions->positions[pop(&stack_ptr)];
            break;
        case OP_JMPB:
            cur_byte = pop(&stack_ptr);
            break;
        case OP_WHEN:
            when_queue_add(&when_queue, 1, pop(&stack_ptr), pop(&stack_ptr), cur_byte+1, pop(&stack_ptr));
            consume_byte(code, &cur_byte);
            skip_instruction(code, &cur_byte);
            break;
        case OP_WHEN_NOT:
            when_queue_add(&when_queue, 0, pop(&stack_ptr), pop(&stack_ptr), cur_byte+1, pop(&stack_ptr));
            consume_byte(code, &cur_byte);
            skip_instruction(code, &cur_byte);
            break;
        case OP_POPR:
            pop(&return_stack_ptr);
            consume_byte(code, &cur_byte);
            break;
        case OP_JMPS:
            push(&return_stack_ptr, cur_byte+1);
            cur_byte = code->line_positions->positions[pop(&stack_ptr)];
            break;
        case OP_JMPBS:
            push(&return_stack_ptr, cur_byte+1);
            cur_byte = pop(&stack_ptr);
            break;
        default: break;
        }

        for (int i = 0; i < when_queue.count; i++) {
            if (when_queue.whens[i].cond == -1) continue;

            int val1 = (when_queue.whens[i].mode & 1) ? vars[when_queue.whens[i].val1] : when_queue.whens[i].val1;
            int val2 = (when_queue.whens[i].mode & 2) ? vars[when_queue.whens[i].val2] : when_queue.whens[i].val2;
            if ((val1 == val2) == when_queue.whens[i].cond) {
                cur_byte = when_queue.whens[i].loc;
                if (i == when_queue.count) {
                    when_queue.count--;
                }
                else {
                    when_queue.whens[i].cond = -1;
                }
                break;
            }
        }
    }
    free(vars);
}

char *disassemble(Code *code) {
    String_Builder disasm = (String_Builder){
        .count = 0,
        .capacity = 4,
        .string = malloc(4 * sizeof(char))
    };

    int i = 0;
    while (i < code->count) {
        sb_appendf(&disasm, "(%d)\n", i);
        switch (code->bytes[i]) {
        case OP_CONST:
            sb_appendf(&disasm, "\tCONST %d (%d)\n", consume_byte(code, &i), code->constant_list->constants[code->bytes[i]]);
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
        case OP_INPUT:
            sb_append(&disasm, "\tINPUT\n");
            consume_byte(code, &i);
            break;
        case OP_JMP:
            sb_appendf(&disasm, "\tJMP\n");
            consume_byte(code, &i);
            break;
        case OP_JMPB:
            sb_appendf(&disasm, "\tJMPB\n");
            consume_byte(code, &i);
            break;
        case OP_WHEN:
            sb_appendf(&disasm, "\tWHEN\n");
            consume_byte(code, &i);
            break;
        case OP_WHEN_NOT:
            sb_appendf(&disasm, "\tWHEN_NOT\n");
            consume_byte(code, &i);
            break;
        case OP_POPR:
            sb_appendf(&disasm, "\tPOPR\n");
            consume_byte(code, &i);
            break;
        case OP_JMPS:
            sb_appendf(&disasm, "\tJMPS\n");
            consume_byte(code, &i);
            break;
        case OP_JMPBS:
            sb_appendf(&disasm, "\tJMPBS\n");
            consume_byte(code, &i);
            break;
        default: fprintf(stderr, "Unknown instruction: %d\n", code->bytes[i]); exit(1);
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
        fprintf(stderr, "Programmer has insufficiently begged\n");
        exit(1);
    }
    if (num_chars/num_spaces > 10) {
        fprintf(stderr, "Programmer has insufficiently begged\n");
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
        fprintf(stderr, "Programmer has insufficiently begged\n");
        exit(1);
    }
}
