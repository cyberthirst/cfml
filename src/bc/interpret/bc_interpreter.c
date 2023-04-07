//
// Created by Mirek Škrabal on 23.03.2023.
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "bc_interpreter.h"
#include "../../heap/heap.h"
#include "../../utils.h"
#include "../bc_shared_globals.h"

//we can have max 1024 * 16 ptrs to the heap
#define MAX_OPERANDS (1024 * 16)
#define MAX_FRAMES (1024 * 16)

typedef struct {
    uint8_t *ret_addr;
    //ptrs to the heap
    uint8_t **locals;
    size_t locals_sz;
} Frame;

//struct for representing the inner state of the itp
typedef struct {
    //instruction pointer
    uint8_t *ip;
    Frame *frames;
    size_t frames_sz;
    //operands stack
    //ptrs to the heap
    Value *operands;
    size_t op_sz;
} Bc_Interpreter;

//GLOBAL VARIABLES
Bc_Interpreter *itp;
Heap *heap;
Value global_null;

void bc_init() {
    itp = malloc(sizeof(Bc_Interpreter));
    itp->ip = const_pool_map[entry_point];
    itp->frames = malloc(sizeof(Frame) * MAX_FRAMES);
    //we have 1 frame at the beginning for global frame
    itp->frames_sz = 0;
    itp->operands = malloc(sizeof(void *) * MAX_OPERANDS);
    itp->op_sz = 0;
    heap = malloc(sizeof(Heap));
    heap->heap_start = malloc(MEM_SZ);
    heap->heap_free = heap->heap_start;
    heap->heap_size = 0;
    global_null = construct_null(heap);
    //array that acts like a hash map - we just allocate as big array as there are constants
    globals.values = malloc(sizeof(void *) * const_pool_count);
    //TODO could we use memset here or smth similar?
    for (int i = 0; i < const_pool_count; i++) {
        globals.values[i] = global_null;
    }
}

void bc_free() {
    free(itp->frames);
    free(itp->operands);
    free(itp);
    free(const_pool);
    free(const_pool_map);
    free(heap->heap_start);
    free(heap);
    free(globals.values);
    free(globals.indexes);
}

Value pop_operand() {
    assert(itp->op_sz > 0);
    return itp->operands[--itp->op_sz];
}

void pop_n_operands(size_t n) {
    assert(itp->op_sz >= n);
    itp->op_sz -= n;
}

Value peek_operand() {
    assert(itp->op_sz > 0);
    return itp->operands[itp->op_sz - 1];
}

void push_operand(uint8_t *value) {
    assert(itp->op_sz < MAX_OPERANDS);
    itp->operands[itp->op_sz++] = value;
}

void push_frame() {
    assert(itp->frames_sz < MAX_FRAMES);
    itp->frames_sz++;
}

void pop_frame() {
    assert(itp->frames_sz > 0);
    free(itp->frames[--itp->frames_sz].locals);
}

bool index_is_global(uint16_t index) {
    assert(*const_pool_map[index] == VK_STRING);
    for (int i = 0; i < globals.count; ++i) {
        if (globals.indexes[i] == index) {
            return true;
        }
    }
    return false;
}

void exec_drop() {
    //assert that there is something to be dropped
    assert(itp->op_sz > 0);
    pop_operand();
}

void init_frame(uint8_t argc, bool is_method) {
    //we will pop argc args
    assert(itp->op_sz >= argc);
    //itp->frames[itp->frames_sz].locals = malloc(fun->params + fun->locals);
    itp->frames[itp->frames_sz].locals = malloc(sizeof(uint8_t *) * (argc + 1));
    for (int i = is_method ? argc - 1: argc; i > 0; --i) {
        itp->frames[itp->frames_sz].locals[i] = pop_operand();
    }
    if (is_method) {
        //set the receiver
        Value obj = pop_operand();
        itp->frames[itp->frames_sz].locals[0] = obj;
    }
    else {
        //for normal function calls set the receiver to null
        itp->frames[itp->frames_sz].locals[0] = global_null;
    }


}

