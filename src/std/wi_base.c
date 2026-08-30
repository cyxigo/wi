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
_print(struct wi_state* state, int arg_count, bool newline) {
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
_base_print(struct wi_state* state, int arg_count) {
    _print(state, arg_count, false);
}

static void
_base_puts(struct wi_state* state, int arg_count) {
    _print(state, arg_count, true);
}

static void
_base_input(struct wi_state* state, int arg_count) {
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
        wi_state_error(state, "invalid utf-8 sequence from input()");
    }

    struct wi_string* line_box = wi_take_cstring(state->gc, line, (int)strlen(line));
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(line_box));
}

static void
_base_is_main(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_call_frame* frame = wi_state_frame(state);
    wi_push_bool(state, !frame->closure->required);
}

static void
_base_exit(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_state_abort(state);
}

static void
_base_error(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_state_error(state, "%s", wi_arg_string(state, 1, NULL, NULL));
}

static void
_base_assert(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    bool is_falsy = wi_value_is_falsy(state->ffi_stack[1]);

    if (is_falsy) {
        wi_state_error(state, "%s", wi_arg_string(state, 2, NULL, NULL));
    }

    wi_push_bool(state, !is_falsy);
}

static void
_base_try(struct wi_state* state, int arg_count) {
    struct wi_object* result = wi_new_object(state->gc);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(result));
    wi_table_reserve(&result->fields, 3);

    uint8_t f_arg_count = (uint8_t)(arg_count - 1);
    wi_arg_function(state, 1, f_arg_count);

    struct wi_recovery* recovery = wi_state_push_recovery(state);

    if (setjmp(recovery->jmp) == WI_RUN_OK) {
        for (uint8_t i = 0; i < f_arg_count; i++) {
            wi_state_ppush(state, state->ffi_stack[i + 2]);
        }

        wi_call(state, f_arg_count, false);
        wi_value call_value = wi_state_pop(state);

        wi_table_set(&result->fields, WI_MAKE_BOX_VALUE(state->ok_str), wi_make_true_value());
        wi_table_set(&result->fields, WI_MAKE_BOX_VALUE(state->value_str), call_value);
        wi_table_set(&result->fields, WI_MAKE_BOX_VALUE(state->error_str), wi_make_null_value());
    } else {
        wi_table_set(&result->fields, WI_MAKE_BOX_VALUE(state->ok_str), wi_make_false_value());
        wi_table_set(&result->fields, WI_MAKE_BOX_VALUE(state->value_str), wi_make_null_value());
        wi_table_set(&result->fields, WI_MAKE_BOX_VALUE(state->error_str), WI_MAKE_BOX_VALUE(recovery->error));
    }

    wi_state_pop_recovery(state);
}

static void
_base_type(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_push_string(state, wi_value_type(state->ffi_stack[1]));
}

static void
_is_type_function(struct wi_state* state, bool (*fn)(wi_value value)) {
    wi_push_bool(state, fn(state->ffi_stack[1]));
}

static void
_base_is_real(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_real);
}

static void
_base_is_null(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_null);
}

static void
_base_is_bool(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_bool);
}

static void
_base_is_string(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_string);
}

static void
_base_is_array(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_array);
}

static void
_base_is_map(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_map);
}

static void
_base_is_foreign(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_foreign);
}

static void
_base_is_function(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_closure);
}

static void
_base_is_object(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_object);
}

static void
_base_is_userdata(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_userdata);
}

static void
_base_is_falsy(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _is_type_function(state, wi_value_is_falsy);
}

static void
_base_to_real(struct wi_state* state, int arg_count) {
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
_base_to_bool(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_push_bool(state, !wi_value_is_falsy(state->ffi_stack[1]));
}

static void
_base_to_string(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);

    if (wi_arg_is_string(state, 1)) {
        wi_state_ppush(state, state->ffi_stack[1]);
        return;
    }

    char* string = wi_value_to_string(state->ffi_stack[1]);

    if (!string) {
        wi_state_oom(state, "failed to allocate a string (_base_string)");
    }

    struct wi_string* box = wi_take_cstring(state->gc, string, (int)strlen(string));
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

static struct wi_object*
_check_arg1_object(struct wi_state* state) {
    if (!wi_value_is_object(state->ffi_stack[1])) {
        wi_state_error(state, "bad argument 1 - expected a value of type object but got %s",
                       wi_value_type(state->ffi_stack[1]));
    }

    return wi_value_as_object(state->ffi_stack[1]);
}

static void
_base_has_field(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_object* object = _check_arg1_object(state);
    wi_arg_string(state, 2, NULL, NULL);
    wi_push_bool(state, wi_table_get(&object->fields, state->ffi_stack[2], NULL));
}

static void
_base_fields(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_object* object = _check_arg1_object(state);
    struct wi_map*    fields = wi_new_map(state->gc);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(fields));
    wi_table_copy(&object->fields, &fields->items);
}

