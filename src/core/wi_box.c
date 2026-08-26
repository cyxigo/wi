#include "wi_box.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "wi_buf.h"
#include "wi_gc.h"
#include "wi_table.h"
#include "wi_value.h"

struct wi_box*
wi_new_box(struct wi_gc* gc, size_t size, enum wi_box_kind kind) {
    struct wi_box* box = wi_gc_realloc(gc, NULL, 0, size);

    box->kind          = kind;
    box->next          = gc->young;
    box->is_marked     = false;
    box->is_old        = false;
    box->is_remembered = false;
    gc->young          = box;

    if (WI_UNLIKELY(wi_log_gc(gc))) {
        printf("allocate box at %p (%zu bytes) of kind %d\n", (void*)box, size, kind);
    }

    return box;
}

struct wi_string*
wi_new_string(struct wi_gc* gc, char* buf, int count, uint32_t hash) {
    struct wi_string* string = WI_NEW_BOX(gc, struct wi_string, WI_BOX_STRING);

    string->buf   = buf;
    string->count = count;
    string->len   = wi_utf8_len(buf, count);
    string->hash  = hash;

    WI_GC_PUSH_ROOT(gc, string);
    wi_table_set(&gc->strings, WI_MAKE_BOX_VALUE(string), wi_make_null_value());
    wi_gc_pop_root(gc);

    return string;
}

struct wi_string*
wi_copy_cstring(struct wi_gc* gc, const char* buf, int count) {
    uint32_t          hash     = wi_string_hash(buf, count);
    struct wi_string* interned = wi_table_find_string(&gc->strings, buf, count, hash);

    if (interned) {
        return interned;
    }

    char* heap_buf = WI_GC_ALLOC(gc, char, count + 1);
    memcpy(heap_buf, buf, (size_t)count);
    heap_buf[count] = '\0';

    return wi_new_string(gc, heap_buf, count, hash);
}

struct wi_string*
wi_take_cstring(struct wi_gc* gc, char* buf, int count) {
    uint32_t          hash     = wi_string_hash(buf, count);
    struct wi_string* interned = wi_table_find_string(&gc->strings, buf, count, hash);

    if (interned) {
        WI_GC_FREE_BUF(gc, char, buf, count + 1);
        return interned;
    }

    return wi_new_string(gc, buf, count, hash);
}

struct wi_array*
wi_new_array(struct wi_gc* gc) {
    struct wi_array* array = WI_NEW_BOX(gc, struct wi_array, WI_BOX_ARRAY);
    wi_value_buf_init(&array->items, gc);
    return array;
}

struct wi_map*
wi_new_map(struct wi_gc* gc) {
    struct wi_map* map = WI_NEW_BOX(gc, struct wi_map, WI_BOX_MAP);
    wi_table_init(&map->items, gc);
    return map;
}

struct wi_prototype*
wi_new_prototype(struct wi_gc* gc, const char* file_path) {
    struct wi_prototype* prototype = WI_NEW_BOX(gc, struct wi_prototype, WI_BOX_PROTOTYPE);

    prototype->file_path = file_path;
    prototype->name      = NULL;
    wi_byte_buf_init(&prototype->bytes, gc);
    wi_int_buf_init(&prototype->lines, gc);
    wi_value_buf_init(&prototype->constants, gc);
    prototype->is_main        = false;
    prototype->is_variadic    = false;
    prototype->arity          = 0;
    prototype->upvalue_count  = 0;
    prototype->max_slot_count = 0;

    return prototype;
}

void
wi_prototype_add_byte(struct wi_prototype* prototype, uint8_t byte, int line) {
    wi_byte_buf_add(&prototype->bytes, byte);
    wi_int_buf_add(&prototype->lines, line);
}

int
wi_prototype_add_constant(struct wi_prototype* prototype, wi_value value) {
    wi_value_buf_add(&prototype->constants, value);
    WI_GC_WRITE_BARRIER(prototype->constants.gc, prototype, value);
    return prototype->constants.count - 1;
}

int
wi_prototype_instr_size(struct wi_prototype* prototype, int offset) {
    static const int opcode_sizes[] = {
#define WI_OPCODE(name, size, __) size,
#include "wi_opcode.h"
#undef WI_OPCODE
    };

    uint8_t* bytes  = prototype->bytes.data;
    uint8_t  opcode = bytes[offset];
    int      size   = opcode_sizes[opcode];

    if (size != -1) {
        return size;
    }

    /* WI_OP_PUSH_CLOSURE */
    uint16_t             constant          = (uint16_t)(bytes[offset + 1] << 8 | bytes[offset + 2]);
    struct wi_prototype* closure_prototype = wi_value_as_prototype(prototype->constants.data[constant]);
    return 3 + closure_prototype->upvalue_count * 2;
}

struct wi_foreign*
wi_new_foreign(struct wi_gc* gc, wi_foreign_fn fn, int arity, bool is_variadic) {
    struct wi_foreign* foreign = WI_NEW_BOX(gc, struct wi_foreign, WI_BOX_FOREIGN);

    foreign->fn          = fn;
    foreign->arity       = arity;
    foreign->is_variadic = is_variadic;

    return foreign;
}

struct wi_closure*
wi_new_closure(struct wi_gc* gc, struct wi_prototype* prototype, struct wi_table* globals) {
    struct wi_upvalue** upvalues = WI_GC_ALLOC(gc, struct wi_upvalue*, prototype->upvalue_count);

    for (uint8_t i = 0; i < prototype->upvalue_count; i++) {
        upvalues[i] = NULL;
    }

    struct wi_closure* closure = WI_NEW_BOX(gc, struct wi_closure, WI_BOX_CLOSURE);

    closure->prototype     = prototype;
    closure->upvalues      = upvalues;
    closure->upvalue_count = prototype->upvalue_count;
    closure->globals       = globals;
    closure->required      = NULL;
    closure->is_main       = false;

    return closure;
}

struct wi_upvalue*
wi_new_upvalue(struct wi_gc* gc, wi_value* slot) {
    struct wi_upvalue* upvalue = WI_NEW_BOX(gc, struct wi_upvalue, WI_BOX_UPVALUE);

    upvalue->next     = NULL;
    upvalue->location = slot;
    upvalue->closed   = wi_make_null_value();

    return upvalue;
}

struct wi_object*
wi_new_object(struct wi_gc* gc) {
    struct wi_object* object = WI_NEW_BOX(gc, struct wi_object, WI_BOX_OBJECT);
    wi_table_init(&object->fields, gc);
    return object;
}

struct wi_userdata*
wi_new_userdata(struct wi_gc* gc, struct wi_string* name, void* data, wi_userdata_finalizer_fn finalizer) {
    struct wi_userdata* userdata = WI_NEW_BOX(gc, struct wi_userdata, WI_BOX_USERDATA);

    userdata->name      = name;
    userdata->data      = data;
    userdata->finalizer = finalizer;

    return userdata;
}
