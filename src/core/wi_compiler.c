#include "wi_compiler.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/wi_conf.h"
#include "wi_box.h"
#include "wi_buf.h"
#include "wi_disasm.h"
#include "wi_gc.h"
#include "wi_lexer.h"
#include "wi_parser.h"
#include "wi_state.h"
#include "wi_table.h"
#include "wi_value.h"

wi_compiler_t*
wi_new_compiler(wi_compiler_t* outer, wi_state_t* state, wi_parser_t* parser) {
    wi_compiler_t* compiler = malloc(sizeof(wi_compiler_t));

    if (!compiler) {
        return NULL;
    }

    wi_compiler_init(compiler, outer, state, parser);
    return compiler;
}

void
wi_delete_compiler(wi_compiler_t* compiler) {
    free(compiler);
}

void
wi_compiler_init(wi_compiler_t* compiler, wi_compiler_t* outer, wi_state_t* state, wi_parser_t* parser) {
    compiler->outer               = outer;
    compiler->state               = state;
    compiler->state->gc->compiler = compiler;
    compiler->parser              = parser;
    compiler->var_name            = WI_BLANK_TOKEN;

    compiler->prototype          = NULL;
    compiler->constants          = NULL;
    compiler->prototype          = wi_new_prototype(compiler->state->gc, compiler->parser->lexer->file_path);
    compiler->prototype->is_main = compiler->outer == NULL;
    compiler->constants          = wi_new_map(compiler->state->gc);

    compiler->local_count = 0;
    compiler->scope_depth = 0;

    compiler->innermost_loop_start       = -1;
    compiler->innermost_loop_scope_depth = 0;
    compiler->last_call_offset           = -1;

    wi_compiler_local_t* local = &compiler->locals[compiler->local_count++];
    local->name                = WI_BLANK_TOKEN;
    local->depth               = 0;
    local->is_captured         = false;
}

static void
_compiler_emit_byte(wi_compiler_t* compiler, uint8_t byte) {
    wi_prototype_add_byte(compiler->prototype, byte, compiler->parser->curr.line);
}

static void
_compiler_emit_bytes(wi_compiler_t* compiler, uint8_t byte1, uint8_t byte2) {
    _compiler_emit_byte(compiler, byte1);
    _compiler_emit_byte(compiler, byte2);
}

static void
_compiler_emit_short(wi_compiler_t* compiler, uint16_t sh) {
    _compiler_emit_bytes(compiler, (uint8_t)(sh >> 8), (uint8_t)(sh & 0xff));
}

static void
_compiler_emit_byte_short(wi_compiler_t* compiler, uint8_t byte, uint16_t sh) {
    _compiler_emit_byte(compiler, byte);
    _compiler_emit_short(compiler, sh);
}

static int
_compiler_emit_jump(wi_compiler_t* compiler, wi_opcode_t opcode) {
    _compiler_emit_byte(compiler, opcode);
    _compiler_emit_bytes(compiler, 0xff, 0xff);
    return compiler->prototype->bytes.count - 2;
}

static void
_compiler_patch_jump(wi_compiler_t* compiler, int offset) {
    uint8_t* bytes = compiler->prototype->bytes.data;
    int      jump  = compiler->prototype->bytes.count - offset - 2;

    if (jump > WI_JUMP_MAX) {
        wi_parser_error_at_curr(compiler->parser, "too much code to jump over (limit is %i)", WI_JUMP_MAX);
        return;
    }

    bytes[offset]     = (uint8_t)(jump >> 8);
    bytes[offset + 1] = (uint8_t)(jump & 0xff);
}

static void
_compiler_emit_loop(wi_compiler_t* compiler, int loop_start) {
    _compiler_emit_byte(compiler, WI_OP_LOOP);
    int offset = compiler->prototype->bytes.count - loop_start + 2;

    if (offset > WI_LOOP_MAX) {
        wi_parser_error_at_curr(compiler->parser, "too much code to loop (limit is %i)", WI_LOOP_MAX);
        return;
    }

    _compiler_emit_short(compiler, (uint16_t)offset);
}

static void
_compiler_end_loop(wi_compiler_t* compiler) {
    int      offset = compiler->innermost_loop_start;
    uint8_t* bytes  = compiler->prototype->bytes.data;

    while (offset < compiler->prototype->bytes.count) {
        if (bytes[offset] == WI_OP_LOOP_END) {
            bytes[offset] = WI_OP_JUMP;
            _compiler_patch_jump(compiler, offset + 1);
            offset += 3;
        } else {
            offset += wi_prototype_instr_size(compiler->prototype, offset);
        }
    }
}

static void
_compiler_pop_loop_locals(wi_compiler_t* compiler) {
    for (int i = compiler->local_count - 1;
         i >= 0 && compiler->locals[i].depth > compiler->innermost_loop_scope_depth; i--) {
        if (compiler->locals[i].is_captured) {
            _compiler_emit_byte(compiler, WI_OP_CLOSE_UPVALUE);
        } else {
            _compiler_emit_byte(compiler, WI_OP_POP);
        }
    }
}

static void
_compiler_emit_return(wi_compiler_t* compiler) {
    _compiler_emit_byte(compiler, WI_OP_PUSH_NULL);
    _compiler_emit_byte(compiler, WI_OP_RETURN);
}

