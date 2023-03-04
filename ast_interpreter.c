#include <stdio.h>

#include "ast_interpreter.h"
#include "parser.h"

IState* init_interpreter() {
    IState *state = malloc(sizeof(IState));
    state->mem = malloc(1024);
    state->stack = malloc(sizeof(Environment) * MAX_STACK_SZ);
    state->stack_size = 0;
    return state;
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
        //case AST_DEFINITION: {return (Value){}; }
        //case AST_VARIABLE_ACCESS: {return (Value){};}
        //case AST_VARIABLE_ASSIGNMENT: {return (Value){};}
        //case AST_FUNCTION: {return (Value){};}
        //case AST_FUNCTION_CALL: {return (Value){};}
        //case AST_PRINT: {return (Value){};}
        //case AST_BLOCK: {return (Value){};}
        //case AST_TOP: {return (Value){};}
        //case AST_LOOP: {return (Value){};}
        //case AST_CONDITIONAL: {return (Value){};}
        default: {
            printf("Ast node not implemented\n");
            (Value){};
        }
    }
}
