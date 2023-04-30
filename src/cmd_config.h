//
// Created by Mirek Škrabal on 30.04.2023.
//

#pragma once

typedef enum { ACTION_AST_INTERPRET, ACTION_BC_INTERPRET, ACTION_RUN, ACTION_BC_COMPILE } ACTION;

typedef struct {
    long long int heap_size;
    char *heap_log_file;
    char *source_file;
    ACTION action;
} Config;

extern Config config;
