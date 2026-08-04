#ifndef WI_GC_H
#define WI_GC_H

#include <stdbool.h>
#include <stddef.h>

#include "../include/wi_conf.h"
#include "wi_compiler.h"
#include "wi_table.h"

struct wi_gc {
    wi_conf conf;

    struct wi_state*    state;
    struct wi_compiler* compiler;

    struct wi_box* boxes;
    size_t         bytes_allocated;
    size_t         next_collection;

    struct wi_box** gray_stack;
    int             gray_capacity;
    int             gray_count;

    struct wi_box** temp_roots;
    int             temp_root_capacity;
    int             temp_root_count;

    struct wi_table strings;
};

struct wi_gc*
wi_new_gc(wi_conf conf);
void
wi_delete_gc(struct wi_gc* gc);

static inline void
wi_gc_reset_roots(struct wi_gc* gc) {
    gc->temp_root_count = 0;
}

void
wi_gc_push_root(struct wi_gc* gc, struct wi_box* root);

#define WI_GC_PUSH_ROOT(gc, root) wi_gc_push_root(gc, (struct wi_box*)root)

static inline void
wi_gc_pop_root(struct wi_gc* gc) {
    gc->temp_root_count--;
}

static inline bool
wi_log_gc(struct wi_gc* gc) {
    return wi_conf_is_set(gc->conf, WI_CONF_LOG_GC);
}

void*
wi_gc_realloc(struct wi_gc* gc, void* ptr, size_t old_size, size_t new_size);
void
wi_gc_collect_garbage(struct wi_gc* gc);

#define WI_GC_ALLOC(gc, type, count) wi_gc_realloc(gc, NULL, 0, sizeof(type) * (size_t)(count))
#define WI_GC_ALLOC_ARRAY(gc, type, ptr, old_count, new_count) \
    wi_gc_realloc(gc, ptr, sizeof(type) * (size_t)old_count, sizeof(type) * (size_t)new_count)
#define WI_GC_FREE_ARRAY(gc, type, ptr, count) wi_gc_realloc(gc, ptr, sizeof(type) * (size_t)(count), 0)
#define WI_GC_FREE(gc, type, ptr) wi_gc_realloc(gc, ptr, sizeof(type), 0)

#endif
