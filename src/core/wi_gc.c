#include "wi_gc.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/wi_conf.h"
#include "wi_box.h"
#include "wi_buf.h"
#include "wi_compiler.h"
#include "wi_state.h"
#include "wi_table.h"
#include "wi_value.h"

struct wi_gc*
wi_new_gc(wi_conf conf) {
    struct wi_gc* gc = malloc(sizeof(struct wi_gc));

    if (!gc) {
        return NULL;
    }

    gc->conf = conf;

    gc->state    = NULL;
    gc->compiler = NULL;

    gc->boxes           = NULL;
    gc->bytes_allocated = 0;
    gc->next_collection = WI_GC_MIN_HEAP;

    gc->gray_stack    = NULL;
    gc->gray_capacity = 0;
    gc->gray_count    = 0;

    gc->temp_roots         = NULL;
    gc->temp_root_capacity = 0;
    gc->temp_root_count    = 0;

    wi_table_init(&gc->strings, gc);
    return gc;
}

static void
_gc_free_box(struct wi_gc* gc, struct wi_box* box);

void
wi_delete_gc(struct wi_gc* gc) {
    struct wi_box* box = gc->boxes;

    while (box) {
        struct wi_box* next = box->next;
        _gc_free_box(gc, box);
        box = next;
    }

    free(gc->gray_stack);
    free(gc->temp_roots);
    wi_table_free(&gc->strings);
    free(gc);
}

void
wi_gc_push_root(struct wi_gc* gc, struct wi_box* root) {
    if (WI_UNLIKELY(gc->temp_root_count + 1 > gc->temp_root_capacity)) {
        gc->temp_root_capacity = WI_GROW_CAPACITY(gc->temp_root_capacity);
        gc->temp_roots         = realloc(gc->temp_roots, sizeof(struct wi_box*) * (size_t)gc->temp_root_capacity);

        if (!gc->temp_roots) {
            wi_state_oom(gc->state, "out of memory: failed to allocate garbage collector temp roots");
        }
    }

    gc->temp_roots[gc->temp_root_count++] = root;
}

void*
wi_gc_realloc(struct wi_gc* gc, void* ptr, size_t old_size, size_t new_size) {
    gc->bytes_allocated += new_size - old_size;

    if (new_size > old_size) {
        if (WI_UNLIKELY(wi_conf_is_set(gc->conf, WI_CONF_STRESS_GC)) ||
            gc->bytes_allocated > gc->next_collection) {
            wi_gc_collect_garbage(gc);
        }
    }

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void* result = realloc(ptr, new_size);

    if (WI_UNLIKELY(!result)) {
        wi_state_oom(gc->state, "out of memory: failed to allocate memory in the garbage collector");
    }

    return result;
}

static void
_gc_free_box(struct wi_gc* gc, struct wi_box* box) {
    if (wi_log_gc(gc)) {
        printf("free box at %p of kind %d\n", (void*)box, box->kind);
    }

    switch (box->kind) {
        case WI_BOX_STRING: {
            struct wi_string* string = (struct wi_string*)box;
            WI_GC_FREE_BUF(gc, char, string->buf, string->count + 1);
            WI_GC_FREE(gc, struct wi_string, box);
            break;
        }
        case WI_BOX_ARRAY: {
            struct wi_array* array = (struct wi_array*)box;
            wi_value_buf_free(&array->items);
            WI_GC_FREE(gc, struct wi_array, box);
            break;
        }
        case WI_BOX_MAP: {
            struct wi_map* map = (struct wi_map*)box;
            wi_table_free(&map->items);
            WI_GC_FREE(gc, struct wi_map, box);
            break;
        }
        case WI_BOX_PROTOTYPE: {
            struct wi_prototype* prototype = (struct wi_prototype*)box;

            wi_byte_buf_free(&prototype->bytes);
            wi_int_buf_free(&prototype->lines);
            wi_value_buf_free(&prototype->constants);
            WI_GC_FREE(gc, struct wi_prototype, box);

            break;
        }
        case WI_BOX_FOREIGN:
            WI_GC_FREE(gc, struct wi_foreign, box);
            break;
        case WI_BOX_CLOSURE: {
            struct wi_closure* closure = (struct wi_closure*)box;
            WI_GC_FREE_BUF(gc, struct wi_upvalue*, closure->upvalues, closure->upvalue_count);
            WI_GC_FREE(gc, struct wi_closure, box);
            break;
        }
        case WI_BOX_UPVALUE:
            WI_GC_FREE(gc, struct wi_upvalue, box);
            break;
        case WI_BOX_OBJECT: {
            struct wi_object* object = (struct wi_object*)box;
            wi_table_free(&object->fields);
            WI_GC_FREE(gc, struct wi_object, box);
            break;
        }
        case WI_BOX_USERDATA: {
            struct wi_userdata* userdata = (struct wi_userdata*)box;

            if (userdata->data && userdata->finalizer) {
                userdata->finalizer(userdata->data);
            }

            WI_GC_FREE(gc, struct wi_userdata, box);
            break;
        }
    }
}

