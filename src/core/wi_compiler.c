#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "wi_compiler.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#include "../../include/wi_conf.h"
#include "wi_box.h"
#include "wi_buf.h"
#include "wi_disasm.h"
#include "wi_gc.h"
#include "wi_lexer.h"
#include "wi_parser.h"
#include "wi_state.h"
#include "wi_table.h"
#include "wi_value.h"

struct wi_compiler*
wi_new_compiler(struct wi_compiler* outer, struct wi_state* state, struct wi_parser* parser,
                struct wi_table* globals) {
    struct wi_compiler* compiler = malloc(sizeof(struct wi_compiler));

    if (!compiler) {
        return NULL;
    }

    wi_compiler_init(compiler, outer, state, parser, globals);
    return compiler;
}

void
wi_delete_compiler(struct wi_compiler* compiler) {
    free(compiler);
}

void
wi_compiler_init(struct wi_compiler* compiler, struct wi_compiler* outer, struct wi_state* state,
                 struct wi_parser* parser, struct wi_table* global_attrs) {
    compiler->outer        = outer;
    compiler->state        = state;
    compiler->gc           = state->gc;
    compiler->gc->compiler = compiler;
    compiler->parser       = parser;
    compiler->var_name     = WI_BLANK_TOKEN;

    compiler->global_attrs       = global_attrs;
    compiler->prototype          = NULL;
    compiler->constants          = NULL;
    compiler->prototype          = wi_new_prototype(compiler->gc, compiler->parser->lexer->file_path);
    compiler->prototype->is_main = compiler->outer == NULL;
    compiler->slot_count         = 0;
    compiler->constants          = wi_new_map(compiler->gc);

    compiler->local_count = 0;
    compiler->scope_depth = 0;

    compiler->innermost_loop_start       = -1;
    compiler->innermost_loop_scope_depth = 0;
    compiler->last_call_offset           = -1;

    struct wi_compiler_local* local = &compiler->locals[compiler->local_count++];
    local->name                     = WI_BLANK_TOKEN;
    local->depth                    = 0;
    local->is_captured              = false;
    local->used                     = true;
    local->attrs                    = WI_DEFAULT_ATTRS;
}

static const int _opcode_effects[] = {
#define WI_OPCODE(_, __, effect) effect,
#include "wi_opcode.h"
#undef WI_OPCODE
};

static void
_compiler_emit_byte(struct wi_compiler* compiler, uint8_t byte) {
    struct wi_prototype* prototype = compiler->prototype;
    wi_byte_buf_add(&prototype->bytes, byte);
    wi_int_buf_add(&prototype->lines, compiler->parser->curr.line);
}

static void
_compiler_emit_opcode(struct wi_compiler* compiler, uint8_t opcode) {
    _compiler_emit_byte(compiler, opcode);
    compiler->slot_count += _opcode_effects[opcode];

    if (compiler->slot_count > compiler->prototype->max_slot_count) {
        compiler->prototype->max_slot_count = compiler->slot_count;
    }
}

static void
_compiler_emit_opcode_byte(struct wi_compiler* compiler, uint8_t opcode, uint8_t byte) {
    _compiler_emit_opcode(compiler, opcode);
    _compiler_emit_byte(compiler, byte);
}

static void
_compiler_emit_bytes(struct wi_compiler* compiler, uint8_t byte1, uint8_t byte2) {
    _compiler_emit_byte(compiler, byte1);
    _compiler_emit_byte(compiler, byte2);
}

static void
_compiler_emit_short(struct wi_compiler* compiler, uint16_t sh) {
    _compiler_emit_bytes(compiler, (uint8_t)(sh >> 8), (uint8_t)(sh & 0xff));
}

static void
_compiler_emit_opcode_short(struct wi_compiler* compiler, uint8_t opcode, uint16_t sh) {
    _compiler_emit_opcode(compiler, opcode);
    _compiler_emit_short(compiler, sh);
}

static int
_compiler_emit_jump(struct wi_compiler* compiler, uint8_t opcode) {
    _compiler_emit_opcode(compiler, opcode);
    _compiler_emit_bytes(compiler, 0xff, 0xff);
    return compiler->prototype->bytes.count - 2;
}

static void
_compiler_patch_jump(struct wi_compiler* compiler, int offset) {
    uint8_t* bytes = compiler->prototype->bytes.data;
    int      jump  = compiler->prototype->bytes.count - offset - 2;

    if (jump > WI_JUMP_MAX) {
        wi_parser_error_at_curr(compiler->parser, "too much code to jump over (limit is %i)", WI_JUMP_MAX);
    }

    bytes[offset]     = (uint8_t)(jump >> 8);
    bytes[offset + 1] = (uint8_t)(jump & 0xff);
}

static void
_compiler_emit_loop(struct wi_compiler* compiler, int loop_start) {
    _compiler_emit_opcode(compiler, WI_OP_LOOP);
    int offset = compiler->prototype->bytes.count - loop_start + 2;

    if (offset > WI_LOOP_MAX) {
        wi_parser_error_at_curr(compiler->parser, "too much code to loop (limit is %i)", WI_LOOP_MAX);
    }

    _compiler_emit_short(compiler, (uint16_t)offset);
}

