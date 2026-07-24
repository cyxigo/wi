#include <stdbool.h>

#include "../core/wi_state.h"
#include "../include/wi.h"

static wi_map_t*
_check_arg_map(wi_state_t* state, int slot) {
    if (!wi_value_is_map(state->api_stack[slot])) {
        wi_state_error(state, "bad argument %i - expected a value of type map but got %s", slot,
                       wi_value_type(state->api_stack[slot]));
    }

    return wi_value_as_map(state->api_stack[slot]);
}

static wi_map_t*
_check_arg1_map(wi_state_t* state) {
    return _check_arg_map(state, 1);
}

static wi_map_t*
_check_arg2_map(wi_state_t* state) {
    return _check_arg_map(state, 2);
}

static void
_map_copy(wi_state_t* state, int arg_count) {
    wi_map_t* src       = _check_arg1_map(state);
    wi_map_t* dest      = wi_new_map(state->gc);
    state->api_stack[0] = WI_MAKE_BOX_VALUE(dest);
    wi_table_copy(&src->items, &dest->items);
}

static void
_map_clear(wi_state_t* state, int arg_count) {
    wi_map_t* map = _check_arg1_map(state);
    wi_table_free(&map->items);
    wi_slot_set_null(state, 0);
}

static void
_map_capacity(wi_state_t* state, int arg_count) {
    wi_map_t* map = _check_arg1_map(state);
    wi_slot_set_real(state, 0, map->items.capacity);
}

static void
_map_count(wi_state_t* state, int arg_count) {
    wi_map_t* map = _check_arg1_map(state);
    wi_slot_set_real(state, 0, wi_table_count(&map->items));
}

static void
_map_equals(wi_state_t* state, int arg_count) {
    wi_map_t* a      = _check_arg1_map(state);
    wi_map_t* b      = _check_arg2_map(state);
    bool      equals = false;

    int a_count = wi_table_count(&a->items);
    int b_count = wi_table_count(&b->items);

    if (a_count != b_count) {
        goto end;
    }

    for (int i = 0; i < a->items.capacity; i++) {
        wi_entry_t* entry = &a->items.entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        wi_value_t b_value;

        if (!wi_table_get(&b->items, entry->key, &b_value) || !wi_values_equal(entry->value, b_value)) {
            goto end;
        }
    }

    equals = true;
end:
    wi_slot_set_bool(state, 0, equals);
}

static void
_map_keys(wi_state_t* state, int arg_count) {
    wi_map_t*   map     = _check_arg1_map(state);
    wi_array_t* result  = wi_new_array(state->gc);
    state->api_stack[0] = WI_MAKE_BOX_VALUE(result);

    wi_value_buf_reserve(&result->items, map->items.count);

    for (int i = 0; i < map->items.capacity; i++) {
        wi_entry_t* entry = &map->items.entries[i];

        if (!wi_value_is_empty(entry->key)) {
            wi_value_buf_add(&result->items, entry->key);
        }
    }
}

static void
_map_values(wi_state_t* state, int arg_count) {
    wi_map_t*   map     = _check_arg1_map(state);
    wi_array_t* result  = wi_new_array(state->gc);
    state->api_stack[0] = WI_MAKE_BOX_VALUE(result);

    wi_value_buf_reserve(&result->items, map->items.count);

    for (int i = 0; i < map->items.capacity; i++) {
        wi_entry_t* entry = &map->items.entries[i];

        if (!wi_value_is_empty(entry->key)) {
            wi_value_buf_add(&result->items, entry->value);
        }
    }
}

static void
_map_has(wi_state_t* state, int arg_count) {
    wi_map_t* map    = _check_arg1_map(state);
    bool      exists = wi_table_get(&map->items, state->api_stack[2], NULL);
    wi_slot_set_bool(state, 0, exists);
}

static void
_map_get_or_default(wi_state_t* state, int arg_count) {
    wi_map_t*  map = _check_arg1_map(state);
    wi_value_t value;

    if (wi_table_get(&map->items, state->api_stack[2], &value)) {
        state->api_stack[0] = value;
        return;
    }

    state->api_stack[0] = state->api_stack[3];
}

static void
_map_remove(wi_state_t* state, int arg_count) {
    wi_map_t* map = _check_arg1_map(state);
    wi_slot_set_bool(state, 0, wi_table_delete(&map->items, state->api_stack[2]));
}

static void
_map_each(wi_state_t* state, int arg_count) {
    wi_map_t*     map     = _check_arg1_map(state);
    wi_closure_t* closure = wi_slot_check_function(state, 2, 2);

    for (int i = 0; i < map->items.capacity; i++) {
        wi_entry_t* entry = &map->items.entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        wi_state_push(state, WI_MAKE_BOX_VALUE(closure));
        wi_state_push(state, entry->key);
        wi_state_push(state, entry->value);

        wi_state_call(state, closure, 2, true);
    }

    wi_slot_set_null(state, 0);
}

void
wi_state_def_map_foreign(wi_state_t* state) {
    wi_object_t* object = wi_def_object(state, "map");

    wi_set_field_foreign(state, object, "copy", _map_copy, 1, false);
    wi_set_field_foreign(state, object, "clear", _map_clear, 1, false);
    wi_set_field_foreign(state, object, "capacity", _map_capacity, 1, false);
    wi_set_field_foreign(state, object, "count", _map_count, 1, false);
    wi_set_field_foreign(state, object, "equals", _map_equals, 2, false);
    wi_set_field_foreign(state, object, "keys", _map_keys, 1, false);
    wi_set_field_foreign(state, object, "values", _map_values, 1, false);
    wi_set_field_foreign(state, object, "has", _map_has, 2, false);
    wi_set_field_foreign(state, object, "get_or_default", _map_get_or_default, 3, false);
    wi_set_field_foreign(state, object, "remove", _map_remove, 2, false);
    wi_set_field_foreign(state, object, "each", _map_each, 2, false);

    state->map_obj = object;
}
