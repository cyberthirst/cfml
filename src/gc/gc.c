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

    MARK(val);

    switch (*val) {
        case VK_ARRAY: {
            Array *arr = (Array *)val;
            for (size_t i = 0; i < arr->size; ++i) {
                mark_value(arr->val[i]);
            }
            break;
        }
        case VK_OBJECT: {
            Object *obj = (Object *)val;
            mark_value(obj->parent);
            for (size_t i = 0; i < obj->field_cnt; ++i) {
                mark_value(obj->val[i].val);
            }
            break;
        }
        case VK_FUNCTION: {
            // TODO:
            break;
        }
            // For other cases, there's no nested Value to mark
    }
}

void mark() {
    // Mark frames
    for (size_t i = 0; i < *(roots->frames_sz); ++i) {
        Frame *frame = roots->frames + i;
        for (size_t j = 0; j < frame->locals_sz; ++j) {
            Value val = frame->locals[j];
            mark_value(val);
        }
    }

    // Mark stack
    for (size_t i = 0; i < *(roots->stack_sz); ++i) {
        Value val = roots->stack + i;
        mark_value(val);
    }

    // Mark globals
    for (uint16_t i = 0; i < roots->globals->count; ++i) {
        Value val = roots->globals->values[i];
        mark_value(val);
    }
}

void sweep(Heap *heap) {
    Block **current = &(heap->free_list);
    while (*current != NULL) {
        uint8_t *block_start = (*current)->free;
        uint8_t *block_end = block_start + (*current)->sz;

        for (uint8_t *ptr = block_start; ptr < block_end; ptr += sizeof(Value)) {
            Value val = (Value)ptr;
            if (IS_MARKED(val)) {
                // If the value was marked, unmark it for the next GC cycle
                UNMARK(val);
            } else {
                // The value wasn't marked, free it
                // Note: This is a simple form of freeing, you may need to adjust for your case
                Block *new_block = malloc(sizeof(Block));
                new_block->next = *current;
                new_block->sz = sizeof(Value);
                new_block->free = ptr;
                *current = new_block;
            }
        }
        current = &((*current)->next);
    }
}

void garbage_collect(Heap *heap) {
    mark();
    sweep(heap);
}
