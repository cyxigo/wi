#include "wi_disasm.h"

#include <stdarg.h>
#include <stdint.h>

#include "wi_box.h"
#include "wi_state.h"
#include "wi_util.h"
#include "wi_value.h"

static int
_simple_instr(struct wi_state* state, int offset, const char* format, ...) {
    va_list args;
    va_start(args, format);

    wi_vprintf(state->out, format, args);
    state->out("\n");

    va_end(args);
    return offset + 1;
}

static int
_byte_instr(struct wi_state* state, const char* name, const char* arg_name, struct wi_prototype* prototype,
            int offset) {
    state->out("%-16s %hhu (%s)\n", name, prototype->bytes.data[offset + 1], arg_name);
    return offset + 2;
}

static int
_short_instr(struct wi_state* state, const char* name, const char* arg_name, struct wi_prototype* prototype,
             int offset) {
    uint16_t arg = (uint16_t)(prototype->bytes.data[offset + 1] << 8 | prototype->bytes.data[offset + 2]);
    state->out("%-16s %hu (%s)\n", name, arg, arg_name);
    return offset + 3;
}

static int
_constant_instr(struct wi_state* state, const char* name, const char* arg_name, struct wi_prototype* prototype,
                int offset) {
    uint16_t constant = (uint16_t)(prototype->bytes.data[offset + 1] << 8 | prototype->bytes.data[offset + 2]);
    wi_value value    = prototype->constants.data[constant];

    state->out("%-16s ", name);
    state->out("C:%05hu ", constant);
    wi_value_print(state, value);
    state->out(" (%s)\n", arg_name);

    return offset + 3;
}

