//
// Created by Mirek Škrabal on 23.03.2023.
//

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../ast/ast_interpreter.h"


typedef struct {
    uint8_t *ret_addr;
    //ptrs to the heap
    uint8_t **locals;
    size_t locals_sz;
} Frame;

void deserialize_bc_file(const char* filename);

void bc_interpret();

void bc_init();

void bc_free();

Value peek_operand();
