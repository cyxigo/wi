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

wi_gc_t*
wi_new_gc(wi_conf_t* conf) {
    wi_gc_t* gc = malloc(sizeof(wi_gc_t));

    if (!gc) {
        return NULL;
    }

    gc->conf = conf;

    gc->state    = NULL;
    gc->compiler = NULL;

    gc->boxes           = NULL;
    gc->bytes_allocated = 0;
    gc->next_collection = 1 << 20;

    gc->gray_stack    = NULL;
    gc->gray_capacity = 0;
    gc->gray_count    = 0;

    gc->temp_root_count = 0;

    wi_table_init(&gc->strings, gc);
    return gc;
}

static void
_gc_free_box(wi_gc_t* gc, wi_box_t* box);

void
wi_delete_gc(wi_gc_t* gc) {
    wi_box_t* box = gc->boxes;

    while (box) {
        wi_box_t* next = box->next;
        _gc_free_box(gc, box);
        box = next;
    }

    free(gc->gray_stack);
    wi_table_free(&gc->strings);
    free(gc);
}

void*
wi_gc_realloc(wi_gc_t* gc, void* ptr, size_t old_size, size_t new_size) {
    if (!gc->state) {
        fprintf(stderr, "memory error: garbage collector does not have a reference to a state\n");
        exit(EXIT_FAILURE);
    }

    gc->bytes_allocated += new_size - old_size;

    if (new_size > old_size) {
        if (wi_conf_is_set(gc->conf, WI_CONF_STRESS_GC) || gc->bytes_allocated > gc->next_collection) {
            wi_gc_collect_garbage(gc);
        }
    }

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void* result = realloc(ptr, new_size);

    if (!result) {
        fprintf(stderr, "memory error: not enough memory to allocate %zu bytes\n", new_size - old_size);
        exit(EXIT_FAILURE);
    }

    return result;
}

static void
_gc_free_box(wi_gc_t* gc, wi_box_t* box) {
    if (wi_log_gc(gc)) {
        printf("free box at %p of kind %d\n", (void*)box, box->kind);
    }

    switch (box->kind) {
        case WI_BOX_STRING: {
            wi_string_t* string = (wi_string_t*)box;
            WI_GC_FREE_ARRAY(gc, char, string->chars, string->len + 1);
            WI_GC_FREE(gc, wi_string_t, box);
            break;
        }
        case WI_BOX_ARRAY: {
            wi_array_t* array = (wi_array_t*)box;
            wi_value_buf_free(&array->items);
            WI_GC_FREE(gc, wi_array_t, box);
            break;
        }
        case WI_BOX_MAP: {
            wi_map_t* map = (wi_map_t*)box;
            wi_table_free(&map->items);
            WI_GC_FREE(gc, wi_map_t, box);
            break;
        }
        case WI_BOX_PROTOTYPE: {
            wi_prototype_t* prototype = (wi_prototype_t*)box;

            wi_byte_buf_free(&prototype->bytes);
            wi_int_buf_free(&prototype->lines);
            wi_value_buf_free(&prototype->constants);
            WI_GC_FREE(gc, wi_prototype_t, box);

            break;
        }
        case WI_BOX_FOREIGN:
            WI_GC_FREE(gc, wi_foreign_t, box);
            break;
        case WI_BOX_CLOSURE: {
            wi_closure_t* closure = (wi_closure_t*)box;
            WI_GC_FREE_ARRAY(gc, wi_upvalue_t*, closure->upvalues, closure->upvalue_count);
            WI_GC_FREE(gc, wi_closure_t, box);
            break;
        }
        case WI_BOX_UPVALUE:
            WI_GC_FREE(gc, wi_upvalue_t, box);
            break;
        case WI_BOX_OBJECT: {
            wi_object_t* object = (wi_object_t*)box;
            wi_table_free(&object->fields);
            WI_GC_FREE(gc, wi_object_t, box);
            break;
        }
        case WI_BOX_USERDATA: {
            wi_userdata_t* userdata = (wi_userdata_t*)box;

            if (userdata->data && userdata->finalizer) {
                userdata->finalizer(userdata->data);
            }

            WI_GC_FREE(gc, wi_userdata_t, box);
            break;
        }
    }
}

static void
_gc_mark_box(wi_gc_t* gc, wi_box_t* box) {
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

    if (gc->gray_count + 1 > gc->gray_capacity) {
        gc->gray_capacity = WI_GROW_CAPACITY(gc->gray_capacity);
        gc->gray_stack    = realloc(gc->gray_stack, sizeof(wi_box_t*) * (size_t)gc->gray_capacity);

        if (!gc->gray_stack) {
            fprintf(stderr, "memory error: failed to allocate memory for the garbage collector gray stack\n");
            exit(EXIT_FAILURE);
        }
    }

    gc->gray_stack[gc->gray_count++] = box;
}

static void
_gc_mark_value(wi_gc_t* gc, wi_value_t value) {
    if (!wi_value_is_box(value)) {
        return;
    }

    _gc_mark_box(gc, wi_value_as_box(value));
}

static void
_gc_mark_value_buf(wi_gc_t* gc, wi_value_buf_t* buf) {
    for (int i = 0; i < buf->count; i++) {
        _gc_mark_value(gc, buf->data[i]);
    }
}