static uint16_t
_compiler_make_constant(wi_compiler_t* compiler, wi_value_t value) {
    bool is_box = wi_value_is_box(value);

    if (is_box) {
        wi_gc_push_root(compiler->state->gc, wi_value_as_box(value));
    }

    wi_value_t existing;
    uint16_t   result;

    if (wi_table_get(&compiler->constants->items, value, &existing)) {
        result = (uint16_t)wi_value_as_real(existing);
    } else {
        int constant = wi_prototype_add_constant(compiler->prototype, value);

        if (constant > WI_CONSTANT_MAX) {
            wi_parser_error_at_curr(compiler->parser, "too many constants in a prototype (limit is %i)",
                                    WI_CONSTANT_MAX);
        }

        wi_table_set(&compiler->constants->items, value, wi_make_real_value(constant));
        result = (uint16_t)constant;
    }

    if (is_box) {
        wi_gc_pop_root(compiler->state->gc);
    }

    return result;
}

static uint16_t
_compiler_name_constant(wi_compiler_t* compiler, wi_token_t name) {
    wi_value_t value = WI_MAKE_BOX_VALUE(wi_copy_cstring(compiler->state->gc, name.start, name.len));
    return _compiler_make_constant(compiler, value);
}

static void
_compiler_emit_push(wi_compiler_t* compiler, wi_value_t value) {
    _compiler_emit_byte_short(compiler, WI_OP_PUSH, _compiler_make_constant(compiler, value));
}

static wi_prototype_t*
_compiler_end(wi_compiler_t* compiler) {
    _compiler_emit_return(compiler);
    wi_prototype_t* prototype = compiler->prototype;

    if (wi_conf_is_set(compiler->state->gc->conf, WI_CONF_PRINT_CODE)) {
        wi_prototype_disasm(prototype);
    }

    compiler->state->gc->compiler = compiler->outer;
    return prototype;
}

static void
_compiler_expr(wi_compiler_t* compiler);
static void
_compiler_stmt(wi_compiler_t* compiler);

static void
_compiler_decl_var(wi_compiler_t* compiler, wi_token_t name) {
    if (compiler->scope_depth == 0) {
        return;
    }

    if (compiler->local_count >= WI_LOCALS_MAX) {
        wi_parser_error_at(compiler->parser, name, "too many local variables (limit is %i)", WI_LOCALS_MAX);
        return;
    }

    for (int i = compiler->local_count - 1; i >= 0; i--) {
        wi_compiler_local_t* local = &compiler->locals[i];

        if (local->depth != -1 && local->depth < compiler->scope_depth) {
            break;
        }

        if (wi_token_lexemes_equal(name, local->name)) {
            wi_parser_error_at(compiler->parser, name, "variable is already defined");
            return;
        }
    }

    wi_compiler_local_t* local = &compiler->locals[compiler->local_count++];
    local->name                = name;
    local->depth               = -1;
    local->is_captured         = false;
}

static void
_compiler_init_local(wi_compiler_t* compiler) {
    if (compiler->scope_depth == 0) {
        return;
    }

    compiler->locals[compiler->local_count - 1].depth = compiler->scope_depth;
}

static void
_compiler_def_var(wi_compiler_t* compiler, wi_token_t name) {
    if (compiler->scope_depth > 0) {
        _compiler_init_local(compiler);
        return;
    }

    uint16_t constant = _compiler_name_constant(compiler, name);
    _compiler_emit_byte_short(compiler, WI_OP_DEF_GLOBAL, constant);
}

static void
_compiler_begin_scope(wi_compiler_t* compiler) {
    compiler->scope_depth++;
}

static void
_compiler_end_scope(wi_compiler_t* compiler) {
    compiler->scope_depth--;

    while (compiler->local_count > 0 &&
           compiler->locals[compiler->local_count - 1].depth > compiler->scope_depth) {
        if (compiler->locals[compiler->local_count - 1].is_captured) {
            _compiler_emit_byte(compiler, WI_OP_CLOSE_UPVALUE);
        } else {
            _compiler_emit_byte(compiler, WI_OP_POP);
        }

        compiler->local_count--;
    }
}

static void
_compiler_block(wi_compiler_t* compiler) {
    while (!wi_parser_check(compiler->parser, WI_TOKEN_CLOSE_BRACE) && !wi_parser_is_at_end(compiler->parser)) {
        _compiler_stmt(compiler);
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_BRACE);
}

static int
_compiler_resolve_local(wi_compiler_t* compiler, wi_token_t name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        wi_compiler_local_t* local = &compiler->locals[i];

        if (wi_token_lexemes_equal(name, local->name)) {
            if (local->depth == -1) {
                wi_parser_error_at(compiler->parser, name,
                                   "cannot read local variable inside its own initializer");
                return -1;
            }

            return i;
        }
    }

    return -1;
}

static int
_compiler_add_upvalue(wi_compiler_t* compiler, uint8_t index, bool is_local) {
    int upvalue_count = compiler->prototype->upvalue_count;

    for (int i = 0; i < upvalue_count; i++) {
        wi_compiler_upvalue_t* upvalue = &compiler->upvalues[i];

        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }

    if (upvalue_count >= WI_UPVALUES_MAX) {
        wi_parser_error_at_curr(compiler->parser, "too many upvalues in a closure (limit is %i)", WI_UPVALUES_MAX);
    }

    wi_compiler_upvalue_t* upvalue = &compiler->upvalues[upvalue_count];
    upvalue->index                 = index;
    upvalue->is_local              = is_local;

    return compiler->prototype->upvalue_count++;
}