void init_fun_call(uint8_t argc, bool is_method) {
    Bc_Func *fun = (Bc_Func *)pop_operand();
    assert(fun->kind == VK_FUNCTION);
    //TODO: we can read the function from the stack beforehand and prevent realloc
    //doing this because it's simple :)
    itp->frames[itp->frames_sz].locals = realloc(itp->frames[itp->frames_sz].locals,
                                                sizeof(uint8_t *)*(fun->params + fun->locals));
    assert(is_method ? argc == fun->params : argc + 1 == fun->params);
    //set the rest of the locals to null
    for (int i = argc + 1; i < fun->params + fun->locals; ++i) {
        itp->frames[itp->frames_sz].locals[i] = global_null;
    }
    itp->frames[itp->frames_sz].locals_sz = fun->params + fun->locals;

    //set the return address
    itp->frames[itp->frames_sz].ret_addr = itp->ip;
    push_frame();
    itp->ip = fun->bytecode;
}

void exec_constant() {
    uint16_t index = deserialize_u16(itp->ip);

    //print_heap(heap);
    switch (*const_pool_map[index]) {
        case VK_INTEGER: {
            Integer *integer = (Integer *)const_pool_map[index];
            assert(integer->kind == VK_INTEGER);
            push_operand(construct_integer(integer->val, heap));
            break;
        }
        case VK_NULL: {
            Null *null = (Null *)const_pool_map[index];
            assert(null->kind == VK_NULL);
            push_operand(global_null);
            break;
        }
        case VK_BOOLEAN: {
            Boolean *boolean = (Boolean *)const_pool_map[index];
            assert(boolean->kind == VK_BOOLEAN);
            push_operand(construct_boolean(boolean->val, heap));
            break;
        }
        case VK_STRING: {
            Bc_String *string = (Bc_String *)const_pool_map[index];
            assert(string->kind == VK_STRING);
            push_operand(construct_bc_string(string, heap));
            break;
        }

        case VK_FUNCTION: {
            Bc_Func *function = (Bc_Func *)const_pool_map[index];
            assert(function->kind == VK_FUNCTION);
            push_operand(construct_bc_function(function, heap));
            break;
        }
        default: {
            printf("Unknown constant type\n");
            exit(1);
        }
    }
    //print_heap(heap);
    itp->ip += 2;
}

void exec_print() {
    //assert that there is index to the constant pool, where the format str is stored
    //assert(itp.op_sz);
    uint16_t index = deserialize_u16(itp->ip);
    itp->ip += 2;
    uint8_t num_args = *itp->ip;
    itp->ip += 1;
    Bc_String *prnt = (Bc_String *)const_pool_map[index];
    assert(prnt->kind == VK_STRING);

    //pop all the operands at once, see the comment before print_val
    pop_n_operands(num_args);
    size_t tilda = 0;
    for (size_t i = 0; i < prnt->len; i++) {
        if (prnt->value[i] == '~') {
            //we popped num_args operands from the stack
            //thus the current stack pointer points to the first argument of the print
            //arg1 is the first field of the "print_args" array thus we only add tilda
            print_val(itp->operands[itp->op_sz + tilda]);
            //print_val((Value)pop_operand());
            tilda++;
        }
        else {
            char c = prnt->value[i];
            //check for escape characters
            if (c == '\\' && prnt->len > i + 1) {
                char next = prnt->value[i + 1];
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
                printf("%c", prnt->value[i]);
            }
        }
    }
    push_operand(global_null);
}

void exec_return() {
    assert(itp->frames_sz > 0);
    itp->ip = itp->frames[--itp->frames_sz].ret_addr;
    free(itp->frames[itp->frames_sz].locals);
}

void exec_get_local() {
    uint16_t index = deserialize_u16(itp->ip);
    itp->ip += 2;
    //printf("get local: \n");
    //print_val((Value)itp->frames[itp->frames_sz - 1].locals[index]);
    push_operand(itp->frames[itp->frames_sz - 1].locals[index]);
}

void exec_set_local() {
    uint16_t index = deserialize_u16(itp->ip);
    itp->ip += 2;
    itp->frames[itp->frames_sz - 1].locals[index] = peek_operand();
}

void exec_set_global() {
    uint16_t index = deserialize_u16(itp->ip);
    assert(index_is_global(index));
    itp->ip += 2;
    globals.values[index] = peek_operand();
}

void exec_get_global() {
    uint16_t index = deserialize_u16(itp->ip);
    assert(index_is_global(index));
    itp->ip += 2;
    push_operand(globals.values[index]);
}

void exec_jump() {
    int16_t offset = deserialize_i16(itp->ip);
    itp->ip += 2;
    itp->ip += offset;
}

void exec_branch() {
    int16_t offset = deserialize_i16(itp->ip);
    itp->ip += 2;
    if (truthiness(pop_operand()))
        itp->ip += offset;
}

