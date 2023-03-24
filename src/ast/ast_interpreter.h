#pragma once

#include "../parser.h"
#include "../heap/heap.h"
#include "../types.h"

#define MAX_ENVS 256
#define MAX_VARS 256
#define MAX_SCOPES 256
#define MAX_VAR_NAME_LEN 64

#define GLOBAL_ENV_INDEX 0

typedef struct {
    Str name;
    Value val;
} Variable;

typedef struct {
    //stack of variables
    Variable vars[MAX_VARS];
    size_t var_cnt;
} Scope;

//represents environment of a function
//also used for global environment
typedef struct {
    //stack of scopes
     Scope scopes[MAX_SCOPES];
     size_t scope_cnt;
     Value ret_val;
} Environment;

typedef struct {
    //ptr to the heap where we store the vars
    Heap *heap;
    //size of the currently allocated memory
    long long int heap_size;
    //environments for functions
    //index 0 is the global environment
    Environment *envs;
    //ptr to the top of the stack
    int current_env;
    //optmization, have just one null
    Value *null;
} IState;


//initializes the state of the itp
IState* init_interpreter();

void free_interpreter(IState *state);

void add_to_scope(Value val, Str name, IState *state);

Value *find_in_env(Str name, Environment *env, size_t scope_cnt);

void print_val(Value val);

//interprets the ast `ast` using the state `state
Value interpret(Ast *ast, IState *state);