static int
_compiler_resolve_upvalue(wi_compiler_t* compiler, wi_token_t name) {
    if (!compiler->outer) {
        return -1;
    }

    int local = _compiler_resolve_local(compiler->outer, name);

    if (local != -1) {
        compiler->outer->locals[local].is_captured = true;
        return _compiler_add_upvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = _compiler_resolve_upvalue(compiler->outer, name);

    if (upvalue != -1) {
        return _compiler_add_upvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void
_compiler_var(wi_compiler_t* compiler, wi_token_t name) {
    int         arg      = _compiler_resolve_local(compiler, name);
    bool        byte_arg = arg != -1;
    wi_opcode_t set_op;
    wi_opcode_t get_op;

    if (arg != -1) {
        set_op = WI_OP_STORE_LOCAL;

        if (arg <= 8) {
            get_op = WI_OP_LOAD_LOCAL_0 + arg;
        } else {
            get_op = WI_OP_LOAD_LOCAL;
        }
    } else if ((arg = _compiler_resolve_upvalue(compiler, name)) != -1) {
        set_op   = WI_OP_STORE_UPVALUE;
        get_op   = WI_OP_LOAD_UPVALUE;
        byte_arg = true;
    } else {
        set_op = WI_OP_SET_GLOBAL;
        get_op = WI_OP_GET_GLOBAL;
        arg    = (int)_compiler_name_constant(compiler, name);
    }

    if (wi_parser_match(compiler->parser, WI_TOKEN_EQUAL)) {
        compiler->var_name = name;
        _compiler_expr(compiler);
        compiler->var_name = WI_BLANK_TOKEN;

        _compiler_emit_byte(compiler, set_op);

        if (byte_arg) {
            _compiler_emit_byte(compiler, (uint8_t)arg);
        } else {
            _compiler_emit_short(compiler, (uint16_t)arg);
        }

        return;
    }

    _compiler_emit_byte(compiler, get_op);

    if (byte_arg) {
        if (get_op == WI_OP_LOAD_LOCAL || get_op == WI_OP_LOAD_UPVALUE) {
            _compiler_emit_byte(compiler, (uint8_t)arg);
        }
    } else {
        _compiler_emit_short(compiler, (uint16_t)arg);
    }
}

static wi_string_t*
_compiler_get_name(wi_compiler_t* compiler) {
    if (compiler->var_name.kind != WI_TOKEN_NAME) {
        return NULL;
    }

    return wi_copy_cstring(compiler->state->gc, compiler->var_name.start, compiler->var_name.len);
}

static void
_compiler_var_expr(wi_compiler_t* compiler) {
    _compiler_var(compiler, compiler->parser->prev);
}

static void
_compiler_real_expr(wi_compiler_t* compiler) {
    wi_token_t token = compiler->parser->prev;
    wi_real_t  real  = wi_string_to_real(token.start, token.len, NULL);
    _compiler_emit_push(compiler, wi_make_real_value(real));
}

static void
_compiler_add_esc_char(wi_compiler_t* compiler, wi_char_buf_t* chars, char c) {
    switch (c) {
        case 'n':
            wi_char_buf_add(chars, '\n');
            break;
        case 't':
            wi_char_buf_add(chars, '\t');
            break;
        case '\\':
            wi_char_buf_add(chars, '\\');
            break;
        case '"':
            wi_char_buf_add(chars, '"');
            break;
        case '0':
            wi_char_buf_add(chars, '\0');
            break;
        default:
            wi_parser_error_at_prev(compiler->parser, "invalid escape sequence \\%c", c);
            break;
    }
}

static void
_compiler_string_expr(wi_compiler_t* compiler) {
    wi_token_t    token = compiler->parser->prev;
    wi_char_buf_t chars;
    wi_char_buf_init(&chars, compiler->state->gc);
    wi_char_buf_reserve(&chars, token.len - 2);

    for (int i = 1; i < token.len - 1; i++) {
        if (token.start[i] == '\\' && i + 1 < token.len - 1) {
            _compiler_add_esc_char(compiler, &chars, token.start[++i]);
        } else {
            wi_char_buf_add(&chars, token.start[i]);
        }
    }

    wi_string_t* string = wi_copy_cstring(compiler->state->gc, chars.data, chars.count);
    wi_char_buf_free(&chars);
    _compiler_emit_push(compiler, WI_MAKE_BOX_VALUE(string));
}

static void
_compiler_group_expr(wi_compiler_t* compiler) {
    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_PAREN);
}

static void
_compiler_array_expr(wi_compiler_t* compiler) {
    uint16_t count = 0;

    if (!wi_parser_check(compiler->parser, WI_TOKEN_CLOSE_BRACKET)) {
        do {
            _compiler_expr(compiler);

            if (count == UINT16_MAX) {
                wi_parser_error_at_curr(compiler->parser,
                                        "cannot have more than %i elements in an array expression", UINT16_MAX);
            }

            count++;
        } while (wi_parser_match(compiler->parser, WI_TOKEN_COMMA));
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_BRACKET);
    _compiler_emit_byte_short(compiler, WI_OP_PUSH_ARRAY, count);
}

static void
_compiler_map_expr(wi_compiler_t* compiler) {
    uint16_t count = 0;

    if (!wi_parser_check(compiler->parser, WI_TOKEN_CLOSE_BRACE)) {
        do {
            _compiler_expr(compiler);
            wi_parser_expect(compiler->parser, WI_TOKEN_COLON);
            _compiler_expr(compiler);

            if (count == UINT16_MAX) {
                wi_parser_error_at_curr(compiler->parser, "cannot have more than %i entries in a map expression",
                                        UINT16_MAX);
            }

            count++;
        } while (wi_parser_match(compiler->parser, WI_TOKEN_COMMA));
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_BRACE);
    _compiler_emit_byte_short(compiler, WI_OP_PUSH_MAP, count);
}

static void
_compiler_null_expr(wi_compiler_t* compiler) {
    _compiler_emit_byte(compiler, WI_OP_PUSH_NULL);
}

static void
_compiler_bool_expr(wi_compiler_t* compiler) {
    wi_opcode_t opcode = compiler->parser->prev.kind == WI_TOKEN_KW_TRUE ? WI_OP_PUSH_TRUE : WI_OP_PUSH_FALSE;
    _compiler_emit_byte(compiler, opcode);
}

static void
_compiler_function_expr(wi_compiler_t* outer) {
    wi_compiler_t compiler;
    wi_compiler_init(&compiler, outer, outer->state, outer->parser);
    _compiler_init_local(&compiler);

    compiler.prototype->name = _compiler_get_name(compiler.outer);

    if (compiler.prototype->name) {
        compiler.locals[0].name = wi_token_from_string(compiler.prototype->name->chars);
    }

    _compiler_begin_scope(&compiler);
    wi_parser_expect(compiler.parser, WI_TOKEN_OPEN_PAREN);

    if (!wi_parser_check(compiler.parser, WI_TOKEN_CLOSE_PAREN)) {
        do {
            compiler.prototype->arity++;

            if (compiler.prototype->arity > UINT8_MAX) {
                wi_parser_error_at_curr(compiler.parser, "cannot have more than 255 parameters");
            }

            wi_token_t name = wi_parser_expect(compiler.parser, WI_TOKEN_NAME);
            _compiler_decl_var(&compiler, name);
            _compiler_def_var(&compiler, name);
        } while (wi_parser_match(compiler.parser, WI_TOKEN_COMMA));
    }

    wi_parser_expect(compiler.parser, WI_TOKEN_CLOSE_PAREN);
    wi_parser_expect(compiler.parser, WI_TOKEN_OPEN_BRACE);
    _compiler_block(&compiler);

    wi_prototype_t* prototype = _compiler_end(&compiler);
    uint16_t        constant  = _compiler_make_constant(outer, WI_MAKE_BOX_VALUE(prototype));
    _compiler_emit_byte_short(outer, WI_OP_PUSH_CLOSURE, constant);

    for (int i = 0; i < prototype->upvalue_count; i++) {
        wi_compiler_upvalue_t* upvalue = &compiler.upvalues[i];
        _compiler_emit_byte(outer, upvalue->index);
        _compiler_emit_byte(outer, upvalue->is_local ? 1 : 0);
    }
}

static uint8_t
_compiler_arg_list(wi_compiler_t* compiler, uint8_t start) {
    uint8_t arg_count = start;

    if (!wi_parser_check(compiler->parser, WI_TOKEN_CLOSE_PAREN)) {
        do {
            _compiler_expr(compiler);

            if (arg_count == UINT8_MAX) {
                wi_parser_error_at_curr(compiler->parser, "cannot have more than 255 arguments in a call");
            }

            arg_count++;
        } while (wi_parser_match(compiler->parser, WI_TOKEN_COMMA));
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_PAREN);
    return arg_count;
}

static void
_compiler_field(wi_compiler_t* compiler);

static void
_compiler_new_expr(wi_compiler_t* compiler) {
    wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
    _compiler_var_expr(compiler);
    _compiler_emit_byte(compiler, WI_OP_NEW);

    if (!wi_parser_match(compiler->parser, WI_TOKEN_OPEN_BRACE)) {
        return;
    }

    while (!wi_parser_check(compiler->parser, WI_TOKEN_CLOSE_BRACE) && !wi_parser_is_at_end(compiler->parser)) {
        wi_token_t name          = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
        uint16_t   name_constant = _compiler_name_constant(compiler, name);

        wi_parser_expect(compiler->parser, WI_TOKEN_EQUAL);
        _compiler_expr(compiler);
        wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);

        _compiler_emit_byte_short(compiler, WI_OP_INIT_FIELD, name_constant);
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_BRACE);
};

static void
_compiler_object_expr(wi_compiler_t* compiler) {
    wi_string_t* name = _compiler_get_name(compiler);

    if (name) {
        wi_gc_push_root(compiler->state->gc, (wi_box_t*)name);
    }

    uint16_t field_count = 0;

    wi_parser_expect(compiler->parser, WI_TOKEN_OPEN_BRACE);

    while (!wi_parser_check(compiler->parser, WI_TOKEN_CLOSE_BRACE) && !wi_parser_is_at_end(compiler->parser)) {
        if (field_count == UINT16_MAX) {
            wi_parser_error_at_curr(compiler->parser, "cannot have more than %i fields in an object", UINT16_MAX);
        }

        wi_token_t field_name = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
        wi_token_t var_name   = compiler->var_name;
        compiler->var_name    = field_name;

        wi_parser_expect(compiler->parser, WI_TOKEN_EQUAL);

        uint16_t constant = _compiler_name_constant(compiler, field_name);
        _compiler_emit_byte_short(compiler, WI_OP_PUSH, constant);
        _compiler_expr(compiler);

        wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);
        field_count++;
        compiler->var_name = var_name;
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_BRACE);

    _compiler_emit_byte_short(compiler, WI_OP_PUSH_OBJECT, field_count);
    _compiler_emit_byte(compiler, name ? 1 : 0);

    if (name) {
        _compiler_emit_short(compiler, _compiler_make_constant(compiler, WI_MAKE_BOX_VALUE(name)));
        wi_gc_pop_root(compiler->state->gc);
    }
}

