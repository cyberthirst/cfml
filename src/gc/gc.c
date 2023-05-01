//
// Created by Mirek Škrabal on 29.04.2023.
//

#include <assert.h>

#include "gc.h"
#include "../heap/heap.h"
#include "../cmd_config.h"

//suppose we're sweeping the heap and we reached a marked array
//we need to unmark it, however we can't unmark its values
//if we did, the values would be unmarked and if they are stored after the array then they would be swept, ie
//they would be freed into the free list
uint8_t * unmark_in_future[1000];
size_t unmark_in_future_cnt = 0;

bool is_on_heap(Value val, Heap *heap) {
    return val >= heap->heap_start && val < (heap->heap_start + heap->total_size);
}

void mark_value(Value val, Heap *heap) {
    //some roots can point to the constant pool, which is not part of the heap
    //we don't manage the constant pool, so we don't need to mark it
    if (!is_on_heap(val, heap)) {
        return;
    }
    if (IS_MARKED(val)) {
        assert(*val - 128 >= VK_INTEGER && *val - 128 <= VK_OBJECT);
        // Already marked, no need to traverse again
        return;
    }
    assert(*val >= VK_INTEGER && *val <= VK_OBJECT);
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

void mark_from_roots(Heap *heap) {
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

    // Mark auxiliary vals (like global null)
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

void unmark(Value val, Heap *heap) {
    /*
     * it can happen that we call unmark on already unmarked value
     * - eg an array contains elements
     * - those elements are pointers to the heap
     * - these heap pointers are also analyzed by the gc
     * - if the pointer is stored "before" the array pointer then it will be analyzed before and also unmarked before
     */
    if (!IS_MARKED(val)) {
        return;
    }
    UNMARK(val);
    //printf("unmarking %p, index: %lu\n", val, total_marks);
    assert(*val >= 0 && *val <= 7);
    switch (*val) {
        case VK_ARRAY: {
            Array *arr = (Array *) val;
            for (size_t i = 0; i < arr->size; ++i) {
                unmark(arr->val[i], heap);
            }
            break;
        }
        case VK_OBJECT: {
            Object *obj = (Object *) val;
            for (size_t i = 0; i < obj->field_cnt; ++i) {
                unmark(obj->val[i].val, heap);
            }
            break;
        }
    }
}

void sweep(Heap *heap) {
    uint8_t *heap_start = heap->heap_start;
    uint8_t *heap_end = heap->heap_start + heap->total_size;
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
            unmark_in_future[unmark_in_future_cnt++] = val;
            assert(*heap_start >= VK_INTEGER && *heap_start <= VK_OBJECT);
            if (is_consecutive_unmarked){
                Block *new_block = malloc(sizeof(Block));
                new_block->next = heap->free_list;
                new_block->sz = block_size;
                new_block->start = block_start;
                heap->free_list = new_block;
                heap->allocated -= block_size;
                memset(block_start, 0x7f, block_size);
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

void push_aux_root(Value val) {
    assert(roots->aux_sz < 64);
    roots->aux[roots->aux_sz++] = val;
}

void pop_aux_root(size_t n) {
    assert(roots->aux_sz - n >= 0);
    roots->aux_sz -= n;
}

void garbage_collect(Heap *heap) {
    //if roots are not allocated, then immediately return, because we're not using bc interpreter
    if (roots == NULL) {
        return;
    }
    heap_log_event(heap, 'B');
    mark_from_roots(heap);
    sweep(heap);
    for (size_t i = 0; i < unmark_in_future_cnt; ++i) {
        unmark(unmark_in_future[i], heap);
    }
    memset(unmark_in_future, 0, sizeof(unmark_in_future));
    unmark_in_future_cnt = 0;
    heap_log_event(heap, 'A');
}
