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
#define MAX_VARS_NUM 256
#define MAX_GLOBALS_NUM 1024
#define FUN_LOCALS_INDEX 2
#define FUN_LENGTH_INDEX 4
#define MAX_ENVS 256
#define MAX_VARS 256
#define MAX_SCOPES 256


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
    Str name;
    uint16_t index;
} Variable;

typedef struct {
    //stack of variables
    Variable vars[MAX_VARS];
    size_t var_cnt;
} Scope;

//represents environment of a function
//also used for global environment
typedef struct {
    //stack of scopes
    Scope scopes[MAX_SCOPES];
    size_t scope_cnt;
    Value ret_val;
} Environment;

typedef struct {
    uint8_t *fun;
    //size of the function in bytes
    uint32_t sz;
    //max number of local variables
    //we use the term "max" because for example after leaving a scope the vars cease to exist, but we
    //still need to be able to store them when we are in the scope
    uint8_t locals;
    //index of the current variable in the vars array
    uint8_t current_var_i;
    //scopes
    Environment *env;
} Fun;

typedef struct {
    //index of the current function
    size_t current;
    Fun **funs;
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
    cfuns->funs = malloc(sizeof(Fun *) * MAX_FUN_NUM);
    cfuns->current = 0;
    globals.indexes = malloc(sizeof(Value) * MAX_GLOBALS_NUM);
    globals.count = 0;
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
    Fun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + fun->sz, src, sz);
    fun->sz += sz;
}

void fun_insert_length(uint32_t length) {
    Fun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + FUN_LENGTH_INDEX, &length, sizeof(length));
}

void fun_insert_locals(uint16_t locals) {
    Fun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + FUN_LOCALS_INDEX, &locals, sizeof(locals));
}

void fun_alloc(uint8_t argc) {
    Fun *fun = malloc(sizeof(Fun));
    fun->fun = malloc(MAX_FUN_BODY_SZ);
    fun->sz = 0;
    fun->locals = 0;
    //at the 0th index is the "this" variable
    fun->current_var_i = 1;
    fun->env = malloc(sizeof(Environment));
    cfuns->funs[cfuns->current] = fun;
    //insert the function opcode
    fun_insert_bytecode(&TAG_FUNCTION, sizeof(TAG_FUNCTION));
    //insert the number of arguments
    fun_insert_bytecode(&argc, sizeof(argc));
    //skip locals and length
    // - locals: the number of local variables defined in this function, represented by a 16b unsigned integer
    //   (this number  does not include the number of parameters nor the hidden this parameter).
    // - length: the total length of the bytecode vector (the total count of bytes in the function’s body),
    //   4B unsigned integer
    fun->sz += sizeof(uint16_t) + sizeof(uint32_t);
}

