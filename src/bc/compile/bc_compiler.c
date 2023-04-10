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
#define MAX_FIXUPS_NUM 64


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

//we have 1 fixup for each missing symbol
typedef struct {
    //which function need to be fixed
    uint16_t cp_index;
    //at which offset the fixup has to occur
    uint32_t offset;
    //name of variable that was missing at the time of compilation of the given function
    Str name;
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
    //max number of local variables
    //we use the term "max" because for example after leaving a scope the vars cease to exist, but we
    //still need to be able to store them when we are in the scope
    uint8_t locals;
    //index of the current variable in the vars array
    uint8_t current_var_i;
    //scopes
    Environment *env;
    Fixups fun_fixups;
} Fun;

typedef struct {
    //index of the current function
    int current;
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
//-------------------------GLOBALS-END-------------------------


void compiler_init() {
    const_pool = malloc(sizeof(uint8_t) * MAX_CONST_POOL_SZ);
    const_pool_map  = malloc(sizeof(void*) * (MAX_CONST_POOL_SZ + 1));
    const_pool_map[cpm_free_i] = const_pool;
    cfuns = malloc(sizeof(CompiledFuns));
    cfuns->funs = malloc(sizeof(Fun *) * MAX_FUN_NUM);
    all_fixups = malloc(sizeof(Fixups) * MAX_FUN_NUM);
    all_fixups->cnt = 0;
    //we alloc function and there we increment current
    //we want the entry-point to be stored at index 0
    //initializing this to -1 is the easiest way how to achieve this
    cfuns->current = -1;
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

//copies the bytecode of the function at the given function index to the const pool
void const_pool_insert_fun(size_t index) {
    assert(index <= cfuns->current);
    Fun *fun = cfuns->funs[index];
    memcpy(const_pool_map[cpm_free_i], fun->fun, fun->sz);
    bump_const_pool_free(fun->sz);
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

void fun_alloc(uint8_t paramc, AstFunction *af, bool defining) {
    Fun *fun = malloc(sizeof(Fun));
    fun->fun = malloc(MAX_FUN_BODY_SZ);
    fun->env = malloc(sizeof(Environment));
    fun->fun_fixups.fixups = malloc(sizeof(Fixup) * MAX_FIXUPS_NUM);
    fun->fun_fixups.cnt = 0;
    fun->sz = 0;
    fun->locals = 0;
    fun->current_var_i = 0;
    //cfuns->current is initialized in the init function to -1, so the entry-point is stored at index 0
    cfuns->current++;
    cfuns->funs[cfuns->current] = fun;
    //at the 0th index is the "this" variable
    add_name_to_scope(STR("this"), 0);

    //for the entry point function af is NULL
    if (af != NULL) {
        for (size_t i = 0; i < af->parameter_cnt; ++i) {
            //add the parameters as local variables
            //curent_var_i is  for i == 0 set to 1, so we don't overwrite the "this" variable
            add_name_to_scope(af->parameters[i], fun->current_var_i);
        }
    }
    //if the function is already defined, we don't need to insert anything to its bytecode
    //TODO probably always called with defining == true? -> remove the parameter
    if (defining) {
        //insert the function opcode
        fun_insert_bytecode(&TAG_FUNCTION, sizeof(TAG_FUNCTION));
        //insert the number of arguments
        //increase paramc by 1 because of the first hidden param "this"
        paramc += 1;
        fun_insert_bytecode(&paramc, sizeof(paramc));
        //skip locals and length
        // - locals: the number of local variables defined in this function, represented by a 16b unsigned integer
        //   (this number  does not include the number of parameters nor the hidden this parameter).
        // - length: the total length of the bytecode vector (the total count of bytes in the function’s body),
        //   4B unsigned integer
        fun->sz += sizeof(uint16_t) + sizeof(uint32_t);
    }
}

void fun_epilogue() {
    Fun *fun = cfuns->funs[cfuns->current];
    fun_insert_locals(fun->locals);
    fun_insert_length(fun->sz);
    //insert the return opcode, every func, even the entry point must end with a return
    fun_insert_bytecode(&BC_OP_RETURN, sizeof(BC_OP_RETURN));
    //copy the top-level function to the const_pool
    //printf("inserting fun to const pool, index: %d\n", cpm_free_i);
    //printf("%.*s", (int)fun.len, str.str);
    const_pool_insert_fun(cfuns->current);
    free(fun->fun);
    free(fun->env);
    //it's ok to overflow for the entry point (we won't compile anything after finishing the compilation of entry point)
    --cfuns->current;
    //update the const pool index in the all_fixups, we now the index of the currently compiled function
    for (size_t i = 0; i < fun->fun_fixups.cnt; ++i) {
        //we need to subtract 1 because the const pool index just got incremented in the const_pool_insert_fun
        fun->fun_fixups.fixups[i].cp_index = cpm_free_i - 1;
    }
    assert(all_fixups->cnt + fun->fun_fixups.cnt < MAX_FIXUPS_NUM);
    //copy the all_fixups to the global variable all_fixups
    all_fixups[all_fixups->cnt++] = fun->fun_fixups;
}

//function gets called when we define new global variable
//this variable is potentially missing in some of the already compiled functions
//we will go through the all_fixups compare the name and fixup the index if match is found
void fixup(Str name, uint16_t fixup_index) {
    //go through all the fixups
    for (size_t i = 0; i < all_fixups->cnt; ++i) {
        //go through all the fixups in the function i
        for (size_t j = 0; j < all_fixups[i].cnt; ++j) {
            //TODO: would be nice to remove the fixed indexes..
            if (str_cmp(name, all_fixups[i].fixups[j].name) == 0) {
                //we found the name in the all_fixups, so we need to fixup the index
                uint16_t fun_index = all_fixups[i].fixups[j].cp_index;
                uint8_t *fun_bc = const_pool_map[fun_index];
                fun_bc[all_fixups[i].fixups[j].offset] = fixup_index;
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

void compile(const Ast *ast) {
    switch(ast->kind) {
        case AST_NULL: {
            //TODO: those 2 lines are repeated multiple times for all constants
            //      move it to a function/extend the existing one
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
            AstFunction *af = (AstFunction *)ast;
            fun_alloc(af->parameter_cnt, af, true);
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
                fun_insert_bytecode(&cpm_free_i, sizeof(cpm_free_i));
                //global variable can fixup some missing indexes in the already compiled functions
                //the problem is that we don't know beforehand at which index in the constant pool the function will
                //end up, however upon definition of that var we already know that index - and it is cpm_free_i
                fixup(ad->name, cpm_free_i);
                //the global variables are accessed via the index to const pool, where their name is stored
                add_name_to_scope(ad->name, cpm_free_i);
                const_pool_insert_str(ad->name);
            }
            else {//add to local environment
                fun_insert_bytecode(&BC_OP_SET_LOCAL, sizeof(BC_OP_SET_LOCAL));
                fun_insert_bytecode(&cfuns->funs[cfuns->current]->current_var_i,
                                    sizeof(cfuns->funs[cfuns->current]->current_var_i));
                //TODO ugly, maybe current_var_i could be managed by the fun itself?
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
                //we still need to insert the bytecode
                //at the same time the missing index must be an index of a global variable
                uint16_t tmp = 0;
                fun_insert_bytecode(&BC_OP_GET_GLOBAL, sizeof(BC_OP_GET_GLOBAL));
                //insert_to_fun_fixups uses the fn->sz as the offset
                //the offset should be such that the index to const pool is later fixed-up
                insert_to_fun_fixups(ava->name);
                fun_insert_bytecode(&tmp, sizeof(uint16_t));
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
                fun_insert_bytecode(&BC_OP_SET_GLOBAL, sizeof(uint8_t));
                //insert_to_fun_fixups uses the fn->sz as the offset
                //the offset should be such that the index to const pool is later fixed-up
                insert_to_fun_fixups(ava->name);
                fun_insert_bytecode(&tmp, sizeof(uint16_t));
            }
            else if (is_global) {
                fun_insert_bytecode(&BC_OP_SET_GLOBAL, sizeof(uint8_t));
                fun_insert_bytecode(&var->index, sizeof(var->index));
            }
            else {
                fun_insert_bytecode(&BC_OP_SET_LOCAL, sizeof(uint8_t));
                fun_insert_bytecode(&var->index, sizeof(var->index));
            }
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
            fun_alloc(0, NULL, true);
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