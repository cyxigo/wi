#include "wi_base.h"

#include <limits.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../core/wi_state.h"
#include "../include/wi.h"

static void
_base_print(struct wi_state* state, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        wi_value_print(state->ffi_stack[i + 1]);
        printf("\n");
    }

    wi_slot_set_null(state, 0);
}

static void
_base_input(struct wi_state* state, int arg_count) {
    char buf[2048];

    if (!fgets(buf, sizeof(buf), stdin)) {
        wi_slot_set_null(state, 0);
        return;
    }

    buf[strcspn(buf, "\n")] = 0;
    state->ffi_stack[0]     = WI_MAKE_BOX_VALUE(wi_make_string(state->gc, buf));
}

static void
_base_is_main(struct wi_state* state, int arg_count) {
    struct wi_call_frame* frame = wi_state_frame(state);
    wi_slot_set_bool(state, 0, !frame->closure->is_required);
}

static void
_base_exit(struct wi_state* state, int arg_count) {
    wi_state_abort(state);
}

static void
_base_error(struct wi_state* state, int arg_count) {
    wi_state_error(state, "%s", wi_slot_check_string(state, 1, NULL));
}

static void
_base_assert(struct wi_state* state, int arg_count) {
    bool is_falsy = wi_value_is_falsy(state->ffi_stack[1]);

    if (is_falsy) {
        wi_state_error(state, "%s", wi_slot_check_string(state, 2, NULL));
    }

    wi_slot_set_bool(state, 0, !is_falsy);
}

static void
_base_try(struct wi_state* state, int arg_count) {
    struct wi_closure*   closure   = wi_slot_check_function(state, 1, -1);
    struct wi_prototype* prototype = closure->prototype;
    wi_state_check_arity(state, prototype->arity, (uint8_t)(arg_count - 1), prototype->is_variadic);

    struct wi_object* result = wi_new_object(state->gc, NULL);
    state->ffi_stack[0]      = WI_MAKE_BOX_VALUE(result);
    wi_table_reserve(&result->fields, 3);

    struct wi_recovery* recovery = wi_state_push_recovery(state);

    if (setjmp(recovery->jmp) == WI_JMP_OK) {
        wi_state_push(state, WI_MAKE_BOX_VALUE(closure));

        for (int i = 0; i < arg_count - 1; i++) {
            wi_state_push(state, state->ffi_stack[i + 2]);
        }

        wi_state_call(state, closure, (uint8_t)(arg_count - 1), false);
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
    wi_slot_set_string(state, 0, wi_value_type(state->ffi_stack[1]));
}

static void
_is_type_function(struct wi_state* state, bool (*fn)(wi_value value)) {
    wi_slot_set_bool(state, 0, fn(state->ffi_stack[1]));
}

static void
_base_is_real(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_real);
}

static void
_base_is_null(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_null);
}

static void
_base_is_bool(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_bool);
}

static void
_base_is_string(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_string);
}

static void
_base_is_array(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_array);
}

static void
_base_is_map(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_map);
}

static void
_base_is_foreign(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_foreign);
}

static void
_base_is_function(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_closure);
}

static void
_base_is_object(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_object);
}

static void
_base_is_userdata(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_userdata);
}

static void
_base_is_falsy(struct wi_state* state, int arg_count) {
    _is_type_function(state, wi_value_is_falsy);
}

static void
_base_to_real(struct wi_state* state, int arg_count) {
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
        wi_real           real   = wi_string_to_real(string->chars, string->len, &end);

        if (end != string->chars + string->len) {
            wi_state_error(state, "invalid real format %s", string->chars);
        }

        result = wi_make_real_value(real);
    } else {
        wi_state_error(state, "bad argument 1 - cannot convert a value of type %s to real", wi_value_type(value));
    }

    state->ffi_stack[0] = result;
}

static void
_base_to_bool(struct wi_state* state, int arg_count) {
    wi_slot_set_bool(state, 0, !wi_value_is_falsy(state->ffi_stack[1]));
}

static void
_base_to_string(struct wi_state* state, int arg_count) {
    if (wi_slot_is_string(state, 1)) {
        state->ffi_stack[0] = state->ffi_stack[1];
        return;
    }

    char*             string     = wi_value_to_string(state->ffi_stack[1]);
    struct wi_string* string_box = wi_take_cstring(state->gc, string, (int)strlen(string));
    state->ffi_stack[0]          = WI_MAKE_BOX_VALUE(string_box);
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
    struct wi_object* object = _check_arg1_object(state);
    wi_slot_check_string(state, 2, NULL);
    wi_slot_set_bool(state, 0, wi_table_get(&object->fields, state->ffi_stack[2], NULL));
}

static void
_base_fields(struct wi_state* state, int arg_count) {
    struct wi_object* object = _check_arg1_object(state);
    struct wi_map*    fields = wi_new_map(state->gc);
    state->ffi_stack[0]      = WI_MAKE_BOX_VALUE(fields);
    wi_table_copy(&object->fields, &fields->items);
}

void
wi_state_def_base_foreign(struct wi_state* state) {
    wi_def_foreign(state, "print", _base_print, 0, true);
    wi_def_foreign(state, "input", _base_input, 0, false);
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
}
