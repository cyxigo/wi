#include "wi_table.h"

#include <stdint.h>
#include <string.h>

#include "wi_box.h"
#include "wi_buf.h"
#include "wi_gc.h"
#include "wi_value.h"

uint32_t
wi_string_hash(const char* chars, int len) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)chars[i];
        hash *= 16777619u;
    }

    return hash;
}

const double WI_TABLE_MAX_LOAD = 0.75;

static void
_init_entries(wi_entry_t* entries, int capacity) {
    wi_value_t empty = wi_make_empty_value();
    wi_value_t null  = wi_make_null_value();

    for (int i = 0; i < capacity; i++) {
        entries[i].key   = empty;
        entries[i].value = null;
    }
}

void
wi_table_init(wi_table_t* table, wi_gc_t* gc) {
    table->gc       = gc;
    table->entries  = NULL;
    table->capacity = 0;
    table->count    = 0;
}

void
wi_table_free(wi_table_t* table) {
    WI_GC_FREE_ARRAY(table->gc, wi_entry_t, table->entries, table->capacity);
    wi_table_init(table, table->gc);
}

static wi_entry_t*
_find_entry(wi_entry_t* entries, int capacity, wi_value_t key) {
    uint32_t    index     = wi_value_hash(key) & (uint32_t)(capacity - 1);
    wi_entry_t* tombstone = NULL;

    for (;;) {
        wi_entry_t* entry = &entries[index];

        if (wi_value_is_empty(entry->key)) {
            if (wi_value_is_null(entry->value)) {
                return tombstone ? tombstone : entry;
            } else if (!tombstone) {
                tombstone = entry;
            }
        } else if (wi_values_equal(key, entry->key)) {
            return entry;
        }

        index = (index + 1) & (uint32_t)(capacity - 1);
    }
}

static void
_table_adjust_capacity(wi_table_t* table, int capacity) {
    wi_entry_t* entries = WI_GC_ALLOC(table->gc, wi_entry_t, capacity);
    _init_entries(entries, capacity);

    table->count = 0;

    for (int i = 0; i < table->capacity; i++) {
        wi_entry_t* entry = &table->entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        wi_entry_t* new_entry = _find_entry(entries, capacity, entry->key);
        new_entry->key        = entry->key;
        new_entry->value      = entry->value;
        table->count++;
    }

    WI_GC_FREE_ARRAY(table->gc, wi_entry_t, table->entries, table->capacity);
    table->capacity = capacity;
    table->entries  = entries;
}

int
wi_table_count(wi_table_t* table) {
    int count = 0;

    for (int i = 0; i < table->capacity; i++) {
        wi_entry_t* entry = &table->entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        count++;
    }

    return count;
}

bool
wi_table_set(wi_table_t* table, wi_value_t key, wi_value_t value) {
    if (table->count + 1 > table->capacity * WI_TABLE_MAX_LOAD) {
        int capacity = WI_GROW_CAPACITY(table->capacity);
        _table_adjust_capacity(table, capacity);
    }

    wi_entry_t* entry      = _find_entry(table->entries, table->capacity, key);
    bool        is_new_key = wi_value_is_empty(entry->key);

    if (is_new_key && wi_value_is_null(entry->value)) {
        table->count++;
    }

    entry->key   = key;
    entry->value = value;

    return is_new_key;
}

bool
wi_table_get(wi_table_t* table, wi_value_t key, wi_value_t* value) {
    if (table->count == 0) {
        return false;
    }

    wi_entry_t* entry = _find_entry(table->entries, table->capacity, key);

    if (wi_value_is_empty(entry->key)) {
        return false;
    }

    if (value) {
        *value = entry->value;
    }

    return true;
}

bool
wi_table_delete(wi_table_t* table, wi_value_t key) {
    if (table->count == 0) {
        return false;
    }

    wi_entry_t* entry = _find_entry(table->entries, table->capacity, key);

    if (wi_value_is_empty(entry->key)) {
        return false;
    }

    entry->key   = wi_make_empty_value();
    entry->value = wi_make_true_value();

    return true;
}

wi_string_t*
wi_table_find_string(wi_table_t* table, const char* chars, int len, uint32_t hash) {
    if (table->count == 0) {
        return NULL;
    }

    uint32_t index = hash & (uint32_t)(table->capacity - 1);

    for (;;) {
        wi_entry_t* entry = &table->entries[index];

        if (wi_value_is_empty(entry->key) && wi_value_is_null(entry->value)) {
            return NULL;
        }

        if (wi_value_is_string(entry->key)) {
            wi_string_t* key = wi_value_as_string(entry->key);

            if (key->len == len && key->hash == hash && memcmp(key->chars, chars, (size_t)len) == 0) {
                return key;
            }
        }

        index = (index + 1) & (uint32_t)(table->capacity - 1);
    }
}

void
wi_table_remove_white(wi_table_t* table) {
    wi_value_t empty = wi_make_empty_value();
    wi_value_t true_ = wi_make_true_value();

    for (int i = 0; i < table->capacity; i++) {
        wi_entry_t* entry = &table->entries[i];

        if (wi_value_is_box(entry->key) && !wi_value_as_box(entry->key)->is_marked) {
            entry->key   = empty;
            entry->value = true_;
        }
    }
}

void
wi_table_reserve(wi_table_t* table, int count) {
    if (count <= 0) {
        return;
    }

    int needed   = table->count + count;
    int capacity = table->capacity;

    while (needed > capacity * WI_TABLE_MAX_LOAD) {
        capacity = WI_GROW_CAPACITY(capacity);
    }

    if (capacity > table->capacity) {
        _table_adjust_capacity(table, capacity);
    }
}

void
wi_table_copy(wi_table_t* src, wi_table_t* dest) {
    if (src == dest) {
        return;
    }

    if (dest->entries) {
        WI_GC_FREE_ARRAY(dest->gc, wi_entry_t, dest->entries, dest->capacity);
    }

    if (src->capacity == 0) {
        wi_table_init(dest, dest->gc);
        return;
    }

    wi_entry_t* entries = WI_GC_ALLOC(dest->gc, wi_entry_t, src->capacity);
    memcpy(entries, src->entries, sizeof(wi_entry_t) * (size_t)src->capacity);

    dest->entries  = entries;
    dest->capacity = src->capacity;
    dest->count    = src->count;
}
