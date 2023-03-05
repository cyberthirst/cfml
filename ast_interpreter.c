#include <stdio.h>

#include "ast_interpreter.h"
#include "parser.h"

IState* init_interpreter() {
    IState *state = malloc(sizeof(IState));
    state->heap = malloc(sizeof(Heap));
    state->heap->heap_start = malloc(MEM_SZ);
    state->heap->heap_free = state->heap->heap_start;
    state->heap->heap_size = 0;
    state->envs = malloc(sizeof(Environment) * MAX_ENVS);
    state->current_env = 0;
    return state;
}

void *heap_alloc(Value *val, Heap *heap) {
    if (heap->heap_size + sizeof(val) > MEM_SZ) {
        printf("Out of memory\n");
        exit(1);
    }
    else {
        void *ptr = heap->heap_free;
        heap->heap_free += sizeof(val);
        heap->heap_size += sizeof(val);
        return ptr;
    }
}

void add_to_env(Value val, Str name, IState *state) {
    size_t env = state->current_env;
    if (state->envs[env].var_cnt == MAX_VARS) {
        printf("Too many variables in local scope\n");
        exit(1);
    }
    else {
        size_t var_cnt = state->envs[env].var_cnt;
        state->envs[env].vars[var_cnt].name = name;
        Value *ptr = heap_alloc(&val, state->heap);
        memcpy(ptr, &val, sizeof(val));
        state->envs[env].vars[var_cnt].val = ptr;
        state->envs[env].var_cnt++;
    }
}

bool my_str_cmp(Str a, Str b) {
    bool len_eq = a.len == b.len;
    bool memcmp_eq = memcmp(a.str, b.str, a.len) == 0;
	return a.len == b.len && memcmp(a.str, b.str, a.len) == 0;
}

Value *find_in_env(Str name, Environment *env) {
    for (size_t i = 0; i < env->var_cnt; i++) {
        if (my_str_cmp(env->vars[i].name, name) == true) {
            return env->vars[i].val;
        }
    }
    return NULL;
}

Value *get_var_ptr(Str name, IState *state) {
    Value *val;
    val = find_in_env(name, &state->envs[state->current_env]);
    if (val != NULL) {
        return val;
    }
    val = find_in_env(name, &state->envs[GLOBAL_ENV_INDEX]);
    return val;
}

void assign_var(ValueKind *val, Value new_val, Heap *heap) {
    memcpy(val, &new_val, sizeof(new_val));
}

void print_val(Value val) {
    switch(val.kind) {
        case VK_INTEGER: {
            printf("%d", val.integer);
            break;
        }
        case VK_BOOLEAN: {
            printf("%s", val.boolean ? "true" : "false");
            break;
        }
        case VK_NULL: {
            printf("null");
            break;
        }
        case VK_GCVALUE: {
            printf("gcvalue");
            break;
        }
        case VK_FUNCTION: {
            printf("function");
            break;
        }
    }
}

Value interpret(Ast *ast, IState *state) {
    switch(ast->kind) {
        case AST_INTEGER: {
            AstInteger *integer = (AstInteger *) ast;
            return (Value) { .kind = VK_INTEGER, .integer = integer->value};
        }
        case AST_BOOLEAN: {
            AstBoolean *boolean = (AstBoolean *) ast; 
            return (Value) { .kind = VK_BOOLEAN, .boolean = boolean->value};
        }
        case AST_NULL: {
            return (Value) { .kind = VK_NULL };
        }
        case AST_DEFINITION: {
            AstDefinition *definition = (AstDefinition *) ast; 
            Value val = interpret(definition->value, state);
            add_to_env(val, definition->name, state);
            return val;
        }
        case AST_VARIABLE_ACCESS: {
            AstVariableAccess *variable_access = (AstVariableAccess *) ast; 
            Value *val = get_var_ptr(variable_access->name, state);
            Value value;
            memcpy(&value, val, sizeof(*val));
            return value;
        }
        case AST_VARIABLE_ASSIGNMENT: {
            AstVariableAssignment *va = (AstVariableAssignment *) ast;
            Value val = interpret(va->value, state);
            ValueKind *val_ptr = get_var_ptr(va->name, state);
            assign_var(val_ptr, val, state->heap);
            return val;
        }
        case AST_FUNCTION: {
            AstFunction *function = (AstFunction *) ast;
	        return (Value) { .kind = VK_FUNCTION, .function = function };
        }
        case AST_FUNCTION_CALL: {
            AstFunctionCall *fc = (AstFunctionCall *) ast; 
            Value fun = interpret(fc->function, state);
            state->current_env++;
            Value val;
            for (size_t i = 0; i < fc->argument_cnt; i++) {
                val = interpret(fc->arguments[i], state);
                add_to_env(val, fun.function->parameters[i], state);
            }
            Value ret = interpret(fun.function->body, state);
            state->current_env--;
            return state->envs[state->current_env + 1].ret_val = ret;
        }
        case AST_PRINT: {
            AstPrint *prnt = (AstPrint *) ast;
            Value val[prnt->argument_cnt];
            for (size_t i = 0; i < prnt->argument_cnt; i++) {
                val[i] = interpret(prnt->arguments[i], state);
                print_val(val[i]);
            }
            return (Value){};}
        case AST_BLOCK: {
            AstBlock *block = (AstBlock *) ast; 
            Value val;
            for (size_t i = 0;  i < block->expression_cnt; i++) {
                val = interpret(block->expressions[i], state);
            }
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
            Value val;
            while (interpret(loop->condition, state).boolean) {
                val = interpret(loop->body, state);
            }
            return val;
        }
        case AST_CONDITIONAL: {
            AstConditional *conditional = (AstConditional *) ast; 
            if (interpret(conditional->condition, state).boolean) {
                return interpret(conditional->consequent, state);
            }
            else {
                return interpret(conditional->alternative, state);
            }
        }
        default: {
            printf("Ast node not implemented\n");
            (Value){};
        }
    }
}
