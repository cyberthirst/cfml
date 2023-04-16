//
// Created by Mirek Škrabal on 16.04.2023.
//

#pragma once

#include "../types.h"

void serialize_header();

void serialize_globals(const Bc_Globals *globals);

void serialize_entry_point(const uint16_t entry_point_index);

void serialize_constant_pool(const size_t pool_size);