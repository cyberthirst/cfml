//
// Created by Mirek Škrabal on 24.03.2023.
//

#include <stdio.h>

#include "heap.h"
#include "../ast/ast_interpreter.h"


void *heap_alloc(size_t sz, Heap *heap) {
    //printf("allocating %lld bytes\n", sz);
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

//TODO add printing of all types
void print_heap(Heap *heap) {
    printf("heap(:\n");
    uint8_t *ptr = heap->heap_start;
    int cnt = 0;
    while (ptr < heap->heap_free) {
        printf("element %d: ", cnt++);
        print_val(ptr);
        printf("\n");
        switch (*(Value)ptr) {
            case VK_INTEGER: {
                ptr += sizeof(Integer);
                break;
            }
            case VK_BOOLEAN: {
                ptr += sizeof(Boolean);
                break;
            }
            case VK_NULL: {
                ptr += sizeof(Null);
                break;
            }
            case VK_STRING: {
                Bc_String *str = (Bc_String *) ptr;
                ptr += sizeof(Bc_String) + str->len;
                break;
            }
            case VK_OBJECT: {
                Object *obj = (Object *) ptr;
                ptr += sizeof(Object) + sizeof(Field) * obj->field_cnt;
                break;
            }
            case VK_FUNCTION: {
                Bc_Func *func = (Bc_Func *) ptr;
                ptr += sizeof(Bc_Func) + func->len;
                break;
            }
        }
        //align to 8 bytes
        size_t diff = 8 - ((uintptr_t)ptr % 8);
        if (diff != 8) {
            ptr += diff;
        }
    }
    printf("\n)\n");
}

Array *array_alloc(int size, Heap *heap) {
    //array is stored as folows:
    // ValueKind | size_t | Value[size]
    // ValueKind == VK_ARRAY | size_t == numOfElems(array) | Value[size] == array of values (aka pointers to the actual valuesa)
    return heap_alloc(sizeof(Array) + sizeof(Value) * size, heap);
}

//TODO not constistent with other allocs
//not setting the init value
Value construct_array(int size, Heap *heap) {
    Array *array = array_alloc(size, heap);
    array->kind = VK_ARRAY;
    array->size = size;
    return array;
}

Object *object_alloc(int size, Heap *heap) {
    //object is stored as follows:
    // ValueKind | parent | size_t | Value[size]
    // ValueKind == VK_OBJECT | parent == VK_OBJECT | size_t == numOfFields(object) | Value[size] == array of Values (aka pointers members of the object)
    return heap_alloc(sizeof(Object) + sizeof(Field) * size, heap);
}

Value construct_object(int size, Value parent, Heap *heap) {
    Object *object = object_alloc(size, heap);
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

Bc_String *bc_string_alloc(uint32_t size, Heap *heap) {
    return heap_alloc(sizeof(Bc_String) + sizeof(uint8_t) * size, heap);
}

//deep copy of the string to the heap
Value construct_bc_string(Bc_String *str, Heap *heap) {
    Bc_String *string = bc_string_alloc(str->len, heap);
    memcpy(string, str, sizeof(Bc_String ) + str->len);
    return string;
}

Bc_Func *bc_function_alloc(uint32_t size, Heap *heap) {
    return heap_alloc(sizeof(Bc_Func) + sizeof(uint8_t) * size, heap);
}

//deep copy of the function to the heap
Value construct_bc_function(Bc_Func *func, Heap *heap) {
    Bc_Func *bc_func = bc_function_alloc(func->len, heap);
    memcpy(bc_func, func, sizeof(Bc_Func) + func->len);
    return bc_func;
}

//BYTECODE HEAP ALLOCS

