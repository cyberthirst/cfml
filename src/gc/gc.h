//
// Created by Mirek Škrabal on 29.04.2023.
//

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../heap/heap.h"

typedef struct Block {
    struct Block *next;
    size_t sz;
    uint8_t *free;
} Block;