static int
_jump_instr(struct wi_state* state, const char* name, int sign, struct wi_prototype* prototype, int offset) {
    uint16_t jump = (uint16_t)(prototype->bytes.data[offset + 1] << 8 | prototype->bytes.data[offset + 2]);
    state->out("%-16s O:%03i -> O:%03i\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int
wi_prototype_disasm_instr(struct wi_state* state, struct wi_prototype* prototype, int offset) {
    state->out("%04i ", offset);
    int line = prototype->lines.data[offset];

    if (offset > 0 && line == prototype->lines.data[offset - 1]) {
        state->out("   | ");
    } else {
        state->out("%4i ", line);
    }

    uint8_t opcode = prototype->bytes.data[offset];

    switch (opcode) {
        case WI_OP_PUSH:
            return _constant_instr(state, "push", "constant index", prototype, offset);
        case WI_OP_PUSH_NULL:
            return _simple_instr(state, offset, "push_null");
        case WI_OP_PUSH_TRUE:
            return _simple_instr(state, offset, "push_true");
        case WI_OP_PUSH_FALSE:
            return _simple_instr(state, offset, "push_false");
        case WI_OP_POP:
            return _simple_instr(state, offset, "pop");
        case WI_OP_DEF_GLOBAL:
            return _constant_instr(state, "def_global", "global name", prototype, offset);
        case WI_OP_SET_GLOBAL:
            return _constant_instr(state, "set_global", "global name", prototype, offset);
        case WI_OP_GET_GLOBAL:
            return _constant_instr(state, "get_global", "global name", prototype, offset);
        case WI_OP_STORE_LOCAL:
            return _byte_instr(state, "store_local", "local slot", prototype, offset);
        case WI_OP_LOAD_LOCAL:
            return _byte_instr(state, "load_local", "local slot", prototype, offset);
        case WI_OP_LOAD_LOCAL_0:
        case WI_OP_LOAD_LOCAL_1:
        case WI_OP_LOAD_LOCAL_2:
        case WI_OP_LOAD_LOCAL_3:
        case WI_OP_LOAD_LOCAL_4:
        case WI_OP_LOAD_LOCAL_5:
        case WI_OP_LOAD_LOCAL_6:
        case WI_OP_LOAD_LOCAL_7:
        case WI_OP_LOAD_LOCAL_8:
            return _simple_instr(state, offset, "load_local_%i", opcode - WI_OP_LOAD_LOCAL_0);
        case WI_OP_ADD:
            return _simple_instr(state, offset, "add");
        case WI_OP_SUBTRACT:
            return _simple_instr(state, offset, "subtract");
        case WI_OP_MULTIPLY:
            return _simple_instr(state, offset, "multiply");
        case WI_OP_DIVIDE:
            return _simple_instr(state, offset, "divide");
        case WI_OP_NEGATE:
            return _simple_instr(state, offset, "negate");
        case WI_OP_POWER:
            return _simple_instr(state, offset, "power");
        case WI_OP_MODULO:
            return _simple_instr(state, offset, "modulo");
        case WI_OP_GREATER:
            return _simple_instr(state, offset, "greater");
        case WI_OP_GREATER_EQUAL:
            return _simple_instr(state, offset, "greater_equal");
        case WI_OP_LESS:
            return _simple_instr(state, offset, "less");
        case WI_OP_LESS_EQUAL:
            return _simple_instr(state, offset, "less_equal");
        case WI_OP_EQUAL:
            return _simple_instr(state, offset, "equal");
        case WI_OP_NOT_EQUAL:
            return _simple_instr(state, offset, "not_equal");
        case WI_OP_LOG_NOT:
            return _simple_instr(state, offset, "log_not");
        case WI_OP_BIT_AND:
            return _simple_instr(state, offset, "bit_and");
        case WI_OP_BIT_OR:
            return _simple_instr(state, offset, "bit_or");
        case WI_OP_BIT_XOR:
            return _simple_instr(state, offset, "bit_xor");
        case WI_OP_BIT_NOT:
            return _simple_instr(state, offset, "bit_not");
        case WI_OP_BIT_SHL:
            return _simple_instr(state, offset, "bit_shl");
        case WI_OP_BIT_SHR:
            return _simple_instr(state, offset, "bit_shr");
        case WI_OP_LEN:
            return _simple_instr(state, offset, "len");
        case WI_OP_CONCAT:
            return _simple_instr(state, offset, "concat");
        case WI_OP_JUMP:
            return _jump_instr(state, "jump", 1, prototype, offset);
        case WI_OP_JUMP_IF_FALSE:
            return _jump_instr(state, "jump_if_false", 1, prototype, offset);
        case WI_OP_AND:
            return _jump_instr(state, "and", 1, prototype, offset);
        case WI_OP_OR:
            return _jump_instr(state, "or", 1, prototype, offset);
        case WI_OP_LOOP:
            return _jump_instr(state, "loop", -1, prototype, offset);
        case WI_OP_LOOP_END:
            return _simple_instr(state, offset, "invalid opcode");
        case WI_OP_PUSH_ARRAY:
            return _short_instr(state, "push_array", "item count", prototype, offset);
        case WI_OP_PUSH_MAP:
            return _short_instr(state, "push_map", "item count", prototype, offset);
        case WI_OP_SUBSCRIPT_SET:
            return _simple_instr(state, offset, "subscript_set");
        case WI_OP_SUBSCRIPT_GET:
            return _simple_instr(state, offset, "subscript_get");
        case WI_OP_PUSH_CLOSURE: {
            offset++;

            uint16_t constant = (uint16_t)(prototype->bytes.data[offset] << 8 | prototype->bytes.data[offset + 1]);
            wi_value prototype_value = prototype->constants.data[constant];

            state->out("%-16s ", "push_closure");
            wi_value_print(state, prototype_value);
            state->out("\n");

            offset += 2;

            for (int i = 0; i < wi_value_as_prototype(prototype_value)->upvalue_count; i++) {
                uint8_t index    = prototype->bytes.data[offset++];
                uint8_t is_local = prototype->bytes.data[offset++];
                state->out("    %04i    | %-16s at %hhu\n", offset - 2, is_local ? "local" : "upvalue", index);
            }

            return offset;
        }
        case WI_OP_STORE_UPVALUE:
            return _byte_instr(state, "store_upvalue", "upvalue index", prototype, offset);
        case WI_OP_LOAD_UPVALUE:
            return _byte_instr(state, "load_upvalue", "upvalue index", prototype, offset);
        case WI_OP_CLOSE_UPVALUE:
            return _simple_instr(state, offset, "close_upvalue");
        case WI_OP_CALL:
            return _byte_instr(state, "call", "argument count", prototype, offset);
        case WI_OP_TAIL_CALL:
            return _byte_instr(state, "tail_call", "argument count", prototype, offset);
        case WI_OP_RETURN:
            return _simple_instr(state, offset, "return");
        case WI_OP_PUSH_OBJECT:
            return _short_instr(state, "push_object", "field count", prototype, offset);
        case WI_OP_INIT_FIELD:
            return _constant_instr(state, "init_field", "field name", prototype, offset);
        case WI_OP_SET_FIELD:
            return _constant_instr(state, "set_field", "field name", prototype, offset);
        case WI_OP_GET_FIELD:
            return _constant_instr(state, "get_field", "field name", prototype, offset);
        case WI_OP_LOAD_METHOD:
            return _constant_instr(state, "load_method", "method name", prototype, offset);
        case WI_OP_NEW:
            return _short_instr(state, "new", "object count", prototype, offset);
        case WI_OP_REQUIRE:
            return _constant_instr(state, "require", "path", prototype, offset);
    }

    state->out("invalid opcode %hhu\n", opcode);
    return offset + 1;
}

void
wi_prototype_disasm(struct wi_state* state, struct wi_prototype* prototype) {
    if (prototype->is_main) {
        state->out("--- main function (%s) ---\n", prototype->file_path);
    } else if (prototype->name) {
        state->out("--- %s() (%s) ---\n", prototype->name->buf, prototype->file_path);
    } else {
        state->out("--- anonymous function (%s) ---\n", prototype->file_path);
    }

    state->out("bytes: %i\n", prototype->bytes.count);
    state->out("constants: %i\n", prototype->constants.count);

    for (int i = 0; i < prototype->constants.count; i++) {
        wi_value value = prototype->constants.data[i];

        state->out("    C:%05i ", i);
        wi_value_print(state, value);
        state->out(" (%s)\n", wi_value_type(value));
    }

    state->out("instructions:\n");

    for (int offset = 0; offset < prototype->bytes.count;) {
        state->out("    ");
        offset = wi_prototype_disasm_instr(state, prototype, offset);
    }
}
