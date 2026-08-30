#include <stdbool.h>

#include "../../include/wi.h"
#include "../core/wi_state.h"

static struct wi_map*
_check_arg_map(struct wi_state* state, int arg) {
    if (!wi_value_is_map(state->ffi_stack[arg])) {
        wi_state_error(state, "bad argument %i - expected a value of type map but got %s", arg,
                       wi_value_type(state->ffi_stack[arg]));
    }

    return wi_value_as_map(state->ffi_stack[arg]);
}

static struct wi_map*
_check_arg1_map(struct wi_state* state) {
    return _check_arg_map(state, 1);
}

static void
_map_copy(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* src  = _check_arg1_map(state);
    struct wi_map* dest = wi_new_map(state->gc);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(dest));
    wi_table_copy(&src->items, &dest->items);
}

static void
_map_clear(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map = _check_arg1_map(state);
    wi_table_free(&map->items);
    wi_push_null(state);
}

static void
_map_capacity(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map = _check_arg1_map(state);
    wi_push_real(state, map->items.capacity);
}

static void
_map_count(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map = _check_arg1_map(state);
    wi_push_real(state, map->items.live_count);
}

static void
_map_keys(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map*   map    = _check_arg1_map(state);
    struct wi_array* result = wi_new_array(state->gc);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(result));
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
    WI_UNUSED(arg_count);
    struct wi_map*   map    = _check_arg1_map(state);
    struct wi_array* result = wi_new_array(state->gc);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(result));

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
    WI_UNUSED(arg_count);
    struct wi_map* map    = _check_arg1_map(state);
    bool           exists = wi_table_get(&map->items, state->ffi_stack[2], NULL);
    wi_push_bool(state, exists);
}

static void
_map_get_or_default(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map = _check_arg1_map(state);
    wi_value       value;

    if (wi_table_get(&map->items, state->ffi_stack[2], &value)) {
        wi_state_ppush(state, value);
        return;
    }

    wi_state_ppush(state, state->ffi_stack[3]);
}

static void
_map_remove(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map = _check_arg1_map(state);
    wi_push_bool(state, wi_table_delete(&map->items, state->ffi_stack[2]));
}

static void
_map_each(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map = _check_arg1_map(state);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(map));

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        wi_arg_function(state, 2, 2);
        wi_state_ppush(state, entry->key);
        wi_state_ppush(state, entry->value);
        wi_call(state, 2, true);
    }
}

static void
_map_select(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map    = _check_arg1_map(state);
    struct wi_map* result = wi_new_map(state->gc);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(result));
    wi_table_reserve(&result->items, map->items.count);

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        wi_arg_function(state, 2, 1);
        wi_state_ppush(state, entry->key);
        wi_call(state, 1, false);

        wi_arg_function(state, 3, 1);
        wi_state_ppush(state, entry->value);
        wi_call(state, 1, false);

        wi_value value = wi_state_pop(state);
        wi_value key   = wi_state_pop(state);

        wi_table_set(&result->items, key, value);
    }
}

static void
_map_where(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map    = _check_arg1_map(state);
    struct wi_map* result = wi_new_map(state->gc);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(result));

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        wi_arg_function(state, 2, 2);
        wi_state_ppush(state, entry->key);
        wi_state_ppush(state, entry->value);
        wi_call(state, 2, false);

        if (!wi_value_is_falsy(wi_state_pop(state))) {
            wi_table_set(&result->items, entry->key, entry->value);
        }
    }
}

void
wi_state_def_stm_map(struct wi_state* state) {
    struct wi_table* table = &state->stm_map;
    wi_table_set_foreign(table, "copy", _map_copy, 1, false);
    wi_table_set_foreign(table, "clear", _map_clear, 1, false);
    wi_table_set_foreign(table, "capacity", _map_capacity, 1, false);
    wi_table_set_foreign(table, "count", _map_count, 1, false);
    wi_table_set_foreign(table, "keys", _map_keys, 1, false);
    wi_table_set_foreign(table, "values", _map_values, 1, false);
    wi_table_set_foreign(table, "has", _map_has, 2, false);
    wi_table_set_foreign(table, "get_or_default", _map_get_or_default, 3, false);
    wi_table_set_foreign(table, "remove", _map_remove, 2, false);
    wi_table_set_foreign(table, "each", _map_each, 2, false);
    wi_table_set_foreign(table, "select", _map_select, 3, false);
    wi_table_set_foreign(table, "where", _map_where, 2, false);
}
