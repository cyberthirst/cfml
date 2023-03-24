//
// Created by Mirek Škrabal on 23.03.2023.
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "bc_interpreter.h"
#include "../heap/heap.h"

//we can have max 1024 * 16 ptrs to the heap
#define MAX_OPERANDS (1024 * 16)
#define MAX_FRAMES (1024 * 16)

typedef enum {
    DROP = 0x00,
    CONSTANT = 0x01,
    PRINT = 0x02,
    ARRAY = 0x03,
    OBJECT = 0x04,
    GET_FIELD = 0x05,
    SET_FIELD = 0x06,
    CALL_METHOD = 0x07,
    CALL_FUNCTION = 0x08,
    SET_LOCAL = 0x09,
    GET_LOCAL = 0x0A,
    SET_GLOBAL = 0x0B,
    GET_GLOBAL = 0x0C,
    BRANCH = 0x0D,
    JUMP = 0x0E,
    RETURN = 0x0F,
} Instruction;

typedef struct {
    void *ret_addr;
    //ptrs to the heap
    uint8_t **locals;
    size_t locals_sz;
} Frame;

//struct for representing the inner state of the interpreter
typedef struct {
    //instruction pointer
    uint8_t *ip;
    Frame *frames;
    size_t frames_sz;
    //operands stack
    uint8_t **operands;
    size_t op_sz;
} Bc_Interpreter;

//GLOBAL VARIABLES
void *const_pool = NULL;
uint8_t **const_pool_map = NULL;
Bc_Globals globals;
uint16_t entry_point = 0;
Bc_Interpreter interpreter;

void bc_init() {
    interpreter.ip = const_pool_map[entry_point];
    interpreter.frames = malloc(sizeof(Frame) * MAX_FRAMES);
    //we have 1 frame at the beginning for global frame
    interpreter.frames_sz = 1;
    interpreter.operands = malloc(sizeof(void *) * MAX_OPERANDS);
    interpreter.op_sz = 0;
}

void bc_free() {
    free(interpreter.frames);
    free(interpreter.operands);
}

uint8_t *get_local_var(uint16_t index) {
    assert(index < interpreter.frames->locals_sz);
    return interpreter.frames->locals[index];
}

void set_local_var(uint16_t index, uint8_t *value) {
    assert(index < interpreter.frames->locals_sz);
    interpreter.frames->locals[index] = value;
}

void exec_drop() {
    assert(interpreter.op_sz);
    free(interpreter.operands[interpreter.op_sz--]);
    --interpreter.op_sz;
}

void exec_constant() {
    uint16_t index;
    index = *(uint16_t *) interpreter.ip;
    switch (*const_pool_map[index]) {
        case VK_INTEGER: {
            Integer *integer = const_pool_map[index];
            integer->kind = VK_INTEGER;
            integer->val = ((Integer *) const_pool_map[index])->val;
            interpreter.operands[++interpreter.op_sz] = (uint8_t *) integer;
            break;
        }
        case VK_NULL: {
            Null *null = malloc(sizeof(Null));
            null->kind = VK_NULL;
            interpreter.operands[++interpreter.op_sz] = (uint8_t *) null;
            break;
        }
        case VK_STRING: {
            Bc_String *string = malloc(sizeof(Bc_String));
            string->kind = VK_STRING;
            string->length = ((Bc_String *) const_pool_map[index])->length;
            string->value = malloc(string->length);
            //memcpy(string->value, ((Bc_String *) const_pool_map[index])->value, string->length);
            interpreter.operands[++interpreter.op_sz] = (uint8_t *) string;
            break;
        }
        case VK_FUNCTION: {
        }
        default: {
            printf("Unknown constant type\n");
            exit(1);
        }
    }
}



