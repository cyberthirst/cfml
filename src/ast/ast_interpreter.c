#include <stdio.h>
#include <assert.h>

#include "ast_interpreter.h"
#include "../heap/heap.h"
#include "../parser.h"
#include "../utils/utils.h"

const long long int MEM_SZ = (1024L * 1024 * 1024 * 1);

IState* init_interpreter() {
    IState *state = malloc(sizeof(IState));
    state->heap = construct_heap(MEM_SZ);
    state->envs = malloc(sizeof(Environment) * MAX_ENVS);
    state->current_env = 0;
    state->null = construct_null(state->heap);
    return state;
}

void free_interpreter(IState *state) {
    free(state->heap->heap_start);
    free(state->heap);
    free(state->envs);
    free(state);
}

void add_to_scope(Value val, Str name, IState *state) {
    size_t env = state->current_env;
    size_t scope_cnt = state->envs[env].scope_cnt;
    if (state->envs[env].scopes[scope_cnt].var_cnt == MAX_VARS) {
        printf("Too many variables in local scope\n");
        exit(1);
    }
    else {
        size_t var_cnt = state->envs[env].scopes[scope_cnt].var_cnt;
        state->envs[env].scopes[scope_cnt].vars[var_cnt].name = name;
        state->envs[env].scopes[scope_cnt].vars[var_cnt].val = val;
        state->envs[env].scopes[scope_cnt].var_cnt++;
    }
}

Value builtins(Value obj, int argc, Value *argv, Str m_name, IState *state) {
    /*
        FROM REFERENCE IMPLEMENTATION
    */
    const u8 *method_name = m_name.str;
	size_t method_name_len = m_name.len;
	#define METHOD(name) \
			if (sizeof(name) - 1 == method_name_len && memcmp(name, method_name, method_name_len) == 0) /* body*/
    uint8_t kind = *(uint8_t *)obj;
    if (kind == VK_INTEGER || kind == VK_BOOLEAN || kind == VK_NULL) { 
        assert(argc == 1);
        METHOD("+") {
            return construct_integer(((Integer *)obj)->val + ((Integer *)argv[0])->val, state->heap);
        }
        METHOD("-") {
            return construct_integer(((Integer *)obj)->val - ((Integer *)argv[0])->val, state->heap);
        }
        METHOD("*") { 
            return construct_integer(((Integer *)obj)->val * ((Integer *)argv[0])->val, state->heap);
        }
        METHOD("/") {
            return construct_integer(((Integer *)obj)->val / ((Integer *)argv[0])->val, state->heap);
        }
        METHOD("%") {
            return construct_integer(((Integer *)obj)->val % ((Integer *)argv[0])->val, state->heap);
        }
        METHOD("<=") {
            return construct_boolean(((Integer *)obj)->val <= ((Integer *)argv[0])->val, state->heap);
        }
        METHOD(">=") {
            return construct_boolean(((Integer *)obj)->val >= ((Integer *)argv[0])->val, state->heap);
        }
        METHOD(">") {
            return construct_boolean(((Integer *)obj)->val > ((Integer *)argv[0])->val, state->heap);
        }
        METHOD("<") {
            return construct_boolean(((Integer *)obj)->val < ((Integer *)argv[0])->val, state->heap );
        }
        METHOD("==") {
            if (kind == VK_INTEGER && *argv[0] != VK_INTEGER){
                return construct_boolean(false, state->heap);
            }
            else if (kind == VK_INTEGER)
                return construct_boolean(((Integer *)obj)->val == ((Integer *)argv[0])->val, state->heap);
            else if (kind == VK_NULL)
                return construct_boolean(*argv[0] == VK_NULL, state->heap);
            else
                return construct_boolean(((Boolean *)obj)->val == ((Boolean *)argv[0])->val, state->heap);
        }
        METHOD("!=") {
            if (kind == VK_INTEGER && *argv[0] != VK_INTEGER){
                return construct_boolean(true, state->heap);
            }
            else if (kind == VK_INTEGER)
                return construct_boolean(((Integer *)obj)->val != ((Integer *)argv[0])->val, state->heap);
            else if (kind == VK_NULL){
                return construct_boolean(*argv[0] != VK_NULL, state->heap);
            }
            else
                return construct_boolean(((Boolean *)obj)->val != ((Boolean *)argv[0])->val, state->heap);
        }
    }
    if (kind == VK_BOOLEAN) {
        assert(argc == 1);
        METHOD("&") {
            return construct_boolean(((Boolean *)obj)->val & ((Boolean *)argv[0])->val, state->heap);
        }
        METHOD("|") {
            return construct_boolean(((Boolean *)obj)->val | ((Boolean *)argv[0])->val, state->heap);
        }
    }

    METHOD("set") {
        if (kind == VK_ARRAY) {
            Array *array = (Array *)obj;
            array->val[((Integer *)argv[0])->val] = argv[1];
            return obj;
        }
    }

    METHOD("get") { 
        if (kind == VK_ARRAY) {
            Array *array = (Array *)obj;
            Integer *test = argv[0];
            size_t index = test->val;
            assert(index < array->size);
            return array->val[((Integer *)argv[0])->val];
        }
    }

    return (Value){NULL};
}

