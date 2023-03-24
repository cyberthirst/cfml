//
// Created by Mirek Škrabal on 24.03.2023.
//

#include "heap.h"
#include "../ast/ast_interpreter.h"


void *heap_alloc(size_t sz, Heap *heap) {
    long long int tmp = MEM_SZ;
    if (heap->heap_size + sz > MEM_SZ) {
        printf("Heap is full, exiting.\n");
        printf("Max heap size is: %ld, and current heap size is: %ld\n", MEM_SZ, heap->heap_size);
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

        //printf("heap size is: %ld\n", heap->heap_size);
        return ptr;
    }
}

Array *ast_array_alloc(int size, Heap *heap) {
    //array is stored as folows:
    // ValueKind | size_t | Value[size]
    // ValueKind == VK_ARRAY | size_t == numOfElems(array) | Value[size] == array of values (aka pointers to the actual valuesa)
    return heap_alloc(sizeof(Array) + sizeof(Value) * size, heap);
}

Value construct_ast_array(int size, Heap *heap) {
    Array *array = ast_array_alloc(size, heap);
    array->kind = VK_ARRAY;
    array->size = size;
    return array;
}

Object *ast_object_alloc(int size, Heap *heap) {
    //object is stored as follows:
    // ValueKind | parent | size_t | Value[size]
    // ValueKind == VK_OBJECT | parent == VK_OBJECT | size_t == numOfFields(object) | Value[size] == array of Values (aka pointers members of the object)
    return heap_alloc(sizeof(Object) + sizeof(Field) * size, heap);
}

Value construct_ast_object(int size, Value parent, Heap *heap) {
    Object *object = ast_object_alloc(size, heap);
    object->kind = VK_OBJECT;
    object->field_cnt = size;
    object->parent = parent;
    return object;
}

Function *ast_function_alloc(Heap *heap) {
    return heap_alloc(sizeof(Function), heap);
}

Value construct_ast_function(AstFunction *ast_func, Heap *heap) {
    Function *func = ast_function_alloc(heap);
    func->kind = VK_FUNCTION;
    func->val = ast_func;
    return func;
}

Integer *integer_alloc(Heap *heap) {
    return heap_alloc(sizeof(Integer), heap);
}

Value construct_integer(i32 val, Heap *heap) {
    Integer *integer = integer_alloc(heap);
    integer->kind = VK_INTEGER;
    integer->val = val;
    return integer;
}

Boolean *boolean_alloc(Heap *heap) {
    return heap_alloc(sizeof(Boolean), heap);
}

Value construct_boolean(bool val, Heap *heap) {
    Boolean *boolean = boolean_alloc(heap);
    boolean->kind = VK_BOOLEAN;
    boolean->val = val;
    return boolean;
}

Null *null_alloc(Heap *heap) {
    return heap_alloc(sizeof(Null), heap);
}

//TODO don't construct null, just set the Value ptr to NULL
Value construct_null(Heap *heap) {
    Null *null = null_alloc(heap);
    null->kind = VK_NULL;
    return null;
    //return state->null;
}

//BYTECODE HEAP ALLOCS

