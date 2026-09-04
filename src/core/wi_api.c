#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../std/wi_base.h"
#include "../std/wi_io.h"
#include "../std/wi_math.h"
#include "../std/wi_os.h"
#include "../stm/wi_array.h"
#include "../stm/wi_map.h"
#include "../stm/wi_string.h"
#include "wi_box.h"
#include "wi_gc.h"
#include "wi_state.h"
#include "wi_table.h"
#include "wi_util.h"
#include "wi_value.h"

void
wi_def_stm(struct wi_state* state) {
    wi_state_def_stm_string(state);
    wi_state_def_stm_array(state);
    wi_state_def_stm_map(state);
}

void
wi_def_std(struct wi_state* state) {
    wi_state_def_std_base(state);
    wi_state_def_std_os(state);
    wi_state_def_std_io(state);
    wi_state_def_std_math(state);
}

void
wi_def(struct wi_state* state, const char* name) {
    wi_value          value    = wi_state_top(state);
    struct wi_string* name_box = wi_make_string(state->gc, name);
    WI_GC_PUSH_ROOT(state->gc, name_box);

    wi_table_set(&state->foreign, WI_MAKE_BOX_VALUE(name_box), value);
    wi_gc_pop_root(state->gc);
    wi_state_drop(state);
}

static bool
_find_in_table(struct wi_state* state, struct wi_table* table, const char* name) {
    struct wi_string* name_box = wi_make_string(state->gc, name);
    WI_GC_PUSH_ROOT(state->gc, name_box);

    wi_value value;
    bool     found = wi_table_get(table, WI_MAKE_BOX_VALUE(name_box), &value);
    wi_gc_pop_root(state->gc);

    if (found) {
        wi_state_ppush(state, value);
    }

    return found;
}

bool
wi_find(struct wi_state* state, const char* name) {
    return _find_in_table(state, &state->globals, name);
}

void
wi_call(struct wi_state* state, uint8_t arg_count, bool drop) {
    wi_value value = wi_state_peek(state, arg_count);

    if (WI_UNLIKELY(!wi_value_is_foreign(value) && !wi_value_is_closure(value))) {
        wi_state_error(state, "cannot call a value of type %s", wi_value_type(value));
    }

    wi_state_call(state, value, arg_count, drop);
}

bool
wi_pcall(struct wi_state* state, uint8_t arg_count, bool drop, char** error) {
    /* an offset in case stack reallocates (very scary) */
    ptrdiff_t           start    = state->stack_top - state->stack - arg_count - 1;
    struct wi_recovery* recovery = wi_state_push_recovery(state);
    bool                failed;

    if (setjmp(recovery->jmp) == WI_RUN_OK) {
        wi_value value = wi_state_peek(state, arg_count);

        if (WI_UNLIKELY(!wi_value_is_foreign(value) && !wi_value_is_closure(value))) {
            wi_state_error(state, "cannot call a value of type %s", wi_value_type(value));
        }

        wi_state_call(state, value, arg_count, drop);
        failed = false;
    } else {
        failed = true;
    }

    if (failed) {
        state->stack_top = state->stack + start;
    }

    if (error) {
        /*
            strdup because after we pop the recovery, GC will free its error
            in _base_try we don't need to do that because it's immediately passed to a new object
        */
        *error = failed ? wi_strdup(recovery->error->buf) : NULL;
    }

    wi_state_pop_recovery(state);
    return !failed;
}

bool
wi_is_real(struct wi_state* state) {
    return wi_value_is_real(wi_state_top(state));
}

bool
wi_is_null(struct wi_state* state) {
    return wi_value_is_null(wi_state_top(state));
}

bool
wi_is_bool(struct wi_state* state) {
    return wi_value_is_bool(wi_state_top(state));
}

bool
wi_is_string(struct wi_state* state) {
    return wi_value_is_string(wi_state_top(state));
}

bool
wi_is_object(struct wi_state* state) {
    return wi_value_is_object(wi_state_top(state));
}

static bool
_is_userdata(wi_value value, const char* name) {
    return wi_value_is_userdata(value) && strcmp(wi_value_as_userdata(value)->name->buf, name) == 0;
}

bool
wi_is_userdata(struct wi_state* state, const char* name) {
    return _is_userdata(wi_state_top(state), name);
}

