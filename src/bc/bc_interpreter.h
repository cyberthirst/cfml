//
// Created by Mirek Škrabal on 23.03.2023.
//

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../ast/ast_interpreter.h"

#define CONST_POOL_SZ (1024 * 1024 * 256)

extern void *const_pool;
extern uint8_t **const_pool_map;

void deserialize(const char* filename);

void bc_interpret();

void bc_init();

void bc_free();