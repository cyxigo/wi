#ifndef WI_TABLE_H
#define WI_TABLE_H

#include <stdint.h>

#include "wi_value.h"

struct wi_string;

/*
    tables grow from a smaller floor than our good old buffers because most of the tables
    are used for object's fields, and objects rarely carry more than 8 fields
    so we start at 4 instead, might need to tune this a bit though
*/
enum {
    WI_TABLE_MIN_CAPACITY = 4,
};

#define WI_TABLE_GROW_CAPACITY(capacity) \
    ((capacity) < WI_TABLE_MIN_CAPACITY ? WI_TABLE_MIN_CAPACITY : (capacity) * WI_BUF_CAPACITY_FACTOR)

struct wi_entry {
    wi_value key;
    wi_value value;
};

extern const double WI_TABLE_MAX_LOAD;

struct wi_table {
    struct wi_gc*    gc;
    struct wi_entry* entries;
    int              capacity;
    int              count;
    int              live_count;
};

void
wi_table_init(struct wi_table* table, struct wi_gc* gc);
void
wi_table_free(struct wi_table* table);

bool
wi_table_set(struct wi_table* table, wi_value key, wi_value value);
void
wi_table_set_foreign(struct wi_table* table, const char* name, wi_foreign_fn fn, int arity, bool is_variadic);
bool
wi_table_get(struct wi_table* table, wi_value key, wi_value* value);
bool
wi_table_delete(struct wi_table* table, wi_value key);
struct wi_string*
wi_table_find_string(struct wi_table* table, const char* buf, int len, uint32_t hash);
void
wi_table_remove_white(struct wi_table* table);
void
wi_table_reserve(struct wi_table* table, int count);
void
wi_table_copy(struct wi_table* src, struct wi_table* dest);

#endif
