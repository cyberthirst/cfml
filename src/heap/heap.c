//
// Created by Mirek Škrabal on 24.03.2023.
//

#include <stdio.h>

#include "heap.h"
#include "../gc/gc.h"
#include "../utils/utils.h"
#include "../ast/ast_interpreter.h"

Heap *construct_heap(const long long int mem_sz) {
    Heap *heap = malloc(sizeof(Heap));
    heap->heap_start = malloc(MEM_SZ);
    heap->heap_free = heap->heap_start;
    heap->free_list = malloc(sizeof(Block));
    heap->free_list->next = NULL;
    heap->free_list->sz = MEM_SZ;
    heap->free_list->start = heap->heap_start;
    heap->heap_size = 0;
    return heap;
}

void heap_free(Heap **heap) {
    // Free the start blocks list
    Block *current = (*heap)->free_list;
    while (current != NULL) {
        Block *next = current->next;
        free(current);
        current = next;
    }

    free((*heap)->heap_start);
    free(*heap);
    *heap = NULL;
}

void *heap_alloc(size_t sz, Heap *heap) {
    bool gc = false;
    // Adjust the requested size for alignment
    size_t sz_aligned = sz;
    size_t alignment = 8; // Align to 8 bytes
    size_t diff = sz_aligned % alignment;
    if (diff != 0) {
        sz_aligned += alignment - diff; // Adjust the size to the next multiple of alignment
    }
    Block **current;
alloc_again:
    garbage_collect(heap);
    current = &(heap->free_list);
    // Try to find a start block of sufficient size
    while (*current != NULL) {
        if ((*current)->sz >= sz_aligned) {
            // Found a block of sufficient size
            void *ptr = (*current)->start;

            // If the block is larger than needed, update it, otherwise remove it
            if ((*current)->sz > sz_aligned) {
                (*current)->start += sz_aligned;
                (*current)->sz -= sz_aligned;
            } else {
                Block *next = (*current)->next;
                //we don't start the data, the heap array stays intact
                free(*current);
                *current = next;
            }

            return ptr;
        }
        current = &((*current)->next);
    }
    /*
    // TODO uncomment this when testing is finished - currently we call gc for every allocation
    garbage_collect(heap);
    if (!gc) {
        gc = true;
        goto alloc_again;
    }*/

    // No start block of sufficient size was found
    printf("Heap is full, exiting.\n");
    printf("Max heap size is: %ld, and current heap size is: %ld\n", MEM_SZ, heap->heap_size);
    exit(1);
}

//TODO this function should be used on multiple places, not only in gc, refactor
size_t get_sizeof_value(Value val) {
    size_t sz = 0;
    switch (*val) {
        case VK_INTEGER: {
            sz += sizeof(Integer);
            break;
        }
        case VK_BOOLEAN: {
            sz += sizeof(Boolean);
            break;
        }
        case VK_NULL: {
            sz += sizeof(Null);
            break;
        }
        case VK_STRING: {
            Bc_String *str = (Bc_String *) sz;
            sz += sizeof(Bc_String) + str->len;
            break;
        }
        case VK_OBJECT: {
            Object *obj = (Object *) sz;
            sz += sizeof(Object) + sizeof(Field) * obj->field_cnt;
            break;
        }
        case VK_FUNCTION: {
            Bc_Func *func = (Bc_Func *) sz;
            sz += sizeof(Bc_Func) + func->len;
            break;
        }
        default: {
            UNREACHABLE;
        }
    }
    //align to 8 bytes
    size_t diff = 8 - ((uintptr_t)val % 8);
    if (diff != 8) {
        sz += diff;
    }
    return sz;
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

//we don't need to copy the string string, we can just refer to it's value in cp
//have this intermediate function so we can change the behaviour later
Value construct_bc_string(Bc_String *str, Heap *heap) {
    //deep copy of the string to the heap
    /*Bc_String *string = bc_string_alloc(str->len, heap);
    memcpy(string, str, sizeof(Bc_String ) + str->len);
    return string;*/
    return str;
}

Bc_Func *bc_function_alloc(uint32_t size, Heap *heap) {
    return heap_alloc(sizeof(Bc_Func) + sizeof(uint8_t) * size, heap);
}


//we don't need to copy the function, we can just refer to it's value in cp
Value construct_bc_function(Bc_Func *func, Heap *heap) {
    //deep copy of the function to the heap
    /*Bc_Func *bc_func = bc_function_alloc(func->len, heap);
    memcpy(bc_func, func, sizeof(Bc_Func) + func->len);
    return bc_func;*/
    return func;
}

//BYTECODE HEAP ALLOCS

