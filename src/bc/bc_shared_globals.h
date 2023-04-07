//
// Created by Mirek Škrabal on 07.04.2023.
//

#pragma once

#include <stdint.h>
#include "../types.h"

/*
 * If we use the run action then the program should act as following:
 *  1. compile the given file to bytecode
 *  2. interpret the bytecode
 *  - we want the compiler and the interpreter to share the same structures (e.g. the constant pool)
 *  - it would be inefficient to output the bytecode to file and then read it again
 *  - this files defines the shared structures
 */

void *const_pool = NULL;
//maps the index of the element in the constant pool
//to the address of the element in the constant pool
uint8_t **const_pool_map = NULL;
//number of constants in the const pool
uint16_t const_pool_count = 0;
Bc_Globals globals;
//index to the constant poool
//the bytecode at the address const_pool[entry_point] is the entry point of the program
uint16_t entry_point = 0;
