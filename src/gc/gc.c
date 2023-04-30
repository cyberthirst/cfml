//
// Created by Mirek Škrabal on 29.04.2023.
//

#include "gc.h"
#include "../heap/heap.h"

//we don't want to add a new flag to the types, so we use the highest bit of the kind field
#define MARK_FLAG 0x80 // 10000000 in binary
#define MARK(val) (*(val) |= MARK_FLAG)
#define UNMARK(val) (*(val) &= ~MARK_FLAG)
#define IS_MARKED(val) (*(val) & MARK_FLAG)
#define GET_KIND(val) (*(val) & ~MARK_FLAG)

void mark_value(Value val) {
    if (IS_MARKED(val)) {
        // Already marked, no need to traverse again
        return;
    }

    switch (*val) {
        case VK_ARRAY: {
            MARK(val);
            Array *arr = (Array *)val;
            for (size_t i = 0; i < arr->size; ++i) {
                mark_value(arr->val[i]);
            }
            break;
        }
        case VK_OBJECT: {
            MARK(val);
            Object *obj = (Object *)val;
            mark_value(obj->parent);
            for (size_t i = 0; i < obj->field_cnt; ++i) {
                mark_value(obj->val[i].val);
            }
            break;
        }
        default:
            MARK(val);
    }
}

void mark() {
    Roots *rt = roots;
    // Mark frames
    for (size_t i = 0; i < *(roots->frames_sz); ++i) {
        Frame *frame = roots->frames + i;
        for (size_t j = 0; j < frame->locals_sz; ++j) {
            mark_value(frame->locals[j]);
        }
    }

    // Mark stack
    for (size_t i = 0; i < *(roots->stack_sz); ++i) {
        mark_value(roots->stack[i]);
    }
    //TODO are all the values really allocated directly in the roots?
    // isn't it the case that I allocate some value and root it later?

    // Mark globals
    //TODO maybe not needed? globals point to const pool
    //for (uint16_t i = 0; i < roots->globals->count; ++i) {
    //    mark_value(roots->globals->values[i]);
    //}
}

void sweep(Heap *heap) {
    uint8_t *heap_start = heap->heap_start;
    uint8_t *heap_end = heap->heap_start + heap->heap_size;
    bool is_consecutive_unmarked = false;
    size_t block_size = 0;
    uint8_t *block_start = NULL;

    while (heap_start < heap_end) {
        Value val = (Value)heap_start;
        if (IS_MARKED(val)) {
            //is marked thus shouldn't be freed, therefore we just skip it
            UNMARK(val);
            if (is_consecutive_unmarked){
                Block *new_block = malloc(sizeof(Block));
                new_block->next = heap->free_list;
                new_block->sz = block_size;
                new_block->start = block_start;
                heap->free_list = new_block;
                block_size = 0;
            }
            is_consecutive_unmarked = false;
        }
        else {
            if (!is_consecutive_unmarked) {
                block_start = heap_start;
            }
            block_size += get_sizeof_value(val);
            is_consecutive_unmarked = true;
        }
        heap_start += get_sizeof_value(val);
    }
}

void garbage_collect(Heap *heap) {
    //TODO add if here that we're running the bc interpreter
    // for AST interpreter we don't mark the roots and thus GC doens't make any sense
    mark();
    sweep(heap);
}