static void
_compiler_end_loop(struct wi_compiler* compiler) {
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
_compiler_pop_loop_locals(struct wi_compiler* compiler) {
    for (int i = compiler->local_count - 1;
         i >= 0 && compiler->locals[i].depth > compiler->innermost_loop_scope_depth; i--) {
        if (compiler->locals[i].is_captured) {
            _compiler_emit_opcode(compiler, WI_OP_CLOSE_UPVALUE);
        } else {
            _compiler_emit_opcode(compiler, WI_OP_POP);
        }
    }
}

static void
_compiler_emit_return(struct wi_compiler* compiler) {
    _compiler_emit_opcode(compiler, WI_OP_PUSH_NULL);
    _compiler_emit_opcode(compiler, WI_OP_RETURN);
}

static uint16_t
_compiler_make_constant(struct wi_compiler* compiler, wi_value value) {
    bool is_box = wi_value_is_box(value);

    if (is_box) {
        WI_GC_PUSH_ROOT(compiler->gc, wi_value_as_box(value));
    }

    wi_value existing;
    uint16_t result;

    if (wi_table_get(&compiler->constants->items, value, &existing)) {
        result = (uint16_t)wi_value_as_real(existing);
    } else {
        wi_value_buf_add(&compiler->prototype->constants, value);
        int index = compiler->prototype->constants.count - 1;

        if (index > WI_CONSTANT_MAX) {
            wi_parser_error_at_curr(compiler->parser, "too many constants in a prototype (limit is %i)",
                                    WI_CONSTANT_MAX);
        }

        wi_table_set(&compiler->constants->items, value, wi_make_real_value(index));
        result = (uint16_t)index;
    }

    if (is_box) {
        wi_gc_pop_root(compiler->gc);
    }

    return result;
}

static uint16_t
_compiler_name_constant(struct wi_compiler* compiler, struct wi_token name) {
    wi_value value = WI_MAKE_BOX_VALUE(wi_copy_cstring(compiler->gc, name.start, name.count));
    return _compiler_make_constant(compiler, value);
}

static void
_compiler_emit_push(struct wi_compiler* compiler, wi_value value) {
    _compiler_emit_opcode_short(compiler, WI_OP_PUSH, _compiler_make_constant(compiler, value));
}

static struct wi_prototype*
_compiler_end(struct wi_compiler* compiler) {
    _compiler_emit_return(compiler);
    struct wi_prototype* prototype = compiler->prototype;

    if (wi_conf_is_set(compiler->gc->conf, WI_CONF_PRINT_CODE)) {
        wi_prototype_disasm(compiler->state, prototype);
    }

    compiler->gc->compiler = compiler->outer;
    return prototype;
}

static void
_compiler_expr(struct wi_compiler* compiler);
static void
_compiler_stmt(struct wi_compiler* compiler);
static void
_compiler_decl(struct wi_compiler* compiler);
static void
_compiler_name_decl(struct wi_compiler* compiler);

static void
_compiler_decl_var(struct wi_compiler* compiler, struct wi_token name, wi_attrs attrs) {
    if (compiler->scope_depth == 0) {
        return;
    }

    if (compiler->local_count >= WI_LOCAL_MAX) {
        wi_parser_error_at(compiler->parser, name, "too many local variables (limit is %i)", WI_LOCAL_MAX);
        return;
    }

    for (int i = compiler->local_count - 1; i >= 0; i--) {
        struct wi_compiler_local* local = &compiler->locals[i];

        if (local->depth != -1 && local->depth < compiler->scope_depth) {
            break;
        }

        if (wi_token_lexemes_equal(name, local->name)) {
            wi_parser_error_at(compiler->parser, name, "variable %.*s is already defined", name.count, name.start);
            return;
        }
    }

    struct wi_compiler_local* local = &compiler->locals[compiler->local_count++];
    local->name                     = name;
    local->depth                    = -1;
    local->is_captured              = false;
    local->used                     = false;
    local->attrs                    = attrs;
}

static void
_compiler_init_local(struct wi_compiler* compiler) {
    if (compiler->scope_depth == 0) {
        return;
    }

    compiler->locals[compiler->local_count - 1].depth = compiler->scope_depth;
}

static void
_compiler_def_var(struct wi_compiler* compiler, struct wi_token name, wi_attrs attrs) {
    if (compiler->scope_depth > 0) {
        _compiler_init_local(compiler);
        return;
    }

    struct wi_string* name_box = wi_copy_cstring(compiler->gc, name.start, name.count);
    WI_GC_PUSH_ROOT(compiler->gc, name_box);

    if (!wi_table_set(compiler->global_attrs, WI_MAKE_BOX_VALUE(name_box), wi_make_real_value(attrs))) {
        wi_parser_error_at(compiler->parser, name, "variable %s is already defined", name_box->buf);
    }

    wi_gc_pop_root(compiler->gc);
    uint16_t constant = _compiler_make_constant(compiler, WI_MAKE_BOX_VALUE(name_box));
    _compiler_emit_opcode_short(compiler, WI_OP_DEF_GLOBAL, constant);
}

static void
_compiler_begin_scope(struct wi_compiler* compiler) {
    compiler->scope_depth++;
}

static void
_compiler_warn_unused(struct wi_compiler* compiler, struct wi_compiler_local* local) {
    if (!local->used && !wi_attr_is_set(local->attrs, WI_ATTR_UNUSED)) {
        wi_parser_warning_at(compiler->parser, local->name, "local variable %.*s was defined but not used",
                             local->name.count, local->name.start);
    }
}

static void
_compiler_end_scope(struct wi_compiler* compiler) {
    compiler->scope_depth--;
    struct wi_compiler_local* local;

    while (compiler->local_count > 0 &&
           ((local = &compiler->locals[compiler->local_count - 1]))->depth > compiler->scope_depth) {
        _compiler_warn_unused(compiler, local);

        if (local->is_captured) {
            _compiler_emit_opcode(compiler, WI_OP_CLOSE_UPVALUE);
        } else {
            _compiler_emit_opcode(compiler, WI_OP_POP);
        }

        compiler->local_count--;
    }
}

static bool
_compiler_is_top_level(struct wi_compiler* compiler) {
    return !compiler->outer && compiler->scope_depth == 0;
}

static void
_compiler_block(struct wi_compiler* compiler) {
    while (!wi_parser_check(compiler->parser, WI_TOKEN_CLOSE_BRACE) && !wi_parser_is_at_end(compiler->parser)) {
        _compiler_decl(compiler);
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_BRACE);
}

static int
_compiler_resolve_local(struct wi_compiler* compiler, struct wi_token name, wi_attrs* attrs) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        struct wi_compiler_local* local = &compiler->locals[i];

        if (wi_token_lexemes_equal(name, local->name)) {
            if (local->depth == -1) {
                wi_parser_error_at(compiler->parser, name, "cannot use local variable inside its own initializer");
                return -1;
            }

            local->used = true;

            if (attrs) {
                *attrs = local->attrs;
            }

            return i;
        }
    }

    return -1;
}

static int
_compiler_add_upvalue(struct wi_compiler* compiler, uint8_t index, bool is_local) {
    int upvalue_count = compiler->prototype->upvalue_count;

    for (int i = 0; i < upvalue_count; i++) {
        struct wi_compiler_upvalue* upvalue = &compiler->upvalues[i];

        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }

    if (upvalue_count >= WI_UPVALUE_MAX) {
        wi_parser_error_at_curr(compiler->parser, "too many upvalues in a closure (limit is %i)", WI_UPVALUE_MAX);
    }

    struct wi_compiler_upvalue* upvalue = &compiler->upvalues[upvalue_count];
    upvalue->index                      = index;
    upvalue->is_local                   = is_local;

    return compiler->prototype->upvalue_count++;
}

