#include <stdio.h>
#include <assert.h>

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

void *heap_alloc(size_t sz, Heap *heap) {
    if (heap->heap_size + sz > MEM_SZ) {
        printf("Out of memory\n");
        exit(1);
    }
    else {
        void *ptr = heap->heap_free;

        heap->heap_free += sz;
        heap->heap_size += sz;

        //align to 8 bytes
        size_t diff = 8 - (heap->heap_size % 8);
        if (diff != 8) {
            heap->heap_free += diff;
            heap->heap_size += diff;
        }

        return ptr;
    }
}

ValueKind *array_alloc(int size, IState *state) {
    //array is stored as folows:
    // ValueKind | size_t | Value[size]
    // ValueKind == VK_ARRAY | size_t == numOfElems(array) | Value[size] == array of values (aka pointers to the actual valuesa)
    return heap_alloc(sizeof(Array) + sizeof(Value) * size, state->heap);
}

Value construct_array(int size, IState *state) {
    Array *array = array_alloc(size, state);
    array->kind = VK_ARRAY;
    array->size = size;
    return array;
}

ValueKind *object_alloc(int size, IState *state) {
    //object is stored as follows:
    // ValueKind | parent | size_t | Value[size]
    // ValueKind == VK_OBJECT | parent == VK_OBJECT | size_t == numOfFields(object) | Value[size] == array of Values (aka pointers members of the object)
    return heap_alloc(sizeof(Object) + sizeof(Field) * size, state->heap);
}

ValueKind *function_alloc(IState *state) {
    return heap_alloc(sizeof(Function), state->heap);
}

Value construct_function(AstFunction *ast_func, IState *state) {
    Function *func = function_alloc(state);
    func->kind = VK_FUNCTION;
    func->val = ast_func;
    return func;
}

ValueKind *integer_alloc(IState *state) {
    return heap_alloc(sizeof(Integer), state->heap);
}

Value construct_integer(i32 val, IState *state) {
    Integer *integer = integer_alloc(state);
    integer->kind = VK_INTEGER;
    integer->val = val;
    return integer;
}

ValueKind *boolean_alloc(IState *state) {
    return heap_alloc(sizeof(Boolean), state->heap);
}

Value construct_boolean(bool val, IState *state) {
    Boolean *boolean = boolean_alloc(state);
    boolean->kind = VK_BOOLEAN;
    boolean->val = val;
    return boolean;
}

ValueKind *null_alloc(IState *state) {
    return heap_alloc(sizeof(Null), state->heap);
}