Value *find_in_env(Str name, Environment *env, size_t scope_cnt) {
    //we don't want to subtract 1 like in the case of vars because by default there is the global scope on index 0
    //search from all scopes starting on top
    for (int i = scope_cnt; i >= 0; i--) {
        //search all vars in the scope starting from the latest added
        int j = env->scopes[i].var_cnt;
        //if there are no vars the whole for loop will be skipped
        //if there are > 0 vars we need to decrement j to properly index the var in the vars array
        if (j > 0) j--;
        for (;j >= 0; j--) {
            if (str_eq(env->scopes[i].vars[j].name, name) == true) {
                    return &env->scopes[i].vars[j].val;
            }
        }
    }

    return NULL;
}

Value *get_var_ptr(Str name, IState *state) {
    Value *val;
    //try to find the symbol in the current environment
    //each function call creates a new environment
    val = find_in_env(name, &state->envs[state->current_env], state->envs[state->current_env].scope_cnt);
    if (val != NULL) {
        return val;
    }
    //if the current environment doesnt have the symbol try to find it in the global environment
    val = find_in_env(name, &state->envs[GLOBAL_ENV_INDEX], 0);
    return val;
}

void push_env(IState *state) {
    if (state->current_env == MAX_ENVS - 1) {
        printf("max envs reached");
        exit(1);
    }
    state->current_env++;
    state->envs[state->current_env].scope_cnt = 0;
    state->envs[state->current_env].scopes[0].var_cnt = 0;
}

void pop_env(IState *state) {
    memset(&state->envs[state->current_env], 0, sizeof(Environment));
    //state->envs[state->current_env].scope_cnt = 0;
    state->current_env--;
}

void push_scope(IState *state) {
    if (state->envs[state->current_env].scope_cnt == MAX_SCOPES - 1) {
        printf("max scopes reached");
        exit(1);
    }
    size_t env = state->current_env;
    size_t scope_cnt = state->envs[env].scope_cnt;
    state->envs[env].scope_cnt++;
    //scope_cnt wasnt incremented yet - so we need to add 1
    state->envs[env].scopes[scope_cnt + 1].var_cnt = 0;
}

void pop_scope(IState *state) {
    size_t env = state->current_env;
    size_t scope_cnt = state->envs[env].scope_cnt;
    //state->envs[env].scopes[scope_cnt].var_cnt = 0;
    memset(&state->envs[env].scopes[scope_cnt], 0, sizeof(Scope));
    state->envs[env].scope_cnt--;
}


Value *field_access(Value obj, Str name, IState *state) {
    Object *object = (Object *)obj;
    assert(*obj == VK_OBJECT);
    //print_val(obj);
    for (size_t i = 0; i < object->field_cnt; i++) {
        if (str_eq(object->val[i].name, name)) {
            return &object->val[i].val;
        }
    }
    return field_access(object->parent, name, state);
}

Value method_call(Value obj, Str name, int argc, Value *argv, IState *state) {
    Object *object = (Object *)obj;
    //if inheriting from a primitive type then call the builtin
    if (object->kind != VK_OBJECT) {
        return builtins(obj, argc, argv, name, state);
    }
    for (size_t i = 0; i < object->field_cnt; i++) {
        if (str_eq(object->val[i].name, name)) {
            Value ret;
            push_env(state);
            add_to_scope(obj, STR("this"), state);
            Field field = object->val[i];
            assert(*field.val == VK_FUNCTION);
            Function *func = (Function *)field.val;
            for (int j = 0; j < argc; j++) {
                add_to_scope(argv[j], func->val->parameters[j], state);
                
            }
            ret = interpret(func->val->body, state);
            pop_env(state);
            return state->envs[state->current_env + 1].ret_val = ret;
        }
    }
    return method_call(object->parent, name, argc, argv, state);
}


