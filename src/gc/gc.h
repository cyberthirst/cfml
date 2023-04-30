//
// Created by Mirek Škrabal on 29.04.2023.
//

#pragma once

#include "../types.h"
#include "../bc/interpret/bc_interpreter.h"


/*
 * roots of the gc:
 * - frames
 *   - local variables
 * - stack
 * TODO - global_null
 */

typedef struct {
    Frame *frames;
    size_t *frames_sz;
    Value *stack;
    size_t *stack_sz;
    Bc_Globals *globals;
    Value *aux;
    size_t aux_sz;

} Roots;

//global variable
//will be initialized in the bc_interpreter
extern Roots *roots;

void garbage_collect(Heap *heap);

void add_aux_root(Value val);