//copies the bytecode of the function at the given function index to the const pool
void cpy_fun_to_cp(size_t index) {
    assert(index <= cfuns->current);
    Fun *fun = cfuns->funs[index];
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

void enter_block() {
    Fun *fun = cfuns->funs[cfuns->current];
    ++fun->env->scope_cnt;
}

void leave_block() {
    Fun *fun = cfuns->funs[cfuns->current];
    //reset the var_i to the last var_i of the previous scope
    //doing this because we want to reuse the indexes of the vars in the previous scope
    fun->current_var_i -=  fun->env->scopes[fun->env->scope_cnt].var_cnt;
    //reset the var_cnt for the scope
    fun->env->scopes[fun->env->scope_cnt].var_cnt = 0;
    //leaving block -> decrement scope_cnt
    --fun->env->scope_cnt;
}

Variable *find_name_in_env(Str name, Environment *env, size_t scope_cnt, bool *found_top_level) {
    //we don't want to subtract 1 like in the case of vars because by default there is the global scope on index 0
    //search from all scopes starting on top
    for (int i = scope_cnt; i >= 0; i--) {
        //search all vars in the scope starting from the latest added
        int j = env->scopes[i].var_cnt;
        //if there are no vars the whole for loop will be skipped
        //if there are > 0 vars we need to decrement j to properly index the var in the vars array
        if (j > 0) j--;
        else continue;
        for (;j >= 0; j--) {
            if (str_eq(env->scopes[i].vars[j].name, name) == true) {
                if (i == 0)
                    *found_top_level = true;
                return &env->scopes[i].vars[j];
            }
        }
    }
    return NULL;
}


Variable *get_name_ptr(Str name, bool *is_global) {
    Variable *var;
    Fun *fun = cfuns->funs[cfuns->current];
    bool found_top_level = false;
    //try to find the symbol in the current environment
    //each function call creates a new environment
    var = find_name_in_env(name, fun->env, fun->env->scope_cnt, &found_top_level);
    if (var != NULL) {
        if (cfuns->current == 0 && found_top_level == true)
            *is_global = true;
        return var;
    }
    //if the current environment doesn't have the symbol try to find it in the global environment
    //in the top-level scope (searching for global variables)
    var = find_name_in_env(name, cfuns->funs[0]->env, 0, &found_top_level);
    if (var != NULL) {
        *is_global = true;
        return var;
    }
    return NULL;
}


void add_name_to_scope(Str name, uint16_t index) {
    Fun *fun = cfuns->funs[cfuns->current];
    size_t scope_cnt = fun->env->scope_cnt;
    size_t var_cnt = fun->env->scopes[scope_cnt].var_cnt;

    if (var_cnt == MAX_VARS) {
        printf("Too many variables in local scope\n");
        exit(1);
    }
    else {
        Variable *vars = fun->env->scopes[scope_cnt].vars;
        vars[var_cnt].name = name;
        vars[var_cnt].index = index;
        fun->env->scopes[scope_cnt].var_cnt++;
        fun->current_var_i++;
        //update the number of max locals if needed
        if (fun->current_var_i > fun->locals) {
            fun->locals = fun->current_var_i;
        }
        //update the globals
        if (cfuns->current == 0 && scope_cnt == 0) {
            //the names of the global variables are stored in the const pool
            //we access them dynamically by finding the name in the const pool and then subsequently search
            //for the name in the globals values
            globals.indexes[globals.count] = index;
            globals.count++;
        }
    }
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
            AstDefinition *ad = (AstDefinition *) ast;
            compile(ad->value);
            //we need to add the variable to the current function's environment
            //we need SET_LOCAL and SET_LOCAL opcodes
            //if the definition is in the entry point, we need to add it to the global environment if it's
            //not inside any block
            //if it is inside a block, we need to add it as a local variable

            if (cfuns->current == 0 && cfuns->funs[0]->env->scope_cnt == 0) { //add to global environment
                fun_insert_bytecode(&BC_OP_SET_GLOBAL, sizeof(BC_OP_SET_GLOBAL));
                fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
                //the global variables are accessed via the index to const pool, where their name is stored
                add_name_to_scope(ad->name, cpm_free_i);
                const_pool_insert_str(ad->name);
            }
            else {//add to local environment
                fun_insert_bytecode(&BC_OP_SET_LOCAL, sizeof(BC_OP_SET_LOCAL));
                fun_insert_bytecode(&cfuns->funs[cfuns->current]->current_var_i, sizeof(cfuns->funs[cfuns->current]->current_var_i));
                add_name_to_scope(ad->name, cfuns->funs[cfuns->current]->current_var_i);
            }
            //for some reason the operands are only peeked, not popped
            //fun_insert_bytecode(&BC_OP_DROP, sizeof(BC_OP_DROP));
            return;
        }
        case AST_VARIABLE_ACCESS: {
            AstVariableAccess *ava = (AstVariableAccess *) ast;
            bool is_global = false;
            Variable *var = get_name_ptr(ava->name , &is_global);
            if (var == NULL) {
                printf("Variable not found, exiting.\n");
                exit(1);
            }
            printf("variable at index %d\n", var->index);
            if (is_global) {
                fun_insert_bytecode(&BC_OP_GET_GLOBAL, sizeof(BC_OP_GET_GLOBAL));
                fun_insert_bytecode(&var->index, sizeof(var->index));
            }
            else {
                fun_insert_bytecode(&BC_OP_GET_LOCAL, sizeof(BC_OP_GET_LOCAL));
                fun_insert_bytecode(&var->index, sizeof(var->index));
            }
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
            AstPrint *prnt = (AstPrint *) ast;
            for (size_t i = 0; i < prnt->argument_cnt; i++) {
                compile(prnt->arguments[i]);
            }
            //insert the print opcode
            fun_insert_bytecode(&BC_OP_PRINT, sizeof(BC_OP_PRINT));
            //insert the index of the format string to the const_pool
            //we are inserting the index of the free space in the const_pool
            //that is because in the next step we will actually store the string in the const_pool at the given index
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            //insert the number of arguments
            uint8_t arg_cnt = prnt->argument_cnt;
            fun_insert_bytecode(&arg_cnt, sizeof(arg_cnt));
            const_pool_insert_str(prnt->format);
            //print instruction pushes ptr to null to the stack
            //fun_insert_bytecode(&BC_OP_DROP, sizeof(BC_OP_DROP));
            return;
        }
        case AST_BLOCK: {
            AstBlock *block = (AstBlock *)ast;
            enter_block();
            for (size_t i = 0; i < block->expression_cnt; ++i) {
                compile(block->expressions[i]);
                //drop the result of the expression if it's not the last one, the block will return the last expression
                if (i < block->expression_cnt - 1) {
                    fun_insert_bytecode(&BC_OP_DROP, sizeof(BC_OP_DROP));
                }
            }
            leave_block();
            return;
        }
        case AST_TOP: {
            AstTop *top = (AstTop *)ast;
            //allocate space for the top-level function, entry point has 1 implicit parameter - this, set to null
            fun_alloc(1);
            cfuns->current = 0;

            for (size_t i = 0; i < top->expression_cnt; i++) {
                compile(top->expressions[i]);
                if (i < top->expression_cnt - 1) {
                    fun_insert_bytecode(&BC_OP_DROP, sizeof(BC_OP_DROP));
                }
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