static int
_compiler_resolve_upvalue(struct wi_compiler* compiler, struct wi_token name, wi_attrs* attrs) {
    if (!compiler->outer) {
        return -1;
    }

    int local = _compiler_resolve_local(compiler->outer, name, attrs);

    if (local != -1) {
        compiler->outer->locals[local].is_captured = true;
        return _compiler_add_upvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = _compiler_resolve_upvalue(compiler->outer, name, attrs);

    if (upvalue != -1) {
        return _compiler_add_upvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void
_compiler_var(struct wi_compiler* compiler, struct wi_token name) {
    wi_attrs          attrs       = WI_DEFAULT_ATTRS;
    int               arg         = _compiler_resolve_local(compiler, name, &attrs);
    struct wi_string* global_name = NULL;
    uint8_t           set_op;
    uint8_t           get_op;

    if (arg != -1) {
        set_op = WI_OP_STORE_LOCAL;

        if (arg <= 8) {
            get_op = (uint8_t)(WI_OP_LOAD_LOCAL_0 + arg);
        } else {
            get_op = WI_OP_LOAD_LOCAL;
        }
    } else if ((arg = _compiler_resolve_upvalue(compiler, name, &attrs)) != -1) {
        set_op = WI_OP_STORE_UPVALUE;
        get_op = WI_OP_LOAD_UPVALUE;
    } else {
        set_op      = WI_OP_SET_GLOBAL;
        get_op      = WI_OP_GET_GLOBAL;
        global_name = wi_copy_cstring(compiler->gc, name.start, name.count);
        arg         = (int)_compiler_make_constant(compiler, WI_MAKE_BOX_VALUE(global_name));
    }

    if (global_name) {
        wi_value name_value = WI_MAKE_BOX_VALUE(global_name);
        wi_value attrs_value;
        wi_value foreign = wi_make_empty_value();

        if (!wi_table_get(compiler->global_attrs, name_value, &attrs_value) &&
            !wi_table_get(&compiler->state->foreign, name_value, &foreign)) {
            wi_parser_error_at(compiler->parser, name, "variable %s is used but not defined", global_name->buf);
        }

        if (!wi_value_is_empty(foreign)) {
            if (wi_parser_check(compiler->parser, WI_TOKEN_EQUAL)) {
                wi_parser_error_at(compiler->parser, name, "cannot reassign a foreign variable %s",
                                   global_name->buf);
            }

            _compiler_emit_push(compiler, foreign);
            return;
        }

        attrs = (wi_attrs)wi_value_as_real(attrs_value);
    }

    if (wi_attr_is_set(attrs, WI_ATTR_DEPRECATED)) {
        wi_parser_warning_at(compiler->parser, name, "use of deprecated variable %.*s", name.count, name.start);
    }

    if (wi_parser_match(compiler->parser, WI_TOKEN_EQUAL)) {
        if (wi_attr_is_set(attrs, WI_ATTR_CONST)) {
            wi_parser_error_at_prev(compiler->parser, "cannot reassign variable %.*s", name.count, name.start);
        }

        compiler->var_name = name;
        _compiler_expr(compiler);
        compiler->var_name = WI_BLANK_TOKEN;

        _compiler_emit_opcode(compiler, set_op);

        if (!global_name) {
            _compiler_emit_byte(compiler, (uint8_t)arg);
        } else {
            _compiler_emit_short(compiler, (uint16_t)arg);
        }

        return;
    }

    _compiler_emit_opcode(compiler, get_op);

    if (!global_name) {
        if (get_op == WI_OP_LOAD_LOCAL || get_op == WI_OP_LOAD_UPVALUE) {
            _compiler_emit_byte(compiler, (uint8_t)arg);
        }
    } else {
        _compiler_emit_short(compiler, (uint16_t)arg);
    }
}

static bool
_check_attr(struct wi_token token, const char* attr) {
    size_t len = strlen(attr);
    return token.count == (int)len && memcmp(token.start, attr, len) == 0;
}

static wi_attrs
_compiler_parse_attrs(struct wi_compiler* compiler) {
    wi_attrs attrs = WI_DEFAULT_ATTRS;

    while (wi_parser_match(compiler->parser, WI_TOKEN_AT)) {
        struct wi_token token = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);

        if (_check_attr(token, "const")) {
            wi_attr_set(&attrs, WI_ATTR_CONST);
        } else if (_check_attr(token, "unused")) {
            wi_attr_set(&attrs, WI_ATTR_UNUSED);
        } else if (_check_attr(token, "deprecated")) {
            wi_attr_set(&attrs, WI_ATTR_DEPRECATED);
        } else {
            wi_parser_error_at(compiler->parser, token, "unknown attribute %.*s", token.count, token.start);
        }
    }

    return attrs;
}

static struct wi_string*
_compiler_get_name(struct wi_compiler* compiler) {
    if (compiler->var_name.kind != WI_TOKEN_NAME) {
        return NULL;
    }

    return wi_copy_cstring(compiler->gc, compiler->var_name.start, compiler->var_name.count);
}

static void
_compiler_var_expr(struct wi_compiler* compiler) {
    _compiler_var(compiler, compiler->parser->prev);
}

static void
_compiler_real_expr(struct wi_compiler* compiler) {
    struct wi_token token = compiler->parser->prev;
    wi_real         real  = wi_string_to_real(token.start, token.count, NULL);
    _compiler_emit_push(compiler, wi_make_real_value(real));
}

static void
_compiler_add_esc_char(struct wi_compiler* compiler, struct wi_char_buf* buf, char c) {
    switch (c) {
        case 'n':
            wi_char_buf_add(buf, '\n');
            break;
        case 't':
            wi_char_buf_add(buf, '\t');
            break;
        case '\\':
            wi_char_buf_add(buf, '\\');
            break;
        case '"':
            wi_char_buf_add(buf, '"');
            break;
        case '0':
            wi_char_buf_add(buf, '\0');
            break;
        case '$':
            wi_char_buf_add(buf, '$');
            break;
        default:
            wi_parser_error_at_prev(compiler->parser, "invalid escape sequence \\%c", c);
            break;
    }
}

static void
_compiler_push_string(struct wi_compiler* compiler, struct wi_token token) {
    struct wi_char_buf buf;
    wi_char_buf_init(&buf, compiler->gc);
    wi_char_buf_reserve(&buf, token.count);

    for (int i = 0; i < token.count; i++) {
        if (token.start[i] == '\\' && i + 1 < token.count) {
            _compiler_add_esc_char(compiler, &buf, token.start[++i]);
        } else {
            wi_char_buf_add(&buf, token.start[i]);
        }
    }

    struct wi_string* string = wi_copy_cstring(compiler->gc, buf.data, buf.count);
    wi_char_buf_free(&buf);
    _compiler_emit_push(compiler, WI_MAKE_BOX_VALUE(string));
}

static void
_compiler_string_expr(struct wi_compiler* compiler) {
    _compiler_push_string(compiler, compiler->parser->prev);
}

static void
_compiler_interp_expr(struct wi_compiler* compiler) {
    /* compile first part of the string, i.e. everything that comes before ${...} */
    _compiler_push_string(compiler, compiler->parser->prev);

    for (;;) {
        /* compile whatever is between ${ and } */
        _compiler_expr(compiler);
        _compiler_emit_opcode(compiler, WI_OP_CONCAT); /* glue it onto result */

        /* did we hit that last WI_TOKEN_STRING? no? then it's just another part */
        if (!wi_parser_match(compiler->parser, WI_TOKEN_INTERP)) {
            break;
        }

        /* compile another part of the string */
        _compiler_push_string(compiler, compiler->parser->prev);
        _compiler_emit_opcode(compiler, WI_OP_CONCAT);
    }

    /* at long last, the last part of the string! a regular WI_TOKEN_STRING */
    wi_parser_expect(compiler->parser, WI_TOKEN_STRING);
    _compiler_push_string(compiler, compiler->parser->prev);
    _compiler_emit_opcode(compiler, WI_OP_CONCAT);
}

static void
_compiler_group_expr(struct wi_compiler* compiler) {
    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_PAREN);
}

static void
_compiler_array_expr(struct wi_compiler* compiler) {
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
    _compiler_emit_opcode_short(compiler, WI_OP_PUSH_ARRAY, count);
}

static void
_compiler_map_expr(struct wi_compiler* compiler) {
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
    _compiler_emit_opcode_short(compiler, WI_OP_PUSH_MAP, count);
}

static void
_compiler_null_expr(struct wi_compiler* compiler) {
    _compiler_emit_opcode(compiler, WI_OP_PUSH_NULL);
}

static void
_compiler_bool_expr(struct wi_compiler* compiler) {
    uint8_t opcode = compiler->parser->prev.kind == WI_TOKEN_TRUE ? WI_OP_PUSH_TRUE : WI_OP_PUSH_FALSE;
    _compiler_emit_opcode(compiler, opcode);
}

static void
_compiler_function_expr(struct wi_compiler* outer) {
    struct wi_compiler compiler;
    wi_compiler_init(&compiler, outer, outer->state, outer->parser, outer->global_attrs);
    _compiler_init_local(&compiler);

    /* check if previous token is truly a | and not || (pipe pipe, empty function) */
    bool has_params          = compiler.parser->prev.kind == WI_TOKEN_PIPE;
    compiler.prototype->name = _compiler_get_name(compiler.outer);

    if (compiler.prototype->name) {
        compiler.locals[0].name = (struct wi_token){
            .kind  = WI_TOKEN_NAME,
            .start = compiler.prototype->name->buf,
            .count = compiler.prototype->name->count,
            .line  = compiler.parser->curr.line,
            .col   = compiler.parser->curr.col,
        };
    }

    _compiler_begin_scope(&compiler);

    if (has_params && !wi_parser_check(compiler.parser, WI_TOKEN_PIPE)) {
        do {
#define _PARAMETER()                                                          \
    struct wi_token name  = wi_parser_expect(compiler.parser, WI_TOKEN_NAME); \
    wi_attrs        attrs = _compiler_parse_attrs(&compiler);                 \
    _compiler_decl_var(&compiler, name, attrs);                               \
    _compiler_def_var(&compiler, name, attrs)

            if (wi_parser_match(compiler.parser, WI_TOKEN_DOT_DOT_DOT)) {
                compiler.prototype->is_variadic = true;
                _PARAMETER();
                break;
            }

            if (compiler.prototype->arity == WI_PARAMETER_MAX) {
                wi_parser_error_at_curr(compiler.parser, "cannot have more than 255 parameters");
            }

            compiler.prototype->arity++;
            _PARAMETER();

#undef _PARAMETER
        } while (wi_parser_match(compiler.parser, WI_TOKEN_COMMA));
    }

    if (has_params) {
        wi_parser_expect(compiler.parser, WI_TOKEN_PIPE);
    }

    wi_parser_expect(compiler.parser, WI_TOKEN_FAT_ARROW);

    if (wi_parser_match(compiler.parser, WI_TOKEN_OPEN_BRACE)) {
        _compiler_block(&compiler);
    } else {
        _compiler_expr(&compiler);
        _compiler_emit_opcode(&compiler, WI_OP_RETURN);
    }

    for (int i = 1; i < compiler.local_count; i++) {
        _compiler_warn_unused(&compiler, &compiler.locals[i]);
    }

    struct wi_prototype* prototype = _compiler_end(&compiler);
    uint16_t             constant  = _compiler_make_constant(outer, WI_MAKE_BOX_VALUE(prototype));
    _compiler_emit_opcode_short(outer, WI_OP_PUSH_CLOSURE, constant);

    for (int i = 0; i < prototype->upvalue_count; i++) {
        struct wi_compiler_upvalue* upvalue = &compiler.upvalues[i];
        _compiler_emit_byte(outer, upvalue->index);
        _compiler_emit_byte(outer, upvalue->is_local ? 1 : 0);
    }
}

static uint8_t
_compiler_arg_list(struct wi_compiler* compiler, uint8_t start) {
    uint8_t arg_count = start;

    if (!wi_parser_check(compiler->parser, WI_TOKEN_CLOSE_PAREN)) {
        do {
            _compiler_expr(compiler);

            if (arg_count == WI_PARAMETER_MAX) {
                wi_parser_error_at_curr(compiler->parser, "cannot have more than 255 arguments in a call");
            }

            arg_count++;
        } while (wi_parser_match(compiler->parser, WI_TOKEN_COMMA));
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_PAREN);
    return arg_count;
}

static void
_compiler_field(struct wi_compiler* compiler);

static void
_compiler_object_expr(struct wi_compiler* compiler) {
    uint16_t field_count = 0;
    wi_parser_expect(compiler->parser, WI_TOKEN_OPEN_BRACE);

    while (!wi_parser_check(compiler->parser, WI_TOKEN_CLOSE_BRACE) && !wi_parser_is_at_end(compiler->parser)) {
        if (field_count == UINT16_MAX) {
            wi_parser_error_at_curr(compiler->parser, "cannot have more than %i fields in an object", UINT16_MAX);
        }

        struct wi_token field_name = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
        struct wi_token var_name   = compiler->var_name;
        compiler->var_name         = field_name;

        wi_parser_expect(compiler->parser, WI_TOKEN_COLON);

        uint16_t constant = _compiler_name_constant(compiler, field_name);
        _compiler_emit_opcode_short(compiler, WI_OP_PUSH, constant);
        _compiler_expr(compiler);

        wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);
        field_count++;
        compiler->var_name = var_name;
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_BRACE);
    _compiler_emit_opcode_short(compiler, WI_OP_PUSH_OBJECT, field_count);
}