Value interpret(Ast *ast, IState *state) {
    switch(ast->kind) {
        case AST_INTEGER: {
            AstInteger *integer = (AstInteger *) ast;
            return construct_integer(integer->value, state->heap);
        }
        case AST_BOOLEAN: {
            AstBoolean *boolean = (AstBoolean *) ast; 
            return construct_boolean(boolean->value, state->heap);
        }
        case AST_NULL: {
            return construct_null(state->heap);
        }
        case AST_DEFINITION: {
            AstDefinition *definition = (AstDefinition *) ast; 
            Value val = interpret(definition->value, state);
            add_to_scope(val, definition->name, state);
            return val;
        }
        case AST_VARIABLE_ACCESS: {
            AstVariableAccess *variable_access = (AstVariableAccess *) ast; 
            Value *val = get_var_ptr(variable_access->name, state);
            if (val == NULL){
                Value nl = construct_null(state->heap);
                add_to_scope(nl, variable_access->name, state);
                return construct_null(state->heap);
            }
            return *val;
        }
        case AST_VARIABLE_ASSIGNMENT: {
            AstVariableAssignment *va = (AstVariableAssignment *) ast;
            Value new_val = interpret(va->value, state);
            Value *var_ptr = get_var_ptr(va->name, state);
            if (var_ptr == NULL) {
                add_to_scope(new_val, va->name, state);
                return new_val;
            } 
            *var_ptr = new_val;
            return *var_ptr;
        }
        case AST_FUNCTION: {
            AstFunction *function = (AstFunction *) ast;
            return construct_ast_function(function, state->heap);
        }
        case AST_FUNCTION_CALL: {
            AstFunctionCall *fc = (AstFunctionCall *) ast; 
            Function *fun = interpret(fc->function, state);
            Value *args = (Value *)malloc(sizeof(Value) * fc->argument_cnt);
            for (size_t i = 0; i < fc->argument_cnt; i++) {
                args[i] = interpret(fc->arguments[i], state);
            }
            push_env(state);
            add_to_scope(construct_null(state->heap), STR("this"), state);
            for (size_t i = 0; i < fc->argument_cnt; i++) {
                add_to_scope(args[i], ((AstFunction *)fun->val)->parameters[i], state);
            }
            Value ret = interpret(((Function *)fun)->val->body, state);
            pop_env(state);
            free(args);
            return state->envs[state->current_env + 1].ret_val = ret;
        }
        case AST_PRINT: {
            AstPrint *prnt = (AstPrint *) ast;
            Value val[prnt->argument_cnt];
            for (size_t i = 0; i < prnt->argument_cnt; i++) {
                val[i] = interpret(prnt->arguments[i], state);
            }
            size_t tilda = 0;
            for (size_t i = 0; i < prnt->format.len; i++) {
                if (prnt->format.str[i] == '~') {
                    //we are only concerned with valid programs so each ~ will have a corresponding value
                    print_val(val[tilda]);
                    tilda++;
                }
                else {
                    char c = prnt->format.str[i];
                    //check for escape characters
                    if (c == '\\' && prnt->format.len > i + 1) {
                        char next = prnt->format.str[i + 1];
                        //increment i so we don't print the next character two times
                        i++;
                        if (next == 'n') {
                            printf("\n");
                        }
                        else if (next == 't') {
                            printf("\t");
                        }
                        else if (next == 'r') {
                            printf("\r");
                        }
                        else if (next == '~') {
                            printf("~");
                        }
                        else {
                            printf("%c", next);
                        }
                    }
                    else {
                        printf("%c", prnt->format.str[i]);
                    }
                }
            } 
            return construct_null(state->heap);
        }
        case AST_BLOCK: {
            AstBlock *block = (AstBlock *) ast; 
            Value val;
            push_scope(state);
            for (size_t i = 0;  i < block->expression_cnt; i++) {
                val = interpret(block->expressions[i], state);
            }
            pop_scope(state);
            return val;
        }
        case AST_TOP: {
            AstTop *top = (AstTop *) ast;
            Value value;
            for (size_t i = 0; i < top->expression_cnt; i++) {
                value = interpret(top->expressions[i], state);
            }
            return value;
        }
        case AST_LOOP: {
            AstLoop *loop = (AstLoop *) ast; 
            Value cond = interpret(loop->condition, state);
            while (true) {
                if ((*cond == VK_BOOLEAN && !((Boolean *)cond)->val) || *cond == VK_NULL) {
                    return construct_null(state->heap);
                }
                push_scope(state);
                //we don't care about the return value of the loop body
                //if the condition is false we return null
                interpret(loop->body, state);
                pop_scope(state);
                cond = interpret(loop->condition, state);
            }
        }
        case AST_CONDITIONAL: {
            AstConditional *conditional = (AstConditional *) ast; 
            Value cond = interpret(conditional->condition, state);
            Value ret;
            if ((*cond == VK_BOOLEAN && !((Boolean *)cond)->val) || *cond == VK_NULL) {
                //else branch
                push_scope(state);
                ret =  interpret(conditional->alternative, state);
            }
            else {
                //then branch
                push_scope(state);
                ret = interpret(conditional->consequent, state);
            }
            pop_scope(state);
            return ret;
        }
        case AST_ARRAY: {
            AstArray *array = (AstArray *) ast;
            Integer *sz = interpret(array->size, state);
            assert(sz->kind == VK_INTEGER);
            assert(sz->val >= 0);
            //alloc space for the array and gete the value (pointer) to the array
            Array *arr = construct_array(sz->val, state->heap);
            //eval the initializer `sz` times and assign it to the array
            for (size_t i = 0; i < (size_t)sz->val; i++) {
                //need to eval the initializer in tmp scope
                push_scope(state);
                arr->val[i] = interpret(array->initializer, state);
                pop_scope(state);
            }
            return arr;
        }

        case AST_INDEX_ACCESS: {
            AstIndexAccess *aa = (AstIndexAccess *) ast;
            Value val = interpret(aa->object, state);
            Integer *idx = interpret(aa->index, state);
            //return method_call(val, (Str){(u8 *)"get", 3}, 1, &idx, state);
            return method_call(val, STR("get"), 1, &idx, state);
        }

        case AST_INDEX_ASSIGNMENT: {
            AstIndexAssignment *ia = (AstIndexAssignment *) ast;
            Value obj = interpret(ia->object, state);
            Integer *idx = interpret(ia->index, state);
            Value val = interpret(ia->value, state);
            //Value *args = malloc(sizeof(Value) * 2);
            Value args[2];
            args[0] = idx; args[1] = val;
            //return method_call(obj, (Str){(u8 *)"set", 3}, 2, args, state);
            return method_call(obj, STR("set"), 2, args, state);
        }
        
        case AST_OBJECT: {
            AstObject *object = (AstObject *) ast;
            Value parent = interpret(object->extends, state);
            size_t sz = object->member_cnt;
            Object *obj = construct_object(sz, parent, state->heap);
            for (size_t i = 0; i < sz; i++) {
                assert(object->members[i]->kind == AST_DEFINITION);
                AstDefinition *member = (AstDefinition *)object->members[i];
                obj->val[i].name = member->name;
                push_scope(state);
                obj->val[i].val = interpret(member->value, state);
                pop_scope(state);
            }
            return obj;
        }
        
        case AST_FIELD_ACCESS: { 
            AstFieldAccess *fa = (AstFieldAccess *) ast;
            Value obj = interpret(fa->object, state);
            return *field_access(obj, fa->field, state);
        }
        
        case AST_FIELD_ASSIGNMENT: {
            AstFieldAssignment *fa = (AstFieldAssignment *) ast;
            Object *obj = interpret(fa->object, state);
            Value val = interpret(fa->value, state);

            Value *field = field_access(obj, fa->field, state);

            *field = val;

            return field;
        }

        case AST_METHOD_CALL: {
            AstMethodCall *mc = (AstMethodCall *) ast;
            Object *obj = interpret(mc->object, state);
            assert(obj->kind == VK_OBJECT || is_primitive(obj->kind));
            uint8_t vk = *(uint8_t *)obj;
            Value *args = malloc(sizeof(Value) * mc->argument_cnt);
            Value val;
            for (size_t i = 0; i < mc->argument_cnt; i++) {
                args[i] = interpret(mc->arguments[i], state);
            }
            if (vk == VK_INTEGER || vk == VK_BOOLEAN || vk == VK_NULL) {
                val = builtins(obj, mc->argument_cnt, args, mc->name, state);
            }
            else {
                val = method_call(obj, mc->name, mc->argument_cnt, args, state);
            }
            free(args);
            return val;
        }

        default: {
            printf("Ast node not implemented\n");
            exit(1);
            return NULL;
        }
    }
}
