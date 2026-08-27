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
wi_def_foreign(struct wi_state* state, const char* name, wi_foreign_fn fn, int arity, bool is_variadic) {
    wi_table_set_foreign(&state->foreign, name, fn, arity, is_variadic);
}

struct wi_object*
wi_def_object(struct wi_state* state, const char* name) {
    struct wi_string* name_box = wi_make_string(state->gc, name);
    WI_GC_PUSH_ROOT(state->gc, name_box);

    struct wi_object* object = wi_new_object(state->gc);
    WI_GC_PUSH_ROOT(state->gc, object);

    wi_table_set(&state->foreign, WI_MAKE_BOX_VALUE(name_box), WI_MAKE_BOX_VALUE(object));

    wi_gc_pop_root(state->gc);
    wi_gc_pop_root(state->gc);

    return object;
}

static void
_set_field(struct wi_state* state, struct wi_object* object, const char* name, wi_value value) {
    bool is_box = wi_value_is_box(value);

    if (is_box) {
        WI_GC_PUSH_ROOT(state->gc, wi_value_as_box(value));
    }

    struct wi_string* name_box   = wi_make_string(state->gc, name);
    wi_value          name_value = WI_MAKE_BOX_VALUE(name_box);
    WI_GC_PUSH_ROOT(state->gc, name_box);

    if (wi_table_set(&object->fields, name_value, value)) {
        WI_GC_WRITE_BARRIER(state->gc, object, name_value);
    }

    WI_GC_WRITE_BARRIER(state->gc, object, value);

    if (is_box) {
        wi_gc_pop_root(state->gc);
    }

    wi_gc_pop_root(state->gc);
}

void
wi_object_set_field_real(struct wi_state* state, struct wi_object* object, const char* name, wi_real real) {
    _set_field(state, object, name, wi_make_real_value(real));
}

void
wi_object_set_field_bool(struct wi_state* state, struct wi_object* object, const char* name, bool boolean) {
    _set_field(state, object, name, wi_make_bool_value(boolean));
}

void
wi_object_set_field_string(struct wi_state* state, struct wi_object* object, const char* name,
                           const char* string) {
    struct wi_string* box = wi_make_string(state->gc, string);
    _set_field(state, object, name, WI_MAKE_BOX_VALUE(box));
}

static struct wi_userdata*
_new_userdata(struct wi_state* state, const char* name, void* userdata, wi_userdata_finalizer_fn finalizer) {
    struct wi_string* name_box = wi_make_string(state->gc, name);
    WI_GC_PUSH_ROOT(state->gc, name_box);

    struct wi_userdata* box = wi_new_userdata(state->gc, name_box, userdata, finalizer);
    wi_gc_pop_root(state->gc);

    return box;
}

void
wi_object_set_field_userdata(struct wi_state* state, struct wi_object* object, const char* field_name,
                             const char* name, void* userdata, wi_userdata_finalizer_fn finalizer) {
    struct wi_userdata* box = _new_userdata(state, name, userdata, finalizer);
    _set_field(state, object, field_name, WI_MAKE_BOX_VALUE(box));
}

void
wi_object_set_field_foreign(struct wi_state* state, struct wi_object* object, const char* name, wi_foreign_fn fn,
                            int arity, bool is_variadic) {
    struct wi_foreign* foreign = wi_new_foreign(state->gc, fn, arity, is_variadic);
    _set_field(state, object, name, WI_MAKE_BOX_VALUE(foreign));
}

void
wi_tune_gc(struct wi_state* state, size_t min_heap, size_t heap_grow_factor, size_t young_max) {
    struct wi_gc* gc     = state->gc;
    gc->min_heap         = min_heap;
    gc->heap_grow_factor = heap_grow_factor < 1 ? 1 : heap_grow_factor;
    gc->young_max        = young_max;
    gc->next_major       = min_heap;
}

bool
wi_find_function(struct wi_state* state, const char* name) {
    struct wi_string* name_box = wi_make_string(state->gc, name);
    WI_GC_PUSH_ROOT(state->gc, name_box);

    wi_value value;
    bool found = wi_table_get(&state->globals, WI_MAKE_BOX_VALUE(name_box), &value) && wi_value_is_closure(value);
    wi_gc_pop_root(state->gc);

    if (found) {
        wi_state_push(state, value);
    }

    return found;
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
    wi_state_push(state, wi_make_real_value(real));
}

void
wi_push_null(struct wi_state* state) {
    wi_state_push(state, wi_make_null_value());
}

void
wi_push_bool(struct wi_state* state, bool boolean) {
    wi_state_push(state, wi_make_bool_value(boolean));
}

void
wi_push_string(struct wi_state* state, const char* string) {
    struct wi_string* box = wi_make_string(state->gc, string);
    wi_state_push(state, WI_MAKE_BOX_VALUE(box));
}

