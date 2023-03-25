//
// Created by Mirek Škrabal on 25.03.2023.
//

#include <printf.h>
#include "utils.h"

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
            for (size_t i = 0; i < obj->field_cnt; i++) {
                //print_my_str(obj->val[i].name);
                printf("%.*s", (int)obj->val[i].name.len, obj->val[i].name.str);
                printf("=");
                print_val(obj->val[i].val);
                if (i != obj->field_cnt - 1) {
                    printf(", ");
                }
            }
            printf(")");
            break;
        }
        default: {
            printf("unknown value kind");
            break;
        }
    }
}

void print_op_stack(Value *stack, size_t size) {
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

bool is_primitive(ValueKind kind){
    return kind == VK_BOOLEAN || kind == VK_INTEGER || kind == VK_FUNCTION || kind == VK_NULL;
}
