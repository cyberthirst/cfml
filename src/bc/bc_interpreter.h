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

typedef struct {
    ValueKind kind;
    uint32_t length;
    char* value;
} Bc_String;

typedef struct {
    ValueKind kind;
    uint8_t params;
    uint16_t locals;
    uint32_t length;
    uint8_t* bytecode;
} Bc_Function;

typedef struct {
    ValueKind kind;
    uint16_t count;
    uint16_t* members;
} Bc_Class;

typedef struct {
    uint16_t count;
    uint16_t* indices;
} Bc_Globals;


void deserialize(const char* filename);