#ifndef WI_GC_H
#define WI_GC_H

#include <stdbool.h>
#include <stddef.h>

#include "../include/wi_conf.h"
#include "wi_box.h"
#include "wi_compiler.h"
#include "wi_table.h"

typedef struct wi_gc {
    wi_conf_t conf;

    wi_state_t*    state;
    wi_compiler_t* compiler;

    wi_box_t* boxes;
    size_t    bytes_allocated;
    size_t    next_collection;

    wi_box_t** gray_stack;
    int        gray_capacity;
    int        gray_count;

    wi_box_t* temp_roots[WI_GC_TEMP_ROOTS_MAX];
    int       temp_root_count;

    wi_table_t strings;
} wi_gc_t;

static inline void
wi_gc_reset_roots(wi_gc_t* gc) {
    gc->temp_root_count = 0;
}

static inline void
wi_gc_push_root(wi_gc_t* gc, wi_box_t* root) {
    if (gc->temp_root_count >= WI_GC_TEMP_ROOTS_MAX) {
        fprintf(stderr, "memory error: temp roots overflow (limit is %i)\n", WI_GC_TEMP_ROOTS_MAX);
        exit(EXIT_FAILURE);
    }

    gc->temp_roots[gc->temp_root_count++] = root;
}

static inline void
wi_gc_pop_root(wi_gc_t* gc) {
    gc->temp_root_count--;
}

static inline bool
wi_log_gc(wi_gc_t* gc) {
    return wi_conf_is_set(gc->conf, WI_CONF_LOG_GC);
}

wi_gc_t*
wi_new_gc(wi_conf_t conf);
void
wi_delete_gc(wi_gc_t* gc);

void*
wi_gc_realloc(wi_gc_t* gc, void* ptr, size_t old_size, size_t new_size);
void
wi_gc_collect_garbage(wi_gc_t* gc);

#define WI_GC_ALLOC(gc, type, count) wi_gc_realloc(gc, NULL, 0, sizeof(type) * (size_t)(count))
#define WI_GC_ALLOC_ARRAY(gc, type, ptr, old_count, new_count) \
    wi_gc_realloc(gc, ptr, sizeof(type) * (size_t)old_count, sizeof(type) * (size_t)new_count);
#define WI_GC_FREE_ARRAY(gc, type, ptr, count) wi_gc_realloc(gc, ptr, sizeof(type) * (size_t)(count), 0)
#define WI_GC_FREE(gc, type, ptr) wi_gc_realloc(gc, ptr, sizeof(type), 0)

#endif
