#pragma once

#include "parser.h"

#define MAX_ENVS 1024
#define MAX_VARS 64
#define MAX_VAR_NAME_LEN 64
#define MEM_SZ 1024 * 1024 * 4
#define GLOBAL_ENV_INDEX 0

typedef enum {
	VK_NULL,
	VK_BOOLEAN,
	VK_INTEGER,
	VK_GCVALUE,
	VK_FUNCTION,
} ValueKind;

typedef struct {
	ValueKind kind;
	union {
		bool boolean;
		i32 integer;
		AstFunction *function;
	};
} Value;

typedef struct {
    Str name;
    Value *val;
} Variable;

typedef struct {
     Variable vars[MAX_VARS];
     size_t var_cnt;
     Value ret_val;
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
    //environments for functions
    //index 0 is the global environment
    Environment *envs;
    //ptr to the top of the stack
    int current_env;
    //functions
    AstFunction **functions;
} IState;


//initializes the state of the interpreter
IState* init_interpreter();

void add_to_env(Value val, Str name, IState *state);

Value *find_in_env(Str name, Environment *env);

void print_val(Value val);

//interprets the ast `ast` using the state `state
Value interpret(Ast *ast, IState *state);
