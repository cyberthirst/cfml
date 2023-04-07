//
// Created by Mirek Škrabal on 23.03.2023.
//

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../ast/ast_interpreter.h"




void deserialize_bc_file(const char* filename);

void bc_interpret();

void bc_init();

void bc_free();
