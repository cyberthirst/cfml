//
// Created by Mirek Škrabal on 25.03.2023.
//

#include <stdio.h>

#include "../utils.h"
#define DEBUG 0
#if DEBUG
    #define PRINT_IF_DEBUG_ON
#else
    #define PRINT_IF_DEBUG_ON return
#endif


uint16_t deserialize_u16(const uint8_t *data) {
    return (data[0]<<0) | (data[1]<<8);
}

int16_t deserialize_i16(const uint8_t *data) {
    return (data[0]<<0) | (data[1]<<8);
}

uint32_t deserialize_u32(const uint8_t *data) {
    return (data[0]<<0) | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
}

bool truthiness(Value val) {
    if (*val == VK_NULL) {
        return false;
    }
    if (*val == VK_BOOLEAN) {
        Boolean *boolean = (Boolean *)val;
        return boolean->val;
    }
    return true;
}

// Comparison function for qsort
int field_cmp(const void *a, const void *b) {
    Field *field1 = *(Field **)a;
    Field *field2 = *(Field **)b;
    size_t min_length = field1->name.len < field2->name.len ? field1->name.len : field2->name.len;
    int cmp = strncmp(field1->name.str, field2->name.str, min_length);

    if (cmp != 0) {
        return cmp;
    } else {
        return field1->name.len - field2->name.len;
    }
}

uint8_t *align_address(uint8_t *ptr){
    size_t diff = 8 - ((uintptr_t)ptr % 8);
    if (diff != 8) {
        ptr += diff;
    }
    return ptr;
}

void print_val(Value val) {
    switch(*val) {
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
            for (size_t i = 0; i < ((Array *)val)->size; i++) {
                print_val(((Array *)val)->val[i]);
                if (i != ((Array *)val)->size - 1) {
                    printf(", ");
                }
            }
            printf("]");
            break;
        }
        case VK_OBJECT: {
            Object *obj = (Object *)val;
            printf("object(");
            if (*(ValueKind *)obj->parent != VK_NULL) {
                printf("..=");
                print_val(obj->parent);
                if (obj->field_cnt > 0) {
                    printf(", ");
                }
            }
            // Allocate a new array of Field pointers and copy the original Field pointers
            Field **fields = (Field **)malloc(obj->field_cnt * sizeof(Field *));
            for (size_t i = 0; i < obj->field_cnt; ++i) {
                fields[i] = &obj->val[i];
            }
            // Sort the new array of Field pointers
            qsort(fields, obj->field_cnt, sizeof(Field *), field_cmp);
            for (size_t i = 0; i < obj->field_cnt; i++) {
                //print_my_str(obj->val[i].name);
                //printf("%.*s", (int)obj->val[i].name.len, obj->val[i].name.str);
                printf("%.*s", (int)fields[i]->name.len, fields[i]->name.str);
                printf("=");
                //print_val(obj->val[i].val);
                print_val(fields[i]->val);
                if (i != obj->field_cnt - 1) {
                    printf(", ");
                }
            }
            printf(")");
            free(fields);
            break;
        }
        default: {
            printf("unknown value kind");
            break;
        }
    }
}

void print_op_stack(Value *stack, size_t size) {
    PRINT_IF_DEBUG_ON;
    printf("op_stack(:\n");
    for (size_t i = 0; i < size; ++i) {
        printf("\top %d: ", i);
        print_val(stack[i]);
        if (i != size - 1) {
            printf(",\n");
        }
        else{
            printf("  <-- TOP\n");
        }
    }
    printf(")\n");
}

void print_instruction_type(int count, const uint8_t *ip) {
    PRINT_IF_DEBUG_ON;
    printf("%d: ", count);
    switch (*ip) {
        case DROP:
            printf("DROP\n");
            break;
        case CONSTANT:
            printf("CONSTANT\n");
            break;
        case PRINT:
            printf("PRINT\n");
            break;
        case ARRAY:
            printf("ARRAY\n");
            break;
        case OBJECT:
            printf("OBJECT\n");
            break;
        case GET_FIELD:
            printf("GET_FIELD\n");
            break;
        case SET_FIELD:
            printf("SET_FIELD\n");
            break;
        case CALL_METHOD:
            printf("CALL_METHOD\n");
            break;
        case CALL_FUNCTION:
            printf("CALL_FUNCTION\n");
            break;
        case SET_LOCAL:
            printf("SET_LOCAL\n");
            break;
        case GET_LOCAL:
            printf("GET_LOCAL\n");
            break;
        case SET_GLOBAL:
            printf("SET_GLOBAL\n");
            break;
        case GET_GLOBAL:
            printf("GET_GLOBAL: %d\n", *(uint16_t *)(ip + 1));
            break;
        case BRANCH:
            printf("BRANCH\n");
            break;
        case JUMP:
            printf("JUMP\n");
            break;
        case RETURN:
            printf("RETURN\n");
            break;
        default:
            printf("Unknown instruction: 0x%02X\n", *ip);
            exit(1);
    }
}

bool is_primitive(ValueKind kind){
    return kind == VK_BOOLEAN || kind == VK_INTEGER || kind == VK_FUNCTION || kind == VK_NULL;
}
