#ifndef WI_TABLE_H
#define WI_TABLE_H

#include <stdint.h>

#include "wi_value.h"

typedef struct wi_string wi_string_t;

uint32_t
wi_string_hash(const char* chars, int len);

typedef struct {
    wi_value_t key;
    wi_value_t value;
} wi_entry_t;

extern const double WI_TABLE_MAX_LOAD;

typedef struct {
    wi_gc_t*    gc;
    wi_entry_t* entries;
    int         capacity;
    int         count;
} wi_table_t;

void
wi_table_init(wi_table_t* table, wi_gc_t* gc);
void
wi_table_free(wi_table_t* table);

int
wi_table_count(wi_table_t* table);
bool
wi_table_set(wi_table_t* table, wi_value_t key, wi_value_t value);
bool
wi_table_get(wi_table_t* table, wi_value_t key, wi_value_t* value);
bool
wi_table_delete(wi_table_t* table, wi_value_t key);
wi_string_t*
wi_table_find_string(wi_table_t* table, const char* chars, int len, uint32_t hash);
void
wi_table_remove_white(wi_table_t* table);
void
wi_table_reserve(wi_table_t* table, int count);
void
wi_table_copy(wi_table_t* src, wi_table_t* dest);

#endif