void exec_call_function() {
    uint8_t argc = *itp->ip;
    itp->ip += 1;
    //Bc_Func *fun= (Bc_Func *)pop_operand();
    //assert(fun->kind == VK_FUNCTION);
    //in normal fun call the receiver is null
    init_frame(argc, false);
    init_fun_call(argc, false);
}

void exec_array() {
    Value init_val = pop_operand();
    Integer *size = (Integer *)pop_operand();
    assert(size->kind == VK_INTEGER);
    assert(size->val >= 0);
    Array *array = (Array *)construct_array(size->val, heap);
    for (int i = 0; i < size->val; i++) {
        array->val[i] = init_val;
    }
    push_operand((uint8_t *)array);
}

void exec_object() {
    uint16_t index = deserialize_u16(itp->ip);
    itp->ip += 2;
    Bc_Class *cls = (Bc_Class *)const_pool_map[index];
    assert(cls->kind == VK_CLASS);
    //construct the object with parent global_null, will modify this later
    Object *obj = (Object *)construct_object(cls->count, global_null, heap);
    //print_heap(heap);

    Bc_String *name;
    assert(itp->op_sz >= cls->count);
    //traverse the class fields in reverse order and set the fields of the object
    for (int i = cls->count - 1; i >= 0; i--) {
        name = (Bc_String *)const_pool_map[cls->members[i]];
        assert(name->kind == VK_STRING);
        obj->val[i].name.len = name->len;
        obj->val[i].name.str = (uint8_t *)name->value;
        obj->val[i].val = pop_operand();
    }
    obj->parent = pop_operand();
    push_operand((uint8_t *)obj);
    //print_heap(heap);
}

Field *get_field(Object *obj, Bc_String *name) {
    assert(obj->kind == VK_OBJECT);
    for (size_t i = 0; i < obj->field_cnt; ++i) {
        if (str_eq(obj->val[i].name, (Str){name->value, name->len})) {
            return &obj->val[i];
        }
    }
    //if the parent is of primitive type then the field is not found
    if (*obj->parent == VK_OBJECT)
        return get_field((Object *)obj->parent, name);
    printf("field not found: %s", name->value);
    exit(1);
}

void exec_get_field() {
    uint16_t index = deserialize_u16(itp->ip);
    itp->ip += 2;
    Bc_String *name = (Bc_String *)const_pool_map[index];
    assert(name->kind == VK_STRING);
    Object *obj = (Object *)pop_operand();
    assert(obj->kind == VK_OBJECT);
    Field *field = get_field(obj, name);
    push_operand(field->val);
}

void exec_set_field() {
    uint16_t index = deserialize_u16(itp->ip);
    itp->ip += 2;
    Bc_String *name = (Bc_String *)const_pool_map[index];
    assert(name->kind == VK_STRING);
    //val is new value for field name
    Value val = (Value)pop_operand();
    Object *obj = (Object *)pop_operand();
    assert(obj->kind == VK_OBJECT);
    Field *field = get_field(obj, name);
    field->val = val;
    push_operand(val);
}

uint8_t *get_nth_local(size_t n) {
    assert(n <= itp->frames[itp->frames_sz - 1].locals_sz);
    //TODO this is hacky.. for builtins we don't call init_fun_call and thus a new frame is not created
    return itp->frames[itp->frames_sz].locals[n];
}

