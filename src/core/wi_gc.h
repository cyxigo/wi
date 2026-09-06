#ifndef WI_GC_H
#define WI_GC_H

#include <stdbool.h>
#include <stddef.h>

#include "../../include/wi_conf.h"
#include "wi_compiler.h"
#include "wi_table.h"

struct wi_gc {
    wi_conf* conf;

    size_t min_heap;
    size_t heap_grow_factor;
    size_t young_max;

    struct wi_state*    state;
    struct wi_compiler* compiler;

    /*
        generational garbage collector!
        two generations, boxes start out young
        the first time one survives a collection (minor or major), it's promoted to old
        a minor collection only ever walks young
        a major collection walks both!
    */
    struct wi_box* young;
    struct wi_box* old;
    size_t         young_bytes;     /* how many allocated bytes are young boxes */
    size_t         bytes_allocated; /* how many bytes allocated... at all */
    size_t         next_major;      /* when to do major collection, checking both young and old */
    bool           minor;           /* is the collection currently in progress a minor one? */

    struct wi_box** gray_stack;
    int             gray_capacity;
    int             gray_count;

    /*
        what if the only thing keeping young box alive is a reference to it from old box?
        we absolutely can't afford to scan old boxes in minor collections
        so we have this: list of "remembered parents", old boxes that have references to young boxes
    */
    struct wi_box** remembered;
    int             remembered_capacity;
    int             remembered_count;

    struct wi_box** temp_roots;
    int             temp_root_capacity;
    int             temp_root_count;

    struct wi_table strings;
};

struct wi_gc*
wi_new_gc(wi_conf* conf);
void
wi_delete_gc(struct wi_gc* gc);

WI_INLINE void
wi_gc_reset_roots(struct wi_gc* gc) {
    gc->temp_root_count = 0;
}

WI_NORETURN void
wi_state_oom(struct wi_state* state, const char* what);

WI_INLINE void
wi_gc_push_root(struct wi_gc* gc, struct wi_box* root) {
    if (WI_UNLIKELY(gc->temp_root_count + 1 > gc->temp_root_capacity)) {
        gc->temp_root_capacity = WI_GROW_CAPACITY(gc->temp_root_capacity);
        gc->temp_roots         = realloc(gc->temp_roots, sizeof(struct wi_box*) * (size_t)gc->temp_root_capacity);

        if (!gc->temp_roots) {
            wi_state_oom(gc->state, "failed to allocate temp roots (wi_gc_push_root)");
        }
    }

    gc->temp_roots[gc->temp_root_count++] = root;
}

#define WI_GC_PUSH_ROOT(gc, root) wi_gc_push_root(gc, (struct wi_box*)root)

WI_INLINE void
wi_gc_pop_root(struct wi_gc* gc) {
    gc->temp_root_count--;
}

WI_INLINE bool
wi_log_gc(struct wi_gc* gc) {
    return wi_conf_is_set(gc->conf, WI_CONF_LOG_GC);
}

void*
wi_gc_realloc(struct wi_gc* gc, void* ptr, size_t old_size, size_t new_size);
void
wi_gc_remember(struct wi_gc* gc, struct wi_box* parent);

/*
    needs to be called when an existing box's field is set to a value that MIGHT be another box
    if the parent is old and the value is a young box, then the parent gets remembered
    the next minor collection checks those remembered parents
    any function that mutates an existing box's contents must call this
*/
WI_INLINE void
wi_gc_write_barrier(struct wi_gc* gc, struct wi_box* parent, wi_value value) {
    if (WI_LIKELY(!parent->is_old || parent->is_remembered || !wi_value_is_box(value) ||
                  wi_value_as_box(value)->is_old)) {
        return;
    }

    wi_gc_remember(gc, parent);
}

#define WI_GC_WRITE_BARRIER(gc, parent, value) wi_gc_write_barrier(gc, (struct wi_box*)(parent), value)

void
wi_gc_collect_minor(struct wi_gc* gc);
void
wi_gc_collect_major(struct wi_gc* gc);

#define WI_GC_ALLOC(gc, type, count) wi_gc_realloc(gc, NULL, 0, sizeof(type) * (size_t)(count))
#define WI_GC_FREE_BUF(gc, type, ptr, count) wi_gc_realloc(gc, ptr, sizeof(type) * (size_t)(count), 0)
#define WI_GC_FREE(gc, type, ptr) wi_gc_realloc(gc, ptr, sizeof(type), 0)

#endif