static void
_compiler_require_expr(struct wi_compiler* compiler) {
    struct wi_token   path     = wi_parser_expect(compiler->parser, WI_TOKEN_STRING);
    struct wi_string* path_box = wi_copy_cstring(compiler->gc, path.start, path.count);

    if (!compiler->state->require_exists(compiler->state, path_box->buf)) {
        wi_parser_error_at(compiler->parser, path, "file %s does not exist", path_box->buf);
    }

    uint16_t path_constant = _compiler_make_constant(compiler, WI_MAKE_BOX_VALUE(path_box));
    _compiler_emit_opcode_short(compiler, WI_OP_REQUIRE, path_constant);
}

static void
_compiler_primary_expr(struct wi_compiler* compiler) {
    wi_parser_advance(compiler->parser);

    switch (compiler->parser->prev.kind) {
        case WI_TOKEN_NAME:
            _compiler_var_expr(compiler);
            break;
        case WI_TOKEN_REAL:
            _compiler_real_expr(compiler);
            break;
        case WI_TOKEN_STRING:
            _compiler_string_expr(compiler);
            break;
        case WI_TOKEN_INTERP:
            _compiler_interp_expr(compiler);
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
        case WI_TOKEN_NULL:
            _compiler_null_expr(compiler);
            break;
        case WI_TOKEN_TRUE:
        case WI_TOKEN_FALSE:
            _compiler_bool_expr(compiler);
            break;
        /* empty function is || which is lexed as "pipe pipe" */
        case WI_TOKEN_PIPE:
        case WI_TOKEN_PIPE_PIPE:
            _compiler_function_expr(compiler);
            break;
        case WI_TOKEN_OBJECT:
            _compiler_object_expr(compiler);
            break;
        case WI_TOKEN_REQUIRE:
            _compiler_require_expr(compiler);
            break;
        default:
            wi_parser_error_at_prev(compiler->parser, "expected expression");
            break;
    }
}