void bc_builtins(Value obj, int argc, Str m_name) {
    /*
        FROM REFERENCE IMPLEMENTATION
    */
    const u8 *method_name = m_name.str;
    size_t method_name_len = m_name.len;
    #define METHOD(name) \
			if (sizeof(name) - 1 == method_name_len && memcmp(name, method_name, method_name_len) == 0) /* body*/
    print_op_stack(itp->operands, itp->op_sz);
    if (*obj == VK_INTEGER || *obj == VK_BOOLEAN || *obj == VK_NULL) {
        assert(argc == 2);
        Value second = get_nth_local(1);
        METHOD("+") {
            assert(*obj == VK_INTEGER && *second == VK_INTEGER);
            push_operand(construct_integer(((Integer *)obj)->val + ((Integer *)second)->val, heap));
            return;
        }
        METHOD("-") {
            assert(*obj == VK_INTEGER && *second == VK_INTEGER);
            push_operand(construct_integer(((Integer *)obj)->val - ((Integer *)second)->val, heap));
            return;
        }
        METHOD("*") {
            assert(*obj == VK_INTEGER && *second == VK_INTEGER);
            push_operand(construct_integer(((Integer *)obj)->val * ((Integer *)second)->val, heap));
            return;
        }
        METHOD("/") {
            assert(*obj == VK_INTEGER && *second == VK_INTEGER);
            push_operand(construct_integer(((Integer *)obj)->val / ((Integer *)second)->val, heap));
            return;
        }
        METHOD("%") {
            assert(*obj == VK_INTEGER && *second == VK_INTEGER);
            push_operand(construct_integer(((Integer *)obj)->val % ((Integer *)second)->val, heap));
            return;
        }
        METHOD("<=") {
            assert(*obj == VK_INTEGER && *second == VK_INTEGER);
            push_operand(construct_boolean(((Integer *)obj)->val <= ((Integer *)second)->val, heap));
            return;
        }
        METHOD(">=") {
            assert(*obj == VK_INTEGER && *second == VK_INTEGER);
            push_operand(construct_boolean(((Integer *)obj)->val >= ((Integer *)second)->val, heap));
            return;
        }
        METHOD(">") {
            assert(*obj == VK_INTEGER && *second == VK_INTEGER);
            push_operand(construct_boolean(((Integer *)obj)->val > ((Integer *)second)->val, heap));
            return;
        }
        METHOD("<") {
            assert(*obj == VK_INTEGER && *second == VK_INTEGER);
            push_operand(construct_boolean(((Integer *)obj)->val < ((Integer *)second)->val, heap));
            return;
        }
        METHOD("==") {
            if (*obj == VK_INTEGER && *second != VK_INTEGER)
                push_operand(construct_boolean(false, heap));
            else if (*obj == VK_INTEGER && *second == VK_INTEGER)
                push_operand(construct_boolean(((Integer *)obj)->val == ((Integer *)second)->val, heap));
            else if (*obj == VK_NULL && *second != VK_NULL)
                push_operand(construct_boolean(false, heap));
            else if (*obj == VK_NULL && *second == VK_NULL)
                push_operand(construct_boolean(true, heap));
            else if (*obj == VK_BOOLEAN && *second != VK_BOOLEAN)
                push_operand(construct_boolean(false, heap));
            else
                push_operand(construct_boolean(((Boolean *) obj)->val == ((Boolean *) second)->val, heap));
            return;
        }
        METHOD("!=") {
            if (*obj == VK_INTEGER && *second != VK_INTEGER)
                push_operand(construct_boolean(true, heap));
            else if (*obj == VK_INTEGER && *second == VK_INTEGER)
                push_operand(construct_boolean(((Integer *)obj)->val != ((Integer *)second)->val, heap));
            else if (*obj == VK_NULL && *second != VK_NULL)
                push_operand(construct_boolean(true, heap));
            else if (*obj == VK_NULL && *second == VK_NULL)
                push_operand(construct_boolean(false, heap));
            else if (*obj == VK_BOOLEAN && *second != VK_BOOLEAN)
                push_operand(construct_boolean(true, heap));
            else
                push_operand(construct_boolean(((Boolean *) obj)->val != ((Boolean *) second)->val, heap));
            return;
        }
    }
    if (*obj == VK_BOOLEAN) {
        assert(argc == 2);
        Value second = get_nth_local(1);
        METHOD("&") {
            assert(*obj == VK_BOOLEAN && *second == VK_BOOLEAN);
            push_operand(construct_boolean(((Boolean *)obj)->val & ((Boolean *)second)->val, heap));
            return;
        }
        METHOD("|") {
            assert(*obj == VK_BOOLEAN && *second == VK_BOOLEAN);
            push_operand(construct_boolean(((Boolean *)obj)->val | ((Boolean *)second)->val, heap));
            return;
        }
    }

    METHOD("set") {
        if (*obj == VK_ARRAY) {
            Value index = get_nth_local(1);
            Value val = get_nth_local(2);
            assert(*index == VK_INTEGER);
            Array *array = (Array *)obj;
            //TODO this is fishy: should i just peek?
            //Array(arr)	set	Integer(i), v	arr(i) ← v; v
            array->val[((Integer *)index)->val] = val;
            push_operand(val);
        }
        return;
    }

    METHOD("get") {
        if (*obj == VK_ARRAY) {
            Array *array = (Array *)obj;
            Integer *index = (Integer *) get_nth_local(1);
            assert(index->kind == VK_INTEGER);
            assert(index->val >= 0 && (size_t)index->val < array->size);
            push_operand(array->val[index->val]);
        }
        return;
    }
    printf("Unknown built-in method: ");
    printf("%.*s\n", (int)m_name.len, m_name.str);
    exit(1);
}

