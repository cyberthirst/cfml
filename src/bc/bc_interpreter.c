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
    uint8_t *ret_addr;
    //ptrs to the heap
    uint8_t **locals;
    size_t locals_sz;
} Frame;

//struct for representing the inner state of the itp
typedef struct {
    //instruction pointer
    uint8_t *ip;
    Frame *frames;
    size_t frames_sz;
    //operands stack
    //ptrs to the heap
    Value *operands;
    size_t op_sz;
} Bc_Interpreter;

//GLOBAL VARIABLES
void *const_pool = NULL;
uint8_t **const_pool_map = NULL;
//number of constants in the const pool
uint16_t const_pool_count = 0;
Bc_Globals globals;
uint16_t entry_point = 0;
Bc_Interpreter *itp;
Heap *heap;
Null *global_null;

void bc_init() {
    itp = malloc(sizeof(Bc_Interpreter));
    itp->ip = const_pool_map[entry_point];
    itp->frames = malloc(sizeof(Frame) * MAX_FRAMES);
    //we have 1 frame at the beginning for global frame
    itp->frames_sz = 0;
    itp->operands = malloc(sizeof(void *) * MAX_OPERANDS);
    itp->op_sz = 0;
    heap = malloc(sizeof(Heap));
    heap->heap_start = malloc(MEM_SZ);
    heap->heap_free = heap->heap_start;
    heap->heap_size = 0;
    global_null = construct_null(heap);
    //array that acts like a hash map - we just allocate as big array as there are constants
    globals.values = malloc(sizeof(void *) * const_pool_count);
}

void bc_free() {
    free(itp->frames);
    free(itp->operands);
    free(itp);
}

uint8_t *pop_operand() {
    assert(itp->op_sz > 0);
    return itp->operands[--itp->op_sz];
}

uint8_t *peek_operand() {
    assert(itp->op_sz > 0);
    return itp->operands[itp->op_sz - 1];
}

void push_operand(uint8_t *value) {
    assert(itp->op_sz < MAX_OPERANDS);
    itp->operands[itp->op_sz++] = value;
}

bool index_is_global(uint16_t index) {
    assert(*const_pool_map[index] == VK_STRING);
    for (int i = 0; i < globals.count; ++i) {
        if (globals.indexes[i] == index) {
            return true;
        }
    }
    return false;
}

void exec_drop() {
    //assert that there is something to be dropped
    assert(itp->op_sz > 0);
    //TODO should I free here?
    //free(itp->operands[itp->op_sz--]);
    pop_operand();
}

void init_function_call(uint8_t argc) {
    //we will atleast pop argc args + 1 for the function
    assert(itp->op_sz > argc);
    //itp->frames[itp->frames_sz].locals = malloc(fun->params + fun->locals);
    itp->frames[itp->frames_sz].locals = malloc(sizeof (uint8_t *)*(argc + 1));
    //set the args
    itp->frames[itp->frames_sz].locals[0] = global_null;
    for (int i = argc; i > 0; --i) {
        itp->frames[itp->frames_sz].locals[i] = pop_operand();
    }
    Bc_Func *fun = (Bc_Func *)pop_operand();
    assert(fun->kind == VK_FUNCTION);
    //TODO: we can read the function from the stack beforehand and prevent realloc
    //doing this because it's simple :)
    itp->frames[itp->frames_sz].locals = realloc(itp->frames[itp->frames_sz].locals,
                                                sizeof(uint8_t *)*(fun->params + fun->locals));
    assert(argc + 1 == fun->params);
    //set the rest of the locals to null
    for (int i = argc + 1; i < fun->params + fun->locals; ++i) {
        itp->frames[itp->frames_sz].locals[i] = global_null;
    }
    //set the return address
    itp->frames[itp->frames_sz].ret_addr = itp->ip;
    itp->frames_sz++;
    uint8_t *tmp_ip = itp->ip;
    itp->ip = fun->bytecode;
}

