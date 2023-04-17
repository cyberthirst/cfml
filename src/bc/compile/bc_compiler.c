//
// Created by Mirek Škrabal on 05.04.2023.
//

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include "../../types.h"
#include "../bc_shared_globals.h"
#include "../../utils.h"
#include "../serialization.h"

//maximum functions that can be declared
#define MAX_FUN_NUM 64
//maximum size of a function body in bytes
#define MAX_FUN_BODY_SZ (1024 * 64)
#define MAX_VARS_NUM 64
#define MAX_GLOBALS_NUM 64
#define FUN_LOCALS_INDEX 2
#define FUN_LENGTH_INDEX 4
#define MAX_ENVS 128
#define MAX_VARS 128
#define MAX_SCOPES 128
#define MAX_FIXUPS_NUM 128


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
    uint16_t scope_cnt;
    Value ret_val;
} Environment;

//we have 1 fixup for each missing symbol
typedef struct {
    //which function need to be fixed
    uint16_t cp_index;
    //at which offset the fixup has to occur
    uint32_t offset;
    //name of variable that was missing at the time of compilation of the given function
    Str name;
    //indicates whether it was already fixed, this flag is used in the final_fixup()
    bool fixed;
} Fixup;


//each function has its own all_fixups
typedef struct {
    //array of all_fixups
    Fixup *fixups;
    //number of all_fixups
    uint32_t cnt;
} Fixups;

typedef struct {
    uint8_t *fun;
    //size of the function in bytes
    uint32_t sz;
    //size of the function body in bytes
    uint32_t body_sz;
    uint32_t body_offset;
    //max number of local variables
    //we use the term "max" because for example after leaving a scope the vars cease to exist, but we
    //still need to be able to store them when we are in the scope
    uint16_t locals;
    //index of the current variable in the vars array
    uint16_t current_var_i;
    //scopes
    Environment *env;
    Fixups fun_fixups;
} Fun;

typedef struct {
    //index of the current function
    int current;
    size_t total_funs;
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
Fixups *all_fixups;
size_t all_fixups_i = 0;
//-------------------------GLOBALS-END-------------------------


void compiler_construct() {
    const_pool = malloc(sizeof(uint8_t) * MAX_CONST_POOL_SZ);
    const_pool_map  = malloc(sizeof(void*) * (MAX_CONST_POOL_SZ + 1));
    const_pool_map[cpm_free_i] = const_pool;
    cfuns = malloc(sizeof(CompiledFuns));
    cfuns->funs = malloc(sizeof(Fun *) * MAX_FUN_NUM);
    all_fixups = malloc(sizeof(Fixups) * MAX_FUN_NUM);
    //we alloc function and there we increment current
    //we want the entry-point to be stored at index 0
    //initializing this to -1 is the easiest way how to achieve this
    cfuns->current = -1;
    globals.indexes = malloc(sizeof(Value) * MAX_GLOBALS_NUM);
    globals.count = 0;
}

//in case of action bc_compile we want to deconstruct everything
//in case of action run we don't..
void compiler_deconstruct(bool deconstruct_all) {
    free(cfuns->funs);
    free(cfuns);
    for (size_t i = 0; i < all_fixups_i; ++i) {
        free(all_fixups[i].fixups);
    }
    free(all_fixups);
    if (deconstruct_all){
        free(const_pool_map);
        free(const_pool);
        free(globals.indexes);
    }
}

void bump_const_pool_free(size_t sz){
    const_pool_map[cpm_free_i + 1] = align_address(const_pool_map[cpm_free_i] + sz);
    if (const_pool_map[cpm_free_i + 1] >= (uint8_t *)const_pool + MAX_CONST_POOL_SZ){
        printf("const pool overflow\n");
        exit(1);
    }
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

void const_pool_insert_class(uint16_t count){
    Bc_Class *pcls = (Bc_Class *)const_pool_map[cpm_free_i];
    pcls->kind = VK_CLASS;
    pcls->count = count;
    bump_const_pool_free(sizeof(Bc_Class) + sizeof(uint16_t) * count);
}

void fun_insert_bytecode(const void *src, size_t sz) {
    Fun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + fun->body_offset + fun->body_sz, src, sz);
    fun->body_sz += sz;
    fun->sz += sz;
}

void fun_insert_metadata(const void *src, size_t sz) {
    Fun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + fun->sz, src, sz);
    fun->sz += sz;
}

