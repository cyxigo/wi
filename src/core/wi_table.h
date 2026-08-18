#ifndef WI_TABLE_H
#define WI_TABLE_H

#include <stdint.h>

#include "wi_value.h"

struct wi_string;

uint32_t
wi_string_hash(const char* buf, int len);

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