static void
_gc_mark_box(struct wi_gc* gc, struct wi_box* box) {
    if (!box) {
        return;
    }

    if (box->is_marked) {
        return;
    }

    if (wi_log_gc(gc)) {
        printf("marked box at %p ", (void*)box);
        wi_value_print(WI_MAKE_BOX_VALUE(box));
        printf("\n");
    }

    box->is_marked = true;

    if (box->kind == WI_BOX_STRING || box->kind == WI_BOX_FOREIGN) {
        return;
    }

    if (gc->gray_count + 1 > gc->gray_capacity) {
        gc->gray_capacity = WI_GROW_CAPACITY(gc->gray_capacity);
        gc->gray_stack    = realloc(gc->gray_stack, sizeof(struct wi_box*) * (size_t)gc->gray_capacity);

        if (WI_UNLIKELY(!gc->gray_stack)) {
            wi_state_oom(gc->state, "out of memory: failed to allocate garbage collector gray stack");
        }
    }

    gc->gray_stack[gc->gray_count++] = box;
}

#define _GC_MARK_BOX(gc, box) _gc_mark_box(gc, (struct wi_box*)box)

static void
_gc_mark_value(struct wi_gc* gc, wi_value value) {
    if (!wi_value_is_box(value)) {
        return;
    }

    _GC_MARK_BOX(gc, wi_value_as_box(value));
}

static void
_gc_mark_value_buf(struct wi_gc* gc, struct wi_value_buf* buf) {
    for (int i = 0; i < buf->count; i++) {
        _gc_mark_value(gc, buf->data[i]);
    }
}

static void
_gc_mark_table(struct wi_gc* gc, struct wi_table* table) {
    for (int i = 0; i < table->capacity; i++) {
        struct wi_entry* entry = &table->entries[i];

        if (wi_value_is_empty(entry->key)) {
            continue;
        }

        _gc_mark_value(gc, entry->key);
        _gc_mark_value(gc, entry->value);
    }
}

static void
_gc_mark_compiler(struct wi_gc* gc) {
    if (!gc->compiler) {
        return;
    }

    struct wi_compiler* compiler = gc->compiler;

    while (compiler) {
        _gc_mark_table(gc, compiler->global_attrs);
        _GC_MARK_BOX(gc, compiler->prototype);
        _GC_MARK_BOX(gc, compiler->constants);
        compiler = compiler->outer;
    }
}

static void
_gc_mark_roots(struct wi_gc* gc) {
    for (int i = 0; i < gc->temp_root_count; i++) {
        _GC_MARK_BOX(gc, gc->temp_roots[i]);
    }

    _gc_mark_compiler(gc);

    struct wi_state*    state    = gc->state;
    struct wi_recovery* recovery = state->recoveries;

    while (recovery) {
        _GC_MARK_BOX(gc, recovery->error);
        recovery = recovery->next;
    }

    for (int i = 0; i < state->frame_count; i++) {
        _GC_MARK_BOX(gc, state->frames[i].closure);
    }

    for (wi_value* slot = state->stack; slot < state->stack_top; slot++) {
        _gc_mark_value(gc, *slot);
    }

    _gc_mark_table(gc, &state->globals);
    _gc_mark_table(gc, &state->required);
    _gc_mark_table(gc, &state->foreign);

    _gc_mark_table(gc, &state->stm_string);
    _gc_mark_table(gc, &state->stm_array);
    _gc_mark_table(gc, &state->stm_map);

    for (struct wi_upvalue* upvalue = state->open_upvalues; upvalue; upvalue = upvalue->next) {
        _GC_MARK_BOX(gc, upvalue);
    }

    _GC_MARK_BOX(gc, state->ok_str);
    _GC_MARK_BOX(gc, state->value_str);
    _GC_MARK_BOX(gc, state->error_str);
}