static void
_compiler_call(struct wi_compiler* compiler) {
    uint8_t arg_count = _compiler_arg_list(compiler, 0);
    _compiler_emit_opcode_byte(compiler, WI_OP_CALL, arg_count);
    compiler->last_call_offset = compiler->prototype->bytes.count - 2;
}

static void
_compiler_subscript(struct wi_compiler* compiler) {
    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_BRACKET);

    if (!wi_parser_match(compiler->parser, WI_TOKEN_EQUAL)) {
        _compiler_emit_opcode(compiler, WI_OP_SUBSCRIPT_GET);
        return;
    }

    _compiler_expr(compiler);
    _compiler_emit_opcode(compiler, WI_OP_SUBSCRIPT_SET);
}

static void
_compiler_field(struct wi_compiler* compiler) {
    struct wi_token name          = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
    uint16_t        name_constant = _compiler_name_constant(compiler, name);

    if (!wi_parser_match(compiler->parser, WI_TOKEN_EQUAL)) {
        _compiler_emit_opcode_short(compiler, WI_OP_GET_FIELD, name_constant);
        return;
    }

    _compiler_expr(compiler);
    _compiler_emit_opcode_short(compiler, WI_OP_SET_FIELD, name_constant);
}

static void
_compiler_invoke(struct wi_compiler* compiler) {
    struct wi_token name          = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
    uint16_t        name_constant = _compiler_name_constant(compiler, name);
    _compiler_emit_opcode_short(compiler, WI_OP_LOAD_METHOD, name_constant);

    wi_parser_expect(compiler->parser, WI_TOKEN_OPEN_PAREN);
    uint8_t arg_count = _compiler_arg_list(compiler, 1);

    _compiler_emit_opcode_byte(compiler, WI_OP_CALL, arg_count);
    compiler->last_call_offset = compiler->prototype->bytes.count - 2;
}

static void
_compiler_call_expr(struct wi_compiler* compiler) {
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
_compiler_unary_expr(struct wi_compiler* compiler);

static void
_compiler_new_expr(struct wi_compiler* compiler) {
    uint16_t count = 0;

    do {
        _compiler_unary_expr(compiler);

        if (count == UINT16_MAX) {
            wi_parser_error_at_curr(compiler->parser, "cannot merge more than %i objects in a 'new' expression",
                                    UINT16_MAX);
        }

        count++;
    } while (wi_parser_match(compiler->parser, WI_TOKEN_COMMA));

    _compiler_emit_opcode_short(compiler, WI_OP_NEW, count);

    if (!wi_parser_match(compiler->parser, WI_TOKEN_OPEN_BRACE)) {
        return;
    }

    while (!wi_parser_check(compiler->parser, WI_TOKEN_CLOSE_BRACE) && !wi_parser_is_at_end(compiler->parser)) {
        struct wi_token name          = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
        uint16_t        name_constant = _compiler_name_constant(compiler, name);
        struct wi_token var_name      = compiler->var_name;
        compiler->var_name            = name;

        wi_parser_expect(compiler->parser, WI_TOKEN_COLON);
        _compiler_expr(compiler);
        wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);

        _compiler_emit_opcode_short(compiler, WI_OP_INIT_FIELD, name_constant);
        compiler->var_name = var_name;
    }

    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_BRACE);
}