static void
_compiler_primary_expr(wi_compiler_t* compiler) {
    wi_parser_advance(compiler->parser);

    switch (compiler->parser->prev.kind) {
        case WI_TOKEN_NAME:
            _compiler_var_expr(compiler);
            break;
        case WI_TOKEN_LIT_REAL:
            _compiler_real_expr(compiler);
            break;
        case WI_TOKEN_LIT_STRING:
            _compiler_string_expr(compiler);
            break;
        case WI_TOKEN_OPEN_PAREN:
            _compiler_group_expr(compiler);
            break;
        case WI_TOKEN_OPEN_BRACKET:
            _compiler_array_expr(compiler);
            break;
        case WI_TOKEN_OPEN_BRACE:
            _compiler_map_expr(compiler);
            break;
        case WI_TOKEN_KW_NULL:
            _compiler_null_expr(compiler);
            break;
        case WI_TOKEN_KW_TRUE:
        case WI_TOKEN_KW_FALSE:
            _compiler_bool_expr(compiler);
            break;
        case WI_TOKEN_KW_FUNCTION:
            _compiler_function_expr(compiler);
            break;
        case WI_TOKEN_KW_NEW:
            _compiler_new_expr(compiler);
            break;
        case WI_TOKEN_KW_OBJECT:
            _compiler_object_expr(compiler);
            break;
        default:
            wi_parser_error_at_prev(compiler->parser, "expected expression");
            break;
    }
}

