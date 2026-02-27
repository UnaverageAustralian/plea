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

Value *allocate_scope(Value *vars, int scope) {
    vars = realloc(vars, (scope+1)*256*sizeof(Value));
    assert(vars != NULL);
    memset(vars + scope*256, 0, 256*sizeof(Value));
    return vars;
}

void when_queue_add(When_Queue *when_queue, uint8_t cond, int val1, int val2, int loc, uint8_t mode, uint8_t is_promise) {
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
        .mode = mode,
        .is_promise = is_promise
    };
    when_queue->count++;
}

void push(Value **stack_ptr, Value val, int type) {
#ifdef PLEA_DEBUG
    printf("(%d)", val.as.integer);
#endif
    **stack_ptr = val;
    (*stack_ptr)->type = type;
    (*stack_ptr)++;
}

void push_p(Value **stack_ptr, uintptr_t val) {
#ifdef PLEA_DEBUG
    printf("(%p)", (Array *)val);
#endif
    (*stack_ptr)->type = 2;
    (*stack_ptr)->as.pointer = val;
    (*stack_ptr)++;
}

void push_i(Value **stack_ptr, int val) {
#ifdef PLEA_DEBUG
    printf("(%d)", val);
#endif
    (*stack_ptr)->type = 0;
    (*stack_ptr)->as.integer = val;
    (*stack_ptr)++;
}

Value pop(Value **stack_ptr) {
    (*stack_ptr)--;
    Value val = **stack_ptr;
    (*stack_ptr)->as.integer = 0;
    (*stack_ptr)->type = 0;
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
    case OP_JMPBSC:
    case OP_JMPBSI:
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
    default: fprintf(stderr, "Unknown instruction: %d\n", code->bytes[*cur_byte]); exit(1);
    }
}

void disassemble_byte(uint8_t byte, int cur_byte);

