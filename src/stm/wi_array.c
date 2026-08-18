#include <stdbool.h>

#include "../core/wi_state.h"
#include "../include/wi.h"

static struct wi_array*
_check_arg_array(struct wi_state* state, int arg) {
    if (!wi_value_is_array(state->ffi_stack[arg])) {
        wi_state_error(state, "bad argument %i - expected a value of type array but got %s", arg,
                       wi_value_type(state->ffi_stack[arg]));
    }

    return wi_value_as_array(state->ffi_stack[arg]);
}

static struct wi_array*
_check_arg1_array(struct wi_state* state) {
    return _check_arg_array(state, 1);
}

static void
_array_copy(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array     = _check_arg1_array(state);
    struct wi_array* new_array = wi_new_array(state->gc);
    state->ffi_stack[0]        = WI_MAKE_BOX_VALUE(new_array);

    if (array->items.count > 0) {
        wi_value_buf_reserve(&new_array->items, array->items.count);
        memcpy(new_array->items.data, array->items.data, sizeof(wi_value) * (size_t)array->items.count);
        new_array->items.count = array->items.count;
    }
}

static void
_array_clear(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);
    wi_value_buf_free(&array->items);
    wi_slot_set_null(state, 0);
}

static void
_array_capacity(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);
    wi_slot_set_real(state, 0, array->items.capacity);
}

static void
_array_count(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);
    wi_slot_set_real(state, 0, array->items.count);
}

static void
_array_reverse(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);
    state->ffi_stack[0]    = WI_MAKE_BOX_VALUE(array);

    for (int i = 0, j = array->items.count - 1; i < j; i++, j--) {
        wi_value temp        = array->items.data[i];
        array->items.data[i] = array->items.data[j];
        array->items.data[j] = temp;
    }
}

static void
_array_reversed(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array     = _check_arg1_array(state);
    struct wi_array* new_array = wi_new_array(state->gc);
    state->ffi_stack[0]        = WI_MAKE_BOX_VALUE(new_array);

    int count = array->items.count;
    wi_value_buf_reserve(&new_array->items, count);

    for (int i = 0; i < count; i++) {
        new_array->items.data[i] = array->items.data[count - 1 - i];
    }

    new_array->items.count = count;
}

static void
_array_add(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);
    wi_value_buf_add(&array->items, state->ffi_stack[2]);
    state->ffi_stack[0] = state->ffi_stack[2];
}

static void
_array_has(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);
    bool             found = false;

    for (int i = 0; i < array->items.count; i++) {
        if (wi_values_equal(array->items.data[i], state->ffi_stack[2])) {
            found = true;
            break;
        }
    }

    wi_slot_set_bool(state, 0, found);
}

static void
_array_index_of(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);
    int              index = -1;

    for (int i = 0; i < array->items.count; i++) {
        if (wi_values_equal(array->items.data[i], state->ffi_stack[2])) {
            index = i;
            break;
        }
    }

    wi_slot_set_real(state, 0, index);
}

static void
_array_remove(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);
    bool             found = false;

    for (int i = 0; i < array->items.count; i++) {
        if (!wi_values_equal(array->items.data[i], state->ffi_stack[2])) {
            continue;
        }

        for (int j = i; j < array->items.count - 1; j++) {
            array->items.data[j] = array->items.data[j + 1];
        }

        array->items.count--;
        found = true;
        break;
    }

    wi_slot_set_bool(state, 0, found);
}

static void
_array_remove_at(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);
    int              index = (int)wi_slot_check_real(state, 2);

    if (index < 0 || index >= array->items.count) {
        wi_state_error(state, "array index out of range: %i", index);
    }

    wi_value removed = array->items.data[index];

    for (int i = index; i < array->items.count - 1; i++) {
        array->items.data[i] = array->items.data[i + 1];
    }

    array->items.count--;
    state->ffi_stack[0] = removed;
}

static void
_array_pop(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);

    if (array->items.count == 0) {
        wi_state_error(state, "cannot pop from an empty array");
    }

    state->ffi_stack[0] = array->items.data[array->items.count - 1];
    array->items.count--;
}

