//
// Created by Mirek Škrabal on 29.04.2023.
//

#include <assert.h>

#include "gc.h"
#include "../heap/heap.h"
#include "../cmd_config.h"

//we don't want to add a new flag to the types, so we use the highest bit of the kind field
#define MARK_FLAG 0x80 // 10000000 in binary
#define MARK(val) (*(val) |= MARK_FLAG)
#define UNMARK(val) (*(val) &= ~MARK_FLAG)
#define IS_MARKED(val) (*(val) & MARK_FLAG)
#define GET_KIND(val) (*(val) & ~MARK_FLAG)

bool is_on_heap(Value val, Heap *heap) {
    return val >= heap->heap_start && val < (heap->heap_start + heap->heap_size);
}

//TODO passing the heap only for testing purposes (see the assert)
// after testing is done, remove the heap parameter
void mark_value(Value val, Heap *heap) {
    //assert(val >= heap->heap_start && val < (heap->heap_start + heap->heap_size));
    if (!is_on_heap(val, heap)) {
        return;
    }
    if (IS_MARKED(val)) {
        // Already marked, no need to traverse again
        return;
    }

    switch (*val) {
        case VK_ARRAY: {
            MARK(val);
            Array *arr = (Array *)val;
            for (size_t i = 0; i < arr->size; ++i) {
                mark_value(arr->val[i], heap);
            }
            break;
        }
        case VK_OBJECT: {
            MARK(val);
            Object *obj = (Object *)val;
            mark_value(obj->parent, heap);
            for (size_t i = 0; i < obj->field_cnt; ++i) {
                mark_value(obj->val[i].val, heap);
            }
            break;
        }
        default:
            MARK(val);
    }
}

void mark(Heap *heap) {
    Roots *rt = roots;
    // Mark frames
    for (size_t i = 0; i < *(roots->frames_sz); ++i) {
        Frame *frame = roots->frames + i;
        for (size_t j = 0; j < frame->locals_sz; ++j) {
            mark_value(frame->locals[j], heap);
        }
    }

    // Mark stack
    for (size_t i = 0; i < *(roots->stack_sz); ++i) {
        mark_value(roots->stack[i], heap);
    }

    for (size_t i = 0; i < roots->aux_sz; ++i) {
        mark_value(roots->aux[i], heap);
    }
}

void dealloc_free_list(Heap *heap) {
    Block *block = heap->free_list;
    Block *tmp;
    while (block != NULL) {
        tmp = block;
        block = block->next;
        free(tmp);
    }
    heap->free_list = NULL;
}

void sweep(Heap *heap) {
    uint8_t *heap_start = heap->heap_start;
    uint8_t *heap_end = heap->heap_start + heap->heap_size;
    bool is_consecutive_unmarked = false;
    size_t block_size = 0;
    uint8_t *block_start = NULL;
    //TODO would be interesting to use the free_list to avoid checking the values in the
    // range of the blocks in the free_list
    dealloc_free_list(heap);

    while (heap_start < heap_end) {
        Value val = (Value)heap_start;
        if (IS_MARKED(val)) {
            //is marked thus shouldn't be freed, therefore we just skip it
            UNMARK(val);
            assert(*val >= VK_INTEGER && *val <= VK_OBJECT);
            if (is_consecutive_unmarked){
                Block *new_block = malloc(sizeof(Block));
                new_block->next = heap->free_list;
                new_block->sz = block_size;
                new_block->start = block_start;
                heap->free_list = new_block;
                heap->allocated -= block_size;
                block_size = 0;
            }
            is_consecutive_unmarked = false;
        }
        else {
            //assert(*val >= VK_INTEGER && *val <= VK_OBJECT);
            if (!is_consecutive_unmarked) {
                block_start = heap_start;
            }
            //TODO 2 call to get_sizeof_value, store the result in a variable
            block_size += get_sizeof_value(val);
            is_consecutive_unmarked = true;
        }
        heap_start += get_sizeof_value(val);
    }
}

void add_aux_root(Value val) {
    if (roots->aux_sz == 0) {
        roots->aux = malloc(sizeof(Value));
        roots->aux_sz = 1;
    }
    else {
        roots->aux = realloc(roots->aux, sizeof(Value) * (roots->aux_sz + 1));
        roots->aux_sz++;
    }
    roots->aux[roots->aux_sz - 1] = val;
}

void garbage_collect(Heap *heap) {
    //if roots are not allocated, then immediately return, because we're not using bc interpreter
    heap_log_event(heap, 'B');
    if (roots == NULL) {
        return;
    }
    mark(heap);
    sweep(heap);
    heap_log_event(heap, 'A');
}