static void
_gc_mark_table(wi_gc_t* gc, wi_table_t* table) {
    for (int i = 0; i < table->capacity; i++) {
        wi_entry_t* entry = &table->entries[i];
        _gc_mark_value(gc, entry->key);
        _gc_mark_value(gc, entry->value);
    }
}

static void
_gc_mark_compiler(wi_gc_t* gc) {
    if (!gc->compiler) {
        return;
    }

    wi_compiler_t* compiler = gc->compiler;

    while (compiler) {
        _gc_mark_box(gc, (wi_box_t*)compiler->prototype);
        _gc_mark_box(gc, (wi_box_t*)compiler->constants);
        compiler = compiler->outer;
    }
}

static void
_gc_mark_roots(wi_gc_t* gc) {
    for (int i = 0; i < gc->temp_root_count; i++) {
        _gc_mark_box(gc, gc->temp_roots[i]);
    }

    _gc_mark_compiler(gc);
    wi_state_t* state = gc->state;

    for (wi_value_t* slot = state->stack; slot < state->stack_top; slot++) {
        _gc_mark_value(gc, *slot);
    }

    for (int i = 0; i < state->frame_count; i++) {
        _gc_mark_box(gc, (wi_box_t*)state->frames[i].closure);
    }

    for (wi_upvalue_t* upvalue = state->open_upvalues; upvalue; upvalue = upvalue->next) {
        _gc_mark_box(gc, (wi_box_t*)upvalue);
    }

    _gc_mark_table(gc, &state->globals);
    _gc_mark_table(gc, &state->required);
    _gc_mark_table(gc, &state->foreign);
}

static void
_gc_blacken_box(wi_gc_t* gc, wi_box_t* box) {
    if (wi_log_gc(gc)) {
        printf("blacken box at %p ", (void*)box);
        wi_value_print(WI_MAKE_BOX_VALUE(box));
        printf("\n");
    }

    switch (box->kind) {
        case WI_BOX_STRING:
            break;
        case WI_BOX_ARRAY: {
            wi_array_t* array = (wi_array_t*)box;
            _gc_mark_value_buf(gc, &array->items);
            break;
        }
        case WI_BOX_MAP: {
            wi_map_t* map = (wi_map_t*)box;
            _gc_mark_table(gc, &map->items);
            break;
        }
        case WI_BOX_PROTOTYPE: {
            wi_prototype_t* prototype = (wi_prototype_t*)box;
            _gc_mark_box(gc, (wi_box_t*)prototype->name);
            _gc_mark_value_buf(gc, &prototype->constants);
            break;
        }
        case WI_BOX_FOREIGN: {
            wi_foreign_t* foreign = (wi_foreign_t*)box;
            _gc_mark_box(gc, (wi_box_t*)foreign->name);
            break;
        }
        case WI_BOX_CLOSURE: {
            wi_closure_t* closure = (wi_closure_t*)box;
            _gc_mark_box(gc, (wi_box_t*)closure->prototype);

            for (int i = 0; i < closure->upvalue_count; i++) {
                _gc_mark_box(gc, (wi_box_t*)closure->upvalues[i]);
            }

            break;
        }
        case WI_BOX_UPVALUE: {
            wi_upvalue_t* upvalue = (wi_upvalue_t*)box;
            _gc_mark_value(gc, upvalue->closed);
            break;
        }
        case WI_BOX_OBJECT: {
            wi_object_t* object = (wi_object_t*)box;
            _gc_mark_box(gc, (wi_box_t*)object->name);
            _gc_mark_table(gc, &object->fields);
            break;
        }
        case WI_BOX_USERDATA: {
            wi_userdata_t* userdata = (wi_userdata_t*)box;
            _gc_mark_box(gc, (wi_box_t*)userdata->name);
            break;
        }
    }
}

static void
_gc_trace_refs(wi_gc_t* gc) {
    while (gc->gray_count > 0) {
        wi_box_t* box = gc->gray_stack[--gc->gray_count];
        _gc_blacken_box(gc, box);
    }
}

static void
_gc_sweep(wi_gc_t* gc) {
    wi_box_t* prev = NULL;
    wi_box_t* box  = gc->boxes;

    while (box) {
        if (box->is_marked) {
            box->is_marked = false;
            prev           = box;
            box            = box->next;
        } else {
            wi_box_t* unreached = box;
            box                 = box->next;

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
wi_gc_collect_garbage(wi_gc_t* gc) {
    size_t before = gc->bytes_allocated;

    if (wi_log_gc(gc)) {
        printf("--- begin gc ---\n");
    }

    _gc_mark_roots(gc);
    _gc_trace_refs(gc);
    wi_table_remove_white(&gc->strings);
    _gc_sweep(gc);

    gc->next_collection = gc->bytes_allocated * WI_GC_HEAP_GROW_FACTOR;

    if (wi_log_gc(gc)) {
        printf("---  end gc  ---\n");
        printf("     collected %zu bytes (from %zu to %zu) next at %zu\n", before - gc->bytes_allocated, before,
               gc->bytes_allocated, gc->next_collection);
    }
}
