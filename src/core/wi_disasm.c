#include "wi_disasm.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <vadefs.h>

#include "wi_box.h"
#include "wi_state.h"
#include "wi_value.h"

static int
_simple_instr(int offset, const char* format, ...) {
    va_list args;
    va_start(args, format);

    vprintf(format, args);
    printf("\n");

    va_end(args);
    return offset + 1;
}

static int
_byte_instr(const char* name, const char* arg_name, wi_prototype_t* prototype, int offset) {
    printf("%-16s %hhu (%s)\n", name, prototype->bytes.data[offset + 1], arg_name);
    return offset + 2;
}

static int
_constant_instr(const char* name, const char* arg_name, wi_prototype_t* prototype, int offset) {
    uint16_t   constant = prototype->bytes.data[offset + 1] << 8 | prototype->bytes.data[offset + 2];
    wi_value_t value    = prototype->constants.data[constant];

    printf("%-16s ", name);
    printf("C:%05hu ", constant);
    wi_value_print(value);
    printf(" (%s)\n", arg_name);

    return offset + 3;
}

static int
_jump_instr(const char* name, int sign, wi_prototype_t* prototype, int offset) {
    uint16_t jump = prototype->bytes.data[offset + 1] << 8 | prototype->bytes.data[offset + 2];
    printf("%-16s O:%03i -> O:%03i\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int
_invoke_instr(const char* name, wi_prototype_t* prototype, int offset) {
    uint16_t     name_constant = prototype->bytes.data[offset + 1] << 8 | prototype->bytes.data[offset + 2];
    uint8_t      arg_count     = prototype->bytes.data[offset + 3];
    wi_string_t* name_box      = wi_value_as_string(prototype->constants.data[name_constant]);

    printf("%-16s C%05hu %s (%hhu args, including receiver)\n", name, name_constant, name_box->chars, arg_count);
    return offset + 4;
}

int
wi_prototype_disasm_instr(wi_prototype_t* prototype, int offset) {
    printf("%04i ", offset);
    int line = prototype->lines.data[offset];

    if (offset > 0 && line == prototype->lines.data[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4i ", line);
    }

    wi_opcode_t opcode = prototype->bytes.data[offset];

    switch (opcode) {
        case WI_OP_PUSH:
            return _constant_instr("push", "constant index", prototype, offset);
        case WI_OP_PUSH_NULL:
            return _simple_instr(offset, "push_null");
        case WI_OP_PUSH_TRUE:
            return _simple_instr(offset, "push_true");
        case WI_OP_PUSH_FALSE:
            return _simple_instr(offset, "push_false");
        case WI_OP_POP:
            return _simple_instr(offset, "pop");
        case WI_OP_DEF_GLOBAL:
            return _constant_instr("def_global", "global name", prototype, offset);
        case WI_OP_SET_GLOBAL:
            return _constant_instr("set_global", "global name", prototype, offset);
        case WI_OP_GET_GLOBAL:
            return _constant_instr("get_global", "global name", prototype, offset);
        case WI_OP_STORE_LOCAL:
            return _byte_instr("store_local", "local slot", prototype, offset);
        case WI_OP_LOAD_LOCAL:
            return _byte_instr("load_local", "local slot", prototype, offset);
        case WI_OP_LOAD_LOCAL_0:
        case WI_OP_LOAD_LOCAL_1:
        case WI_OP_LOAD_LOCAL_2:
        case WI_OP_LOAD_LOCAL_3:
        case WI_OP_LOAD_LOCAL_4:
        case WI_OP_LOAD_LOCAL_5:
        case WI_OP_LOAD_LOCAL_6:
        case WI_OP_LOAD_LOCAL_7:
        case WI_OP_LOAD_LOCAL_8:
            return _simple_instr(offset, "load_local_%i", opcode - WI_OP_LOAD_LOCAL_0);
        case WI_OP_ADD:
            return _simple_instr(offset, "add");
        case WI_OP_SUBTRACT:
            return _simple_instr(offset, "subtract");
        case WI_OP_MULTIPLY:
            return _simple_instr(offset, "multiply");
        case WI_OP_DIVIDE:
            return _simple_instr(offset, "divide");
        case WI_OP_NEGATE:
            return _simple_instr(offset, "negate");
        case WI_OP_POWER:
            return _simple_instr(offset, "power");
        case WI_OP_MODULO:
            return _simple_instr(offset, "modulo");
        case WI_OP_GREATER:
            return _simple_instr(offset, "greater");
        case WI_OP_GREATER_EQUAL:
            return _simple_instr(offset, "greater_equal");
        case WI_OP_LESS:
            return _simple_instr(offset, "less");
        case WI_OP_LESS_EQUAL:
            return _simple_instr(offset, "less_equal");
        case WI_OP_EQUAL:
            return _simple_instr(offset, "equal");
        case WI_OP_NOT_EQUAL:
            return _simple_instr(offset, "not_equal");
        case WI_OP_LOG_NOT:
            return _simple_instr(offset, "log_not");
        case WI_OP_BIT_AND:
            return _simple_instr(offset, "bit_and");
        case WI_OP_BIT_OR:
            return _simple_instr(offset, "bit_or");
        case WI_OP_BIT_XOR:
            return _simple_instr(offset, "bit_xor");
        case WI_OP_BIT_NOT:
            return _simple_instr(offset, "bit_not");
        case WI_OP_BIT_SHL:
            return _simple_instr(offset, "bit_shl");
        case WI_OP_BIT_SHR:
            return _simple_instr(offset, "bit_shr");
        case WI_OP_LEN:
            return _simple_instr(offset, "len");
        case WI_OP_CONCAT:
            return _simple_instr(offset, "concat");
        case WI_OP_JUMP:
            return _jump_instr("jump", 1, prototype, offset);
        case WI_OP_JUMP_IF_FALSE:
            return _jump_instr("jump_if_false", 1, prototype, offset);
        case WI_OP_LOOP:
            return _jump_instr("loop", -1, prototype, offset);
        case WI_OP_LOOP_END:
            return _simple_instr(offset, "invalid opcode");
        case WI_OP_PUSH_ARRAY:
            return _constant_instr("push_array", "item count", prototype, offset);
        case WI_OP_PUSH_MAP:
            return _constant_instr("push_map", "item count", prototype, offset);
        case WI_OP_SUBSCRIPT_SET:
            return _simple_instr(offset, "subscript_set");
        case WI_OP_SUBSCRIPT_GET:
            return _simple_instr(offset, "subscript_get");
        case WI_OP_PUSH_CLOSURE: {
            offset++;

            uint16_t   constant        = prototype->bytes.data[offset] << 8 | prototype->bytes.data[offset + 1];
            wi_value_t prototype_value = prototype->constants.data[constant];

            printf("%-16s ", "push_closure");
            wi_value_print(prototype_value);
            printf("\n");

            offset += 2;

            wi_prototype_t* closure_prototype = wi_value_as_prototype(prototype_value);

            for (int i = 0; i < closure_prototype->upvalue_count; i++) {
                uint8_t index    = closure_prototype->bytes.data[offset++];
                uint8_t is_local = closure_prototype->bytes.data[offset++];
                printf("    %04i    | %-16s at %hhu\n", offset - 2, is_local ? "local" : "upvalue", index);
            }

            return offset;
        }
        case WI_OP_STORE_UPVALUE:
            return _byte_instr("store_upvalue", "upvalue index", prototype, offset);
        case WI_OP_LOAD_UPVALUE:
            return _byte_instr("load_upvalue", "upvalue index", prototype, offset);
        case WI_OP_CLOSE_UPVALUE:
            return _simple_instr(offset, "close_upvalue");
        case WI_OP_CALL:
            return _byte_instr("call", "argument count", prototype, offset);
        case WI_OP_TAIL_CALL:
            return _byte_instr("tail_call", "argument count", prototype, offset);
        case WI_OP_INVOKE:
            return _invoke_instr("invoke", prototype, offset);
        case WI_OP_TAIL_INVOKE:
            return _invoke_instr("tail_invoke", prototype, offset);
        case WI_OP_RETURN:
            return _simple_instr(offset, "return");
        case WI_OP_PUSH_OBJECT: {
            offset++;

            uint16_t field_count = prototype->bytes.data[offset] << 8 | prototype->bytes.data[offset + 1];
            uint8_t  has_name    = prototype->bytes.data[offset + 2];
            offset += 3;

            printf("%-16s ", "push_object");

            if (has_name) {
                uint16_t     constant = prototype->bytes.data[offset] << 8 | prototype->bytes.data[offset + 1];
                wi_string_t* name     = wi_value_as_string(prototype->constants.data[constant]);

                printf("C:%05hu %s (name)", constant, name->chars);

                offset += 2;
            } else {
                printf("(anonymous)");
            }

            printf(" (%hu fields) \n", field_count);
            return offset;
        }
        case WI_OP_REQUIRE: {
            offset++;

            uint16_t path_constant = prototype->bytes.data[offset] << 8 | prototype->bytes.data[offset + 1];
            uint16_t name_constant = prototype->bytes.data[offset + 2] << 8 | prototype->bytes.data[offset + 3];

            offset += 4;

            char* path = wi_value_as_cstring(prototype->constants.data[path_constant]);
            char* name = wi_value_as_cstring(prototype->constants.data[name_constant]);
            printf("require '%s' = %s;\n", path, name);

            return offset;
        }
        case WI_OP_INIT_FIELD:
            return _constant_instr("init_field", "field name", prototype, offset);
        case WI_OP_SET_FIELD:
            return _constant_instr("set_field", "field name", prototype, offset);
        case WI_OP_GET_FIELD:
            return _constant_instr("get_field", "field name", prototype, offset);
        case WI_OP_NEW:
            return _simple_instr(offset, "new");
    }

    printf("invalid opcode %hhu\n", opcode);
    return offset + 1;
}

void
wi_prototype_disasm(wi_prototype_t* prototype) {
    if (prototype->is_main) {
        printf("--- main function (%s) ---\n", prototype->file_path);
    } else if (prototype->name) {
        printf("--- %s() (%s) ---\n", prototype->name->chars, prototype->file_path);
    } else {
        printf("--- anonymous function (%s) ---\n", prototype->file_path);
    }

    printf("bytes: %i\n", prototype->bytes.count);
    printf("constants: %i\n", prototype->constants.count);

    for (int i = 0; i < prototype->constants.count; i++) {
        wi_value_t value = prototype->constants.data[i];

        printf("    C:%05i ", i);
        wi_value_print(value);
        printf(" (%s)\n", wi_value_type(value));
    }

    printf("instructions:\n");

    for (int offset = 0; offset < prototype->bytes.count;) {
        printf("    ");
        offset = wi_prototype_disasm_instr(prototype, offset);
    }
}
