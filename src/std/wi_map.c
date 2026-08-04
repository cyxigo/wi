#include <stdbool.h>

#include "../core/wi_state.h"
#include "../include/wi.h"

static struct wi_map*
_check_arg_map(struct wi_state* state, int slot) {
    if (!wi_value_is_map(state->ffi_stack[slot])) {
        wi_state_error(state, "bad argument %i - expected a value of type map but got %s", slot,
                       wi_value_type(state->ffi_stack[slot]));
    }

    return wi_value_as_map(state->ffi_stack[slot]);
}

static struct wi_map*
_check_arg1_map(struct wi_state* state) {
    return _check_arg_map(state, 1);
}

static struct wi_map*
_check_arg2_map(struct wi_state* state) {
    return _check_arg_map(state, 2);
}

static void
_map_copy(struct wi_state* state, int arg_count) {
    struct wi_map* src  = _check_arg1_map(state);
    struct wi_map* dest = wi_new_map(state->gc);
    state->ffi_stack[0] = WI_MAKE_BOX_VALUE(dest);
    wi_table_copy(&src->items, &dest->items);
}

static void
_map_clear(struct wi_state* state, int arg_count) {
    struct wi_map* map = _check_arg1_map(state);
    wi_table_free(&map->items);
    wi_slot_set_null(state, 0);
}

static void
_map_capacity(struct wi_state* state, int arg_count) {
    struct wi_map* map = _check_arg1_map(state);
    wi_slot_set_real(state, 0, map->items.capacity);
}

static void
_map_count(struct wi_state* state, int arg_count) {
    struct wi_map* map = _check_arg1_map(state);
    wi_slot_set_real(state, 0, map->items.live_count);
}

static void
_map_equals(struct wi_state* state, int arg_count) {
    struct wi_map* a      = _check_arg1_map(state);
    struct wi_map* b      = _check_arg2_map(state);
    bool           equals = false;

    int a_count = a->items.live_count;
    int b_count = b->items.live_count;

    if (a_count != b_count) {
        goto end;
    }

    for (int i = 0; i < a->items.capacity; i++) {
        struct wi_entry* entry = &a->items.entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        wi_value b_value;

        if (!wi_table_get(&b->items, entry->key, &b_value) || !wi_values_equal(entry->value, b_value)) {
            goto end;
        }
    }

    equals = true;
end:
    wi_slot_set_bool(state, 0, equals);
}

static void
_map_keys(struct wi_state* state, int arg_count) {
    struct wi_map*   map    = _check_arg1_map(state);
    struct wi_array* result = wi_new_array(state->gc);
    state->ffi_stack[0]     = WI_MAKE_BOX_VALUE(result);

    wi_value_buf_reserve(&result->items, map->items.count);

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

        if (!wi_value_is_empty(entry->key)) {
            wi_value_buf_add(&result->items, entry->key);
        }
    }
}

static void
_map_values(struct wi_state* state, int arg_count) {
    struct wi_map*   map    = _check_arg1_map(state);
    struct wi_array* result = wi_new_array(state->gc);
    state->ffi_stack[0]     = WI_MAKE_BOX_VALUE(result);

    wi_value_buf_reserve(&result->items, map->items.count);

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

        if (!wi_value_is_empty(entry->key)) {
            wi_value_buf_add(&result->items, entry->value);
        }
    }
}

static void
_map_has(struct wi_state* state, int arg_count) {
    struct wi_map* map    = _check_arg1_map(state);
    bool           exists = wi_table_get(&map->items, state->ffi_stack[2], NULL);
    wi_slot_set_bool(state, 0, exists);
}

static void
_map_get_or_default(struct wi_state* state, int arg_count) {
    struct wi_map* map = _check_arg1_map(state);
    wi_value       value;

    if (wi_table_get(&map->items, state->ffi_stack[2], &value)) {
        state->ffi_stack[0] = value;
        return;
    }

    state->ffi_stack[0] = state->ffi_stack[3];
}

static void
_map_remove(struct wi_state* state, int arg_count) {
    struct wi_map* map = _check_arg1_map(state);
    wi_slot_set_bool(state, 0, wi_table_delete(&map->items, state->ffi_stack[2]));
}

static void
_map_each(struct wi_state* state, int arg_count) {
    struct wi_map*     map     = _check_arg1_map(state);
    struct wi_closure* closure = wi_slot_check_function(state, 2, 2);

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

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
wi_state_def_map_foreign(struct wi_state* state) {
    struct wi_object* object = wi_def_object(state, "map");

    wi_object_set_field_foreign(state, object, "copy", _map_copy, 1, false);
    wi_object_set_field_foreign(state, object, "clear", _map_clear, 1, false);
    wi_object_set_field_foreign(state, object, "capacity", _map_capacity, 1, false);
    wi_object_set_field_foreign(state, object, "count", _map_count, 1, false);
    wi_object_set_field_foreign(state, object, "equals", _map_equals, 2, false);
    wi_object_set_field_foreign(state, object, "keys", _map_keys, 1, false);
    wi_object_set_field_foreign(state, object, "values", _map_values, 1, false);
    wi_object_set_field_foreign(state, object, "has", _map_has, 2, false);
    wi_object_set_field_foreign(state, object, "get_or_default", _map_get_or_default, 3, false);
    wi_object_set_field_foreign(state, object, "remove", _map_remove, 2, false);
    wi_object_set_field_foreign(state, object, "each", _map_each, 2, false);

    state->map_obj = object;
}
