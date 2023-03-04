#pragma once

#include "parser.h"

#define MAX_STACK_SZ 1024
#define MAX_VARS 64
#define MAX_VAR_NAME_LEN 64
#define MEM_SZ 1024 * 1024 * 4

typedef enum {
	VK_NULL,
	VK_BOOLEAN,
	VK_INTEGER,
	VK_GCVALUE,
	VK_FUNCTION,
} ValueKind;

typedef struct {
    Str name;
    ValueKind *values;
} Variable;


typedef struct {
     Variable vars[MAX_VARS];
     size_t var_cnt;
     int ret_val;
} Environment;

typedef struct {
    uint8_t *heap_start;
    uint8_t *heap_free;
    size_t heap_size;
} Heap;

typedef struct {
    //ptr to the heap where we store the vars
    Heap *heap;
    //size of the currently allocated memory
    size_t heap_size;
    //stack of environments for functions
    Environment *stack;
    //ptr to the top of the stack
    //if we have global scope, this is -1
    int current_env;
    //gloabl variables
    Environment *globals;
    //functions
    //AstFunction *functions;
    AstFunction **functions;
} IState;

typedef struct {
	ValueKind kind;
	union {
		bool boolean;
		i32 integer;
		AstFunction *function;
	};
} Value;

//initializes the state of the interpreter
IState* init_interpreter();

void add_to_env(Value val, Str name, IState *state);

//interprets the ast `ast` using the state `state
Value interpret(Ast *ast, IState *state);