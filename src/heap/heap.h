//
// Created by Mirek Škrabal on 23.03.2023.
//

#pragma once

#include <stdint.h>
#include "../types.h"

//#include "../ast/ast_interpreter.h"
//#include "../bc/bc_interpreter.h"

extern const long long int MEM_SZ;

typedef struct {
    struct Block *next;
    size_t sz;
    uint8_t *free;
} Block;

typedef struct {
    uint8_t *heap_start;
    uint8_t *heap_free;
    Block *free_list;
    size_t heap_size;
} Heap;

void print_heap(Heap *heap);

Heap *construct_heap(const long long int mem_sz);

void heap_free(Heap **heap);

void *heap_alloc(size_t sz, Heap *heap);


Array *array_alloc(int size, Heap *heap);


Value construct_array(int size, Heap *heap);


Object *object_alloc(int size, Heap *heap);


Value construct_object(int size, Value parent, Heap *heap);

Function *ast_function_alloc(Heap *heap);

Value construct_ast_function(AstFunction *ast_func, Heap *heap);

Integer *integer_alloc(Heap *heap);

Value construct_integer(i32 val, Heap *heap);

Boolean *boolean_alloc(Heap *heap);

Value construct_boolean(bool val, Heap *heap);

Null *null_alloc(Heap *heap);

//TODO don't construct null, just set the Value ptr to NULL
Value construct_null(Heap *heap);

Value construct_bc_string(Bc_String *str, Heap *heap);

Value construct_bc_function(Bc_Func *func, Heap *heap);

//BYTECODE HEAP ALLOCS