static void
_compiler_call(wi_compiler_t* compiler) {
    uint8_t arg_count = _compiler_arg_list(compiler, 0);
    _compiler_emit_bytes(compiler, WI_OP_CALL, arg_count);
    compiler->last_call_offset = compiler->prototype->bytes.count - 2;
}

static void
_compiler_subscript(wi_compiler_t* compiler) {
    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_BRACKET);

    if (!wi_parser_match(compiler->parser, WI_TOKEN_EQUAL)) {
        _compiler_emit_byte(compiler, WI_OP_SUBSCRIPT_GET);
        return;
    }

    _compiler_expr(compiler);
    _compiler_emit_byte(compiler, WI_OP_SUBSCRIPT_SET);
}

static void
_compiler_field(wi_compiler_t* compiler) {
    wi_token_t name          = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
    uint16_t   name_constant = _compiler_name_constant(compiler, name);

    if (!wi_parser_match(compiler->parser, WI_TOKEN_EQUAL)) {
        _compiler_emit_byte_short(compiler, WI_OP_GET_FIELD, name_constant);
        return;
    }

    _compiler_expr(compiler);
    _compiler_emit_byte_short(compiler, WI_OP_SET_FIELD, name_constant);
}

static void
_compiler_invoke(wi_compiler_t* compiler) {
    wi_token_t name          = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
    uint16_t   name_constant = _compiler_name_constant(compiler, name);

    wi_parser_expect(compiler->parser, WI_TOKEN_OPEN_PAREN);
    uint8_t arg_count = _compiler_arg_list(compiler, 1);

    _compiler_emit_byte_short(compiler, WI_OP_INVOKE, name_constant);
    _compiler_emit_byte(compiler, arg_count);
    compiler->last_call_offset = compiler->prototype->bytes.count - 4;
}

static void
_compiler_call_expr(wi_compiler_t* compiler) {
    _compiler_primary_expr(compiler);

    for (;;) {
        if (wi_parser_match(compiler->parser, WI_TOKEN_OPEN_PAREN)) {
            _compiler_call(compiler);
        } else if (wi_parser_match(compiler->parser, WI_TOKEN_OPEN_BRACKET)) {
            _compiler_subscript(compiler);
        } else if (wi_parser_match(compiler->parser, WI_TOKEN_DOT)) {
            _compiler_field(compiler);
        } else if (wi_parser_match(compiler->parser, WI_TOKEN_ARROW)) {
            _compiler_invoke(compiler);
        } else {
            break;
        }
    }
}

static void
_compiler_unary_expr(wi_compiler_t* compiler) {
    if (wi_parser_match(compiler->parser, WI_TOKEN_HASH)) {
        _compiler_unary_expr(compiler);
        _compiler_emit_byte(compiler, WI_OP_LEN);
        return;
    }

    if (wi_parser_match(compiler->parser, WI_TOKEN_MINUS)) {
        _compiler_unary_expr(compiler);
        _compiler_emit_byte(compiler, WI_OP_NEGATE);
        return;
    }

    if (wi_parser_match(compiler->parser, WI_TOKEN_TILDE)) {
        _compiler_unary_expr(compiler);
        _compiler_emit_byte(compiler, WI_OP_BIT_NOT);
        return;
    }

    if (wi_parser_match(compiler->parser, WI_TOKEN_BANG)) {
        _compiler_unary_expr(compiler);
        _compiler_emit_byte(compiler, WI_OP_LOG_NOT);
        return;
    }

    _compiler_call_expr(compiler);
}

