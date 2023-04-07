//
// Created by Mirek Škrabal on 24.03.2023.
//

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "parser.h"


typedef enum {
    VK_INTEGER = 0x00,
    VK_NULL = 0x01,
    VK_STRING = 0x02,
    VK_FUNCTION = 0x3,
    VK_BOOLEAN = 0x04,
    VK_CLASS = 0x05,
    VK_ARRAY,
    VK_OBJECT,
} ValueKind;

typedef enum {
    DROP = 0x00,
    CONSTANT = 0x01,
    PRINT = 0x02,
    ARRAY = 0x03,
    OBJECT = 0x04,
    GET_FIELD = 0x05,
    SET_FIELD = 0x06,
    CALL_METHOD = 0x07,
    CALL_FUNCTION = 0x08,
    SET_LOCAL = 0x09,
    GET_LOCAL = 0x0A,
    SET_GLOBAL = 0x0B,
    GET_GLOBAL = 0x0C,
    BRANCH = 0x0D,
    JUMP = 0x0E,
    RETURN = 0x0F,
} Instruction;

typedef uint8_t *Value;

typedef struct {
    uint8_t kind;
    bool val;
} Boolean;

typedef struct {
    uint8_t kind;
    i32 val;
}Integer;

typedef struct {
    uint8_t kind;
    AstFunction *val;
}Function;

typedef struct {
    uint8_t kind;
}Null;

typedef struct {
    uint8_t kind;
    size_t size;
    Value val[];
}Array;

typedef struct {
    Str name;
    Value val;
} Field;

typedef struct {
    uint8_t kind;
    Value parent;
    size_t field_cnt;
    Field val[];
}Object;


// BC INTERPRETER TYPES

typedef struct {
    uint8_t kind;
    uint32_t len;
    uint8_t value[];
} Bc_String;

typedef struct {
    uint8_t kind;
    uint8_t params;
    uint16_t locals;
    uint32_t len;
    uint8_t bytecode[];
} Bc_Func;

typedef struct {
    uint8_t kind;
    uint16_t count;
    uint16_t members[];
} Bc_Class;

typedef struct {
    uint16_t count;
    uint16_t *indexes;
    uint8_t **values;
} Bc_Globals;