//TODO don't construct null, just set the Value ptr to NULL
Value construct_null(IState *state) {
    Null *null = null_alloc(state);
    null->kind = VK_NULL;
    return null;
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

bool my_str_cmp(Str a, Str b) {
    bool len_eq = a.len == b.len;
    bool memcmp_eq = memcmp(a.str, b.str, a.len) == 0;
	return a.len == b.len && memcmp(a.str, b.str, a.len) == 0;
}

Value builtins(Value obj, int argc, Value *argv, Str m_name, IState *state) {
    /*
        FROM REFERENCE IMPLEMENTATION
    */
    const u8 *method_name = m_name.str;
	size_t method_name_len = m_name.len;
	#define METHOD(name) \
			if (sizeof(name) - 1 == method_name_len && memcmp(name, method_name, method_name_len) == 0) /* body*/
    ValueKind kind = *(ValueKind *)obj;
    if (kind == VK_INTEGER || kind == VK_BOOLEAN || kind == VK_NULL) { 
        assert(argc == 1);
        METHOD("+") {
            return construct_integer(((Integer *)obj)->val + ((Integer *)argv[0])->val, state);
        }
        METHOD("-") {
            return construct_integer(((Integer *)obj)->val - ((Integer *)argv[0])->val, state);
        }
        METHOD("*") { 
            return construct_integer(((Integer *)obj)->val * ((Integer *)argv[0])->val, state);
        }
        METHOD("/") {
            return construct_integer(((Integer *)obj)->val / ((Integer *)argv[0])->val, state);
        }
        METHOD("%") {
            return construct_integer(((Integer *)obj)->val % ((Integer *)argv[0])->val, state);
        }
        METHOD("<=") {
            return construct_boolean(((Integer *)obj)->val <= ((Integer *)argv[0])->val, state);
        }
        METHOD(">=") {
            return construct_boolean(((Integer *)obj)->val >= ((Integer *)argv[0])->val, state);
        }
        METHOD(">") {
            return construct_boolean(((Integer *)obj)->val > ((Integer *)argv[0])->val, state);
        }
        METHOD("<") {
            return construct_boolean(((Integer *)obj)->val < ((Integer *)argv[0])->val, state);
        }
        METHOD("==") { 
            if (kind == VK_INTEGER)
                return construct_boolean(((Integer *)obj)->val == ((Integer *)argv[0])->val, state);
            else if (kind == VK_NULL)
                return construct_boolean(*(ValueKind *)argv[0] == VK_NULL, state);
            else
                return construct_boolean(((Boolean *)obj)->val == ((Boolean *)argv[0])->val, state);
        }
        METHOD("!=") { 
            if (kind == VK_INTEGER)
                return construct_boolean(((Integer *)obj)->val != ((Integer *)argv[0])->val, state);
            else if (kind == VK_NULL)
                return construct_boolean(*(ValueKind *)argv[0] != VK_NULL, state);
            else
                return construct_boolean(((Boolean *)obj)->val != ((Boolean *)argv[0])->val, state);
        }
    }
    if (kind == VK_BOOLEAN) {
        assert(argc == 1);
        METHOD("&") {
            return construct_boolean(((Boolean *)obj)->val == ((Boolean *)argv[0])->val, state);
        }
        METHOD("|") {
            return construct_boolean(((Boolean *)obj)->val != ((Boolean *)argv[0])->val, state);
        }
    }

    METHOD("set") {
        if (kind == VK_ARRAY) {
            Array *array = (Array *)obj;
            array->val[((Integer *)argv[0])->val] = argv[1];
            return obj;
        }
        else {
            Object *object = (Object *)obj;
            for (int i = 0; i < object->field_cnt; i++) {
                if (my_str_cmp(object->val[i].name, m_name)) {
                    object->val[i].val = argv[1];
                    return obj;
                }
            }
            builtins(object->parent, argc, argv, m_name, state);
        }

    }

    METHOD("get") { 
        if (kind == VK_ARRAY) {
            Array *array = (Array *)obj;
            return array->val[((Integer *)argv[0])->val];
        }
        else {
            Object *object = (Object *)obj;
            for (int i = 0; i < object->field_cnt; i++) {
                if (my_str_cmp(object->val[i].name, m_name)) {
                    return object->val[i].val;
                }
            }
            builtins(object->parent, argc, argv, m_name, state);
        }

    }


    return (Value){NULL};
}

Value *find_in_env(Str name, Environment *env) {
    //search from all scopes starting on top
    for (int i = env->scope_cnt; i >= 0; i--) {
        //search all vars in the scope starting from the latest added
        int j = env->scopes[i].var_cnt;
        //if there are no vars the whole for loop will be skipped
        //if there are > 0 vars we need to decrement j to properly index the var in the vars array
        if (j > 0) j--;
        for (;j >= 0; j--) {
            if (my_str_cmp(env->scopes[i].vars[j].name, name) == true) {
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
    val = find_in_env(name, &state->envs[state->current_env]);
    if (val != NULL) {
        return val;
    }
    //if the current environment doesnt have the symbol try to find it in the global environment
    val = find_in_env(name, &state->envs[GLOBAL_ENV_INDEX]);
    return val;
}

bool is_primitive(ValueKind kind){
    return kind == VK_BOOLEAN || kind == VK_INTEGER || kind == VK_FUNCTION || kind == VK_NULL;
}


//Value *val is pointer to Value (which is a pointer to the heap)
void assign_var(Value *val, Value new_val, Heap *heap) {
    ValueKind kind = *((ValueKind *)*val);
    //primitive types are immutable
    //to assign a value to a variable of a primitive type we need to allocate a new value on the heap
    //and assign the original pointer to the pointer to the new value on the heap
    if (is_primitive(kind)) {
        *val = new_val;
    }
    //mutable types are modified using refs
    else {

    }

}

void print_val(Value val) {
    switch(*(ValueKind *)val) {
        case VK_INTEGER: {
            printf("%d", ((Integer *)val)->val);
            break;
        }
        case VK_BOOLEAN: {
            printf("%s", ((Boolean *)val)->val ? "true" : "false");
            break;
        }
        case VK_NULL: {
            printf("null");
            break;
        }
        case VK_FUNCTION: {
            printf("function");
            break;
        }
        case VK_ARRAY: {
            printf("[");
            for (int i = 0; i < ((Array *)val)->size; i++) {
                print_val(((Array *)val)->val[i]);
                if (i != ((Array *)val)->size - 1) {
                    printf(", ");
                }
            }
            printf("]");
            break;
        }
    }
}

void push_env(IState *state) {
    state->current_env++;
    state->envs[state->current_env].scope_cnt = 0;
}

void pop_env(IState *state) {
    state->envs[state->current_env].scope_cnt = 0;
    state->current_env--;
}

void push_scope(IState *state) {
    size_t env = state->current_env;
    size_t scope_cnt = state->envs[env].scope_cnt;
    state->envs[env].scope_cnt++;
    //scope_cnt wasnt incremented yet - so we need to add 1
    state->envs[env].scopes[scope_cnt + 1].var_cnt = 0;
}

void pop_scope(IState *state) {
    size_t env = state->current_env;
    size_t scope_cnt = state->envs[env].scope_cnt;
    state->envs[env].scopes[scope_cnt].var_cnt = 0;
    state->envs[env].scope_cnt--;
}


Value interpret(Ast *ast, IState *state) {
    switch(ast->kind) {
        case AST_INTEGER: {
            AstInteger *integer = (AstInteger *) ast;
            return construct_integer(integer->value, state);
        }
        case AST_BOOLEAN: {
            AstBoolean *boolean = (AstBoolean *) ast; 
            return construct_boolean(boolean->value, state);
        }
        case AST_NULL: {
            return construct_null(state);
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
            return *val;
        }
        case AST_VARIABLE_ASSIGNMENT: {
            AstVariableAssignment *va = (AstVariableAssignment *) ast;
            Value new_val = interpret(va->value, state);
            Value *var_ptr = get_var_ptr(va->name, state);
            assign_var(var_ptr, new_val, state->heap);
            return var_ptr;
        }
        case AST_FUNCTION: {
            AstFunction *function = (AstFunction *) ast;
            return construct_function(function, state);
        }
        case AST_FUNCTION_CALL: {
            AstFunctionCall *fc = (AstFunctionCall *) ast; 
            Value fun = interpret(fc->function, state);
            push_env(state);
            Value val;
            for (size_t i = 0; i < fc->argument_cnt; i++) {
                val = interpret(fc->arguments[i], state);
                add_to_scope(val, ((Function *)fun)->val->parameters[i], state);
            }
            Value ret = interpret(((Function *)fun)->val->body, state);
            pop_env(state);
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
            return (Value){VK_NULL, NULL};
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
            Value val;
            push_scope(state);
            while (((Boolean  *)interpret(loop->condition, state))->val) {
                val = interpret(loop->body, state);
            }
            pop_scope(state);
            return val;
        }
        case AST_CONDITIONAL: {
            AstConditional *conditional = (AstConditional *) ast; 
            Value cond = interpret(conditional->condition, state);
            if ((*(ValueKind *)cond == VK_BOOLEAN && !((Boolean *)cond)->val) || *(ValueKind *)cond == VK_NULL) {
                //else branch
                return interpret(conditional->alternative, state);
            }
            else {
                //then branch
                return interpret(conditional->consequent, state);
            }
        }
        case AST_ARRAY: {
            AstArray *array = (AstArray *) ast;
            Integer *sz = interpret(array->size, state);
            //alloc space for the array and gete the value (pointer) to the array
            Array *arr = construct_array(sz->val, state);
            //eval the initializer `sz` times and assign it to the array
            for (size_t i = 0; i < sz->val; i++) {
                //need to eval the initializer in tmp scope
                push_scope(state);
                arr->val[i] = interpret(array->initializer, state);
                pop_scope(state);
            }
            return arr;
        }
        //TODO generalize to objects
        case AST_INDEX_ACCESS: {
            AstIndexAccess *aa = (AstIndexAccess *) ast;
            Array *arr = interpret(aa->object, state);
            Integer *idx = interpret(aa->index, state);
            return builtins(arr, 1, &idx, (Str){"get", 3}, state);
        }
        //TODO generalize to objects
        case AST_INDEX_ASSIGNMENT: {
            AstIndexAssignment *ia = (AstIndexAssignment *) ast;
            Value obj = interpret(ia->object, state);
            Integer *idx = interpret(ia->index, state);
            Value val = interpret(ia->value, state);
            Value *args = malloc(sizeof(Value) * 2);
            args[0] = idx; args[1] = val;
            builtins(obj, 1, args, (Str){"set", 3}, state);
            return val;
        }
        /*
        case AST_OBJECT: {
            AstObject *object = (AstObject *) ast;
            Value sz = interpret(object->member_cnt, state);
            Value obj = object_alloc(sz.integer, state);
            obj.object->kind = VK_OBJECT;
            obj.object->parent = interpret(object->extends, state);
            obj.object->field_cnt = sz.integer;
            for (size_t i = 0; i < sz.integer; i++) {
                //TODO add check if ast object definition
                AstDefinition *member = object->members[i];
                obj.object->fields[i].name = member->name;
                obj.object->fields[i].value = interpret(member->value, state);
            }
            return obj;
        }

        case AST_FIELD_ACCESS: { 
            AstFieldAccess *fa = (AstFieldAccess *) ast;
            Value obj = interpret(fa->object, state);
            for (size_t i = 0; i < obj.object->field_cnt; i++) {
                if (my_str_cmp(obj.object->fields[i].name, fa->field) == true) {
                    return obj.object->fields[i].value;
                }
            }
            //TODO add error handling
            return (Value){};
        }

        case AST_FIELD_ASSIGNMENT: {
            AstFieldAssignment *fa = (AstFieldAssignment *) ast;
            Value obj = interpret(fa->object, state);
            Value val = interpret(fa->value, state);
            for (size_t i = 0; i < obj.object->field_cnt; i++) {
                if (my_str_cmp(obj.object->fields[i].name, fa->field) == true) {
                    obj.object->fields[i].value = val;
                    return obj;
                }
            }
            //TODO add error handling
            return (Value){};
        }*/

        case AST_METHOD_CALL: {
            AstMethodCall *mc = (AstMethodCall *) ast;
            Value obj = interpret(mc->object, state);
            ValueKind vk = *(ValueKind *)obj;
            Value *args = malloc(sizeof(Value) * mc->argument_cnt);
            Value val;
            //TODO add cmpt if name is SET or GET
            if (vk == VK_INTEGER || vk == VK_BOOLEAN || vk == VK_NULL) {
                for (size_t i = 0; i < mc->argument_cnt; i++) {
                    args[i] = interpret(mc->arguments[i], state);
                }
                val = builtins(obj, mc->argument_cnt, args, mc->name, state);
            }
            /*else {
                Value val;
                for (size_t i = 0; i < obj.object->field_cnt; i++) {
                    if (my_str_cmp(obj.object->fields[i].name, mc->name) == true) {
                        val = interpret(obj.object->fields[i].value.function, state);
                        return val;
                }
            }*/
            //TODO add error handling
            free(args);
            return val;
        }

        default: {
            printf("Ast node not implemented\n");
            return NULL;
        }
    }
}
