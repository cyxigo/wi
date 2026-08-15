#include "wi_table.h"

#include <stdint.h>
#include <string.h>

#include "wi_box.h"
#include "wi_buf.h"
#include "wi_gc.h"
#include "wi_util.h"
#include "wi_value.h"

uint32_t
wi_string_hash(const char* buf, int len) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)buf[i];
        hash *= 16777619u;
    }

    return hash;
}

const double WI_TABLE_MAX_LOAD = 0.75;

static void
_init_entries(struct wi_entry* entries, int capacity) {
    wi_value empty = wi_make_empty_value();
    wi_value null  = wi_make_null_value();

    for (int i = 0; i < capacity; i++) {
        entries[i].key   = empty;
        entries[i].value = null;
    }
}

void
wi_table_init(struct wi_table* table, struct wi_gc* gc) {
    table->gc         = gc;
    table->entries    = NULL;
    table->capacity   = 0;
    table->count      = 0;
    table->live_count = 0;
}

void
wi_table_free(struct wi_table* table) {
    WI_GC_FREE_BUF(table->gc, struct wi_entry, table->entries, table->capacity);
    wi_table_init(table, table->gc);
}

WI_INLINE uint32_t
_hash_key(wi_value key) {
    if (WI_LIKELY(wi_value_is_string(key))) {
        return wi_value_as_string(key)->hash;
    }

    return wi_value_hash(key);
}

WI_INLINE struct wi_entry*
_find_entry(struct wi_entry* entries, int capacity, wi_value key) {
    uint32_t         index     = _hash_key(key) & (uint32_t)(capacity - 1);
    struct wi_entry* tombstone = NULL;

    for (;;) {
        struct wi_entry* entry = &entries[index];

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
_table_adjust_capacity(struct wi_table* table, int capacity) {
    struct wi_entry* entries = WI_GC_ALLOC(table->gc, struct wi_entry, capacity);
    _init_entries(entries, capacity);

    table->count = 0;

    for (int i = 0; i < table->capacity; i++) {
        struct wi_entry* entry = &table->entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        struct wi_entry* new_entry = _find_entry(entries, capacity, entry->key);
        new_entry->key             = entry->key;
        new_entry->value           = entry->value;
        table->count++;
    }

    WI_GC_FREE_BUF(table->gc, struct wi_entry, table->entries, table->capacity);
    table->capacity = capacity;
    table->entries  = entries;
}

bool
wi_table_set(struct wi_table* table, wi_value key, wi_value value) {
    if (WI_UNLIKELY(table->count + 1 > table->capacity * WI_TABLE_MAX_LOAD)) {
        int capacity = WI_GROW_CAPACITY(table->capacity);
        _table_adjust_capacity(table, capacity);
    }

    struct wi_entry* entry      = _find_entry(table->entries, table->capacity, key);
    bool             is_new_key = wi_value_is_empty(entry->key);

    if (is_new_key) {
        if (wi_value_is_null(entry->value)) {
            table->count++;
        }

        table->live_count++;
    }

    entry->key   = key;
    entry->value = value;

    return is_new_key;
}

bool
wi_table_get(struct wi_table* table, wi_value key, wi_value* value) {
    if (table->count == 0) {
        return false;
    }

    struct wi_entry* entry = _find_entry(table->entries, table->capacity, key);

    if (wi_value_is_empty(entry->key)) {
        return false;
    }

    if (value) {
        *value = entry->value;
    }

    return true;
}

bool
wi_table_delete(struct wi_table* table, wi_value key) {
    if (table->count == 0) {
        return false;
    }

    struct wi_entry* entry = _find_entry(table->entries, table->capacity, key);

    if (wi_value_is_empty(entry->key)) {
        return false;
    }

    entry->key   = wi_make_empty_value();
    entry->value = wi_make_true_value();
    table->live_count--;

    return true;
}

struct wi_string*
wi_table_find_string(struct wi_table* table, const char* buf, int len, uint32_t hash) {
    if (table->count == 0) {
        return NULL;
    }

    uint32_t index = hash & (uint32_t)(table->capacity - 1);

    for (;;) {
        struct wi_entry* entry = &table->entries[index];

        if (wi_value_is_empty(entry->key) && wi_value_is_null(entry->value)) {
            return NULL;
        }

        if (wi_value_is_string(entry->key)) {
            struct wi_string* key = wi_value_as_string(entry->key);

            if (key->count == len && key->hash == hash && memcmp(key->buf, buf, (size_t)len) == 0) {
                return key;
            }
        }

        index = (index + 1) & (uint32_t)(table->capacity - 1);
    }
}

void
wi_table_remove_white(struct wi_table* table) {
    wi_value empty = wi_make_empty_value();
    wi_value true_ = wi_make_true_value();

    for (int i = 0; i < table->capacity; i++) {
        struct wi_entry* entry = &table->entries[i];

        if (wi_value_is_box(entry->key) && !wi_value_as_box(entry->key)->is_marked) {
            entry->key   = empty;
            entry->value = true_;
        }
    }
}

void
wi_table_reserve(struct wi_table* table, int count) {
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
wi_table_copy(struct wi_table* src, struct wi_table* dest) {
    if (src == dest) {
        return;
    }

    if (dest->entries) {
        WI_GC_FREE_BUF(dest->gc, struct wi_entry, dest->entries, dest->capacity);
    }

    if (src->capacity == 0) {
        wi_table_init(dest, dest->gc);
        return;
    }

    struct wi_entry* entries = WI_GC_ALLOC(dest->gc, struct wi_entry, src->capacity);
    memcpy(entries, src->entries, sizeof(struct wi_entry) * (size_t)src->capacity);

    dest->entries    = entries;
    dest->capacity   = src->capacity;
    dest->count      = src->count;
    dest->live_count = src->live_count;
}
