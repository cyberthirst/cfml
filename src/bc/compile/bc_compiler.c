//
// Created by Mirek Škrabal on 05.04.2023.
//

#include <stdio.h>
#include <assert.h>
#include "bc_compiler.h"
#include "../../types.h"
#include "../bc_shared_globals.h"
#include "../../utils.h"
#include "../../utils/hash_map.h"

#define FML_HEADER 0x464D4C0A
//maximum functions that can be declared
#define MAX_FUN_NUM 1024
//maximum size of a function body in bytes
#define MAX_FUN_BODY_SZ (1024 * 1024)
#define FUN_LOCALS_INDEX 2
#define FUN_LENGTH_INDEX 4


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

const uint8_t TAG_INTEGER = 0x00;
const uint8_t TAG_NULL = 0x01;
const uint8_t TAG_STRING = 0x02;
const uint8_t TAG_FUNCTION = 0x3;
const uint8_t TAG_BOOLEAN = 0x04;
const uint8_t TAG_CLASS = 0x05;

typedef struct {
    uint8_t *fun;
    //size of the function in bytes
    uint32_t sz;
    //number of local variables
    uint8_t locals;
}CompiledFun;

typedef struct {
    size_t current;
    CompiledFun **funs;
} CompiledFuns;


//---------------------------GLOBALS---------------------------
//const_pool_map index, indexes the first aligned free space in the const_pool
uint16_t cpm_free_i = 0;
//helper object to create bytecode for function
//is later copied to the const pool
CompiledFuns *cfuns;
//map for the const pool
//key is the value that should be inserted to cp converted to string, value is the index in the cp
//struct hashmap *cpmap;
//-------------------------GLOBALS-END-------------------------



void compiler_init() {
    const_pool = malloc(sizeof(uint8_t) * MAX_CONST_POOL_SZ);
    const_pool_map  = malloc(sizeof(void*) * (MAX_CONST_POOL_SZ + 1));
    const_pool_map[cpm_free_i] = const_pool;
    cfuns = malloc(sizeof(CompiledFuns));
    cfuns->funs = malloc(sizeof(CompiledFun *) * MAX_FUN_NUM);
    cfuns->current = 0;
    //cpmap = hashmap_new(sizeof(Str), 0, 0, 0, user_hash, user_compare, NULL, NULL);
}

void compiler_deconstruct() {
    free(cfuns);
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

//TODO use the hashmap to check if the value is already in the const pool to avoid duplicates
void const_pool_insert_null(){
    Null *pnull = (Null *)const_pool_map[cpm_free_i];
    pnull->kind = VK_NULL;
    bump_const_pool_free(sizeof(Null));
}

void const_pool_insert_int(i32 i){
    Integer *pint = (Integer *)const_pool_map[cpm_free_i];
    pint->kind = VK_INTEGER;
    pint->val = i;
    bump_const_pool_free(sizeof(Integer));
}

void const_pool_insert_bool(bool b){
    Boolean *pbool = (Boolean *)const_pool_map[cpm_free_i];
    pbool->kind = VK_BOOLEAN;
    pbool->val = b;
    bump_const_pool_free(sizeof(Boolean));
}

void fun_insert_bytecode(const void *src, size_t sz) {
    CompiledFun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + fun->sz, src, sz);
    fun->sz += sz;
}

void fun_insert_length(uint32_t length) {
    CompiledFun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + FUN_LENGTH_INDEX, &length, sizeof(length));
}

void fun_insert_locals(uint16_t locals) {
    CompiledFun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + FUN_LOCALS_INDEX, &locals, sizeof(locals));
}

void fun_alloc(uint8_t argc) {
    CompiledFun *fun = malloc(sizeof(CompiledFun));
    fun->fun = malloc(MAX_FUN_BODY_SZ);
    fun->sz = 0;
    fun->locals = 0;
    cfuns->funs[cfuns->current] = fun;
    //insert the function opcode
    fun_insert_bytecode(&TAG_FUNCTION, sizeof(TAG_FUNCTION));
    //insert the number of arguments
    fun_insert_bytecode(&argc, sizeof(argc));
    //skip locals and length
    // - locals: the number of local variables defined in this function, represented by a 16b unsigned integer (this number
    //   does not include the number of parameters nor the hidden this parameter).
    // - length: the total length of the bytecode vector (the total count of bytes in the function’s body), 4B unsigned
    //   integer
    fun->sz += sizeof(uint16_t) + sizeof(uint32_t);
}

//copies the bytecode of the function at the given function index to the const pool
void cpy_fun_to_cp(size_t index) {
    assert(index <= cfuns->current);
    CompiledFun *fun = cfuns->funs[index];
    memcpy(const_pool_map[cpm_free_i], fun->fun, fun->sz);
    bump_const_pool_free(fun->sz);
    free(fun->fun);
}

void fun_epilogue() {
    fun_insert_locals(cfuns->funs[cfuns->current]->locals);
    fun_insert_length(cfuns->funs[cfuns->current]->sz);
    //insert the return opcode, every func, even the entry point must end with a return
    fun_insert_bytecode(&BC_OP_RETURN, sizeof(BC_OP_RETURN));
    //copy the top-level function to the const_pool
    cpy_fun_to_cp(cfuns->current);
    //it's ok to overflow for the entry point (we won't compile anything after finishing the compilation of entry point)
    --cfuns->current;
}

void compile(const Ast *ast) {
    switch(ast->kind) {
        case AST_NULL: {
            fun_insert_bytecode(&BC_OP_CONSTANT, sizeof(BC_OP_CONSTANT));
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            const_pool_insert_null();
            return;
        }
        case AST_BOOLEAN: {
            AstBoolean *ab = (AstBoolean *)ast;
            fun_insert_bytecode(&BC_OP_CONSTANT, sizeof(BC_OP_CONSTANT));
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            const_pool_insert_bool(ab->value);
            return;
        }
        case AST_INTEGER: {
            AstInteger *ai = (AstInteger *)ast;
            fun_insert_bytecode(&BC_OP_CONSTANT, sizeof(BC_OP_CONSTANT));
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            const_pool_insert_int(ai->value);
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

            for (size_t i = 0; i < prnt->argument_cnt; i++) {
                compile(prnt->arguments[i]);
            }
            //insert the print opcode
            fun_insert_bytecode(&BC_OP_PRINT, sizeof(BC_OP_PRINT));
            //insert the index of the format string to the const_pool
            //we are inserting insertingthe index of the free space in the const_pool
            //that is because in the next step we will actually store the string in the const_pool at the given index
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            //insert the number of arguments
            uint8_t arg_cnt = prnt->argument_cnt;
            fun_insert_bytecode(&arg_cnt, sizeof(arg_cnt));
            const_pool_insert_str(prnt->format);
            return;
        }
        case AST_BLOCK: {
            return;
        }
        case AST_TOP: {
            AstTop *top = (AstTop *)ast;
            //allocate space for the top-level function, entry point has 1 implicit parameter - this, set to null
            fun_alloc(1);
            cfuns->current = 0;

            for (size_t i = 0; i < top->expression_cnt; i++) {
                compile(top->expressions[i]);
            }

            fun_epilogue();
            //set the entry point to the top-level function
            assert(cpm_free_i > 0);
            //the entry point is the index of the top-level function in the const_pool
            //we wrote the entry point to the const_pool in the fun_epilogue
            entry_point = cpm_free_i - 1;
            //set the number of items inserted to the const_pool
            const_pool_count = cpm_free_i;
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
    compiler_deconstruct();
}