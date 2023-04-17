//
// Created by Mirek Škrabal on 24.03.2023.
//

#pragma once

#include <stddef.h>
#include "types.h"

void print_val(Value val);

void print_op_stack(Value *stack, size_t size);

void print_instruction(int count, const uint8_t *ip);

bool is_primitive(ValueKind kind);

uint16_t deserialize_u16(const uint8_t *data);

int16_t deserialize_i16(const uint8_t *data);

uint32_t deserialize_u32(const uint8_t *data);

bool truthiness(Value val);

uint8_t *align_address(uint8_t *ptr);