static void
_compiler_unary_expr(struct wi_compiler* compiler) {
    wi_parser_enter(compiler->parser);

    if (wi_parser_match(compiler->parser, WI_TOKEN_NEW)) {
        _compiler_new_expr(compiler);
        wi_parser_leave(compiler->parser);
        return;
    }

    if (!(wi_parser_match(compiler->parser, WI_TOKEN_HASH) || wi_parser_match(compiler->parser, WI_TOKEN_MINUS) ||
          wi_parser_match(compiler->parser, WI_TOKEN_TILDE) || wi_parser_match(compiler->parser, WI_TOKEN_BANG))) {
        _compiler_call_expr(compiler);
        wi_parser_leave(compiler->parser);
        return;
    }

    struct wi_token op = compiler->parser->prev;
    _compiler_unary_expr(compiler);
    wi_parser_leave(compiler->parser);

    switch (op.kind) {
        case WI_TOKEN_HASH:
            _compiler_emit_opcode(compiler, WI_OP_LEN);
            break;
        case WI_TOKEN_MINUS:
            _compiler_emit_opcode(compiler, WI_OP_NEGATE);
            break;
        case WI_TOKEN_TILDE:
            _compiler_emit_opcode(compiler, WI_OP_BIT_NOT);
            break;
        case WI_TOKEN_BANG:
            _compiler_emit_opcode(compiler, WI_OP_LOG_NOT);
            break;
        default:
            WI_UNREACHABLE();
            break;
    }
}

static void
_compiler_power_expr(struct wi_compiler* compiler) {
    wi_parser_enter(compiler->parser);
    _compiler_unary_expr(compiler);

    if (wi_parser_match(compiler->parser, WI_TOKEN_STAR_STAR)) {
        _compiler_power_expr(compiler);
        _compiler_emit_opcode(compiler, WI_OP_POWER);
    }

    wi_parser_leave(compiler->parser);
}

static void
_compiler_factor_expr(struct wi_compiler* compiler) {
    _compiler_power_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_STAR) || wi_parser_match(compiler->parser, WI_TOKEN_SLASH) ||
           wi_parser_match(compiler->parser, WI_TOKEN_PERCENT)) {
        uint8_t opcode;

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
        _compiler_emit_opcode(compiler, opcode);
    }
}

static void
_compiler_term_expr(struct wi_compiler* compiler) {
    _compiler_factor_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_PLUS) || wi_parser_match(compiler->parser, WI_TOKEN_MINUS)) {
        uint8_t opcode = compiler->parser->prev.kind == WI_TOKEN_PLUS ? WI_OP_ADD : WI_OP_SUBTRACT;
        _compiler_factor_expr(compiler);
        _compiler_emit_opcode(compiler, opcode);
    }
}

static void
_compiler_shift_expr(struct wi_compiler* compiler) {
    _compiler_term_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_GREATER_GREATER) ||
           wi_parser_match(compiler->parser, WI_TOKEN_LESS_LESS)) {
        uint8_t opcode = compiler->parser->prev.kind == WI_TOKEN_LESS_LESS ? WI_OP_BIT_SHL : WI_OP_BIT_SHR;
        _compiler_term_expr(compiler);
        _compiler_emit_opcode(compiler, opcode);
    }
}

static void
_compiler_bit_and_expr(struct wi_compiler* compiler) {
    _compiler_shift_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_AMPER)) {
        _compiler_shift_expr(compiler);
        _compiler_emit_opcode(compiler, WI_OP_BIT_AND);
    }
}

static void
_compiler_bit_xor_expr(struct wi_compiler* compiler) {
    _compiler_bit_and_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_CARET)) {
        _compiler_bit_and_expr(compiler);
        _compiler_emit_opcode(compiler, WI_OP_BIT_XOR);
    }
}

static void
_compiler_bit_or_expr(struct wi_compiler* compiler) {
    _compiler_bit_xor_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_PIPE)) {
        _compiler_bit_xor_expr(compiler);
        _compiler_emit_opcode(compiler, WI_OP_BIT_OR);
    }
}

static void
_compiler_comparison_expr(struct wi_compiler* compiler) {
    _compiler_bit_or_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_GREATER) ||
           wi_parser_match(compiler->parser, WI_TOKEN_GREATER_EQUAL) ||
           wi_parser_match(compiler->parser, WI_TOKEN_LESS) ||
           wi_parser_match(compiler->parser, WI_TOKEN_LESS_EQUAL)) {
        uint8_t opcode;

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
        _compiler_emit_opcode(compiler, opcode);
    }
}

static void
_compiler_equality_expr(struct wi_compiler* compiler) {
    _compiler_comparison_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_EQUAL_EQUAL) ||
           wi_parser_match(compiler->parser, WI_TOKEN_BANG_EQUAL)) {
        uint8_t opcode = compiler->parser->prev.kind == WI_TOKEN_EQUAL_EQUAL ? WI_OP_EQUAL : WI_OP_NOT_EQUAL;
        _compiler_comparison_expr(compiler);
        _compiler_emit_opcode(compiler, opcode);
    }
}

static void
_compiler_log_and_expr(struct wi_compiler* compiler) {
    _compiler_equality_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_AMPER_AMPER)) {
        int jump = _compiler_emit_jump(compiler, WI_OP_AND);
        _compiler_equality_expr(compiler);
        _compiler_patch_jump(compiler, jump);
    }
}

static void
_compiler_log_or_expr(struct wi_compiler* compiler) {
    _compiler_log_and_expr(compiler);

    while (wi_parser_match(compiler->parser, WI_TOKEN_PIPE_PIPE)) {
        int jump = _compiler_emit_jump(compiler, WI_OP_OR);
        _compiler_log_and_expr(compiler);
        _compiler_patch_jump(compiler, jump);
    }
}

static void
_compiler_assignment_expr(struct wi_compiler* compiler) {
    if (!wi_parser_check(compiler->parser, WI_TOKEN_NAME) || compiler->parser->next.kind != WI_TOKEN_EQUAL) {
        _compiler_log_or_expr(compiler);
        return;
    }

    wi_parser_advance(compiler->parser);
    _compiler_var(compiler, compiler->parser->prev);
}

static void
_compiler_expr(struct wi_compiler* compiler) {
    wi_parser_enter(compiler->parser);
    _compiler_assignment_expr(compiler);
    wi_parser_leave(compiler->parser);
}

static void
_compiler_expr_stmt(struct wi_compiler* compiler) {
    _compiler_expr(compiler);
    _compiler_emit_opcode(compiler, WI_OP_POP);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);
}