static void
_compiler_power_expr(wi_compiler_t* compiler) {
    _compiler_unary_expr(compiler);

    if (wi_parser_match(compiler->parser, WI_TOKEN_STAR_STAR)) {
        _compiler_power_expr(compiler);
        _compiler_emit_byte(compiler, WI_OP_POWER);
    }
}

static void
_compiler_factor_expr(wi_compiler_t* compiler) {
    _compiler_power_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_STAR) || wi_parser_match(compiler->parser, WI_TOKEN_SLASH) ||
           wi_parser_match(compiler->parser, WI_TOKEN_PERCENT)) {
        wi_opcode_t opcode;

        switch (compiler->parser->prev.kind) {
            case WI_TOKEN_STAR:
                opcode = WI_OP_MULTIPLY;
                break;
            case WI_TOKEN_SLASH:
                opcode = WI_OP_DIVIDE;
                break;
            default:
                opcode = WI_OP_MODULO;
                break;
        }

        _compiler_power_expr(compiler);
        _compiler_emit_byte(compiler, opcode);
    }
}

static void
_compiler_term_expr(wi_compiler_t* compiler) {
    _compiler_factor_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_PLUS) || wi_parser_match(compiler->parser, WI_TOKEN_MINUS)) {
        wi_opcode_t opcode = compiler->parser->prev.kind == WI_TOKEN_PLUS ? WI_OP_ADD : WI_OP_SUBTRACT;
        _compiler_factor_expr(compiler);
        _compiler_emit_byte(compiler, opcode);
    }
}

static void
_compiler_shift_expr(wi_compiler_t* compiler) {
    _compiler_term_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_GREATER_GREATER) ||
           wi_parser_match(compiler->parser, WI_TOKEN_LESS_LESS)) {
        wi_opcode_t opcode = compiler->parser->prev.kind == WI_TOKEN_LESS_LESS ? WI_OP_BIT_SHL : WI_OP_BIT_SHR;
        _compiler_term_expr(compiler);
        _compiler_emit_byte(compiler, opcode);
    }
}

static void
_compiler_bit_and_expr(wi_compiler_t* compiler) {
    _compiler_shift_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_AMPER)) {
        _compiler_shift_expr(compiler);
        _compiler_emit_byte(compiler, WI_OP_BIT_AND);
    }
}

static void
_compiler_bit_xor_expr(wi_compiler_t* compiler) {
    _compiler_bit_and_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_CARET)) {
        _compiler_bit_and_expr(compiler);
        _compiler_emit_byte(compiler, WI_OP_BIT_XOR);
    }
}

static void
_compiler_bit_or_expr(wi_compiler_t* compiler) {
    _compiler_bit_xor_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_PIPE)) {
        _compiler_bit_xor_expr(compiler);
        _compiler_emit_byte(compiler, WI_OP_BIT_OR);
    }
}

static void
_compiler_comparison_expr(wi_compiler_t* compiler) {
    _compiler_bit_or_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_GREATER) ||
           wi_parser_match(compiler->parser, WI_TOKEN_GREATER_EQUAL) ||
           wi_parser_match(compiler->parser, WI_TOKEN_LESS) ||
           wi_parser_match(compiler->parser, WI_TOKEN_LESS_EQUAL)) {
        wi_opcode_t opcode;

        switch (compiler->parser->prev.kind) {
            case WI_TOKEN_GREATER:
                opcode = WI_OP_GREATER;
                break;
            case WI_TOKEN_GREATER_EQUAL:
                opcode = WI_OP_GREATER_EQUAL;
                break;
            case WI_TOKEN_LESS:
                opcode = WI_OP_LESS;
                break;
            default:
                opcode = WI_OP_LESS_EQUAL;
                break;
        }

        _compiler_bit_or_expr(compiler);
        _compiler_emit_byte(compiler, opcode);
    }
}

static void
_compiler_equality_expr(wi_compiler_t* compiler) {
    _compiler_comparison_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_EQUAL_EQUAL) ||
           wi_parser_match(compiler->parser, WI_TOKEN_BANG_EQUAL)) {
        wi_opcode_t opcode = compiler->parser->prev.kind == WI_TOKEN_EQUAL_EQUAL ? WI_OP_EQUAL : WI_OP_NOT_EQUAL;
        _compiler_comparison_expr(compiler);
        _compiler_emit_byte(compiler, opcode);
    }
}

static void
_compiler_log_and_expr(wi_compiler_t* compiler) {
    _compiler_equality_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_AMPER_AMPER)) {
        int end_jump = _compiler_emit_jump(compiler, WI_OP_JUMP_IF_FALSE);

        _compiler_emit_byte(compiler, WI_OP_POP);
        _compiler_equality_expr(compiler);
        _compiler_patch_jump(compiler, end_jump);
    }
}

static void
_compiler_log_or_expr(wi_compiler_t* compiler) {
    _compiler_log_and_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_PIPE_PIPE)) {
        int else_jump = _compiler_emit_jump(compiler, WI_OP_JUMP_IF_FALSE);
        int end_jump  = _compiler_emit_jump(compiler, WI_OP_JUMP);

        _compiler_patch_jump(compiler, else_jump);
        _compiler_emit_byte(compiler, WI_OP_POP);

        _compiler_log_and_expr(compiler);
        _compiler_patch_jump(compiler, end_jump);
    }
}