static void
_gc_blacken_box(struct wi_gc* gc, struct wi_box* box) {
    if (wi_log_gc(gc)) {
        printf("blacken box at %p ", (void*)box);
        wi_value_print(WI_MAKE_BOX_VALUE(box));
        printf("\n");
    }

    switch (box->kind) {
        case WI_BOX_STRING:
        case WI_BOX_FOREIGN:
            break;
        case WI_BOX_ARRAY: {
            struct wi_array* array = (struct wi_array*)box;
            _gc_mark_value_buf(gc, &array->items);
            break;
        }
        case WI_BOX_MAP: {
            struct wi_map* map = (struct wi_map*)box;
            _gc_mark_table(gc, &map->items);
            break;
        }
        case WI_BOX_PROTOTYPE: {
            struct wi_prototype* prototype = (struct wi_prototype*)box;
            _GC_MARK_BOX(gc, prototype->name);
            _gc_mark_value_buf(gc, &prototype->constants);
            break;
        }
        case WI_BOX_CLOSURE: {
            struct wi_closure* closure = (struct wi_closure*)box;
            _GC_MARK_BOX(gc, closure->prototype);

            for (int i = 0; i < closure->upvalue_count; i++) {
                _GC_MARK_BOX(gc, closure->upvalues[i]);
            }

            _GC_MARK_BOX(gc, closure->required);
            break;
        }
        case WI_BOX_UPVALUE: {
            struct wi_upvalue* upvalue = (struct wi_upvalue*)box;
            _gc_mark_value(gc, upvalue->closed);
            break;
        }
        case WI_BOX_OBJECT: {
            struct wi_object* object = (struct wi_object*)box;
            _gc_mark_table(gc, &object->fields);
            break;
        }
        case WI_BOX_USERDATA: {
            struct wi_userdata* userdata = (struct wi_userdata*)box;
            _GC_MARK_BOX(gc, userdata->name);
            break;
        }
    }
}

#undef _GC_MARK_BOX

static void
_gc_trace_refs(struct wi_gc* gc) {
    while (gc->gray_count > 0) {
        struct wi_box* box = gc->gray_stack[--gc->gray_count];
        _gc_blacken_box(gc, box);
    }
}

static void
_gc_sweep(struct wi_gc* gc) {
    struct wi_box* prev = NULL;
    struct wi_box* box  = gc->boxes;

    while (box) {
        if (box->is_marked) {
            box->is_marked = false;
            prev           = box;
            box            = box->next;
        } else {
            struct wi_box* unreached = box;
            box                      = box->next;

            if (prev) {
                prev->next = box;
            } else {
                gc->boxes = box;
            }

            _gc_free_box(gc, unreached);
        }
    }
}

void
wi_gc_collect_garbage(struct wi_gc* gc) {
    size_t before = gc->bytes_allocated;

    if (wi_log_gc(gc)) {
        printf("--- begin gc ---\n");
    }

    _gc_mark_roots(gc);
    _gc_trace_refs(gc);
    wi_table_remove_white(&gc->strings);
    _gc_sweep(gc);

    size_t grown        = gc->bytes_allocated * WI_GC_HEAP_GROW_FACTOR;
    gc->next_collection = grown > WI_GC_MIN_HEAP ? grown : WI_GC_MIN_HEAP;

    if (wi_log_gc(gc)) {
        printf("---  end gc  ---\n");
        printf("     collected %zu bytes (from %zu to %zu) next at %zu\n", before - gc->bytes_allocated, before,
               gc->bytes_allocated, gc->next_collection);
    }
}