void bc_method_call(Value obj, Str name, int argc) {
    Object *object = (Object *)obj;
    //if inheriting from a primitive type then call the builtin
    if (object->kind != VK_OBJECT) {
        bc_builtins(obj, argc, name);
        return;
    }
    for (size_t i = 0; i < object->field_cnt; i++) {
        //if we find the method, then we just need to set the instruction pointer
        //and prepare the locals, rest will be handled in the bytecode_loop
        if (str_eq(object->val[i].name, name)) {
            Field field = object->val[i];
            assert(*field.val == VK_FUNCTION);
            Function *func = (Function *)field.val;
            //we push here the pointer to the function object
            //this function object will be popped by the init_fun_call function
            push_operand((uint8_t *)func);
            init_fun_call(argc, true);
            return;
        }
    }
    bc_method_call(object->parent, name, argc);
}


void exec_call_method() {
    //index for the method to be called
    uint16_t index = deserialize_u16(itp->ip);
    itp->ip += 2;
    uint8_t argc = *itp->ip;
    itp->ip += 1;

    //name of the method
    Bc_String *m_name = (Bc_String *)const_pool_map[index];

    assert(m_name->kind == VK_STRING);

    init_frame(argc, true);
    print_op_stack(itp->operands, itp->op_sz);

    Object *obj = (Object *)itp->frames[itp->frames_sz].locals[0];

    bc_method_call((Value)obj, (Str) {m_name->value, m_name->len}, argc);
}

void bytecode_loop(){
    while (itp->frames_sz) {
        //print_heap(heap);
        print_instruction_type(*itp->ip);
        switch (*itp->ip++) {
            case DROP: {
                exec_drop();
                break;
            }
            case CONSTANT: {
                exec_constant();
                break;
            }
            case PRINT:
                exec_print();
                break;
            case ARRAY:
                exec_array();
                break;
            case OBJECT:
                exec_object();
                break;
            case GET_FIELD:
                exec_get_field();
                break;
            case SET_FIELD:
                exec_set_field();
                break;
            case CALL_METHOD:
                exec_call_method();
                break;
            case CALL_FUNCTION:
                exec_call_function();
                break;
            case SET_LOCAL:
                exec_set_local();
                break;
            case GET_LOCAL:
                exec_get_local();
                break;
            case SET_GLOBAL:
                exec_set_global();
                break;
            case GET_GLOBAL:
                exec_get_global();
                break;
            case BRANCH:
                exec_branch();
                break;
            case JUMP:
                exec_jump();
                break;
            case RETURN:
                exec_return();
                break;
            default:
                printf("Unknown instruction: 0x%02X\n", *itp->ip);
                exit(1);
        }
        print_op_stack(itp->operands, itp->op_sz);
    }
}


void bc_interpret() {
    bc_init();
    //we push the etry point function to the operand stack
    //this function will be popped by the init_fun_call function
    push_operand(itp->ip);
    init_frame(0, false);
    init_fun_call(0, false);
    bytecode_loop();
    bc_free();
}

uint8_t *align_address(uint8_t *ptr){
    size_t diff = 8 - ((uintptr_t)ptr % 8);
    if (diff != 8) {
        ptr += diff;
    }
    return ptr;
}