static void
_compiler_concat_expr(wi_compiler_t* compiler) {
    _compiler_log_or_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_DOT_DOT)) {
        _compiler_log_or_expr(compiler);
        _compiler_emit_byte(compiler, WI_OP_CONCAT);
    }
}

static void
_compiler_assignment_expr(wi_compiler_t* compiler) {
    if (!wi_parser_check(compiler->parser, WI_TOKEN_NAME) || compiler->parser->next.kind != WI_TOKEN_EQUAL) {
        _compiler_concat_expr(compiler);
        return;
    }

    wi_parser_advance(compiler->parser);
    _compiler_var(compiler, compiler->parser->prev);
}

static void
_compiler_expr(wi_compiler_t* compiler) {
    _compiler_assignment_expr(compiler);
}

static void
_compiler_expr_stmt(wi_compiler_t* compiler) {
    _compiler_expr(compiler);
    _compiler_emit_byte(compiler, WI_OP_POP);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);
}

static void
_compiler_block_stmt(wi_compiler_t* compiler) {
    _compiler_begin_scope(compiler);
    _compiler_block(compiler);
    _compiler_end_scope(compiler);
}

static void
_compiler_var_stmt(wi_compiler_t* compiler) {
    wi_token_t name    = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
    compiler->var_name = name;
    _compiler_decl_var(compiler, name);

    wi_parser_expect(compiler->parser, WI_TOKEN_EQUAL);
    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);

    _compiler_def_var(compiler, name);
    compiler->var_name = WI_BLANK_TOKEN;
}

static void
_compiler_if_stmt(wi_compiler_t* compiler) {
    wi_parser_expect(compiler->parser, WI_TOKEN_OPEN_PAREN);
    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_PAREN);

    int then_jump = _compiler_emit_jump(compiler, WI_OP_JUMP_IF_FALSE);

    _compiler_emit_byte(compiler, WI_OP_POP);
    _compiler_stmt(compiler);

    int else_jump = _compiler_emit_jump(compiler, WI_OP_JUMP);

    _compiler_patch_jump(compiler, then_jump);
    _compiler_emit_byte(compiler, WI_OP_POP);

    if (wi_parser_match(compiler->parser, WI_TOKEN_KW_ELSE)) {
        _compiler_stmt(compiler);
    }

    _compiler_patch_jump(compiler, else_jump);
}

static void
_compiler_while_stmt(wi_compiler_t* compiler) {
    int enclosing_start       = compiler->innermost_loop_start;
    int enclosing_scope_depth = compiler->innermost_loop_scope_depth;

    compiler->innermost_loop_start       = compiler->prototype->bytes.count;
    compiler->innermost_loop_scope_depth = compiler->scope_depth;

    wi_parser_expect(compiler->parser, WI_TOKEN_OPEN_PAREN);
    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_PAREN);

    int exit_jump = _compiler_emit_jump(compiler, WI_OP_JUMP_IF_FALSE);

    _compiler_emit_byte(compiler, WI_OP_POP);
    _compiler_stmt(compiler);
    _compiler_emit_loop(compiler, compiler->innermost_loop_start);

    _compiler_patch_jump(compiler, exit_jump);
    _compiler_emit_byte(compiler, WI_OP_POP);
    _compiler_end_loop(compiler);

    compiler->innermost_loop_start       = enclosing_start;
    compiler->innermost_loop_scope_depth = enclosing_scope_depth;
}

static void
_compiler_for_init(wi_compiler_t* compiler) {
    if (wi_parser_match(compiler->parser, WI_TOKEN_SEMICOLON)) {
        return;
    }

    if (wi_parser_match(compiler->parser, WI_TOKEN_KW_VAR)) {
        _compiler_var_stmt(compiler);
    } else {
        _compiler_expr_stmt(compiler);
    }
}

static int
_compiler_for_cond(wi_compiler_t* compiler) {
    if (wi_parser_match(compiler->parser, WI_TOKEN_SEMICOLON)) {
        return -1;
    }

    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);
    int exit_jump = _compiler_emit_jump(compiler, WI_OP_JUMP_IF_FALSE);
    _compiler_emit_byte(compiler, WI_OP_POP);

    return exit_jump;
}

static void
_compiler_for_incr(wi_compiler_t* compiler) {
    if (wi_parser_match(compiler->parser, WI_TOKEN_CLOSE_PAREN)) {
        return;
    }

    int body_jump  = _compiler_emit_jump(compiler, WI_OP_JUMP);
    int incr_start = compiler->prototype->bytes.count;

    _compiler_expr(compiler);
    _compiler_emit_byte(compiler, WI_OP_POP);
    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_PAREN);

    _compiler_emit_loop(compiler, compiler->innermost_loop_start);
    compiler->innermost_loop_start = incr_start;
    _compiler_patch_jump(compiler, body_jump);
}

static void
_compiler_for_stmt(wi_compiler_t* compiler) {
    _compiler_begin_scope(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_OPEN_PAREN);

    _compiler_for_init(compiler);

    int enclosing_start       = compiler->innermost_loop_start;
    int enclosing_scope_depth = compiler->innermost_loop_scope_depth;

    compiler->innermost_loop_start       = compiler->prototype->bytes.count;
    compiler->innermost_loop_scope_depth = compiler->scope_depth;

    int exit_jump = _compiler_for_cond(compiler);
    _compiler_for_incr(compiler);

    _compiler_stmt(compiler);
    _compiler_emit_loop(compiler, compiler->innermost_loop_start);

    if (exit_jump != -1) {
        _compiler_patch_jump(compiler, exit_jump);
        _compiler_emit_byte(compiler, WI_OP_POP);
    }

    _compiler_end_scope(compiler);
    _compiler_end_loop(compiler);

    compiler->innermost_loop_start       = enclosing_start;
    compiler->innermost_loop_scope_depth = enclosing_scope_depth;
}

