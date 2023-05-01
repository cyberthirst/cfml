//
// Created by Mirek Škrabal on 24.03.2023.
//

#include <stdio.h>
#include <time.h>

#include "heap.h"
#include "../gc/gc.h"
#include "../utils/utils.h"
#include "../ast/ast_interpreter.h"
#include "../cmd_config.h"

Roots *roots;

Heap *construct_heap(const long long int mem_sz) {
    Heap *heap = malloc(sizeof(Heap));
    heap->heap_start = calloc(mem_sz, sizeof(uint8_t));
    heap->heap_free = heap->heap_start;
    heap->total_size = mem_sz;
    heap->free_list = malloc(sizeof(Block));
    heap->free_list->next = NULL;
    heap->free_list->sz = mem_sz;
    heap->free_list->start = heap->heap_start;
    heap->allocated = 0;
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

//TODO this function should be used on multiple places, not only in gc, refactor
size_t get_sizeof_value(Value val) {
    size_t sz = 0;
    switch (*val) {
        case VK_INTEGER: {
            sz += sizeof(Integer);
            break;
        }
        case VK_NULL: {
            sz += sizeof(Null);
            break;
        }
        case VK_FUNCTION: {
            Bc_Func *func = (Bc_Func *) val;
            sz += sizeof(Bc_Func) + func->len;
            break;
        }
        case VK_BOOLEAN: {
            sz += sizeof(Boolean);
            break;
        }
        case VK_ARRAY: {
            Array *arr = (Array *) val;
            sz += sizeof(Array) + sizeof(Value) * arr->size;
            break;
        }
        case VK_OBJECT: {
            Object *obj = (Object *) val;
            sz += sizeof(Object) + sizeof(Field) * obj->field_cnt;
            break;
        }

        default: {
            while (*val == 0x7f) {
                sz++;
                val++;
            }
        }
    }
    //align to 8 bytes
    //size_t diff = 8 - ((uintptr_t)(val + sz) % 8);
    size_t diff = sz % 8;
    if (diff != 0) {
        sz += 8 - diff;
    }
    return sz;
}

void validate_integrity_of_tags(Heap *heap) {
    uint8_t *heap_start = heap->heap_start;
    uint8_t *heap_end = heap->heap_start + heap->total_size;
    while (heap_start < heap_end) {
        Value val = (Value) heap_start;
        assert(*val >= 0 && *val <= 7);
        heap_start += get_sizeof_value(val);
    }
}

void *heap_alloc(size_t sz, Heap *heap) {
    bool gc_performed = false;
    // Adjust the requested size for alignment
    size_t sz_aligned = sz;
    size_t alignment = 8; // Align to 8 bytes
    size_t diff = sz_aligned % alignment;
    if (diff != 0) {
        sz_aligned += alignment - diff; // Adjust the size to the next multiple of alignment
    }
    Block **current;
alloc_again:
    current = &(heap->free_list);
    // Try to find a start block of sufficient size
    while (*current != NULL) {
        if ((*current)->sz >= sz_aligned) {
            // Found a block of nsufficient size
            void *ptr = (*current)->start;

            //update the total allocated bytes
            heap->allocated += sz_aligned;

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
    if (!gc_performed) {
        garbage_collect(heap);
        gc_performed = true;
        goto alloc_again;
    }
    // No block of sufficient size was found
    printf("Heap is full, exiting.\n");
    printf("Max heap size is: %ld, and current request is: %ld\n", heap->total_size, sz);
    exit(1);
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

void heap_log_event(Heap *heap, char event) {
    if (config.heap_log_file == NULL) {
        return;
    }
    // Open the log file in append mode
    FILE *log_file = fopen(config.heap_log_file, "a");
    if (log_file == NULL) {
        printf("Failed to open heap log file\n");
        return;
    }

    // Get the current time in nanoseconds
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long long timestamp = ts.tv_sec * 1e9 + ts.tv_nsec;

    fprintf(log_file, "%lld,%c,%zu\n", timestamp, event, heap->allocated);

    fclose(log_file);
}

//BYTECODE HEAP ALLOCS