void deserialize_bc_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Cannot open file %s\n", filename);
        exit(1);
    }

    // Read and check the header
    uint8_t header[4];
    fread(header, sizeof(uint8_t), 4, file);
    if (header[0] != 0x46 || header[1] != 0x4D || header[2] != 0x4C || header[3] != 0x0A) {
        printf("Error: Invalid header\n");
        fclose(file);
        exit(1);
    }

    // Read how many objs are in the const pool
    fread(&const_pool_count, sizeof(uint16_t), 1, file);

    // Allocate the const pool
    // The pool has constant size, we don't initially know how big the objs actually are
    // If the pool is full we have to exit(1)
    //TODO implement the checks on cp size
    const_pool = malloc(CONST_POOL_SZ);

    //we allocate + 1 because in the for-loop below we always assign the addr for i+1th element
    //this would be annoying to solve for the last elem
    const_pool_map  = malloc(sizeof(void*) * (const_pool_count + 1));

    //the first obj start at the beginning of the const_pool
    const_pool_map[0] = const_pool;
    //tmp array for deserialization
    uint8_t tmp_data[4];
    // Read the constant pool objects and fill the const_pool & const_pool_map array
    uint8_t tag;
    for (uint16_t i = 0; i < const_pool_count; ++i) {
        fread(&tag, sizeof(uint8_t), 1, file);

        switch (tag) {
            case VK_INTEGER: {
                Integer *integer = (Integer  *)const_pool_map[i];
                integer->kind = tag;
                fread(&tmp_data, sizeof(int32_t), 1, file);
                integer->val = (tmp_data[0]<<0) | (tmp_data[1]<<8) | (tmp_data[2]<<16) | ((uint32_t)tmp_data[3]<<24);
                //we use ValueKind and not uint8_t as tag, thus we don't have jsut sizeof(Integer)
                const_pool_map[i + 1] = align_address(const_pool_map[i] + sizeof(Integer));
                break;
            }
            case VK_BOOLEAN: {
                Boolean *boolean = (Boolean  *)const_pool_map[i];
                boolean->kind = tag;
                fread(&boolean->val, sizeof(uint8_t), 1, file);
                const_pool_map[i + 1] = align_address(const_pool_map[i] + sizeof(Boolean));
                break;
            }
            case VK_NULL: {
                Null *null = (Null  *)const_pool_map[i];
                null->kind = tag;
                const_pool_map[i + 1] = align_address(const_pool_map[i] + sizeof(Null));
                break;
            }
            case VK_STRING: {
                Bc_String *string = (Bc_String  *)const_pool_map[i];
                string->kind = tag;
                fread(&string->len, sizeof(uint32_t), 1, file);
                fread(string->value, sizeof(char), string->len, file);
                string->value[string->len] = '\0';
                const_pool_map[i + 1] = align_address(const_pool_map[i] + sizeof(Bc_String) + string->len);
                break;
            }
            case VK_FUNCTION: {
                Bc_Func *function = (Bc_Func  *)const_pool_map[i];
                function->kind = tag;
                fread(&function->params, sizeof(uint8_t), 1, file);
                fread(&tmp_data, sizeof(uint16_t), 1, file);
                function->locals = (tmp_data[0]<<0) | (tmp_data[1]<<8);
                fread(&tmp_data, sizeof(uint32_t), 1, file);
                function->len = (tmp_data[0]<<0) | (tmp_data[1]<<8) | (tmp_data[2]<<16) | (tmp_data[3]<<24);
                //we don't have a special deserialization for bytecode, bytecode is deserialized upon execution
                fread(function->bytecode, sizeof(uint8_t), function->len, file);
                const_pool_map[i + 1] = align_address(const_pool_map[i] + sizeof(Bc_Func) + function->len);
                break;
            }
            case VK_CLASS: {
                Bc_Class *class = (Bc_Class *)const_pool_map[i];
                class->kind = tag;
                fread(&tmp_data, sizeof(uint16_t), 1, file);
                class->count = (tmp_data[0]<<0) | (tmp_data[1]<<8);
                for (uint16_t j = 0; j < class->count; ++j) {
                    fread(&tmp_data, sizeof(uint16_t), 1, file);
                    class->members[j] = (tmp_data[0]<<0) | (tmp_data[1]<<8);
                }
                const_pool_map[i + 1] = align_address(const_pool_map[i] + sizeof(Bc_Class) + sizeof(uint16_t) * class->count);
                break;
            }
            default: {
                printf("Error: Invalid kind\n");
                fclose(file);
                exit(1);
            }
        }
    }

    // Read the globals
    fread(&tmp_data, sizeof(uint16_t), 1, file);
    globals.count = (tmp_data[0]<<0) | (tmp_data[1]<<8);
    globals.indexes = malloc(sizeof(uint16_t) * globals.count);
    fread(globals.indexes, sizeof(uint16_t), globals.count, file);
    uint8_t *tmp_i = (uint8_t *)globals.indexes;
    for (uint16_t i = 0; i < globals.count; i += 1) {
        globals.indexes[i] = (tmp_i[2*i]<<0) | (tmp_i[2*i + 1]<<8);
    }

    // Read the entry point
    fread(&tmp_data, sizeof(uint16_t), 1, file);
    entry_point = (tmp_data[0]<<0) | (tmp_data[1]<<8);
    assert(entry_point < const_pool_count);

    // Cleanup
    fclose(file);
}
