#pragma once

#include "parser.h"

#define MAX_ENVS 256
#define MAX_VARS 256
#define MAX_SCOPES 256
#define MAX_VAR_NAME_LEN 64
#define MEM_SZ 1024 * 1024 * 1024 * 5
#define GLOBAL_ENV_INDEX 0

typedef enum {
	VK_NULL,
	VK_BOOLEAN,
	VK_INTEGER,
	VK_ARRAY,
	VK_FUNCTION,
    VK_OBJECT,
} ValueKind;

typedef struct Array Array;
typedef struct Object Object;
typedef void * Value;

typedef struct {
    ValueKind kind;
    bool val;
} Boolean;

typedef struct {
    ValueKind kind;
    i32 val;
} Integer;

typedef struct {
    ValueKind kind;
    AstFunction *val;
} Function;

typedef struct {
    ValueKind kind;
} Null;

struct Array {
    ValueKind kind;
    size_t size;
    Value val[];
};

typedef struct {
	Str name;
	Value val;
} Field;

struct Object {
    ValueKind kind;
    Value parent;
    size_t field_cnt;
    Field val[];
};

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
    //optmization, have just one null
    Value *null;
} IState;


//initializes the state of the interpreter
IState* init_interpreter();

void free_interpreter(IState *state);

void add_to_scope(Value val, Str name, IState *state);

Value *find_in_env(Str name, Environment *env, size_t scope_cnt);

void print_val(Value val);

//interprets the ast `ast` using the state `state
Value interpret(Ast *ast, IState *state);
