//
// Created by Mirek Škrabal on 29.04.2023.
//


//we don't want to add a new flag to the types, so we use the highest bit of the kind field
#define MARK_FLAG 0x80 // 10000000 in binary
#define MARK(val) (*(Value *)(val) |= MARK_FLAG)
#define UNMARK(val) (*(Value *)(val) &= ~MARK_FLAG)
#define IS_MARKED(val) (*(Value *)(val) & MARK_FLAG)
#define GET_KIND(val) (*(Value *)(val) & ~MARK_FLAG)

/*void mark(void *object) {
    switch (GET_KIND(object)) {
        case VK_INTEGER:
            mark_integer((Integer *)object);
            break;
            // Other cases...
    }
}

void merge_free_blocks(Heap *heap) {
    sort_free_list(heap); // This needs to be implemented
    uint8_t *current = heap->heap_free_list;
    while (current != NULL) {
        // If the next block is right after the current one, merge them
        uint8_t *next = *(uint8_t **)current;
        if (next != NULL && next == current + get_block_size(current)) {
            // Update the size of the current block
            set_block_size(current, get_block_size(current) + get_block_size(next));
            // Skip the next block in the free list
            *(uint8_t **)current = *(uint8_t **)next;
        } else {
            // Move on to the next block
            current = next;
        }
    }
}

void sweep(Heap *heap) {
    for (void *object = heap->start; object != heap->free; object = get_next_object(object)) {
        if (!IS_MARKED(object)) {
            free_object(heap, object);
        } else {
            UNMARK(object);
        }
    }
    merge_free_blocks(heap);
}*/