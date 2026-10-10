#include "wi_map.h"

#include <stdbool.h>

#include "../../include/wi.h"
#include "../core/wi_state.h"

static void
_map_copy(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* src  = wi_arg_map(state, 1);
    struct wi_map* dest = wi_push_map(state);
    wi_table_copy(&src->items, &dest->items);
}

static void
_map_clear(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map       = wi_arg_map(state, 1);
    int            mod_count = map->items.mod_count;
    wi_table_free(&map->items);
    map->items.mod_count = mod_count + 1;
    wi_push_arg(state, 1);
}

static void
_map_capacity(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map = wi_arg_map(state, 1);
    wi_push_real(state, map->items.capacity);
}

static void
_map_count(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map = wi_arg_map(state, 1);
    wi_push_real(state, map->items.live_count);
}

static void
_map_collect(struct wi_state* state, bool keys) {
    struct wi_map*   map    = wi_arg_map(state, 1);
    struct wi_array* result = wi_push_array(state);
    wi_value_buf_reserve(&result->items, map->items.live_count);

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

        if (!wi_value_is_empty(entry->key)) {
            wi_gc_array_add(state->gc, result, keys ? entry->key : entry->value);
        }
    }
}

static void
_map_keys(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _map_collect(state, true);
}

static void
_map_values(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _map_collect(state, false);
}

static void
_map_has(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map    = wi_arg_map(state, 1);
    bool           exists = wi_table_get(&map->items, state->ffi_stack[2], NULL);
    wi_push_bool(state, exists);
}

static void
_map_getordefault(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map = wi_arg_map(state, 1);
    wi_value       value;

    if (wi_table_get(&map->items, state->ffi_stack[2], &value)) {
        wi_state_ppush(state, value);
        return;
    }

    wi_push_arg(state, 3);
}

static void
_map_remove(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map = wi_arg_map(state, 1);
    wi_push_bool(state, wi_table_delete(&map->items, state->ffi_stack[2]));
}

static void
_map_call(struct wi_state* state, struct wi_map* map, int mod_count, wi_value key, wi_value value, bool drop) {
    wi_arg_function(state, 2, 2);
    wi_state_ppush(state, key);
    wi_state_ppush(state, value);
    wi_call(state, 2, drop);
    wi_state_check_mod_count(state, "map", map->items.mod_count, mod_count);
}

static void
_map_each(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map       = wi_arg_map(state, 1);
    int            mod_count = map->items.mod_count;
    wi_arg_check_function(state, 2, 2);

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        _map_call(state, map, mod_count, entry->key, entry->value, true);
    }

    wi_push_arg(state, 1);
}

static void
_map_select(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map       = wi_arg_map(state, 1);
    int            mod_count = map->items.mod_count;
    wi_arg_check_function(state, 2, 1);
    wi_arg_check_function(state, 3, 1);
    struct wi_map* result = wi_push_map(state);
    wi_table_reserve(&result->items, map->items.live_count);

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

        wi_value key   = entry->key;
        wi_value value = entry->value;

        if (wi_value_is_empty(key)) {
            continue;
        }

        bool value_is_box = wi_value_is_box(value);

        if (value_is_box) {
            WI_GC_PUSH_ROOT(state->gc, wi_value_as_box(value));
        }

        wi_arg_function(state, 2, 1);
        wi_state_ppush(state, key);
        wi_call(state, 1, false);

        wi_arg_function(state, 3, 1);
        wi_state_ppush(state, value);
        wi_call(state, 1, false);

        if (value_is_box) {
            wi_gc_pop_root(state->gc);
        }

        wi_state_check_mod_count(state, "map", map->items.mod_count, mod_count);
        wi_value new_value = wi_state_pop(state);
        wi_value new_key   = wi_state_pop(state);
        wi_state_check_map_key(state, new_key);
        WI_GC_TABLE_SET(state->gc, result, &result->items, new_key, new_value);
    }
}

static void
_map_where(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map       = wi_arg_map(state, 1);
    int            mod_count = map->items.mod_count;
    wi_arg_check_function(state, 2, 2);
    struct wi_map* result = wi_push_map(state);
    wi_table_reserve(&result->items, map->items.live_count);

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

        wi_value key   = entry->key;
        wi_value value = entry->value;

        if (wi_value_is_empty(key)) {
            continue;
        }

        _map_call(state, map, mod_count, key, value, false);

        if (wi_value_is_falsy(wi_state_pop(state))) {
            continue;
        }

        WI_GC_TABLE_SET(state->gc, result, &result->items, key, value);
    }
}

static void
_map_find(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_map* map       = wi_arg_map(state, 1);
    int            mod_count = map->items.mod_count;
    wi_arg_check_function(state, 2, 2);

    for (int i = 0; i < map->items.capacity; i++) {
        struct wi_entry* entry = &map->items.entries[i];

        wi_value key   = entry->key;
        wi_value value = entry->value;

        if (wi_value_is_empty(key)) {
            continue;
        }

        _map_call(state, map, mod_count, key, value, false);

        if (!wi_value_is_falsy(wi_state_pop(state))) {
            wi_state_ppush(state, key);
            return;
        }
    }

    wi_push_null(state);
}

void
wi_state_def_stm_map(struct wi_state* state) {
    wi_foreign_entry functions[] = {
        {"copy",         _map_copy,         1, false},
        {"clear",        _map_clear,        1, false},
        {"capacity",     _map_capacity,     1, false},
        {"count",        _map_count,        1, false},
        {"keys",         _map_keys,         1, false},
        {"values",       _map_values,       1, false},
        {"has",          _map_has,          2, false},
        {"getordefault", _map_getordefault, 3, false},
        {"remove",       _map_remove,       2, false},
        {"each",         _map_each,         2, false},
        {"select",       _map_select,       3, false},
        {"where",        _map_where,        2, false},
        {"find",         _map_find,         2, false},
    };
    WI_TABLE_SET_FOREIGN_ALL(&state->stm_map, functions);
}