void exec_constant() {
    uint16_t index;
    index = *(uint16_t *) itp->ip;
    Bc_Interpreter *tmp_itp = itp;
    //print_heap(heap);
    switch (*const_pool_map[index]) {
        case VK_INTEGER: {
            Integer *integer = const_pool_map[index];
            assert(integer->kind == VK_INTEGER);
            integer = (Integer *) construct_integer(integer->val, heap);
            push_operand((uint8_t *)integer);
            Integer *tmp = (Integer *)itp->operands[itp->op_sz - 1];
            break;
        }
        case VK_NULL: {
            Null *null = const_pool_map[index];
            assert(null->kind == VK_NULL);
            null = global_null;
            push_operand((uint8_t *)null);
            break;
        }
        case VK_BOOLEAN: {
            Boolean *boolean = const_pool_map[index];
            assert(boolean->kind == VK_BOOLEAN);
            boolean = (Boolean *) construct_boolean(boolean->val, heap);
            push_operand((uint8_t *)boolean);
            break;
        }
        case VK_STRING: {
            Bc_String *string = const_pool_map[index];
            string = construct_bc_string(string, heap);
            push_operand((uint8_t *)string);
            break;
        }

        case VK_FUNCTION: {
            Bc_Func *function = const_pool_map[index];
            function = construct_bc_function(function, heap);
            push_operand((uint8_t *)function);
            break;
        }
        default: {
            printf("Unknown constant type\n");
            exit(1);
        }
    }
    //print_heap(heap);
    itp->ip += 2;
}

void exec_print() {
    Bc_Interpreter *tmp_itp = itp;
    //assert that there is index to the constant pool, where the format str is stored
    //assert(itp.op_sz);
    uint16_t index = *(uint16_t *) itp->ip;
    itp->ip += 2;
    uint8_t num_args = *(uint8_t *)itp->ip;
    itp->ip += 1;
    Bc_String *prnt = const_pool_map[index];
    assert(prnt->kind == VK_STRING);
    char *tmp = prnt->value;

    Value val[num_args];
    for (int i = num_args - 1; i >= 0; --i) {
        val[i] = (Value)pop_operand();
    }
    size_t tilda = 0;
    for (size_t i = 0; i < prnt->len; i++) {
        if (prnt->value[i] == '~') {
            //we are only concerned with valid programs so each ~ will have a corresponding value
            print_val(val[tilda]);
            tilda++;
        }
        else {
            char c = prnt->value[i];
            //check for escape characters
            if (c == '\\' && prnt->len > i + 1) {
                char next = prnt->value[i + 1];
                //increment i so we don't print the next character two times
                i++;
                if (next == 'n') {
                    printf("\n");
                }
                else if (next == 't') {
                    printf("\t");
                }
                else if (next == 'r') {
                    printf("\r");
                }
                else if (next == '~') {
                    printf("~");
                }
                else {
                    printf("%c", next);
                }
            }
            else {
                printf("%c", prnt->value[i]);
            }
        }
    }
    push_operand((uint8_t *)global_null);
}

void exec_return() {
    assert(itp->frames_sz > 0);
    itp->ip = itp->frames[--itp->frames_sz].ret_addr;
    free(itp->frames[itp->frames_sz].locals);
}

void exec_get_local() {
    uint16_t index = *(uint16_t *)itp->ip;
    itp->ip += 2;
    //printf("get local: \n");
    //print_val((Value)itp->frames[itp->frames_sz - 1].locals[index]);
    push_operand(itp->frames[itp->frames_sz - 1].locals[index]);
}

void exec_set_local() {
    uint16_t index = *(uint16_t *)itp->ip;
    itp->ip += 2;
    itp->frames[itp->frames_sz - 1].locals[index] = peek_operand();
}

void exec_set_global() {
    uint16_t index = *(uint16_t *)itp->ip;
    assert(index_is_global(index));
    itp->ip += 2;
    globals.values[index] = peek_operand();
}