void
wi_push_userdata(struct wi_state* state, const char* name, void* userdata, wi_userdata_finalizer_fn finalizer) {
    struct wi_userdata* box = _new_userdata(state, name, userdata, finalizer);
    wi_state_push(state, WI_MAKE_BOX_VALUE(box));
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

void*
wi_pop_userdata(struct wi_state* state, const char* name) {
    if (WI_UNLIKELY(!wi_is_userdata(state, name))) {
        wi_state_error(state, "expected a value of type userdata %s but got %s", name,
                       wi_value_type(wi_state_pop(state)));
    }

    return wi_value_as_userdata(wi_state_pop(state))->data;
}

bool
wi_call(struct wi_state* state, uint8_t arg_count, char** error) {
    /* an offset, not a pointer - in case stack reallocates (very scary) */
    ptrdiff_t           start    = state->stack_top - state->stack - arg_count - 1;
    struct wi_recovery* recovery = wi_state_push_recovery(state);
    bool                failed;

    if (setjmp(recovery->jmp) == WI_RUN_OK) {
        wi_value value = wi_state_peek(state, arg_count);

        if (WI_UNLIKELY(!wi_value_is_foreign(value) && !wi_value_is_closure(value))) {
            wi_state_error(state, "cannot call a value of type %s", wi_value_type(value));
        }

        wi_state_call(state, value, arg_count, false);
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

static void
_slot_set(struct wi_state* state, int slot, wi_value value) {
    state->ffi_stack[slot] = value;
}

bool
wi_slot_is_real(struct wi_state* state, int slot) {
    return wi_value_is_real(state->ffi_stack[slot]);
}

bool
wi_slot_is_null(struct wi_state* state, int slot) {
    return wi_value_is_null(state->ffi_stack[slot]);
}

bool
wi_slot_is_bool(struct wi_state* state, int slot) {
    return wi_value_is_bool(state->ffi_stack[slot]);
}

bool
wi_slot_is_string(struct wi_state* state, int slot) {
    return wi_value_is_string(state->ffi_stack[slot]);
}

bool
wi_slot_is_userdata(struct wi_state* state, int slot, const char* name) {
    return _is_userdata(state->ffi_stack[slot], name);
}

void
wi_slot_set_real(struct wi_state* state, int slot, wi_real real) {
    _slot_set(state, slot, wi_make_real_value(real));
}

void
wi_slot_set_null(struct wi_state* state, int slot) {
    _slot_set(state, slot, wi_make_null_value());
}

void
wi_slot_set_bool(struct wi_state* state, int slot, bool boolean) {
    _slot_set(state, slot, wi_make_bool_value(boolean));
}

void
wi_slot_set_string(struct wi_state* state, int slot, const char* string) {
    struct wi_string* box = wi_make_string(state->gc, string);
    _slot_set(state, slot, WI_MAKE_BOX_VALUE(box));
}

void
wi_slot_set_userdata(struct wi_state* state, int slot, const char* name, void* userdata,
                     wi_userdata_finalizer_fn finalizer) {
    struct wi_userdata* box = _new_userdata(state, name, userdata, finalizer);
    _slot_set(state, slot, WI_MAKE_BOX_VALUE(box));
}

wi_real
wi_slot_get_real(struct wi_state* state, int slot) {
    if (WI_UNLIKELY(!wi_slot_is_real(state, slot))) {
        wi_state_error(state, "bad argument %i - expected a value of type real but got %s", slot,
                       wi_value_type(state->ffi_stack[slot]));
    }

    return wi_value_as_real(state->ffi_stack[slot]);
}

void
wi_slot_get_null(struct wi_state* state, int slot) {
    if (WI_UNLIKELY(!wi_slot_is_null(state, slot))) {
        wi_state_error(state, "bad argument %i - expected a value of type null but got %s", slot,
                       wi_value_type(state->ffi_stack[slot]));
    }
}

bool
wi_slot_get_bool(struct wi_state* state, int slot) {
    if (WI_UNLIKELY(!wi_slot_is_bool(state, slot))) {
        wi_state_error(state, "bad argument %i - expected a value of type bool but got %s", slot,
                       wi_value_type(state->ffi_stack[slot]));
    }

    return wi_value_as_bool(state->ffi_stack[slot]);
}

char*
wi_slot_get_string(struct wi_state* state, int slot, int* count, int* len) {
    if (WI_UNLIKELY(!wi_slot_is_string(state, slot))) {
        wi_state_error(state, "bad argument %i - expected a value of type string but got %s", slot,
                       wi_value_type(state->ffi_stack[slot]));
    }

    struct wi_string* string = wi_value_as_string(state->ffi_stack[slot]);

    if (count) {
        *count = string->count;
    }

    if (len) {
        *len = string->len;
    }

    return string->buf;
}

void*
wi_slot_get_userdata(struct wi_state* state, int slot, const char* name) {
    wi_value value = state->ffi_stack[slot];

    if (WI_UNLIKELY(!wi_slot_is_userdata(state, slot, name))) {
        wi_state_error(state, "bad argument %i - expected a value of type userdata %s but got %s", slot, name,
                       wi_value_type(value));
    }

    return wi_value_as_userdata(value)->data;
}
