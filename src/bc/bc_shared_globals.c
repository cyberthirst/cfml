//
// Created by Mirek Škrabal on 07.04.2023.
//

#include "bc_shared_globals.h"

#define CONST_POOL_SZ (1024 * 1024 * 256)

void *const_pool = NULL;
//maps the index of the element in the constant pool
//to the address of the element in the constant pool
uint8_t **const_pool_map = NULL;
//number of constants in the const pool
uint16_t const_pool_count = 0;
Bc_Globals globals;
//index to the constant pool
//the bytecode at the address const_pool[entry_point] is the entry point of the program
uint16_t entry_point = 0;