static void
_compiler_block_stmt(struct wi_compiler* compiler) {
    _compiler_begin_scope(compiler);
    _compiler_block(compiler);
    _compiler_end_scope(compiler);
}

static void
_compiler_if_stmt(struct wi_compiler* compiler) {
    wi_parser_expect(compiler->parser, WI_TOKEN_OPEN_PAREN);
    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_PAREN);

    int then_jump = _compiler_emit_jump(compiler, WI_OP_JUMP_IF_FALSE);
    _compiler_stmt(compiler);

    int else_jump = _compiler_emit_jump(compiler, WI_OP_JUMP);
    _compiler_patch_jump(compiler, then_jump);

    if (wi_parser_match(compiler->parser, WI_TOKEN_ELSE)) {
        _compiler_stmt(compiler);
    }

    _compiler_patch_jump(compiler, else_jump);
}

static void
_compiler_while_stmt(struct wi_compiler* compiler) {
    int enclosing_start       = compiler->innermost_loop_start;
    int enclosing_scope_depth = compiler->innermost_loop_scope_depth;

    compiler->innermost_loop_start       = compiler->prototype->bytes.count;
    compiler->innermost_loop_scope_depth = compiler->scope_depth;

    wi_parser_expect(compiler->parser, WI_TOKEN_OPEN_PAREN);
    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_PAREN);

    int exit_jump = _compiler_emit_jump(compiler, WI_OP_JUMP_IF_FALSE);
    _compiler_stmt(compiler);
    _compiler_emit_loop(compiler, compiler->innermost_loop_start);

    _compiler_patch_jump(compiler, exit_jump);
    _compiler_end_loop(compiler);

    compiler->innermost_loop_start       = enclosing_start;
    compiler->innermost_loop_scope_depth = enclosing_scope_depth;
}

static void
_compiler_for_init(struct wi_compiler* compiler) {
    if (wi_parser_match(compiler->parser, WI_TOKEN_SEMICOLON)) {
        return;
    }

    if (wi_parser_check_decl(compiler->parser)) {
        _compiler_name_decl(compiler);
    } else {
        _compiler_expr_stmt(compiler);
    }
}

static int
_compiler_for_cond(struct wi_compiler* compiler) {
    if (wi_parser_match(compiler->parser, WI_TOKEN_SEMICOLON)) {
        return -1;
    }

    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);

    return _compiler_emit_jump(compiler, WI_OP_JUMP_IF_FALSE);
}

static void
_compiler_for_incr(struct wi_compiler* compiler) {
    if (wi_parser_match(compiler->parser, WI_TOKEN_CLOSE_PAREN)) {
        return;
    }

    int body_jump  = _compiler_emit_jump(compiler, WI_OP_JUMP);
    int incr_start = compiler->prototype->bytes.count;

    _compiler_expr(compiler);
    _compiler_emit_opcode(compiler, WI_OP_POP);
    wi_parser_expect(compiler->parser, WI_TOKEN_CLOSE_PAREN);

    _compiler_emit_loop(compiler, compiler->innermost_loop_start);
    compiler->innermost_loop_start = incr_start;
    _compiler_patch_jump(compiler, body_jump);
}

static void
_compiler_for_stmt(struct wi_compiler* compiler) {
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
    }

    _compiler_end_loop(compiler);
    _compiler_end_scope(compiler);

    compiler->innermost_loop_start       = enclosing_start;
    compiler->innermost_loop_scope_depth = enclosing_scope_depth;
}

static void
_compiler_break_stmt(struct wi_compiler* compiler) {
    if (compiler->innermost_loop_start == -1) {
        wi_parser_error_at_prev(compiler->parser, "cannot use 'break' outside of a loop");
    }

    _compiler_pop_loop_locals(compiler);
    _compiler_emit_jump(compiler, WI_OP_LOOP_END);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);
}

static void
_compiler_continue_stmt(struct wi_compiler* compiler) {
    if (compiler->innermost_loop_start == -1) {
        wi_parser_error_at_prev(compiler->parser, "cannot use 'continue' outside of a loop");
    }

    _compiler_pop_loop_locals(compiler);
    _compiler_emit_loop(compiler, compiler->innermost_loop_start);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);
}

static void
_compiler_return_stmt(struct wi_compiler* compiler) {
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

    _compiler_emit_opcode(compiler, WI_OP_RETURN);
}

static void
_compiler_load_stmt(struct wi_compiler* compiler) {
    if (!_compiler_is_top_level(compiler)) {
        wi_parser_error_at_prev(compiler->parser, "can only use 'load' from top-level code");
    }

    /* wasm, macos, etc. */
#if !defined(_WIN32) && !defined(__linux__)
    wi_parser_error_at_prev(compiler->parser, "load statement is not supported on this platform");
#else

    /* prepare for seeing horrifying things... platform-specific code!!! */
    struct wi_token   path_token = wi_parser_expect(compiler->parser, WI_TOKEN_STRING);
    struct wi_string* path_box   = wi_copy_cstring(compiler->gc, path_token.start, path_token.count);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);

    struct wi_state* state = compiler->state;

    size_t raw_path_len = (size_t)path_box->count;
    char*  raw_path     = path_box->buf;
    char   path[4096]; /* i assume 4kb is enough for this mess */
    size_t path_size = sizeof(path);

    typedef void (*foreign_init_fn)(struct wi_state* state);

    /* platform-specific code is a legitimate way of torturing */
#ifdef _WIN32
    DWORD len = GetModuleFileName(NULL, path, (DWORD)path_size);

    if (len < 1 || len >= path_size) {
        wi_parser_error_at_prev(compiler->parser, "call to GetModuleFileName failed or path truncated");
    }

    char* last_slash = strrchr(path, '\\');

    if (last_slash) {
        *last_slash = '\0';
    }

    size_t path_len  = strlen(path);
    size_t remaining = path_size - path_len;

    /* 14: '\foreign\' + '.dll' + '\0' */
    if (remaining < 14 || raw_path_len > (remaining - 14)) {
        wi_parser_error_at_prev(compiler->parser, "foreign path too long");
    }

    snprintf(path + path_len, remaining, "\\foreign\\%s.dll", raw_path);
    HMODULE lib = LoadLibraryA(path);

    if (!lib) {
        wi_parser_error_at_prev(compiler->parser, "failed to load foreign %s\nattempted path: %s\nload error: %lu",
                                raw_path, path, GetLastError());
    }

    union {
        FARPROC         proc;
        foreign_init_fn fn;
    } proc_conv;

    proc_conv.proc       = GetProcAddress(lib, "wi_foreign_init");
    foreign_init_fn init = proc_conv.fn;
