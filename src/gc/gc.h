//
// Created by Mirek Škrabal on 29.04.2023.
//

#pragma once

#include "../types.h"
#include "../bc/interpret/bc_interpreter.h"


/*
 * roots of the gc:
 * - global variables
 * - frames
 *   - local variables
 * - stack
 */

typedef struct {
    Frame *frames;
    size_t *frames_sz;
    Value *stack;
    size_t *stack_sz;
    Bc_Globals *globals;
} Roots;

//global variable
//will be initialized in the bc_interpreter
extern Roots *roots;

void garbage_collect(Heap *heap);