//
// Created by Mirek Škrabal on 24.03.2023.
//

#pragma once

#include <stddef.h>
#include "types.h"

void print_val(Value val);

void print_op_stack(Value *stack, size_t size);

bool is_primitive(ValueKind kind);