void
wi_push_real(struct wi_state* state, wi_real real) {
    wi_state_ppush(state, wi_make_real_value(real));
}

void
wi_push_null(struct wi_state* state) {
    wi_state_ppush(state, wi_make_null_value());
}

void
wi_push_bool(struct wi_state* state, bool boolean) {
    wi_state_ppush(state, wi_make_bool_value(boolean));
}

void
wi_push_string(struct wi_state* state, const char* string) {
    struct wi_string* box = wi_make_string(state->gc, string);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

void
wi_push_foreign(struct wi_state* state, wi_foreign_fn fn, uint8_t arity, bool is_variadic) {
    struct wi_foreign* box = wi_new_foreign(state->gc, fn, arity, is_variadic);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

struct wi_object*
wi_push_object(struct wi_state* state) {
    struct wi_object* box = wi_new_object(state->gc);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
    return box;
}

void
wi_push_userdata(struct wi_state* state, const char* name, void* userdata, wi_userdata_finalizer_fn finalizer) {
    struct wi_string* name_box = wi_make_string(state->gc, name);
    WI_GC_PUSH_ROOT(state->gc, name_box);

    struct wi_userdata* box = wi_new_userdata(state->gc, name_box, userdata, finalizer);
    wi_gc_pop_root(state->gc);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

void
wi_drop(struct wi_state* state) {
    wi_state_drop(state);
}

wi_real
wi_pop_real(struct wi_state* state) {
    wi_value value = wi_state_pop(state);

    if (WI_UNLIKELY(!wi_value_is_real(value))) {
        wi_state_error(state, "expected a value of type real but got %s", wi_value_type(value));
    }

    return wi_value_as_real(value);
}

void
wi_pop_null(struct wi_state* state) {
    wi_value value = wi_state_pop(state);

    if (WI_UNLIKELY(!wi_value_is_null(value))) {
        wi_state_error(state, "expected a value of type null but got %s", wi_value_type(value));
    }
}

bool
wi_pop_bool(struct wi_state* state) {
    wi_value value = wi_state_pop(state);

    if (WI_UNLIKELY(!wi_value_is_bool(value))) {
        wi_state_error(state, "expected a value of type bool but got %s", wi_value_type(value));
    }

    return wi_value_as_bool(value);
}

char*
wi_pop_string(struct wi_state* state, int* count, int* len) {
    wi_value value = wi_state_pop(state);

    if (WI_UNLIKELY(!wi_value_is_string(value))) {
        wi_state_error(state, "expected a value of type string but got %s", wi_value_type(value));
    }

    struct wi_string* string = wi_value_as_string(value);

    if (count) {
        *count = string->count;
    }

    if (len) {
        *len = string->len;
    }

    return string->buf;
}

struct wi_object*
wi_pop_object(struct wi_state* state) {
    wi_value value = wi_state_pop(state);

    if (WI_UNLIKELY(!wi_value_is_object(value))) {
        wi_state_error(state, "expected a value of type object but got %s", wi_value_type(value));
    }

    return wi_value_as_object(value);
}

void*
wi_pop_userdata(struct wi_state* state, const char* name) {
    if (WI_UNLIKELY(!wi_is_userdata(state, name))) {
        wi_state_error(state, "expected a value of type %s but got %s", name, wi_value_type(wi_state_pop(state)));
    }

    return wi_value_as_userdata(wi_state_pop(state))->data;
}

bool
wi_arg_is_real(struct wi_state* state, uint8_t arg) {
    return wi_value_is_real(state->ffi_stack[arg]);
}

bool
wi_arg_is_null(struct wi_state* state, uint8_t arg) {
    return wi_value_is_null(state->ffi_stack[arg]);
}

bool
wi_arg_is_bool(struct wi_state* state, uint8_t arg) {
    return wi_value_is_bool(state->ffi_stack[arg]);
}

bool
wi_arg_is_string(struct wi_state* state, uint8_t arg) {
    return wi_value_is_string(state->ffi_stack[arg]);
}

bool
wi_arg_is_function(struct wi_state* state, uint8_t arg) {
    wi_value value = state->ffi_stack[arg];
    return wi_value_is_foreign(value) || wi_value_is_closure(value);
}

bool
wi_arg_is_object(struct wi_state* state, uint8_t arg) {
    return wi_value_is_object(state->ffi_stack[arg]);
}

bool
wi_arg_is_userdata(struct wi_state* state, uint8_t arg, const char* name) {
    return _is_userdata(state->ffi_stack[arg], name);
}

wi_real
wi_arg_real(struct wi_state* state, uint8_t arg) {
    if (WI_UNLIKELY(!wi_arg_is_real(state, arg))) {
        wi_state_error(state, "bad argument %i - expected a value of type real but got %s", arg,
                       wi_value_type(state->ffi_stack[arg]));
    }

    return wi_value_as_real(state->ffi_stack[arg]);
}

void
wi_arg_null(struct wi_state* state, uint8_t arg) {
    if (WI_UNLIKELY(!wi_arg_is_null(state, arg))) {
        wi_state_error(state, "bad argument %i - expected a value of type null but got %s", arg,
                       wi_value_type(state->ffi_stack[arg]));
    }
}

bool
wi_arg_bool(struct wi_state* state, uint8_t arg) {
    if (WI_UNLIKELY(!wi_arg_is_bool(state, arg))) {
        wi_state_error(state, "bad argument %i - expected a value of type bool but got %s", arg,
                       wi_value_type(state->ffi_stack[arg]));
    }

    return wi_value_as_bool(state->ffi_stack[arg]);
}

char*
wi_arg_string(struct wi_state* state, uint8_t arg, int* count, int* len) {
    if (WI_UNLIKELY(!wi_arg_is_string(state, arg))) {
        wi_state_error(state, "bad argument %i - expected a value of type string but got %s", arg,
                       wi_value_type(state->ffi_stack[arg]));
    }

    struct wi_string* string = wi_value_as_string(state->ffi_stack[arg]);

    if (count) {
        *count = string->count;
    }

    if (len) {
        *len = string->len;
    }

    return string->buf;
}

void
wi_arg_function(struct wi_state* state, uint8_t arg, uint8_t arity) {
    wi_value function = state->ffi_stack[arg];

    if (!wi_value_is_foreign(function) && !wi_value_is_closure(function)) {
        wi_state_error(state, "bad argument %i - cannot use a value of type %s as a callback", arg,
                       wi_value_type(function));
    }

    if (wi_value_is_closure(function)) {
        struct wi_prototype* prototype = wi_value_as_closure(function)->prototype;
        wi_state_check_arity(state, prototype->arity, arity, prototype->is_variadic);
    } else {
        struct wi_foreign* foreign = wi_value_as_foreign(function);
        wi_state_check_arity(state, foreign->arity, arity, foreign->is_variadic);
    }

    wi_state_ppush(state, function);
}

struct wi_object*
wi_arg_object(struct wi_state* state, uint8_t arg) {
    wi_value value = state->ffi_stack[arg];

    if (WI_UNLIKELY(!wi_value_is_object(value))) {
        wi_state_error(state, "bad argument %i - expected a value of type object but got %s", arg,
                       wi_value_type(value));
    }

    return wi_value_as_object(value);
}

void*
wi_arg_userdata(struct wi_state* state, uint8_t arg, const char* name) {
    wi_value value = state->ffi_stack[arg];

    if (WI_UNLIKELY(!wi_arg_is_userdata(state, arg, name))) {
        wi_state_error(state, "bad argument %i - expected a value of type %s but got %s", arg, name,
                       wi_value_type(value));
    }

    return wi_value_as_userdata(value)->data;
}

void
wi_object_set(struct wi_state* state, struct wi_object* object, const char* name) {
    wi_value value = wi_state_top(state);

    struct wi_string* name_box   = wi_make_string(state->gc, name);
    wi_value          name_value = WI_MAKE_BOX_VALUE(name_box);
    WI_GC_PUSH_ROOT(state->gc, name_box);

    if (wi_table_set(&object->fields, name_value, value)) {
        WI_GC_WRITE_BARRIER(state->gc, object, name_value);
    }

    WI_GC_WRITE_BARRIER(state->gc, object, value);
    wi_gc_pop_root(state->gc);
    wi_state_drop(state);
}

bool
wi_object_get(struct wi_state* state, struct wi_object* object, const char* name) {
    return _find_in_table(state, &object->fields, name);
}
