#include "wi_base.h"

#include <limits.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../include/wi.h"
#include "../core/wi_state.h"

static void
_print(struct wi_state* state, uint8_t arg_count, bool newline) {
    for (int i = 0; i < arg_count; i++) {
        wi_value_print(state, state->ffi_stack[i + 1]);

        if (!newline) {
            continue;
        }

        state->out("\n");
    }

    wi_push_null(state);
}

static void
_base_print(struct wi_state* state, uint8_t arg_count) {
    _print(state, arg_count, false);
}

static void
_base_puts(struct wi_state* state, uint8_t arg_count) {
    _print(state, arg_count, true);
}

static void
_base_input(struct wi_state* state, uint8_t arg_count) {
    const char* prompt = "";

    if (arg_count == 1) {
        prompt = wi_arg_string(state, 1, NULL, NULL);
    } else if (arg_count == 0) {
        /* do nothing */
    } else {
        wi_state_error(state, "input() takes only 0 or 1 arguments");
    }

    char* line;

    if (!wi_read_line(&line, prompt)) {
        wi_push_null(state);
        return;
    }

    if (!wi_utf8_validate(line, (int)strlen(line))) {
        free(line);
        wi_state_error(state, "invalid utf-8 sequence in input()");
    }

    struct wi_string* line_box = wi_take_calloc_string(state->gc, line, (int)strlen(line));
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(line_box));
}

static void
_base_ismain(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_call_frame* frame = wi_state_frame(state);
    wi_push_bool(state, frame->closure->module->is_main);
}

static void
_base_exit(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_state_abort(state);
}

static void
_base_error(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_state_error(state, "%s", wi_arg_string(state, 1, NULL, NULL));
}

static void
_base_assert(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    bool is_falsy = wi_value_is_falsy(state->ffi_stack[1]);

    if (is_falsy) {
        wi_state_error(state, "%s", wi_arg_string(state, 2, NULL, NULL));
    }

    wi_push_bool(state, !is_falsy);
}

static void
_base_try(struct wi_state* state, uint8_t arg_count) {
    struct wi_object*   result      = wi_push_object(state);
    uint8_t             f_arg_count = (uint8_t)(arg_count - 1);
    struct wi_recovery* recovery    = wi_state_push_recovery(state);

    if (setjmp(recovery->jmp) == WI_RUN_OK) {
        wi_arg_function(state, 1, f_arg_count);

        for (uint8_t i = 0; i < f_arg_count; i++) {
            wi_state_ppush(state, state->ffi_stack[i + 2]);
        }

        wi_call(state, f_arg_count, false);
        wi_object_set(state, result, "value");

        wi_push_bool(state, true);
        wi_object_set(state, result, "ok");

        wi_push_null(state);
        wi_object_set(state, result, "error");
    } else {
        wi_push_null(state);
        wi_object_set(state, result, "value");

        wi_push_bool(state, false);
        wi_object_set(state, result, "ok");

        wi_state_ppush(state, WI_MAKE_BOX_VALUE(recovery->error));
        wi_object_set(state, result, "error");
    }

    wi_state_pop_recovery(state);
}

static void
_base_type(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_push_string(state, wi_value_type(state->ffi_stack[1]));
}

static void
_istype(struct wi_state* state, bool (*fn)(wi_value value)) {
    wi_push_bool(state, fn(state->ffi_stack[1]));
}

static void
_base_isreal(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_real);
}

static void
_base_isnull(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_null);
}

static void
_base_isbool(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_bool);
}

static void
_base_isstring(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_string);
}

static void
_base_isarray(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_array);
}

static void
_base_ismap(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_map);
}

static void
_base_isforeign(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_foreign);
}

static void
_base_isfunction(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_closure);
}

static void
_base_isobject(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_object);
}

static void
_base_isuserdata(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_userdata);
}

static void
_base_isfalsy(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _istype(state, wi_value_is_falsy);
}

static void
_base_real(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_value value = state->ffi_stack[1];
    wi_value result;

    if (wi_value_is_real(value)) {
        result = value;
    } else if (wi_value_is_null(value)) {
        result = wi_make_real_value(0);
    } else if (wi_value_is_bool(value)) {
        result = wi_make_real_value(wi_value_as_bool(value) ? 1 : 0);
    } else if (wi_value_is_string(value)) {
        struct wi_string* string = wi_value_as_string(state->ffi_stack[1]);
        char*             end    = NULL;
        wi_real           real   = wi_string_to_real(string->buf, string->count, &end);

        if (end != string->buf + string->count) {
            wi_state_error(state, "invalid real format %s", string->buf);
        }

        result = wi_make_real_value(real);
    } else {
        wi_state_error(state, "bad argument 1 - cannot convert a value of type %s to real", wi_value_type(value));
    }

    wi_state_ppush(state, result);
}

static void
_base_bool(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_push_bool(state, !wi_value_is_falsy(state->ffi_stack[1]));
}

static void
_base_string(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);

    if (wi_arg_is_string(state, 1)) {
        wi_state_ppush(state, state->ffi_stack[1]);
        return;
    }

    char* string = wi_value_to_string(state->ffi_stack[1]);

    if (!string) {
        wi_state_oom(state, "failed to allocate a string (_base_string)");
    }

    struct wi_string* box = wi_take_calloc_string(state->gc, string, (int)strlen(string));
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