void fun_fixup_bytecode(const void *src, size_t sz, uint32_t offset) {
    Fun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + offset, src, sz);
}

void fun_insert_length(uint32_t length) {
    Fun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + FUN_LENGTH_INDEX, &length, sizeof(length));
}

void fun_insert_locals(uint16_t locals) {
    Fun *fun = cfuns->funs[cfuns->current];
    memcpy(fun->fun + FUN_LOCALS_INDEX, &locals, sizeof(locals));
}

//copies the bytecode of the function at the given function index to the const pool
void const_pool_insert_fun(size_t index) {
    assert(index <= cfuns->current);
    Fun *fun = cfuns->funs[index];
    memcpy(const_pool_map[cpm_free_i], fun->fun, fun->sz);
    bump_const_pool_free(fun->sz);
}

void enter_block() {
    Fun *fun = cfuns->funs[cfuns->current];
    fun->env->scope_cnt++;
    fun->env->scopes[fun->env->scope_cnt].var_cnt = 0;
}

void leave_block() {
    Fun *fun = cfuns->funs[cfuns->current];
    //reset the var_i to the last var_i of the previous scope
    //doing this because we want to reuse the indexes of the vars in the previous scope
    fun->current_var_i -=  fun->env->scopes[fun->env->scope_cnt].var_cnt;
    //reset the var_cnt for the scope
    fun->env->scopes[fun->env->scope_cnt].var_cnt = 0;
    //leaving block -> decrement scope_cnt
    fun->env->scope_cnt--;
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

void add_global(uint16_t index) {
    if (globals.count == MAX_GLOBALS_NUM){
        printf("Error: too many global variables.\n");
        exit(1);
    }
    globals.indexes[globals.count++] = index;
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

void add_name_to_scope(Str name, uint16_t index, bool is_this) {
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
        if (cfuns->current == 0 && scope_cnt == 0 && !is_this) {
            //the names of the global variables are stored in the const pool
            //we access them dynamically by finding the name in the const pool and then subsequently search
            //for the name in the globals values
            add_global(index);
        }
    }
}

void fun_alloc(uint8_t paramc, AstFunction *af) {
    Fun *fun = calloc(1, sizeof(Fun));
    fun->fun = calloc(1, MAX_FUN_BODY_SZ);
    fun->env = calloc(1, sizeof(Environment));
    fun->fun_fixups.fixups = calloc(MAX_FIXUPS_NUM, sizeof(Fixup));
    //cfuns->current is initialized in the init function to -1, so the entry-point is stored at index 0
    cfuns->current++;
    cfuns->total_funs++;
    cfuns->funs[cfuns->current] = fun;
    //at the 0th index is the "this" variable
    add_name_to_scope(STR("this"), 0, true);


    //for the entry point function af is NULL
    if (af != NULL) {
        for (size_t i = 0; i < af->parameter_cnt; ++i) {
            //add the parameters as local variables
            //curent_var_i is  for i == 0 set to 1, so we don't overwrite the "this" variable
            add_name_to_scope(af->parameters[i], fun->current_var_i, false);
        }
    }
    //insert the function opcode
    fun_insert_metadata(&TAG_FUNCTION, sizeof(TAG_FUNCTION));
    //insert the number of arguments
    //increase paramc by 1 because of the first hidden param "this"
    paramc += 1;
    fun_insert_metadata(&paramc, sizeof(paramc));
    //skip locals and length
    // - locals: the number of local variables defined in this function, represented by a 16b unsigned integer
    //   (this number  does not include the number of parameters nor the hidden this parameter).
    // - length: the total length of the bytecode vector (the total count of bytes in the function’s body),
    //   4B unsigned integer
    fun->sz += sizeof(uint16_t) + sizeof(uint32_t);
    fun->body_offset = fun->sz;
}

void fun_epilogue() {
    Fun *fun = cfuns->funs[cfuns->current];
    //insert the return opcode, every func, even the entry point must end with a return
    fun_insert_bytecode(&BC_OP_RETURN, sizeof(BC_OP_RETURN));
    fun_insert_locals(fun->locals);
    fun_insert_length(fun->body_sz);
    //copy the top-level function to the const_pool
    //printf("%.*s", (int)fun.len, str.str);
    const_pool_insert_fun(cfuns->current);
    if (cfuns->current > 0)
        cfuns->current--;
    //update the const pool index in the all_fixups, we now know the index of the currently compiled function
    for (size_t i = 0; i < fun->fun_fixups.cnt; ++i) {
        //we need to subtract 1 because the const pool index just got incremented in the const_pool_insert_fun
        fun->fun_fixups.fixups[i].cp_index = cpm_free_i - 1;
    }
    assert(all_fixups_i + fun->fun_fixups.cnt < MAX_FIXUPS_NUM);
    //copy the all_fixups to the global variable all_fixups
    if (fun->fun_fixups.cnt > 0) {
        all_fixups[all_fixups_i].fixups = fun->fun_fixups.fixups;
        all_fixups[all_fixups_i].cnt = fun->fun_fixups.cnt;
        all_fixups_i++;
    }
    else {
        free(fun->fun_fixups.fixups);
    }
    free(fun->fun);
    free(fun->env);
    free(fun);
}


//this function gets called when we define new global variable
//the global variable is potentially missing in some of the already compiled functions
//we will go through the all_fixups compare the name and fixup the index if match is found
void fixup(Str name, uint16_t fixup_index, bool final) {
    for (size_t i = 0; i < all_fixups_i; ++i) {
        for (size_t j = 0; j < all_fixups[i].cnt; ++j) {
            //TODO: would be nice to remove the fixed indexes.. (maybe linked list?)
            if (str_cmp(name, all_fixups[i].fixups[j].name) == 0) {
                //we found the name in the all_fixups, so we need to fixup the index
                uint16_t fun_index = all_fixups[i].fixups[j].cp_index;
                uint8_t *fun_bc = const_pool_map[fun_index];
                uint16_t fixup_offset = all_fixups[i].fixups[j].offset;
                fun_bc[fixup_offset] = (uint8_t)(fixup_index & 0x00FF);
                fun_bc[fixup_offset + 1] = (uint8_t)((fixup_index & 0xFF00) >> 8);
                all_fixups[i].fixups[j].fixed = true;
            }
        }
    }
    //current function doesn't yet have the index in the const pool, thus we need to handle it differently
    //it can happen that the global variable that is yet missing is defined in this function
    //therefore we need to check it too
    if (!final) {
        Fun *fun = cfuns->funs[cfuns->current];
        for (size_t i = 0; i < fun->fun_fixups.cnt; ++i) {
            if (str_cmp(name, fun->fun_fixups.fixups[i].name) == 0) {
                //we found the name in the all_fixups, so we need to fixup the index
                uint8_t *fun_bc = fun->fun;
                uint16_t fixup_offset = fun->fun_fixups.fixups[i].offset;
                fun_bc[fixup_offset] = (uint8_t)(fixup_index & 0x00FF);
                fun_bc[fixup_offset + 1] = (uint8_t)((fixup_index & 0xFF00) >> 8);
                fun->fun_fixups.fixups[i].fixed = true;
            }
        }
    }
}

//some missing indexes might not be fixed (this can happen if the global variable is defined
// only by using = and not let) those indexes will be fixed in the final_fixup
void final_fixup() {
    //go through all the fixups
    for (size_t i = 0; i < all_fixups_i; ++i) {
        //go through all the fixups in the function i
        for (size_t j = 0; j < all_fixups[i].cnt; ++j) {
            //those indexes that are not yet fixed will have cp_index == 0
            if (all_fixups[i].fixups[j].fixed == false) {
                //we need to insert the missing name to the const_pool
                //so the current cpm_free_i will be the index of the name in the const_pool
                //printf("fixing up: %.*s with index: %d\n", (int)all_fixups[i].fixups[j].name.len, all_fixups[i].fixups[j].name.str, cpm_free_i);
                fixup(all_fixups[i].fixups[j].name, cpm_free_i, true);
                //insert the index to globals
                //ie here we finally define the global variable
                add_global(cpm_free_i);
                const_pool_insert_str(all_fixups[i].fixups[j].name);
                //printf("fixup at indexes: i: %d, j: %d of the name: %.*s\n", (int)i, (int)j, (int)all_fixups[i].fixups[j].name.len, all_fixups[i].fixups[j].name.str);
            }
        }
    }
}

void insert_to_fun_fixups(Str name) {
    Fun *fun = cfuns->funs[cfuns->current];
    fun->fun_fixups.fixups[fun->fun_fixups.cnt].name = name;
    fun->fun_fixups.fixups[fun->fun_fixups.cnt].offset = fun->sz;
    fun->fun_fixups.cnt++;
    //we don't know the index in the const pool in which the current function will be stored
    //thus we leave cp_index unset
}

void insert_null() {
    fun_insert_bytecode(&BC_OP_CONSTANT, sizeof(BC_OP_CONSTANT));
    fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
    const_pool_insert_null();
}

void insert_int(i32 value) {
    fun_insert_bytecode(&BC_OP_CONSTANT, sizeof(BC_OP_CONSTANT));
    fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
    const_pool_insert_int(value);
}

void compile(Ast *ast) {
    switch(ast->kind) {
        case AST_NULL: {
            //printf("const null\n");
            insert_null();
            return;
        }
        case AST_BOOLEAN: {
            AstBoolean *ab = (AstBoolean *)ast;
            //printf("const bool\n");
            fun_insert_bytecode(&BC_OP_CONSTANT, sizeof(BC_OP_CONSTANT));
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            const_pool_insert_bool(ab->value);
            return;
        }
        case AST_INTEGER: {
            AstInteger *ai = (AstInteger *)ast;
            //printf("const int\n");
            insert_int(ai->value);
            return;
        }
        case AST_ARRAY: {
            AstArray *aa = (AstArray *)ast;
            //static initialization
            switch (aa->initializer->kind) {
                case AST_NULL:
                case AST_BOOLEAN:
                case AST_INTEGER:
                case AST_VARIABLE_ACCESS:
                    compile(aa->size);
                    compile( aa->initializer);
                    fun_insert_bytecode(&BC_OP_ARRAY, sizeof(BC_OP_ARRAY));
                    return;
                default:;
            }

            //dynamic initialization aka sea of PAIN
            //we need to create a new dummy array and then iteratively fill it with the values of the initializer

            //create local helpers to construct the array
            //TODO unsafe: we skip certain important checks - see add_name_to_scope
            //             we also don't free the local here
            Fun *fun = cfuns->funs[cfuns->current];
            //we need to create 3 helper locals, those are their indexes in the locals array
            uint16_t array_i = fun->current_var_i++;
            //the iterator variable of the comprehension loop
            uint16_t iterator_i = fun->current_var_i++;
            //the size of the iterator, used for the loop condition
            uint16_t size_i = fun->current_var_i++;
            //additionally we need increment and compare methods for the iterator
            if (fun->current_var_i > fun->locals) {
                fun->locals = fun->current_var_i;
            }
            uint16_t inc_cp = cpm_free_i;
            const_pool_insert_str(STR("+"));
            uint16_t cmp_cp = cpm_free_i;
            const_pool_insert_str(STR("<"));
            uint16_t set_cp = cpm_free_i;
            const_pool_insert_str(STR("set"));

            //initialize the helpers
            compile(aa->size);
            fun_insert_bytecode(&BC_OP_SET_LOCAL, sizeof(BC_OP_SET_LOCAL));
            fun_insert_bytecode(&size_i, sizeof(size_i));
            fun_insert_bytecode(&BC_OP_DROP, sizeof(BC_OP_DROP));
            insert_int(0);
            fun_insert_bytecode(&BC_OP_SET_LOCAL, sizeof(BC_OP_SET_LOCAL));
            fun_insert_bytecode(&iterator_i, sizeof(iterator_i));
            fun_insert_bytecode(&BC_OP_DROP, sizeof(BC_OP_DROP));
            compile(aa->size);
            insert_null();
            fun_insert_bytecode(&BC_OP_ARRAY, sizeof(BC_OP_ARRAY));
            fun_insert_bytecode(&BC_OP_SET_LOCAL, sizeof(BC_OP_SET_LOCAL));
            fun_insert_bytecode(&array_i, sizeof(array_i));
            fun_insert_bytecode(&BC_OP_DROP, sizeof(BC_OP_DROP));

            //the loop:
            // while iterator < size
            //   array[iterator] = initializer
            //will reuse the existing loop code

            //the condition: call the < method on the iterator
            uint32_t condition_offset = cfuns->funs[cfuns->current]->sz;
            fun_insert_bytecode(&BC_OP_GET_LOCAL, sizeof(BC_OP_GET_LOCAL));
            fun_insert_bytecode(&iterator_i, sizeof(iterator_i));
            fun_insert_bytecode(&BC_OP_GET_LOCAL, sizeof(BC_OP_GET_LOCAL));
            fun_insert_bytecode(&size_i, sizeof(size_i));
            fun_insert_bytecode(&BC_OP_CALL_METHOD, sizeof(BC_OP_CALL_METHOD));
            fun_insert_bytecode(&cmp_cp, sizeof(cmp_cp));
            uint8_t argc = 2;
            fun_insert_bytecode(&argc, sizeof(argc));

            //if the condition is true, jump to the body
            fun_insert_bytecode(&BC_OP_BRANCH, sizeof(BC_OP_BRANCH));
            int16_t offset = 3;
            //branch past the jump instruction and it's offset
            fun_insert_bytecode(&offset, sizeof(offset));

            //jump to the end of the loop
            fun_insert_bytecode(&BC_OP_JUMP, sizeof(BC_OP_JUMP));
            uint32_t jump_to_after_body_offset = cfuns->funs[cfuns->current]->sz;
            //this offset is temporary, we will fix it later
            fun_insert_bytecode(&offset, sizeof(offset));
            //will be used to calculate body size, which is needed to calculate the jump_after_body offset
            uint32_t before_body_offset = cfuns->funs[cfuns->current]->sz;

            //compile the body
            // 1. compile the initializer
            // 2. set the array element at the iterator index to the initializer
            // 3. increment the iterator
            // 4. jump to the condition

            //retrieve the array and the iterator, those locals will be used in the method call to "set"
            //array: on this we call the "set"
            fun_insert_bytecode(&BC_OP_GET_LOCAL, sizeof(BC_OP_GET_LOCAL));
            fun_insert_bytecode(&array_i, sizeof(array_i));
            //iterator: this is the index argument to "set"
            fun_insert_bytecode(&BC_OP_GET_LOCAL, sizeof(BC_OP_GET_LOCAL));
            fun_insert_bytecode(&iterator_i, sizeof(iterator_i));

            //value: this is the value argument to "set"
            enter_block();
            compile(aa->initializer);
            leave_block();

            //call the "set" method on the array
            fun_insert_bytecode(&BC_OP_CALL_METHOD, sizeof(BC_OP_CALL_METHOD));
            fun_insert_bytecode(&set_cp, sizeof(set_cp));
            argc = 3;
            fun_insert_bytecode(&argc, sizeof(argc));
            fun_insert_bytecode(&BC_OP_DROP, sizeof(BC_OP_DROP));

            //increment the iterator
            fun_insert_bytecode(&BC_OP_GET_LOCAL, sizeof(BC_OP_GET_LOCAL));
            fun_insert_bytecode(&iterator_i, sizeof(iterator_i));
            insert_int(1);
            fun_insert_bytecode(&BC_OP_CALL_METHOD, sizeof(BC_OP_CALL_METHOD));
            fun_insert_bytecode(&inc_cp, sizeof(inc_cp));
            argc = 2;
            fun_insert_bytecode(&argc, sizeof(argc));
            //set the result of the increment as the new iterator value
            fun_insert_bytecode(&BC_OP_SET_LOCAL, sizeof(BC_OP_SET_LOCAL));
            fun_insert_bytecode(&iterator_i, sizeof(iterator_i));
            fun_insert_bytecode(&BC_OP_DROP, sizeof(BC_OP_DROP));

            //jump to the condition
            //jump is relative to the next opcode, so we must subtract the size of jump, which is 3B
            offset = (condition_offset - cfuns->funs[cfuns->current]->sz) - 3;// - 1;
            fun_insert_bytecode(&BC_OP_JUMP, sizeof(BC_OP_JUMP));
            fun_insert_bytecode(&offset, sizeof(offset));
            //fix the after body offset for the after body jump
            uint32_t  after_body_offset = cfuns->funs[cfuns->current]->sz;
            offset = after_body_offset - before_body_offset;
            fun_fixup_bytecode(&offset, sizeof(offset), jump_to_after_body_offset);

            //almost done, just need to push the resulting array to the stack
            fun_insert_bytecode(&BC_OP_GET_LOCAL, sizeof(BC_OP_GET_LOCAL));
            fun_insert_bytecode(&array_i, sizeof(array_i));
            return;
        }
        case AST_OBJECT: {
            AstObject *ao = (AstObject *)ast;
            compile(ao->extends);
            uint16_t cnt = ao->member_cnt;
            //we allocate the class first and then fill the members
            //thus we must save the index, because we need to insert the object opcode
            uint16_t class_index = cpm_free_i;
            Bc_Class *cls = (Bc_Class *)const_pool_map[cpm_free_i];
            const_pool_insert_class(cnt);
            for (uint16_t i = 0; i < cnt; ++i) {
                AstDefinition *member = (AstDefinition *)ao->members[i];
                enter_block();
                compile(member->value);
                //members are const_pool indexes to strings which represent the name of the member
                cls->members[i] = cpm_free_i;
                const_pool_insert_str(member->name);
                leave_block();
            }
            fun_insert_bytecode(&BC_OP_OBJECT, sizeof(BC_OP_OBJECT));
            fun_insert_bytecode(&class_index, sizeof(cpm_free_i));
            return;
        }
        case AST_FUNCTION: {
            AstFunction *af = (AstFunction *)ast;
            fun_alloc(af->parameter_cnt, af);
            compile(af->body);
            fun_epilogue();
            //important to insert the bytecode after the epilogue, because otherwise we would insert the bytecode
            //into the body of the function which we're currently compiling
            fun_insert_bytecode(&BC_OP_CONSTANT, sizeof(BC_OP_CONSTANT));
            //we did fun_epilogue which copies the function to the constant pool and also increment the free index
            //thus we need to decrement it by  1
            //we also need to insert "after" the epilogue, otherwise we would encounter the problem described above
            uint16_t tmp_cpm_free_i = cpm_free_i - 1;
            fun_insert_bytecode(&tmp_cpm_free_i, sizeof(cpm_free_i));
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
                //printf("setting global on index: %d\n", cpm_free_i);
                //set the index to the first free slot in the constant pool
                //in the compile step we created a constant and stored it in the constant pool, and pushed the object
                //on the stack
                //here we create a new name at the specified index, which will be asoociated with the value
                fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
                //global variable can fixup some missing indexes in the already compiled functions
                //the problem is that we don't know beforehand at which index in the constant pool the function will
                //end up, however upon definition of that var we already know that index - and it is cpm_free_i
                fixup(ad->name, cpm_free_i, false);
                //the global variables are accessed via the index to const pool, where their name is stored
                add_name_to_scope(ad->name, cpm_free_i, false);
                const_pool_insert_str(ad->name);
            }
            else {//add to local environment
                uint16_t cvar = cfuns->funs[cfuns->current]->current_var_i;
                fun_insert_bytecode(&BC_OP_SET_LOCAL, sizeof(BC_OP_SET_LOCAL));
                fun_insert_bytecode(&cvar, sizeof(cvar));
                add_name_to_scope(ad->name, cvar, false);
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
                //we still need to insert the bytecode
                //at the same time the missing index must be an index of a global variable
                uint16_t tmp = 0;
                fun_insert_bytecode(&BC_OP_GET_GLOBAL, sizeof(BC_OP_GET_GLOBAL));
                //insert_to_fun_fixups uses the fn->sz as the offset
                //the offset should be such that the index to const pool is later fixed-up
                insert_to_fun_fixups(ava->name);
                fun_insert_bytecode(&tmp, sizeof(tmp));
            }
            else if (is_global) {
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
            AstVariableAssignment *(ava) = (AstVariableAssignment *) ast;
            compile(ava->value);
            //TODO: same as AST_VARIABLE_ACCESS maybe refactor
            bool is_global = false;
            Variable *var = get_name_ptr(ava->name, &is_global);
            if (var == NULL) {
                //we still need to insert the bytecode
                //at the same time the missing index must be an index of a global variable
                uint16_t tmp = 0;
                fun_insert_bytecode(&BC_OP_SET_GLOBAL, sizeof(BC_OP_SET_GLOBAL));
                //insert_to_fun_fixups uses the fn->sz as the offset
                //the offset should be such that the index to const pool is later fixed-up
                insert_to_fun_fixups(ava->name);
                fun_insert_bytecode(&tmp, sizeof(tmp));
            }
            else if (is_global) {
                fun_insert_bytecode(&BC_OP_SET_GLOBAL, sizeof(BC_OP_SET_GLOBAL));

                //printf("setting global on index: %d", cpm_free_i);
                fun_insert_bytecode(&var->index, sizeof(var->index));
            }
            else {
                fun_insert_bytecode(&BC_OP_SET_LOCAL, sizeof(BC_OP_SET_LOCAL));
                fun_insert_bytecode(&var->index, sizeof(var->index));
            }
            return;
        }
        case AST_INDEX_ACCESS: {
            AstIndexAccess *aia = (AstIndexAccess *) ast;
            uint8_t argc = 2;
            compile(aia->object);
            compile(aia->index);
            //index access is realized via a method call to the built-in function "get"
            fun_insert_bytecode(&BC_OP_CALL_METHOD, sizeof(BC_OP_CALL_METHOD));
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            fun_insert_bytecode(&argc, sizeof(argc));
            const_pool_insert_str(STR("get"));
            return;
        }
        case AST_INDEX_ASSIGNMENT: {
            AstIndexAssignment *aia = (AstIndexAssignment *) ast;
            uint8_t argc = 3;
            compile(aia->object);
            compile(aia->index);
            compile(aia->value);
            //index access is realized via a method call to the built-in function "get"
            fun_insert_bytecode(&BC_OP_CALL_METHOD, sizeof(BC_OP_CALL_METHOD));
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            fun_insert_bytecode(&argc, sizeof(argc));
            const_pool_insert_str(STR("set"));
            return;
        }
        case AST_FIELD_ACCESS: {
            AstFieldAccess *afa = (AstFieldAccess *) ast;
            compile(afa->object);
            fun_insert_bytecode(&BC_OP_GET_FIELD, sizeof(BC_OP_GET_FIELD));
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            //TODO: each time inserting a new string...
            const_pool_insert_str(afa->field);
            return;
        }
        case AST_FIELD_ASSIGNMENT: {
            AstFieldAssignment *afa = (AstFieldAssignment *) ast;
            compile(afa->object);
            compile(afa->value);
            fun_insert_bytecode(&BC_OP_SET_FIELD, sizeof(BC_OP_SET_FIELD));
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            const_pool_insert_str(afa->field);
            return;
        }
        case AST_FUNCTION_CALL: {
            AstFunctionCall *afc = (AstFunctionCall *) ast;
            /*AstVariableAccess *ava = (AstVariableAccess *) afc->function;
            Variable *var = get_name_ptr(ava->name, NULL);
            if (var == NULL) {
                compile()
            }*/
            compile(afc->function);
            for (size_t i = 0; i < afc->argument_cnt; i++) {
                compile(afc->arguments[i]);
            }
            fun_insert_bytecode(&BC_OP_CALL_FUNCTION, sizeof(BC_OP_CALL_FUNCTION));
            uint8_t argument_cnt = afc->argument_cnt;
            fun_insert_bytecode(&argument_cnt, sizeof(argument_cnt));
            return;
        }
        case AST_METHOD_CALL: {
            AstMethodCall *amc = (AstMethodCall *) ast;
            uint8_t argc = amc->argument_cnt + 1;
            compile(amc->object);
            for (size_t i = 0; i < amc->argument_cnt; i++) {
                compile(amc->arguments[i]);
            }
            fun_insert_bytecode(&BC_OP_CALL_METHOD, sizeof(BC_OP_CALL_METHOD));
            fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
            fun_insert_bytecode(&argc, sizeof(argc));
            const_pool_insert_str(amc->name);
            return;
        }
        case AST_CONDITIONAL: {
            AstConditional *ac = (AstConditional *) ast;
            compile(ac->condition);
            fun_insert_bytecode(&BC_OP_BRANCH, sizeof(BC_OP_BRANCH));
            //the 2B offset that is associated with the jmp instructions
            //we want to branch past the jump instruction and it's offset -> 3 bytes
            int16_t offset = 3;
            fun_insert_bytecode(&offset, sizeof(offset));
            //we want to jump to the end of the consequent
            fun_insert_bytecode(&BC_OP_JUMP, sizeof(BC_OP_JUMP));
            //we need to store the current position so we can fixup the jump instruction
            uint32_t to_else_jump_offset = cfuns->funs[cfuns->current]->sz;
            //we store offset, but thats only some temporary garbage
            fun_insert_bytecode(&offset, sizeof(offset));
            enter_block();
            compile(ac->consequent);
            leave_block();
            //we add 1 because we want to jump past the jump instruction
            //the jmp instruction is 3B, also the offset is relative to next instruction
            //we stored the sz before storing the offset, thus we only add 1, not 3
            offset = cfuns->funs[cfuns->current]->sz - to_else_jump_offset + 1;
            fun_fixup_bytecode(&offset, sizeof(offset), to_else_jump_offset);
            fun_insert_bytecode(&BC_OP_JUMP, sizeof(BC_OP_JUMP));
            uint32_t to_end_jump_offset = cfuns->funs[cfuns->current]->sz;
            fun_insert_bytecode(&offset, sizeof(offset));
            enter_block();
            compile(ac->alternative);
            leave_block();
            //we subtract 2 because the offset is relative to the next instruction
            //and again we stored the sz before storing the offset
            offset = cfuns->funs[cfuns->current]->sz - to_end_jump_offset - 2;
            fun_fixup_bytecode(&offset, sizeof(offset), to_end_jump_offset);
            return;
        }
        case AST_LOOP: {
            AstLoop *al = (AstLoop *) ast;
            //if the loop runs 0 times, it must eval to null
            insert_null();
            uint32_t condition_offset = cfuns->funs[cfuns->current]->sz;
            compile(al->condition);
            //we have the following bytecode:
            //  null
            //  condition
            //  branch 3
            //  jump to after_body <- needs fixup
            //  body
            //  jump to condition
            //  after body
            fun_insert_bytecode(&BC_OP_BRANCH, sizeof(BC_OP_BRANCH));
            int16_t offset = 3;
            //branch past the jump instruction and it's offset
            fun_insert_bytecode(&offset, sizeof(offset));
            fun_insert_bytecode(&BC_OP_JUMP, sizeof(BC_OP_JUMP));
            uint32_t jump_to_after_body_offset = cfuns->funs[cfuns->current]->sz;
            //this offset is temporary, we will fix it later
            fun_insert_bytecode(&offset, sizeof(offset));
            //will be used to calculate body size, which is needed to calculate the jump_after_body offset
            uint32_t before_body_offset = cfuns->funs[cfuns->current]->sz;
            //first iter of the loop will drop the null
            fun_insert_bytecode(&BC_OP_DROP, sizeof(BC_OP_DROP));
            enter_block();
            compile(al->body);
            leave_block();
            //jump to the condition
            //jump is relative to the next opcode, so we must subtract the size of jump, which is 3B
            offset = (condition_offset - cfuns->funs[cfuns->current]->sz) - 3;// - 1;
            fun_insert_bytecode(&BC_OP_JUMP, sizeof(BC_OP_JUMP));
            fun_insert_bytecode(&offset, sizeof(offset));
            //fix the after body offset for the after body jump
            uint32_t  after_body_offset = cfuns->funs[cfuns->current]->sz;
            offset = after_body_offset - before_body_offset;
            fun_fixup_bytecode(&offset, sizeof(offset), jump_to_after_body_offset);
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
            fun_alloc(0, NULL);
            //we have to treat the entry point specially, the fun_alloc function increments cfuns->current
            //which is the correct thing to do, however for the entry-point we want to start at index 0
            //cfuns->current = 0;

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
            //fixup the missing global variables
            final_fixup();
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

void bc_output(){
    serialize_header();
    serialize_constant_pool(const_pool_count);
    serialize_globals(&globals);
    serialize_entry_point(entry_point);
}

void bc_compile(Ast *ast, bool output) {
    compiler_construct();
    compile(ast);
    if (output) {
        bc_output();
    }
    compiler_deconstruct(output);
}
