//
// Created by Mirek Škrabal on 23.03.2023.
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "bc_interpreter.h"

void *const_pool = NULL;
void **const_pool_map = NULL;
Bc_Globals globals;
uint16_t entry_point = 0;

void deserialize(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Cannot open file %s\n", filename);
        exit(1);
    }

    // Read and check the header
    uint8_t header[4];
    fread(header, sizeof(uint8_t), 4, file);
    if (header[0] != 0x46 || header[1] != 0x4D || header[2] != 0x4C || header[3] != 0x0A) {
        printf("Error: Invalid header\n");
        fclose(file);
        exit(1);
    }

    // Read how many objs are in the const pool
    uint16_t const_pool_count;
    fread(&const_pool_count, sizeof(uint16_t), 1, file);

    // Allocate the const pool
    // The pool has constant size, we don't initially know how big the objs actually are
    // If the pool is full we have to exit(1)
    const_pool = malloc(CONST_POOL_SZ);

    const_pool_map  = malloc(sizeof(void*) * const_pool_count);

    //the first obj start at the beginning of the const_pool
    const_pool_map[0] = const_pool;

    // Read the constant pool objects and fill the const_pool & const_pool_map array
    uint8_t tag;
    for (uint16_t i = 0; i < const_pool_count; ++i) {
        fread(&tag, sizeof(uint8_t), 1, file);

        switch (tag) {
            case INTEGER_TAG: {
                Bc_Integer *integer = (Bc_Integer  *)const_pool_map[i];
                integer->tag = tag;
                fread(&integer->value, sizeof(int32_t), 1, file);
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_Integer);
                break;
            }
            case BOOLEAN_TAG: {
                Bc_Boolean *boolean = (Bc_Boolean  *)const_pool_map[i];
                boolean->tag = tag;
                fread(&boolean->value, sizeof(uint8_t), 1, file);
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_Boolean);
                break;
            }
            case NULL_TAG: {
                Bc_Null *null = (Bc_Null  *)const_pool_map[i];
                null->tag = tag;
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_Null);
                break;
            }
            case STRING_TAG: {
                Bc_String *string = (Bc_String  *)const_pool_map[i];
                string->tag = tag;
                fread(&string->length, sizeof(uint32_t), 1, file);
                string->value = (char *)(string + sizeof(uint32_t));
                fread(string->value, sizeof(char), string->length, file);
                string->value[string->length] = '\0';
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_String) + string->length;
                break;
            }
            case FUNCTION_TAG: {
                Bc_Function *function = (Bc_Function  *)const_pool_map[i];
                function->tag = tag;
                fread(&function->params, sizeof(uint8_t), 1, file);
                fread(&function->locals, sizeof(uint16_t), 1, file);
                fread(&function->length, sizeof(uint32_t), 1, file);
                function->bytecode = (uint8_t *)(function + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t));
                fread(function->bytecode, sizeof(uint8_t), function->length, file);
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_Function) + function->length;
                break;
            }
            case CLASS_TAG: {
                Bc_Class *class = (Bc_Class *)const_pool_map[i];
                class->tag = tag;
                fread(&class->count, sizeof(uint16_t), 1, file);
                class->members = (uint16_t *)(class + sizeof(uint16_t));
                fread(class->members, sizeof(uint16_t), class->count, file);
                const_pool_map[i + 1] = const_pool_map[i] + sizeof(Bc_Class) + sizeof(uint16_t) * class->count;
                break;
            }
            default: {
                printf("Error: Invalid tag\n");
                fclose(file);
                exit(1);
            }
        }
    }

    // Read the globals
    fread(&globals.count, sizeof(uint16_t), 1, file);
    globals.indices = malloc(sizeof(uint16_t) * globals.count);
    fread(globals.indices, sizeof(uint16_t), globals.count, file);

    // Read the entry point
    fread(&entry_point, sizeof(uint16_t), 1, file);
    assert(entry_point < const_pool_count);

    // Cleanup
    fclose(file);
}