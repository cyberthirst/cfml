//
// Created by Mirek Škrabal on 23.03.2023.
//

#pragma once

#include <stdint.h>
#include "../types.h"

//we don't want to add a new flag to the types, so we use the highest bit of the kind field
#define MARK_FLAG 0x80 // 10000000 in binary
#define MARK(val) (*(val) |= MARK_FLAG)
#define UNMARK(val) (*(val) &= ~MARK_FLAG)
#define IS_MARKED(val) (*(val) & MARK_FLAG)
#define GET_KIND(val) (*(val) & ~MARK_FLAG)

extern const long long int MEM_SZ;

typedef struct {
    struct Block *next;
    //the amount of free bytes in this block
    size_t sz;
    uint8_t *start;
} Block;

typedef struct {
    //the start of the heap
    uint8_t *heap_start;
    //the first free byte in the heap
    uint8_t *heap_free;
    //the list of free blocks
    Block *free_list;
    //the total max size of the heap
    size_t total_size;
    //the amount of allocated bytes
    size_t allocated;
} Heap;

void print_heap(Heap *heap);

size_t get_sizeof_value(Value val);

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

void heap_log_event(Heap *heap, char event);

void validate_integrity_of_tags(Heap *heap);

//BYTECODE HEAP ALLOCS

