//
// Created by Mirek Škrabal on 16.04.2023.
//

#include <stdio.h>

#include "../types.h"
#include "serialization.h"
#include "bc_shared_globals.h"

void write_int32_le(int32_t value) {
    uint8_t bytes[4] = {
            (uint8_t)(value & 0xFF),
            (uint8_t)((value >> 8) & 0xFF),
            (uint8_t)((value >> 16) & 0xFF),
            (uint8_t)((value >> 24) & 0xFF),
    };
    fwrite(bytes, sizeof(uint8_t), 4, stdout);
}

void write_uint16_le(uint16_t value) {
    uint8_t bytes[2] = {
            (uint8_t)(value & 0xFF),
            (uint8_t)((value >> 8) & 0xFF),
    };
    fwrite(bytes, sizeof(uint8_t), 2, stdout);
}

void write_uint32_le(uint32_t value) {
    uint8_t bytes[4] = {
            (uint8_t)(value & 0xFF),
            (uint8_t)((value >> 8) & 0xFF),
            (uint8_t)((value >> 16) & 0xFF),
            (uint8_t)((value >> 24) & 0xFF),
    };
    fwrite(bytes, sizeof(uint8_t), 4, stdout);
}


void serialize_integer(const Integer *integer) {
    putchar(integer->kind);
    fwrite(&(integer->val), sizeof(int32_t), 1, stdout);
}

void serialize_boolean(const Boolean *boolean) {
    putchar(boolean->kind);
    fwrite(&(boolean->val), sizeof(bool), 1, stdout);
}

void serialize_null() {
    putchar(VK_NULL);
}

void serialize_string(const Bc_String *str) {
    putchar(str->kind);
    fwrite(&(str->len), sizeof(uint32_t), 1, stdout);
    fwrite(str->value, sizeof(uint8_t), str->len, stdout);
}

void serialize_function(const Bc_Func *func) {
    putchar(func->kind);
    fwrite(&(func->params), sizeof(uint8_t), 1, stdout);
    fwrite(&(func->locals), sizeof(uint16_t), 1, stdout);
    fwrite(&(func->len), sizeof(uint32_t), 1, stdout);
    fwrite(func->bytecode, sizeof(uint8_t), func->len, stdout);
}

void serialize_class(const Bc_Class *cls) {
    putchar(cls->kind);
    fwrite(&(cls->count), sizeof(uint16_t), 1, stdout);
    fwrite(cls->members, sizeof(uint16_t), cls->count, stdout);
}

void serialize_header() {
    const uint8_t header[4] = {0x46, 0x4D, 0x4C, 0x0A};
    fwrite(header, sizeof(uint8_t), 4, stdout);
}

void serialize_globals(const Bc_Globals *globals) {
    write_uint16_le(globals->count);

    for (uint16_t i = 0; i < globals->count; i++) {
        write_uint16_le(globals->indexes[i]);
    }
}

void serialize_entry_point(const uint16_t entry_point_index) {
    write_uint16_le(entry_point_index);
}

void serialize_constant_pool(const size_t pool_size) {
    write_uint16_le(pool_size);
    for (size_t i = 0; i < pool_size; i++) {
        uint8_t *element = const_pool_map[i];
        uint8_t kind = *element;

        switch (kind) {
            case VK_INTEGER:
                serialize_integer((const Integer *)element);
                break;
            case VK_NULL:
                serialize_null();
                break;
            case VK_STRING:
                serialize_string((const Bc_String *)element);
                break;
            case VK_FUNCTION:
                serialize_function((const Bc_Func *)element);
                break;
            case VK_BOOLEAN:
                serialize_boolean((const Boolean *)element);
                break;
            case VK_CLASS:
                serialize_class((const Bc_Class *)element);
                break;
            default:
                fprintf(stderr, "Unknown kind: 0x%02x\n", kind);
                break;
        }
    }
}