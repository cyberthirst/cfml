//
// Created by Mirek Škrabal on 05.04.2023.
//

#include <stdio.h>
#include "bc_compiler.h"
#include "../../types.h"
#include "../bc_shared_globals.h"
#include "../../utils.h"

#define FML_HEADER 0x464D4C0A

const uint8_t BC_OP_DROP = 0x00;
const uint8_t BC_OP_CONSTANT = 0x01;
const uint8_t BC_OP_PRINT = 0x02;
const uint8_t BC_OP_ARRAY = 0x03;
const uint8_t BC_OP_OBJECT = 0x04;
const uint8_t BC_OP_GET_FIELD = 0x05;
const uint8_t BC_OP_SET_FIELD = 0x06;
const uint8_t BC_OP_CALL_METHOD = 0x07;
const uint8_t BC_OP_CALL_FUNCTION = 0x08;
const uint8_t BC_OP_SET_LOCAL = 0x09;
const uint8_t BC_OP_GET_LOCAL = 0x0A;
const uint8_t BC_OP_SET_GLOBAL = 0x0B;
const uint8_t BC_OP_GET_GLOBAL = 0x0C;
const uint8_t BC_OP_BRANCH = 0x0D;
const uint8_t BC_OP_JUMP = 0x0E;
const uint8_t BC_OP_RETURN = 0x0F;


//---------------------------GLOBALS---------------------------
//const_pool_map index, indexes the first aligned free space in the const_pool
uint16_t cpm_free_i = 0;
//helper object to create bytecode for function
//is later copied to the const pool
uint8_t **funcs;
//-------------------------GLOBALS-END-------------------------

void compiler_init() {
    const_pool_map[cpm_free_i] = const_pool;
    const_pool_map  = malloc(sizeof(void*) * (const_pool_count + 1));
}

void output_bc(){

}

void bump_const_pool_free(size_t sz){
    const_pool_map[cpm_free_i + 1] = align_address(const_pool_map[cpm_free_i] + sz);
    ++cpm_free_i;
}

void const_pool_insert_str(Str str){
    Bc_String * pstr = (Bc_String *)const_pool_map[cpm_free_i];
    pstr->kind = VK_STRING;
    pstr->len = str.len;
    memcpy(pstr->value, str.str, str.len);
    bump_const_pool_free(sizeof(Bc_String) + pstr->len);
}


void compile(const Ast *ast) {
    switch(ast->kind) {
        case AST_NULL: {
            return;
        }
        case AST_BOOLEAN: {
            return;
        }
        case AST_INTEGER: {
            return;
        }
        case AST_ARRAY: {
            return;
        }
        case AST_OBJECT: {
            return;
        }
        case AST_FUNCTION: {
            return;
        }
        case AST_DEFINITION: {
            return;
        }
        case AST_VARIABLE_ACCESS: {
            return;
        }
        case AST_VARIABLE_ASSIGNMENT: {
            return;
        }
        case AST_INDEX_ACCESS: {
            return;
        }
        case AST_INDEX_ASSIGNMENT: {
            return;
        }
        case AST_FIELD_ACCESS: {
            return;
        }
        case AST_FIELD_ASSIGNMENT: {
            return;
        }
        case AST_FUNCTION_CALL: {
            return;
        }
        case AST_METHOD_CALL: {
            return;
        }
        case AST_CONDITIONAL: {
            return;
        }
        case AST_LOOP: {
            return;
        }
        case AST_PRINT: {
            /*
             Interpretation: Retrieve a program object from the constant pool at the specified index.
             This program object must be a String and it represents the format string. Pop as many pointers
             to runtime objects from the operand stack as indicated by the number of arguments.

            Interpret the format string according to the formatting specification and print the resulting
            string to standard output. Placeholder sequences (~) shall be filled by the popped arguments such
            that the argument popped last fills the first placeholder, while the argument popped first fills
            the last placeholder. Expect the number of placeholders to equal number of print arguments.

            Write out the formatted output string to standard output.

            Push a pointer to a null object to stack.
             */
            AstPrint *prnt = (AstPrint *) ast;

            const_pool_insert_str(prnt->format);
            Value val[prnt->argument_cnt];
            for (size_t i = 0; i < prnt->argument_cnt; i++) {
                compile(prnt->arguments[i]);
            }
        }
        case AST_BLOCK: {
            return;
        }
        case AST_TOP: {
            AstTop *top = (AstTop *)ast;
            for (size_t i = 0; i < top->expression_cnt; i++) {
                compile(top->expressions[i]);
            }
            return;
        }
        default: {
            // Handle unknown AstKind
            printf("Uknown AST node, exiting.\n");
            exit(1);
        }
    }

}

void bc_compile(const Ast *ast) {
    compiler_init();
    compile(ast);
}