static void
_compiler_break_stmt(wi_compiler_t* compiler) {
    if (compiler->innermost_loop_start == -1) {
        wi_parser_error_at_prev(compiler->parser, "cannot use 'break' outside of a loop");
    }

    _compiler_pop_loop_locals(compiler);
    _compiler_emit_jump(compiler, WI_OP_LOOP_END);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);
}

static void
_compiler_continue_stmt(wi_compiler_t* compiler) {
    if (compiler->innermost_loop_start == -1) {
        wi_parser_error_at_prev(compiler->parser, "cannot use 'continue' outside of a loop");
    }

    _compiler_pop_loop_locals(compiler);
    _compiler_emit_loop(compiler, compiler->innermost_loop_start);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);
}

static void
_compiler_return_stmt(wi_compiler_t* compiler) {
    if (!compiler->outer) {
        wi_parser_error_at_prev(compiler->parser, "cannot return from top-level code");
    }

    if (wi_parser_match(compiler->parser, WI_TOKEN_SEMICOLON)) {
        _compiler_emit_return(compiler);
        return;
    }

    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);

    int      end    = compiler->prototype->bytes.count;
    uint8_t* bytes  = compiler->prototype->bytes.data;
    int      offset = compiler->last_call_offset;

    if (offset == end - 2 && bytes[offset] == WI_OP_CALL) {
        bytes[offset] = WI_OP_TAIL_CALL;
        return;
    }

    if (offset == end - 4 && bytes[offset] == WI_OP_INVOKE) {
        bytes[offset] = WI_OP_TAIL_INVOKE;
        return;
    }

    _compiler_emit_byte(compiler, WI_OP_RETURN);
}

static void
_compiler_require_stmt(wi_compiler_t* compiler) {
    if (compiler->outer) {
        wi_parser_error_at_prev(compiler->parser, "can only require files from top-level code");
    }

    wi_token_t   path     = wi_parser_expect(compiler->parser, WI_TOKEN_LIT_STRING);
    wi_string_t* path_box = wi_copy_cstring(compiler->state->gc, path.start + 1, path.len - 2);

    if (access(path_box->chars, F_OK) != 0) {
        wi_parser_error_at(compiler->parser, path, "file %s does not exist", path_box->chars);
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_EQUAL);
    wi_token_t name = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
    _compiler_decl_var(compiler, name);

    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);

    uint16_t path_constant = _compiler_make_constant(compiler, WI_MAKE_BOX_VALUE(path_box));
    uint16_t name_constant = _compiler_name_constant(compiler, name);

    _compiler_emit_byte_short(compiler, WI_OP_REQUIRE, path_constant);
    _compiler_emit_short(compiler, name_constant);
}

static void
_compiler_stmt(wi_compiler_t* compiler) {
    switch (compiler->parser->curr.kind) {
        case WI_TOKEN_OPEN_BRACE:
            wi_parser_advance(compiler->parser);
            _compiler_block_stmt(compiler);
            break;
        case WI_TOKEN_KW_VAR:
            wi_parser_advance(compiler->parser);
            _compiler_var_stmt(compiler);
            break;
        case WI_TOKEN_KW_IF:
            wi_parser_advance(compiler->parser);
            _compiler_if_stmt(compiler);
            break;
        case WI_TOKEN_KW_WHILE:
            wi_parser_advance(compiler->parser);
            _compiler_while_stmt(compiler);
            break;
        case WI_TOKEN_KW_FOR:
            wi_parser_advance(compiler->parser);
            _compiler_for_stmt(compiler);
            break;
        case WI_TOKEN_KW_BREAK:
            wi_parser_advance(compiler->parser);
            _compiler_break_stmt(compiler);
            break;
        case WI_TOKEN_KW_CONTINUE:
            wi_parser_advance(compiler->parser);
            _compiler_continue_stmt(compiler);
            break;
        case WI_TOKEN_KW_RETURN:
            wi_parser_advance(compiler->parser);
            _compiler_return_stmt(compiler);
            break;
        case WI_TOKEN_KW_REQUIRE:
            wi_parser_advance(compiler->parser);
            _compiler_require_stmt(compiler);
            break;
        default:
            _compiler_expr_stmt(compiler);
            break;
    }
}

wi_prototype_t*
wi_compile(wi_state_t* state, const char* file_path, const char* src) {
    wi_lexer_t lexer;
    wi_lexer_init(&lexer, file_path, src);

    wi_parser_t* parser = wi_new_parser(&lexer, state->gc);

    if (!parser) {
        return NULL;
    }

    wi_compiler_t* compiler = wi_new_compiler(NULL, state, parser);

    if (!compiler) {
        return NULL;
    }

    if (setjmp(compiler->parser->error_jmp) == 0) {
        while (!wi_parser_is_at_end(compiler->parser)) {
            _compiler_stmt(compiler);
        }

        wi_prototype_t* prototype = _compiler_end(compiler);

        wi_delete_parser(parser);
        wi_delete_compiler(compiler);

        return prototype;
    }

    wi_delete_parser(parser);
    wi_delete_compiler(compiler);

    return NULL;
}