void exec_get_global() {
    uint16_t index = *(uint16_t *)itp->ip;
    assert(index_is_global(index));
    itp->ip += 2;
    push_operand(globals.values[index]);
}

void exec_jump() {
    int16_t offset = *(int16_t *)itp->ip;
    itp->ip += 2;
    //TODO is it ok to add int16_t to uint8_t*?
    itp->ip += offset;
}

void exec_branch() {
    int16_t offset = *(int16_t *)itp->ip;
    itp->ip += 2;
    Boolean *boolean = (Boolean *)pop_operand();
    assert(boolean->kind == VK_BOOLEAN);
    if (boolean->val) {
        itp->ip += offset;
    }
}

void exec_call_function() {
    uint8_t argc = *(uint8_t *)itp->ip;
    itp->ip += 1;
    //Bc_Func *fun= (Bc_Func *)pop_operand();
    //assert(fun->kind == VK_FUNCTION);
    init_function_call(argc);
}

void bytecode_loop(){
    Bc_Interpreter *tmp_interpreter = itp;
    while (itp->frames_sz) {
        switch (*itp->ip++) {
            case DROP: {
                exec_drop();
                break;
            }
            case CONSTANT: {
                exec_constant();
                break;
            }
            case PRINT:
                exec_print();
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
                exec_call_function();
                break;
            case SET_LOCAL:
                exec_set_local();
                break;
            case GET_LOCAL:
                exec_get_local();
                break;
            case SET_GLOBAL:
                exec_set_global();
                break;
            case GET_GLOBAL:
                exec_get_global();
                break;
            case BRANCH:
                exec_branch();
                break;
            case JUMP:
                exec_jump();
                break;
            case RETURN:
                exec_return();
                break;
            default:
                printf("Unknown instruction: 0x%02X\n", *itp->ip);
                exit(1);
        }
    }
}


void bc_interpret() {
    bc_init();
    //TODO initialize the global function
    Bc_Func *entry_point = itp->ip;
    push_operand(construct_bc_function(entry_point, heap));
    init_function_call(0);
    bytecode_loop();
    bc_free();
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
    fread(&const_pool_count, sizeof(uint16_t), 1, file);

    // Allocate the const pool
    // The pool has constant size, we don't initially know how big the objs actually are
    // If the pool is full we have to exit(1)
    //TODO implement the checks on cp size
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
                //we use ValueKind and not uint8_t as tag, thus we don't have jsut sizeof(Integer)
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
                fread(&string->len, sizeof(uint32_t), 1, file);
                //string + sizefof(uint32_t) + 1 is the address where value should start
                //string->value = (char *)((uint8_t *)string + sizeof(uint32_t) + 1);
                fread(string->value, sizeof(char), string->len, file);
                string->value[string->len] = '\0';
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_String) + string->len;
                break;
            }
            case VK_FUNCTION: {
                Bc_Func *function = (Bc_Func  *)const_pool_map[i];
                function->kind = tag;
                fread(&function->params, sizeof(uint8_t), 1, file);
                fread(&function->locals, sizeof(uint16_t), 1, file);
                fread(&function->len, sizeof(uint32_t), 1, file);
                //function->bytecode = (uint8_t *)(function + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t));
                fread(function->bytecode, sizeof(uint8_t), function->len, file);
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_Func) + function->len;
                break;
            }
            case VK_CLASS: {
                Bc_Class *class = (Bc_Class *)const_pool_map[i];
                class->kind = tag;
                fread(&class->count, sizeof(uint16_t), 1, file);
                //class->members = (uint16_t *)(class + sizeof(uint16_t));
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
    globals.indexes = malloc(sizeof(uint16_t) * globals.count);
    fread(globals.indexes, sizeof(uint16_t), globals.count, file);

    // Read the entry point
    fread(&entry_point, sizeof(uint16_t), 1, file);
    assert(entry_point < const_pool_count);

    // Cleanup
    fclose(file);
}