static bool
_equals(wi_value a, wi_value b);

static bool
_tables_equal(struct wi_table* a, struct wi_table* b) {
    if (a->live_count != b->live_count) {
        return false;
    }

    for (int i = 0; i < a->capacity; i++) {
        struct wi_entry* entry = &a->entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        wi_value b_value;

        if (!wi_table_get(b, entry->key, &b_value) || !_equals(entry->value, b_value)) {
            return false;
        }
    }

    return true;
}

static bool
_equals(wi_value a, wi_value b) {
    if (wi_values_equal(a, b)) {
        return true;
    }

    if (wi_value_is_array(a) && wi_value_is_array(b)) {
        struct wi_array* a_box = wi_value_as_array(a);
        struct wi_array* b_box = wi_value_as_array(b);

        if (a_box->items.count != b_box->items.count) {
            return false;
        }

        for (int i = 0; i < a_box->items.count; i++) {
            if (!_equals(a_box->items.data[i], b_box->items.data[i])) {
                return false;
            }
        }

        return true;
    }

    if (wi_value_is_map(a) && wi_value_is_map(b)) {
        return _tables_equal(&wi_value_as_map(a)->items, &wi_value_as_map(b)->items);
    }

    if (wi_value_is_object(a) && wi_value_is_object(b)) {
        return _tables_equal(&wi_value_as_object(a)->fields, &wi_value_as_object(b)->fields);
    }

    return false;
}

static void
_base_equals(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_push_bool(state, _equals(state->ffi_stack[1], state->ffi_stack[2]));
}

void
wi_state_def_std_base(struct wi_state* state) {
    wi_def_foreign(state, "print", _base_print, 0, true);
    wi_def_foreign(state, "puts", _base_puts, 0, true);
    wi_def_foreign(state, "input", _base_input, 0, true);
    wi_def_foreign(state, "is_main", _base_is_main, 0, false);
    wi_def_foreign(state, "exit", _base_exit, 0, false);

    wi_def_foreign(state, "error", _base_error, 1, false);
    wi_def_foreign(state, "assert", _base_assert, 2, false);
    wi_def_foreign(state, "try", _base_try, 1, true);

    wi_def_foreign(state, "type", _base_type, 1, false);
    wi_def_foreign(state, "is_real", _base_is_real, 1, false);
    wi_def_foreign(state, "is_null", _base_is_null, 1, false);
    wi_def_foreign(state, "is_bool", _base_is_bool, 1, false);
    wi_def_foreign(state, "is_string", _base_is_string, 1, false);
    wi_def_foreign(state, "is_array", _base_is_array, 1, false);
    wi_def_foreign(state, "is_map", _base_is_map, 1, false);
    wi_def_foreign(state, "is_foreign", _base_is_foreign, 1, false);
    wi_def_foreign(state, "is_function", _base_is_function, 1, false);
    wi_def_foreign(state, "is_object", _base_is_object, 1, false);
    wi_def_foreign(state, "is_userdata", _base_is_userdata, 1, false);
    wi_def_foreign(state, "is_falsy", _base_is_falsy, 1, false);

    wi_def_foreign(state, "to_real", _base_to_real, 1, false);
    wi_def_foreign(state, "to_bool", _base_to_bool, 1, false);
    wi_def_foreign(state, "to_string", _base_to_string, 1, false);

    wi_def_foreign(state, "has_field", _base_has_field, 2, false);
    wi_def_foreign(state, "fields", _base_fields, 1, false);

    wi_def_foreign(state, "equals", _base_equals, 2, false);
}
