#include <stdio.h>

#include "ast_interpreter.h"
#include "parser.h"

IState* init_interpreter() {
    IState *state = malloc(sizeof(IState));
    state->heap = malloc(sizeof(Heap));
    state->heap->heap_start = malloc(MEM_SZ);
    state->heap->heap_free = state->heap->heap_start;
    state->heap->heap_size = 0;
    state->stack = malloc(sizeof(Environment) * MAX_STACK_SZ);
    state->current_env = -1;
    state->globals = malloc(sizeof(Environment));
    state->globals->var_cnt = 0;
    return state;
}

void add_to_env(Value val, Str name, IState *state) {
    if (state->current_env == -1) { //global scope
        if (state->globals->var_cnt == MAX_VARS) {
            printf("Too many variables in global scope\n");
            exit(1);
        }
        else {
            size_t var_cnt = state->globals->var_cnt;
            state->globals->vars[var_cnt].name = name;
            state->globals->var_cnt++;
        }
    
    } else { //local scope
        if (state->stack[state->current_env].var_cnt == MAX_VARS) {
            printf("Too many variables in local scope\n");
            exit(1);
        }
        else {
            size_t var_cnt = state->stack[state->current_env].var_cnt;
            state->stack[state->current_env].vars[var_cnt].name = name;
            state->stack[state->current_env].var_cnt++;
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
        //case AST_VARIABLE_ACCESS: {return (Value){};}
        //case AST_VARIABLE_ASSIGNMENT: {return (Value){};}
        //case AST_FUNCTION: {return (Value){};}
        //case AST_FUNCTION_CALL: {return (Value){};}
        //case AST_PRINT: {return (Value){};}
        //case AST_BLOCK: {return (Value){};}
        case AST_TOP: {
            AstTop *top = (AstTop *) ast;
            Value value;
            for (size_t i = 0; i < top->expression_cnt; i++) {
                value = interpret(top->expressions[i], state);
            }
            return value;
        }
        //case AST_LOOP: {return (Value){};}
        //case AST_CONDITIONAL: {return (Value){};}
        default: {
            printf("Ast node not implemented\n");
            (Value){};
        }
    }
}
