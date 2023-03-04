#pragma once

#include "parser.h"

#define MAX_STACK_SZ 1024
#define MAX_VARS 64
#define MAX_VAR_NAME_LEN 64
#define MEM_SZ 1024 * 1024 * 4

typedef struct { Str vars[MAX_VARS]; int ret_val; } Environment;

typedef struct {
    void *mem;
    Environment *stack;
    size_t stack_size;
} IState;

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

IState* init_interpreter();
Value interpret(Ast *ast, IState *state);