void bc_interpret() {
    while (interpreter.frames_sz) {
        switch (*interpreter.ip++) {
            case DROP: {
                exec_drop();
                break;
            }
            case CONSTANT: {
                exec_constant();
                break;
            }
            case PRINT:
                // Implement PRINT logic here
                break;
            case ARRAY:
                // Implement ARRAY logic here
                break;
            case OBJECT:
                // Implement OBJECT logic here
                break;
            case GET_FIELD:
                // Implement GET_FIELD logic here
                break;
            case SET_FIELD:
                // Implement SET_FIELD logic here
                break;
            case CALL_METHOD:
                // Implement CALL_METHOD logic here
                break;
            case CALL_FUNCTION:
                // Implement CALL_FUNCTION logic here
                break;
            case SET_LOCAL:
                // Implement SET_LOCAL logic here
                break;
            case GET_LOCAL:
                // Implement GET_LOCAL logic here
                break;
            case SET_GLOBAL:
                // Implement SET_GLOBAL logic here
                break;
            case GET_GLOBAL:
                // Implement GET_GLOBAL logic here
                break;
            case BRANCH:
                // Implement BRANCH logic here
                break;
            case JUMP:
                // Implement JUMP logic here
                break;
            case RETURN:
                // Implement RETURN logic here
                break;
            default:
                printf("Unknown instruction: 0x%02X\n", *interpreter.ip);
                exit(1);
        }
    }
}


void deserialize(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Cannot open file %s\n", filename);
        exit(1);
    }

    // Read and check the header
    uint8_t header[4];
    fread(header, sizeof(uint8_t), 4, file);
    if (header[0] != 0x46 || header[1] != 0x4D || header[2] != 0x4C || header[3] != 0x0A) {
        printf("Error: Invalid header\n");
        fclose(file);
        exit(1);
    }

    // Read how many objs are in the const pool
    uint16_t const_pool_count;
    fread(&const_pool_count, sizeof(uint16_t), 1, file);

    // Allocate the const pool
    // The pool has constant size, we don't initially know how big the objs actually are
    // If the pool is full we have to exit(1)
    const_pool = malloc(CONST_POOL_SZ);

    const_pool_map  = malloc(sizeof(void*) * const_pool_count);

    //the first obj start at the beginning of the const_pool
    const_pool_map[0] = const_pool;

    // Read the constant pool objects and fill the const_pool & const_pool_map array
    uint8_t tag;
    for (uint16_t i = 0; i < const_pool_count; ++i) {
        fread(&tag, sizeof(uint8_t), 1, file);

        switch (tag) {
            case VK_INTEGER: {
                Integer *integer = (Integer  *)const_pool_map[i];
                integer->kind = tag;
                fread(&integer->val, sizeof(int32_t), 1, file);
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Integer);
                break;
            }
            case VK_BOOLEAN: {
                Boolean *boolean = (Boolean  *)const_pool_map[i];
                boolean->kind = tag;
                fread(&boolean->val, sizeof(uint8_t), 1, file);
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Boolean);
                break;
            }
            case VK_NULL: {
                Null *null = (Null  *)const_pool_map[i];
                null->kind = tag;
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Null);
                break;
            }
            case VK_STRING: {
                Bc_String *string = (Bc_String  *)const_pool_map[i];
                string->kind = tag;
                fread(&string->length, sizeof(uint32_t), 1, file);
                string->value = (char *)(string + sizeof(uint32_t));
                fread(string->value, sizeof(char), string->length, file);
                string->value[string->length] = '\0';
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_String) + string->length;
                break;
            }
            case VK_FUNCTION: {
                Bc_Function *function = (Bc_Function  *)const_pool_map[i];
                function->kind = tag;
                fread(&function->params, sizeof(uint8_t), 1, file);
                fread(&function->locals, sizeof(uint16_t), 1, file);
                fread(&function->length, sizeof(uint32_t), 1, file);
                function->bytecode = (uint8_t *)(function + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t));
                fread(function->bytecode, sizeof(uint8_t), function->length, file);
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_Function) + function->length;
                break;
            }
            case VK_CLASS: {
                Bc_Class *class = (Bc_Class *)const_pool_map[i];
                class->kind = tag;
                fread(&class->count, sizeof(uint16_t), 1, file);
                class->members = (uint16_t *)(class + sizeof(uint16_t));
                fread(class->members, sizeof(uint16_t), class->count, file);
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_Class) + sizeof(uint16_t) * class->count;
                break;
            }
            default: {
                printf("Error: Invalid kind\n");
                fclose(file);
                exit(1);
            }
        }
    }

    // Read the globals
    fread(&globals.count, sizeof(uint16_t), 1, file);
    globals.indices = malloc(sizeof(uint16_t) * globals.count);
    fread(globals.indices, sizeof(uint16_t), globals.count, file);

    // Read the entry point
    fread(&entry_point, sizeof(uint16_t), 1, file);
    assert(entry_point < const_pool_count);

    // Cleanup
    fclose(file);
}