void run_bytecode(Code *code) {
    Value return_stack[256];
    Value stack[1024];
    Value *vars = calloc(256, sizeof(Value));

    When_Queue when_queue = (When_Queue){
        .count = 0,
        .capacity = 4,
        .whens = malloc(4 * sizeof(When))
    };

    Value *stack_ptr = stack;
    Value *return_stack_ptr = return_stack;

    int vars_count = 256;
    int scope = -1;
    int cur_byte = 0;
    if (code->bytes[cur_byte] != OP_BEG) {
        fprintf(stderr, "Programmer has insufficiently begged\n");
        exit(1);
    }

    int cur_function = 0;
    while (code->bytes[cur_byte] != OP_HLT) {
        switch (code->bytes[cur_byte]) {
        case OP_CONST:
            push(&stack_ptr, code->constant_list->constants[consume_byte(code, &cur_byte)], 0);
            consume_byte(code, &cur_byte);
            break;
        case OP_INC:
            push_i(&stack_ptr, pop(&stack_ptr).as.integer+1);
            consume_byte(code, &cur_byte);
            break;
        case OP_DEC:
            push_i(&stack_ptr, pop(&stack_ptr).as.integer-1);
            consume_byte(code, &cur_byte);
            break;
        case OP_SET_VAR: {
            int index = consume_byte(code, &cur_byte) + scope*256;
            int val = consume_byte(code, &cur_byte);
            vars[index].as.integer = val;
            vars[index].type = 0;
            consume_byte(code, &cur_byte);
            break;
        }
        case OP_PUSH: {
            int index = consume_byte(code, &cur_byte) + scope*256;
            push(&stack_ptr, vars[index], vars[index].type);
            consume_byte(code, &cur_byte);
            break;
        }
        case OP_PUSHI:
            push_i(&stack_ptr, consume_byte(code, &cur_byte));
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

                    push_i(&return_stack_ptr, cur_byte);
                    cur_byte = code->function_list->functions[i].location;
                    cur_function = i;

                    scope++;
                    vars = allocate_scope(vars, scope);
                    vars_count = (scope+1)*256;
                    if (scope >= MAX_SCOPE) {
                        fprintf(stderr, "The scope is too deep\n");
                        exit(1);
                    }

                    if (strcmp(func_name, "main") == 0) {
                        if (code->bytes[cur_byte] != OP_CALL) exit(1);
                        consume_byte(code, &cur_byte);
                        if (strcmp((char *)&code->bytes[cur_byte], "main") != 0) exit(1);

                        while (code->bytes[cur_byte] != 0) {
                            consume_byte(code, &cur_byte);
                        }
                        consume_byte(code, &cur_byte);
                    }
                    break;
                }
                if (i == code->function_list->count - 1) {
                    if (strcmp(func_name, "print") == 0) {
                        Value v = pop(&stack_ptr);
                        if (v.type != 2) {
                            char c = (char)v.as.integer;
                            printf("%c", c);
                            push_i(&stack_ptr, c);
                        }
                        else {
                            Array *char_array = (Array *)v.as.pointer;
                            for (int j = 0; j < char_array->len; j++) {
                                printf("%c", char_array->items[j].integer);
                            }
                            push_p(&stack_ptr, (uintptr_t)char_array);
                        }

                        while (code->bytes[cur_byte] != 0) {
                            consume_byte(code, &cur_byte);
                        }
                        consume_byte(code, &cur_byte);
                    }
                }
            }
            break;
        case OP_RET:
            for (int i = 0; i < when_queue.count; i++) {
                if (when_queue.whens[i].cond == -1) continue;
                if (when_queue.whens[i].loc < code->function_list->functions[cur_function].location ||
                    (code->function_list->count-cur_function <= 0 && when_queue.whens[i].loc >= code->function_list->functions[cur_function+1].location))
                    continue;

                if (when_queue.whens[i].is_promise) {
                    fprintf(stderr, "You promised :(\n");
                    exit(1);
                }
            }

            cur_byte = pop(&return_stack_ptr).as.integer;
            scope--;
            break;
        case OP_RETS:
            cur_byte = pop(&return_stack_ptr).as.integer;
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
        case OP_INPUT: {
            vars[vars_count-1].type = 2;
            vars[vars_count-1].as.pointer = (uintptr_t)malloc(sizeof(Array));
            ((Array *)vars[vars_count-1].as.pointer)->len = 64;
            ((Array *)vars[vars_count-1].as.pointer)->items = malloc(64 * sizeof(Value32));
            assert(((Array *)vars[vars_count-1].as.pointer)->items != NULL);

            printf("\n");

            char buf[64];
            fgets(buf, sizeof(buf), stdin);

            int i = 0;
            while (buf[i] != '\n' && buf[i] != '\0') {
                ((Array *)vars[vars_count-1].as.pointer)->items[i].integer = (int)buf[i];
                i++;
            }
            ((Array *)vars[vars_count-1].as.pointer)->len = i;

            push(&stack_ptr, vars[vars_count-1], vars[vars_count-1].type);
            consume_byte(code, &cur_byte);
            break;
        }
        case OP_JMP:
            cur_byte = code->line_positions->positions[pop(&stack_ptr).as.integer];
            break;
        case OP_JMPB:
            cur_byte = pop(&stack_ptr).as.integer;
            break;
        case OP_WHEN:
            when_queue_add(&when_queue, 1, pop(&stack_ptr).as.integer, pop(&stack_ptr).as.integer, cur_byte+1, pop(&stack_ptr).as.integer, 0);
            consume_byte(code, &cur_byte);
            skip_instruction(code, &cur_byte);
            break;
        case OP_WHEN_NOT:
            when_queue_add(&when_queue, 0, pop(&stack_ptr).as.integer, pop(&stack_ptr).as.integer, cur_byte+1, pop(&stack_ptr).as.integer, 0);
            consume_byte(code, &cur_byte);
            skip_instruction(code, &cur_byte);
            break;
        case OP_PROMISE:
            when_queue_add(&when_queue, 1, pop(&stack_ptr).as.integer, pop(&stack_ptr).as.integer, cur_byte+1, pop(&stack_ptr).as.integer, 1);
            consume_byte(code, &cur_byte);
            skip_instruction(code, &cur_byte);
            break;
        case OP_PROMISE_NOT:
            when_queue_add(&when_queue, 0, pop(&stack_ptr).as.integer, pop(&stack_ptr).as.integer, cur_byte+1, pop(&stack_ptr).as.integer, 1);
            consume_byte(code, &cur_byte);
            skip_instruction(code, &cur_byte);
            break;
        case OP_POPR:
            pop(&return_stack_ptr);
            consume_byte(code, &cur_byte);
            break;
        case OP_JMPS:
            push_i(&return_stack_ptr, cur_byte+1);
            cur_byte = code->line_positions->positions[pop(&stack_ptr).as.integer];
            break;
        case OP_JMPBS:
            push_i(&return_stack_ptr, cur_byte+1);
            cur_byte = pop(&stack_ptr).as.integer;
            break;
        case OP_JMPBSI:
            push_i(&return_stack_ptr, cur_byte+2);
            cur_byte = consume_byte(code, &cur_byte);
            break;
        case OP_JMPBSC:
            push_i(&return_stack_ptr, cur_byte+2);
            cur_byte = code->constant_list->constants[consume_byte(code, &cur_byte)].as.integer;
            break;
        case OP_ADD:
            push_i(&stack_ptr, pop(&stack_ptr).as.integer+pop(&stack_ptr).as.integer);
            consume_byte(code, &cur_byte);
            break;
        case OP_SUB: {
            int num1 = pop(&stack_ptr).as.integer;
            int num2 = pop(&stack_ptr).as.integer;
            push_i(&stack_ptr, num2 - num1);
            consume_byte(code, &cur_byte);
            break;
        }
        case OP_SET_ARRAY: {
            int index = consume_byte(code, &cur_byte) + scope*256;
            vars[index].type = 2;
            vars[index].as.pointer = (uintptr_t)malloc(sizeof(Array));
            ((Array *)vars[index].as.pointer)->len = 16;
            ((Array *)vars[index].as.pointer)->items = malloc(16 * sizeof(Value32));
            assert(((Array *)vars[index].as.pointer)->items != NULL);
            consume_byte(code, &cur_byte);
            break;
        }
        case OP_SET_INDEX: {
            int val = pop(&stack_ptr).as.integer;
            int index = pop(&stack_ptr).as.integer;
            ((Array *)vars[pop(&stack_ptr).as.integer + scope*256].as.pointer)->items[index].integer = val;
            consume_byte(code, &cur_byte);
            break;
        }
        case OP_SET_LEN: {
            int array = pop(&stack_ptr).as.integer + scope*256;
            int len = pop(&stack_ptr).as.integer;
            ((Array *)vars[array].as.pointer)->len = len;
            ((Array *)vars[array].as.pointer)->items = realloc(((Array *)vars[array].as.pointer)->items, len * sizeof(Value32));
            assert(((Array *)vars[array].as.pointer)->items != NULL);
            consume_byte(code, &cur_byte);
            break;
        }
        case OP_PUSH_INDEX: {
            int index = pop(&stack_ptr).as.integer;
            push_i(&stack_ptr, ((Array *)vars[pop(&stack_ptr).as.integer + scope*256].as.pointer)->items[index].integer);
            consume_byte(code, &cur_byte);
            break;
        }
        default: break;
        }

        for (int i = 0; i < when_queue.count; i++) {
            if (when_queue.whens[i].cond == -1) continue;
            if (when_queue.whens[i].loc < code->function_list->functions[cur_function].location ||
                (code->function_list->count-cur_function <= 0 && when_queue.whens[i].loc >= code->function_list->functions[cur_function+1].location))
                continue;

            int val1 = (when_queue.whens[i].mode & 1) ? vars[when_queue.whens[i].val1].as.integer : when_queue.whens[i].val1;
            int val2 = (when_queue.whens[i].mode & 2) ? vars[when_queue.whens[i].val2].as.integer : when_queue.whens[i].val2;
            if ((val1 == val2) == when_queue.whens[i].cond) {
                cur_byte = when_queue.whens[i].loc;
                if (i == when_queue.count-1) {
                    when_queue.count--;
                }
                else {
                    when_queue.whens[i].cond = -1;
                }
                break;
            }
            if (when_queue.whens[i].loc > cur_byte) when_queue.whens[i].cond = -1;
        }