static void
_base_char(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int64_t cp = wi_state_real_to_int(state, wi_arg_real(state, 1));

    if (cp < 0 || cp > 0x10ffff) {
        wi_state_error(state, "invalid codepoint: %lld", cp);
    }

    char cp_buf[5] = {0};
    int  cp_len;

    if (cp < 0x80) { /* 0xxxxxxx */
        cp_buf[0] = (char)cp;
        cp_len    = 1;
    } else if (cp < 0x800) { /* 110xxxxx 10xxxxxx */
        cp_buf[0] = (char)(0xc0 | (cp >> 6));
        cp_buf[1] = (char)(0x80 | (cp & 0x3f));
        cp_len    = 2;
    } else if (cp < 0x10000) { /* 1110xxxx 10xxxxxx 10xxxxxx */
        cp_buf[0] = (char)(0xe0 | (cp >> 12));
        cp_buf[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        cp_buf[2] = (char)(0x80 | (cp & 0x3f));
        cp_len    = 3;
    } else { /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        cp_buf[0] = (char)(0xf0 | (cp >> 18));
        cp_buf[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
        cp_buf[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
        cp_buf[3] = (char)(0x80 | (cp & 0x3f));
        cp_len    = 4;
    }

    if (!wi_utf8_validate(cp_buf, cp_len)) {
        wi_state_error(state, "invalid codepoint: %lld", cp);
    }

    struct wi_string* box = wi_copy_cstring(state->gc, cp_buf, cp_len);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

static void
_base_hasfield(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_object* object = wi_arg_object(state, 1);
    wi_arg_string(state, 2, NULL, NULL);
    wi_push_bool(state, wi_table_get(&object->fields, state->ffi_stack[2], NULL));
}

static void
_base_fields(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_object* object = wi_arg_object(state, 1);
    struct wi_map*    fields = wi_push_map(state);
    wi_table_copy(&object->fields, &fields->items);
}

static bool
_equals(struct wi_state* state, wi_value a, wi_value b, int c_depth);

static bool
_tables_equal(struct wi_state* state, struct wi_table* a, struct wi_table* b, int c_depth) {
    if (a->live_count != b->live_count) {
        return false;
    }

    for (int i = 0; i < a->capacity; i++) {
        struct wi_entry* entry = &a->entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        wi_value b_value;

        if (!wi_table_get(b, entry->key, &b_value) || !_equals(state, entry->value, b_value, c_depth + 1)) {
            return false;
        }
    }

    return true;
}

static bool
_equals(struct wi_state* state, wi_value a, wi_value b, int c_depth) {
    if (wi_values_equal(a, b)) {
        return true;
    }

    if (WI_UNLIKELY(c_depth == WI_CSTACK_MAX)) {
        wi_state_error(state, "C stack overflow (limit is %i)", WI_CSTACK_MAX);
    }

    if (wi_value_is_array(a) && wi_value_is_array(b)) {
        struct wi_array* a_box = wi_value_as_array(a);
        struct wi_array* b_box = wi_value_as_array(b);

        if (a_box->items.count != b_box->items.count) {
            return false;
        }

        for (int i = 0; i < a_box->items.count; i++) {
            if (!_equals(state, a_box->items.data[i], b_box->items.data[i], c_depth + 1)) {
                return false;
            }
        }

        return true;
    }

    if (wi_value_is_map(a) && wi_value_is_map(b)) {
        return _tables_equal(state, &wi_value_as_map(a)->items, &wi_value_as_map(b)->items, c_depth + 1);
    }

    if (wi_value_is_object(a) && wi_value_is_object(b)) {
        return _tables_equal(state, &wi_value_as_object(a)->fields, &wi_value_as_object(b)->fields, c_depth + 1);
    }

    return false;
}

static void
_base_equals(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_push_bool(state, _equals(state, state->ffi_stack[1], state->ffi_stack[2], state->c_depth));
}

void
wi_state_def_std_base(struct wi_state* state) {
    struct wi_module* module = wi_push_module(state);
    wi_def(state, "std");
    wi_foreign_entry functions[] = {
        {"print",      _base_print,      0, true },
        {"puts",       _base_puts,       0, true },
        {"input",      _base_input,      0, true },
        {"ismain",     _base_ismain,     0, false},
        {"exit",       _base_exit,       0, false},

        {"error",      _base_error,      1, false},
        {"assert",     _base_assert,     2, false},
        {"try",        _base_try,        1, true },

        {"type",       _base_type,       1, false},
        {"isreal",     _base_isreal,     1, false},
        {"isnull",     _base_isnull,     1, false},
        {"isbool",     _base_isbool,     1, false},
        {"isstring",   _base_isstring,   1, false},
        {"isarray",    _base_isarray,    1, false},
        {"ismap",      _base_ismap,      1, false},
        {"isforeign",  _base_isforeign,  1, false},
        {"isfunction", _base_isfunction, 1, false},
        {"isobject",   _base_isobject,   1, false},
        {"isuserdata", _base_isuserdata, 1, false},
        {"isfalsy",    _base_isfalsy,    1, false},

        {"real",       _base_real,       1, false},
        {"bool",       _base_bool,       1, false},
        {"string",     _base_string,     1, false},
        {"char",       _base_char,       1, false},

        {"hasfield",   _base_hasfield,   2, false},
        {"fields",     _base_fields,     1, false},

        {"equals",     _base_equals,     2, false},
    };

    WI_MODULE_EXPORT_FOREIGN_ALL(state, module, functions);
}
