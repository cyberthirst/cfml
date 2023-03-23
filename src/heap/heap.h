//
// Created by Mirek Škrabal on 23.03.2023.
//

#pragma once

#include <printf.h>
#include "../ast/ast_interpreter.h"

const long long int MEM_SZ = 1024L * 1024 * 1024 * 1;


void *heap_alloc(size_t sz, Heap *heap) {
    if (heap->heap_size + sz > MEM_SZ) {
        printf("Out of memory\n");
        exit(1);
    }
    else {
        void *ptr = heap->heap_free;

        heap->heap_free += sz;
        heap->heap_size += sz;

        //align to 8 bytes
        size_t diff = 8 - (heap->heap_size % 8);
        if (diff != 8) {
            heap->heap_free += diff;
            heap->heap_size += diff;
        }

        return ptr;
    }
}

Array *array_alloc(int size, IState *state) {
    //array is stored as folows:
    // ValueKind | size_t | Value[size]
    // ValueKind == VK_ARRAY | size_t == numOfElems(array) | Value[size] == array of values (aka pointers to the actual valuesa)
    return heap_alloc(sizeof(Array) + sizeof(Value) * size, state->heap);
}

Value construct_array(int size, IState *state) {
    Array *array = array_alloc(size, state);
    array->kind = VK_ARRAY;
    array->size = size;
    return array;
}

Object *object_alloc(int size, IState *state) {
    //object is stored as follows:
    // ValueKind | parent | size_t | Value[size]
    // ValueKind == VK_OBJECT | parent == VK_OBJECT | size_t == numOfFields(object) | Value[size] == array of Values (aka pointers members of the object)
    return heap_alloc(sizeof(Object) + sizeof(Field) * size, state->heap);
}

Value construct_object(int size, Value parent, IState *state) {
    Object *object = object_alloc(size, state);
    object->kind = VK_OBJECT;
    object->field_cnt = size;
    object->parent = parent;
    return object;
}

Function *function_alloc(IState *state) {
    return heap_alloc(sizeof(Function), state->heap);
}

Value construct_function(AstFunction *ast_func, IState *state) {
    Function *func = function_alloc(state);
    func->kind = VK_FUNCTION;
    func->val = ast_func;
    return func;
}

Integer *integer_alloc(IState *state) {
    return heap_alloc(sizeof(Integer), state->heap);
}

Value construct_integer(i32 val, IState *state) {
    Integer *integer = integer_alloc(state);
    integer->kind = VK_INTEGER;
    integer->val = val;
    return integer;
}

Boolean *boolean_alloc(IState *state) {
    return heap_alloc(sizeof(Boolean), state->heap);
}

Value construct_boolean(bool val, IState *state) {
    Boolean *boolean = boolean_alloc(state);
    boolean->kind = VK_BOOLEAN;
    boolean->val = val;
    return boolean;
}

Null *null_alloc(IState *state) {
    return heap_alloc(sizeof(Null), state->heap);
}


//TODO don't construct null, just set the Value ptr to NULL
Value construct_null(IState *state) {
    Null *null = null_alloc(state);
    null->kind = VK_NULL;
    return null;
    //return state->null;
}
