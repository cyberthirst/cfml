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

typedef ValueKind *Value;

/*typedef struct Array Array;
typedef struct Object Object;
typedef struct Function Function;
typedef struct Integer Integer;
typedef struct Boolean Boolean;
typedef struct Null Null;*/

typedef struct {
    ValueKind kind;
    bool val;
} Boolean;

typedef struct {
    ValueKind kind;
    i32 val;
}Integer;

typedef struct {
    ValueKind kind;
    AstFunction *val;
}Function ;

typedef struct {
    ValueKind kind;
}Null ;

typedef struct {
    ValueKind kind;
    size_t size;
    Value val[];
}Array ;

typedef struct {
    Str name;
    Value val;
} Field;

typedef struct {
    ValueKind kind;
    Value parent;
    size_t field_cnt;
    Field val[];
}Object ;

