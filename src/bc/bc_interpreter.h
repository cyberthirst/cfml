//
// Created by Mirek Škrabal on 23.03.2023.
//

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define CONST_POOL_SZ (1024 * 1024 * 256)

extern void *const_pool;
extern void **const_pool_map;

typedef struct {
    uint8_t tag;
    int32_t value;
} Bc_Integer;

typedef struct {
    uint8_t tag;
    uint8_t value;
} Bc_Boolean;

typedef struct {
    uint8_t tag;
} Bc_Null;

typedef struct {
    uint8_t tag;
    uint32_t length;
    char* value;
} Bc_String;

typedef struct {
    uint8_t tag;
    uint8_t params;
    uint16_t locals;
    uint32_t length;
    uint8_t* bytecode;
} Bc_Function;

typedef struct {
    uint8_t tag;
    uint16_t count;
    uint16_t* members;
} Bc_Class;

typedef struct {
    uint16_t count;
    uint16_t* indices;
} Bc_Globals;

typedef enum {
    INTEGER_TAG = 0x00,
    NULL_TAG = 0x01,
    STRING_TAG = 0x02,
    FUNCTION_TAG = 0x03,
    BOOLEAN_TAG = 0x04,
    CLASS_TAG = 0x05
} Bc_Tag;


void deserialize(const char* filename);