#else  /* __linux__ */
    ssize_t len = readlink("/proc/self/exe", path, path_size - 1);

    if (len == -1) {
        wi_parser_error_at_prev(compiler->parser, "call to readlink failed");
    }

    path[len] = '\0';

    char* last_slash = strrchr(path, '/');

    if (last_slash) {
        *last_slash = '\0';
    }

    size_t path_len  = strlen(path);
    size_t remaining = path_size - path_len;

    /* 13: '/foreign/' + '.so' + '\0' */
    if (remaining < 13 || raw_path_len > (remaining - 13)) {
        wi_parser_error_at_prev(compiler->parser, "foreign path too long");
    }

    snprintf(path + path_len, remaining, "/foreign/%s.so", raw_path);
    void* lib = dlopen(path, RTLD_NOW | RTLD_GLOBAL);

    if (!lib) {
        wi_parser_error_at_prev(compiler->parser, "failed to load foreign %s\nattempted path: %s\nload error: %s",
                                raw_path, path, dlerror());
    }

    union {
        void*           ptr;
        foreign_init_fn fn;
    } sym_conv;

    sym_conv.ptr         = dlsym(lib, "wi_foreign_init");
    foreign_init_fn init = sym_conv.fn;
#endif /* _WIN32 */

    if (!init) {
        wi_lib_close(lib);
        wi_parser_error_at_prev(compiler->parser, "foreign %s does not export wi_foreign_init", raw_path);
    }

    if (wi_state_add_lib(state, lib)) {
        init(state);
    }

#endif /* !defined(_WIN32) && !defined(__linux__) */
}

static void
_compiler_stmt(struct wi_compiler* compiler) {
    wi_parser_enter(compiler->parser);

    switch (compiler->parser->curr.kind) {
        case WI_TOKEN_OPEN_BRACE:
            wi_parser_advance(compiler->parser);
            _compiler_block_stmt(compiler);
            break;
        case WI_TOKEN_IF:
            wi_parser_advance(compiler->parser);
            _compiler_if_stmt(compiler);
            break;
        case WI_TOKEN_WHILE:
            wi_parser_advance(compiler->parser);
            _compiler_while_stmt(compiler);
            break;
        case WI_TOKEN_FOR:
            wi_parser_advance(compiler->parser);
            _compiler_for_stmt(compiler);
            break;
        case WI_TOKEN_BREAK:
            wi_parser_advance(compiler->parser);
            _compiler_break_stmt(compiler);
            break;
        case WI_TOKEN_CONTINUE:
            wi_parser_advance(compiler->parser);
            _compiler_continue_stmt(compiler);
            break;
        case WI_TOKEN_RETURN:
            wi_parser_advance(compiler->parser);
            _compiler_return_stmt(compiler);
            break;
        case WI_TOKEN_LOAD:
            wi_parser_advance(compiler->parser);
            _compiler_load_stmt(compiler);
            break;
        default:
            _compiler_expr_stmt(compiler);
            break;
    }

    wi_parser_leave(compiler->parser);
}

static void
_compiler_name_decl(struct wi_compiler* compiler) {
    struct wi_token name  = wi_parser_expect(compiler->parser, WI_TOKEN_NAME);
    wi_attrs        attrs = _compiler_parse_attrs(compiler);
    compiler->var_name    = name;
    _compiler_decl_var(compiler, name, attrs);

    wi_parser_expect(compiler->parser, WI_TOKEN_COLON_EQUAL);
    _compiler_expr(compiler);
    wi_parser_expect(compiler->parser, WI_TOKEN_SEMICOLON);

    _compiler_def_var(compiler, name, attrs);
    compiler->var_name = WI_BLANK_TOKEN;
}

static void
_compiler_decl(struct wi_compiler* compiler) {
    if (wi_parser_check_decl(compiler->parser)) {
        _compiler_name_decl(compiler);
    } else {
        _compiler_stmt(compiler);
    }
}

struct wi_prototype*
wi_compile(struct wi_state* state, const char* file_path, const char* src, struct wi_table* globals) {
    if (!wi_utf8_validate(src, (int)strlen(src))) {
        /* we can't use amazing wi_parser_X functions so... we do it the barbaric way... */
        state->error("compile error: invalid utf-8 sequence\n");
        state->error("   --> %s\n", file_path);
        return NULL;
    }

    struct wi_lexer lexer;
    wi_lexer_init(&lexer, file_path, src);

    struct wi_parser* parser = wi_new_parser(&lexer, state->gc);

    if (!parser) {
        wi_state_oom(state, "failed to allocate the parser (wi_compile)");
    }

    struct wi_compiler* compiler = wi_new_compiler(NULL, state, parser, globals);

    if (!compiler) {
        wi_delete_parser(parser);
        wi_state_oom(state, "failed to allocate the compiler (wi_compile)");
    }

    /*
        we capture this because of the load statement
        when a compilation of the script fails, we need to close any open foreign handles by
        the load statement, but using wi_state_close_libs would close every single handle opened
        even by a different script, so we do this:

        script1: [lib1] [lib2]
                 ^^^^^^^^^^^^^ these are captured by script1, script2 will have no idea these exist
        script2: [lib3] [lib4] <--- BOOM! ERROR!
                 ^^^^^^^^^^^^^ these are captured by script2, so only these will be closed in the case
                               of a compile error

        while this may not seem useful in general scripts, if you use REPL, technically every line is
        a different "script" (mostly because it compiles and runs over and over, a more correct term would
        be something like a "compilation unit"), so it's pretty useful there or in any similar situation!
    */
    struct wi_lib_node* libs = state->libs;

    if (setjmp(compiler->parser->error_jmp) == WI_RUN_OK) {
        while (!wi_parser_is_at_end(compiler->parser)) {
            _compiler_decl(compiler);
        }

        struct wi_prototype* prototype = _compiler_end(compiler);

        wi_delete_parser(parser);
        wi_delete_compiler(compiler);

        return prototype;
    }

    wi_state_close_libs_from(state, libs);

    wi_delete_parser(parser);
    wi_delete_compiler(compiler);
    state->gc->compiler = NULL;

    return NULL;
}
