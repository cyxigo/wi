#include <stdbool.h>

#include "../core/wi_state.h"
#include "../include/wi.h"

static wi_array_t*
_check_arg_array(wi_state_t* state, int arg) {
    if (!wi_value_is_array(state->api_stack[arg])) {
        wi_state_error(state, "bad argument %i - expected a value of type array but got %s", arg,
                       wi_value_type(state->api_stack[arg]));
    }

    return wi_value_as_array(state->api_stack[arg]);
}

static wi_array_t*
_check_arg1_array(wi_state_t* state) {
    return _check_arg_array(state, 1);
}

static wi_array_t*
_check_arg2_array(wi_state_t* state) {
    return _check_arg_array(state, 2);
}

static void
_array_copy(wi_state_t* state, int arg_count) {
    wi_array_t* array     = _check_arg1_array(state);
    wi_array_t* new_array = wi_new_array(state->gc);
    state->api_stack[0]   = WI_MAKE_BOX_VALUE(new_array);

    if (array->items.count > 0) {
        wi_value_buf_reserve(&new_array->items, array->items.count);
        memcpy(new_array->items.data, array->items.data, sizeof(wi_value_t) * (size_t)array->items.count);
        new_array->items.count = array->items.count;
    }
}

static void
_array_clear(wi_state_t* state, int arg_count) {
    wi_array_t* array = _check_arg1_array(state);
    wi_value_buf_free(&array->items);
    wi_value_buf_init(&array->items, state->gc);
}

static void
_array_count(wi_state_t* state, int arg_count) {
    wi_array_t* array = _check_arg1_array(state);
    wi_slot_set_real(state, 0, array->items.count);
}

static void
_array_capacity(wi_state_t* state, int arg_count) {
    wi_array_t* array = _check_arg1_array(state);
    wi_slot_set_real(state, 0, array->items.capacity);
}

static void
_array_equals(wi_state_t* state, int arg_count) {
    wi_array_t* a      = _check_arg1_array(state);
    wi_array_t* b      = _check_arg2_array(state);
    bool        equals = false;

    if (a->items.count != b->items.count) {
        goto end;
    }

    for (int i = 0; i < a->items.count; i++) {
        if (!wi_values_equal(a->items.data[i], b->items.data[i])) {
            goto end;
        }
    }

    equals = true;

end:
    wi_slot_set_bool(state, 0, equals);
}

static void
_array_reverse(wi_state_t* state, int arg_count) {
    wi_array_t* array   = _check_arg1_array(state);
    state->api_stack[0] = WI_MAKE_BOX_VALUE(array);

    for (int i = 0, j = array->items.count - 1; i < j; i++, j--) {
        wi_value_t temp      = array->items.data[i];
        array->items.data[i] = array->items.data[j];
        array->items.data[j] = temp;
    }
}

static void
_array_reversed(wi_state_t* state, int arg_count) {
    wi_array_t* array     = _check_arg1_array(state);
    wi_array_t* new_array = wi_new_array(state->gc);
    state->api_stack[0]   = WI_MAKE_BOX_VALUE(new_array);

    int count = array->items.count;
    wi_value_buf_reserve(&new_array->items, count);

    for (int i = 0; i < count; i++) {
        new_array->items.data[i] = array->items.data[count - 1 - i];
    }

    new_array->items.count = count;
}

static void
_array_add(wi_state_t* state, int arg_count) {
    wi_array_t* array = _check_arg1_array(state);
    wi_value_buf_add(&array->items, state->api_stack[2]);
    state->api_stack[0] = state->api_stack[2];
}

static void
_array_has(wi_state_t* state, int arg_count) {
    wi_array_t* array = _check_arg1_array(state);
    bool        found = false;

    for (int i = 0; i < array->items.count; i++) {
        if (wi_values_equal(array->items.data[i], state->api_stack[2])) {
            found = true;
            break;
        }
    }

    wi_slot_set_bool(state, 0, found);
}

static void
_array_remove(wi_state_t* state, int arg_count) {
    wi_array_t* array = _check_arg1_array(state);
    bool        found = false;

    for (int i = 0; i < array->items.count; i++) {
        if (!wi_values_equal(array->items.data[i], state->api_stack[2])) {
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
_array_remove_at(wi_state_t* state, int arg_count) {
    wi_array_t* array = _check_arg1_array(state);
    int         index = (int)wi_slot_check_real(state, 2);

    if (index < 0 || index >= array->items.count) {
        wi_state_error(state, "array index out of range: %i", index);
    }

    wi_value_t removed = array->items.data[index];

    for (int i = index; i < array->items.count - 1; i++) {
        array->items.data[i] = array->items.data[i + 1];
    }

    array->items.count--;
    state->api_stack[0] = removed;
}

static void
_array_concat(wi_state_t* state, int arg_count) {
    wi_array_t* result  = wi_new_array(state->gc);
    state->api_stack[0] = WI_MAKE_BOX_VALUE(result);

    int total = 0;

    for (int i = 0; i < arg_count; i++) {
        total += _check_arg_array(state, i + 1)->items.count;
    }

    wi_value_buf_reserve(&result->items, total);

    for (int i = 0; i < arg_count; i++) {
        wi_array_t* array = _check_arg_array(state, i + 1);

        for (int j = 0; j < array->items.count; j++) {
            wi_value_buf_add(&result->items, array->items.data[j]);
        }
    }
}

static void
_array_slice(wi_state_t* state, int arg_count) {
    wi_array_t* array = _check_arg1_array(state);
    int         start = (int)wi_slot_check_real(state, 2);
    int         end   = (int)wi_slot_check_real(state, 3);

    if (start < 0 || start >= array->items.count || end < 0 || end > array->items.count || start > end) {
        wi_state_error(state, "array slice bounds out of range: %i to %i", start, end);
    }

    wi_array_t* result  = wi_new_array(state->gc);
    state->api_stack[0] = WI_MAKE_BOX_VALUE(result);

    int count = end - start;

    if (count < 0) {
        return;
    }

    wi_value_buf_reserve(&result->items, count);
    memcpy(result->items.data, array->items.data + start, sizeof(wi_value_t) * (size_t)count);
    result->items.count = count;
}

void
wi_state_def_array_foreign(wi_state_t* state) {
    wi_object_t* object = wi_def_object(state, "array");

    wi_set_field_foreign(state, object, "copy", _array_copy, 1);
    wi_set_field_foreign(state, object, "clear", _array_clear, 1);
    wi_set_field_foreign(state, object, "count", _array_count, 1);
    wi_set_field_foreign(state, object, "capacity", _array_capacity, 1);
    wi_set_field_foreign(state, object, "equals", _array_equals, 2);
    wi_set_field_foreign(state, object, "reverse", _array_reverse, 1);
    wi_set_field_foreign(state, object, "reversed", _array_reversed, 1);
    wi_set_field_foreign(state, object, "add", _array_add, 2);
    wi_set_field_foreign(state, object, "has", _array_has, 2);
    wi_set_field_foreign(state, object, "remove", _array_remove, 2);
    wi_set_field_foreign(state, object, "remove_at", _array_remove_at, 2);
    wi_set_field_foreign(state, object, "concat", _array_concat, -1);
    wi_set_field_foreign(state, object, "slice", _array_slice, 3);
}
