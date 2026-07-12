#include "../core/wi_state.h"
#include "../include/wi.h"

static wi_map_t*
_check_arg1_map(wi_state_t* state) {
    if (!wi_value_is_map(state->api_stack[1])) {
        wi_state_error(state, "bad argument 1 - expected a value of type map but got %s",
                       wi_value_type(state->api_stack[1]));
    }

    return wi_value_as_map(state->api_stack[1]);
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

void
wi_state_def_map_foreign(wi_state_t* state) {
    wi_object_t* object = wi_state_def_object(state, "map");

    wi_state_def_field_foreign(state, "has", _map_has, 2, object);
    wi_state_def_field_foreign(state, "get_or_default", _map_get_or_default, 3, object);
    wi_state_def_field_foreign(state, "keys", _map_keys, 1, object);
    wi_state_def_field_foreign(state, "values", _map_values, 1, object);
}