static void
_array_concat(struct wi_state* state, int arg_count) {
    struct wi_array* result = wi_new_array(state->gc);
    state->ffi_stack[0]     = WI_MAKE_BOX_VALUE(result);

    for (int i = 0; i < arg_count; i++) {
        struct wi_array* array = _check_arg_array(state, i + 1);

        for (int j = 0; j < array->items.count; j++) {
            wi_value_buf_add(&result->items, array->items.data[j]);
        }
    }
}

static void
_array_slice(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = _check_arg1_array(state);
    int              start = (int)wi_slot_check_real(state, 2);
    int              end   = (int)wi_slot_check_real(state, 3);

    if (start < 0 || start > array->items.count || end < 0 || end > array->items.count || start > end) {
        wi_state_error(state, "array slice bounds out of range: %i to %i", start, end);
    }

    struct wi_array* result = wi_new_array(state->gc);
    state->ffi_stack[0]     = WI_MAKE_BOX_VALUE(result);

    int count = end - start;

    wi_value_buf_reserve(&result->items, count);
    memcpy(result->items.data, array->items.data + start, sizeof(wi_value) * (size_t)count);
    result->items.count = count;
}

static void
_array_each(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array*   array   = _check_arg1_array(state);
    struct wi_closure* closure = wi_slot_check_function(state, 2, 1);

    for (int i = 0; i < array->items.count; i++) {
        wi_state_push(state, WI_MAKE_BOX_VALUE(closure));
        wi_state_push(state, array->items.data[i]);
        wi_state_call(state, closure, 1, true);
    }

    wi_slot_set_null(state, 0);
}

static void
_array_select(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array*   array   = _check_arg1_array(state);
    struct wi_closure* closure = wi_slot_check_function(state, 2, 1);
    struct wi_array*   result  = wi_new_array(state->gc);
    state->ffi_stack[0]        = WI_MAKE_BOX_VALUE(result);

    wi_value_buf_reserve(&result->items, array->items.count);

    for (int i = 0; i < array->items.count; i++) {
        wi_state_push(state, WI_MAKE_BOX_VALUE(closure));
        wi_state_push(state, array->items.data[i]);
        wi_state_call(state, closure, 1, false);
        wi_value_buf_add(&result->items, wi_state_pop(state));
    }
}

static void
_array_where(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array*   array   = _check_arg1_array(state);
    struct wi_closure* closure = wi_slot_check_function(state, 2, 1);
    struct wi_array*   result  = wi_new_array(state->gc);
    state->ffi_stack[0]        = WI_MAKE_BOX_VALUE(result);

    for (int i = 0; i < array->items.count; i++) {
        wi_value item = array->items.data[i];

        wi_state_push(state, WI_MAKE_BOX_VALUE(closure));
        wi_state_push(state, item);
        wi_state_call(state, closure, 1, false);

        if (!wi_value_is_falsy(wi_state_pop(state))) {
            wi_value_buf_add(&result->items, item);
        }
    }
}

void
wi_state_def_array_stm(struct wi_state* state) {
    struct wi_table* table = &state->stm_array;
    wi_table_set_foreign(table, "copy", _array_copy, 1, false);
    wi_table_set_foreign(table, "clear", _array_clear, 1, false);
    wi_table_set_foreign(table, "capacity", _array_capacity, 1, false);
    wi_table_set_foreign(table, "count", _array_count, 1, false);
    wi_table_set_foreign(table, "reverse", _array_reverse, 1, false);
    wi_table_set_foreign(table, "reversed", _array_reversed, 1, false);
    wi_table_set_foreign(table, "add", _array_add, 2, false);
    wi_table_set_foreign(table, "has", _array_has, 2, false);
    wi_table_set_foreign(table, "index_of", _array_index_of, 2, false);
    wi_table_set_foreign(table, "remove", _array_remove, 2, false);
    wi_table_set_foreign(table, "remove_at", _array_remove_at, 2, false);
    wi_table_set_foreign(table, "pop", _array_pop, 1, false);
    wi_table_set_foreign(table, "concat", _array_concat, 0, true);
    wi_table_set_foreign(table, "slice", _array_slice, 3, false);
    wi_table_set_foreign(table, "each", _array_each, 2, false);
    wi_table_set_foreign(table, "select", _array_select, 2, false);
    wi_table_set_foreign(table, "where", _array_where, 2, false);
}
