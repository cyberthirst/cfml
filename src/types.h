//
// Created by Mirek Škrabal on 24.03.2023.
//

#pragma once

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

typedef uint8_t *Value;

/*typedef struct Array Array;
typedef struct Object Object;
typedef struct Function Function;
typedef struct Integer Integer;
typedef struct Boolean Boolean;
typedef struct Null Null;*/

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
    char value[];
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
    uint16_t *indices;
} Bc_Globals;