#ifdef PLEA_DEBUG
        disassemble_byte(code->bytes[cur_byte], cur_byte);
#endif
    }

    uintptr_t *freed = malloc(vars_count * sizeof(uintptr_t));
    int freed_count = 0;
    for (int i = 0; i < vars_count; i++) {
        if (vars[i].type == 2) {
            int already_freed = 0;
            for (int j = 0; j < freed_count; j++) {
                if (freed[j] == vars[i].as.pointer) {
                    already_freed = 1;
                    break;
                }
            }

            if (!already_freed) {
                free(((Array *)vars[i].as.pointer)->items);
                free((Array *)vars[i].as.pointer);
                freed[freed_count++] = vars[i].as.pointer;
            }
        }
    }
    free(freed);
    free(when_queue.whens);
    free(vars);
}

void disassemble_byte(uint8_t byte, int cur_byte) {
    switch (byte) {
    case OP_CONST: printf("\tCONST");             break;
    case OP_INC: printf("\tINC");                 break;
    case OP_DEC: printf("\tDEC");                 break;
    case OP_SET_VAR: printf("\tSET_VAR");         break;
    case OP_SET_ARRAY: printf("\tSET_ARRAY");     break;
    case OP_SET_INDEX: printf("\tSET_INDEX");     break;
    case OP_SET_LEN: printf("\tSET_LEN");         break;
    case OP_PUSH: printf("\tPUSH");               break;
    case OP_PUSH_INDEX: printf("\tPUSH_INDEX");   break;
    case OP_PUSHI: printf("\tPUSHI");             break;
    case OP_POP: printf("\tPOP");                 break;
    case OP_CALL: printf("\tCALL");               break;
    case OP_RET: printf("\tRET");                 break;
    case OP_BEG: printf("\tBEG");                 break;
    case OP_FNCTN: printf("\tFNCTN");             break;
    case OP_HLT: printf("\tHLT");                 break;
    case OP_INPUT: printf("\tINPUT");             break;
    case OP_JMP: printf("\tJMP");                 break;
    case OP_JMPB: printf("\tJMPB");               break;
    case OP_WHEN: printf("\tWHEN");               break;
    case OP_WHEN_NOT: printf("\tWHEN_NOT");       break;
    case OP_PROMISE: printf("\tPROMISE");         break;
    case OP_PROMISE_NOT: printf("\tPROMISE_NOT"); break;
    case OP_POPR: printf("\tPOPR");               break;
    case OP_JMPS: printf("\tJMPS");               break;
    case OP_JMPBS: printf("\tJMPBS");             break;
    case OP_ADD: printf("\tADD");                 break;
    case OP_SUB: printf("\tSUB");                 break;
    case OP_RETS: printf("\tRETS");               break;
    case OP_JMPBSI: printf("\tJMPBSI");           break;
    case OP_JMPBSC: printf("\tJMPBSC");           break;
    default: fprintf(stderr, "Unknown instruction: %d\n", byte); exit(1);
    }
    printf(" (%d)\n", cur_byte);
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
            sb_appendf(&disasm, "\tCONST %d (%d)\n", consume_byte(code, &i), code->constant_list->constants[code->bytes[i]].as.integer);
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
        case OP_SET_VAR: {
            int index = consume_byte(code, &i);
            int val = consume_byte(code, &i);
            sb_appendf(&disasm, "\tSET_VAR %d %d\n", index, val);
            consume_byte(code, &i);
            break;
        }
        case OP_SET_ARRAY: {
            int index = consume_byte(code, &i);
            sb_appendf(&disasm, "\tSET_ARRAY %d\n", index);
            consume_byte(code, &i);
            break;
        }
        case OP_SET_INDEX:
            sb_appendf(&disasm, "\tSET_INDEX\n");
            consume_byte(code, &i);
            break;
        case OP_SET_LEN:
            sb_appendf(&disasm, "\tSET_LEN\n");
            consume_byte(code, &i);
            break;
        case OP_PUSH:
            sb_appendf(&disasm, "\tPUSH %d\n", consume_byte(code, &i));
            consume_byte(code, &i);
            break;
        case OP_PUSH_INDEX:
            sb_appendf(&disasm, "\tPUSH_INDEX\n");
            consume_byte(code, &i);
            break;
        case OP_PUSHI:
            sb_appendf(&disasm, "\tPUSHI %d\n", consume_byte(code, &i));
            consume_byte(code, &i);
            break;
        case OP_JMPBSI:
            sb_appendf(&disasm, "\tJMPBSI %d\n", consume_byte(code, &i));
            consume_byte(code, &i);
            break;
        case OP_JMPBSC:
            sb_appendf(&disasm, "\tJMPBSC %d\n", consume_byte(code, &i));
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
        case OP_PROMISE:
            sb_appendf(&disasm, "\tPROMISE\n");
            consume_byte(code, &i);
            break;
        case OP_PROMISE_NOT:
            sb_appendf(&disasm, "\tPROMISE_NOT\n");
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
        case OP_ADD:
            sb_appendf(&disasm, "\tADD\n");
            consume_byte(code, &i);
            break;
        case OP_SUB:
            sb_appendf(&disasm, "\tSUB\n");
            consume_byte(code, &i);
            break;
        case OP_RETS:
            sb_appendf(&disasm, "\tRETS\n");
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
