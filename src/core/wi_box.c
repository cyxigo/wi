#include "wi_box.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "wi_buf.h"
#include "wi_gc.h"
#include "wi_state.h"
#include "wi_table.h"
#include "wi_value.h"

wi_box_t*
wi_new_box(wi_gc_t* gc, size_t size, wi_box_kind_t kind) {
    wi_box_t* box = wi_gc_realloc(gc, NULL, 0, size);

    box->kind      = kind;
    box->next      = gc->boxes;
    box->is_marked = false;
    gc->boxes      = box;

    if (wi_log_gc(gc)) {
        printf("allocate box at %p (%zu bytes) of kind %d\n", (void*)box, size, kind);
    }

    return box;
}

wi_string_t*
wi_new_string(wi_gc_t* gc, char* chars, int len, uint32_t hash) {
    wi_string_t* string = WI_NEW_BOX(gc, wi_string_t, WI_BOX_STRING);

    string->chars = chars;
    string->len   = len;
    string->hash  = hash;

    wi_gc_push_root(gc, (wi_box_t*)string);
    wi_table_set(&gc->strings, WI_MAKE_BOX_VALUE(string), wi_make_null_value());
    wi_gc_pop_root(gc);

    return string;
}

wi_string_t*
wi_copy_cstring(wi_gc_t* gc, const char* chars, int len) {
    uint32_t     hash     = wi_string_hash(chars, len);
    wi_string_t* interned = wi_table_find_string(&gc->strings, chars, len, hash);

    if (interned) {
        return interned;
    }

    char* heap_chars = WI_GC_ALLOC(gc, char, len + 1);
    memcpy(heap_chars, chars, (size_t)len);
    heap_chars[len] = '\0';

    return wi_new_string(gc, heap_chars, len, hash);
}

wi_string_t*
wi_take_cstring(wi_gc_t* gc, char* chars, int len) {
    uint32_t     hash     = wi_string_hash(chars, len);
    wi_string_t* interned = wi_table_find_string(&gc->strings, chars, len, hash);

    if (interned) {
        WI_GC_FREE_ARRAY(gc, char, chars, len + 1);
        return interned;
    }

    return wi_new_string(gc, chars, len, hash);
}

wi_array_t*
wi_new_array(wi_gc_t* gc) {
    wi_array_t* array = WI_NEW_BOX(gc, wi_array_t, WI_BOX_ARRAY);
    wi_value_buf_init(&array->items, gc);
    return array;
}

wi_map_t*
wi_new_map(wi_gc_t* gc) {
    wi_map_t* map = WI_NEW_BOX(gc, wi_map_t, WI_BOX_MAP);
    wi_table_init(&map->items, gc);
    return map;
}

wi_prototype_t*
wi_new_prototype(wi_gc_t* gc, const char* file_path) {
    wi_prototype_t* prototype = WI_NEW_BOX(gc, wi_prototype_t, WI_BOX_PROTOTYPE);

    prototype->file_path = file_path;
    prototype->name      = NULL;
    wi_byte_buf_init(&prototype->bytes, gc);
    wi_int_buf_init(&prototype->lines, gc);
    wi_value_buf_init(&prototype->constants, gc);
    prototype->is_main       = false;
    prototype->is_variadic   = false;
    prototype->arity         = 0;
    prototype->upvalue_count = 0;

    return prototype;
}

void
wi_prototype_add_byte(wi_prototype_t* prototype, uint8_t byte, int line) {
    wi_byte_buf_add(&prototype->bytes, byte);
    wi_int_buf_add(&prototype->lines, line);
}

int
wi_prototype_add_constant(wi_prototype_t* prototype, wi_value_t value) {
    wi_value_buf_add(&prototype->constants, value);
    return prototype->constants.count - 1;
}

int
wi_prototype_instr_size(wi_prototype_t* prototype, int offset) {
    static const int opcode_sizes[] = {
#define WI_OPCODE(name, size) size,
#include "wi_opcodes.h"
#undef WI_OPCODE
    };

    uint8_t*    bytes  = prototype->bytes.data;
    wi_opcode_t opcode = bytes[offset];
    int         size   = opcode_sizes[opcode];

    if (size != -1) {
        return size;
    }

    if (opcode == WI_OP_PUSH_CLOSURE) {
        uint16_t        constant          = bytes[offset + 1] << 8 | bytes[offset + 2];
        wi_prototype_t* closure_prototype = wi_value_as_prototype(prototype->constants.data[constant]);
        return 3 + closure_prototype->upvalue_count * 2;
    }

    // WI_OP_PUSH_OBJECT
    uint8_t has_name = bytes[offset + 3];
    return 4 + (has_name ? 2 : 0);
}

wi_foreign_t*
wi_new_foreign(wi_gc_t* gc, wi_foreign_fn_t fn, wi_string_t* name, int arity, bool is_variadic) {
    wi_foreign_t* foreign = WI_NEW_BOX(gc, wi_foreign_t, WI_BOX_FOREIGN);

    foreign->fn          = fn;
    foreign->name        = name;
    foreign->arity       = arity;
    foreign->is_variadic = is_variadic;

    return foreign;
}

wi_closure_t*
wi_new_closure(wi_gc_t* gc, wi_prototype_t* prototype, wi_table_t* globals) {
    wi_upvalue_t** upvalues = WI_GC_ALLOC(gc, wi_upvalue_t*, prototype->upvalue_count);

    for (int i = 0; i < prototype->upvalue_count; i++) {
        upvalues[i] = NULL;
    }

    wi_closure_t* closure = WI_NEW_BOX(gc, wi_closure_t, WI_BOX_CLOSURE);

    closure->prototype     = prototype;
    closure->upvalues      = upvalues;
    closure->upvalue_count = prototype->upvalue_count;
    closure->globals       = globals;
    closure->is_required   = false;

    return closure;
}

wi_upvalue_t*
wi_new_upvalue(wi_gc_t* gc, wi_value_t* slot) {
    wi_upvalue_t* upvalue = WI_NEW_BOX(gc, wi_upvalue_t, WI_BOX_UPVALUE);

    upvalue->next     = NULL;
    upvalue->location = slot;
    upvalue->closed   = wi_make_null_value();

    return upvalue;
}

wi_object_t*
wi_new_object(wi_gc_t* gc, wi_string_t* name) {
    wi_object_t* object = WI_NEW_BOX(gc, wi_object_t, WI_BOX_OBJECT);

    object->name = name;
    wi_table_init(&object->fields, gc);

    return object;
}

wi_userdata_t*
wi_new_userdata(wi_gc_t* gc, wi_string_t* name, void* data, wi_userdata_finalizer_fn_t finalizer) {
    wi_userdata_t* userdata = WI_NEW_BOX(gc, wi_userdata_t, WI_BOX_USERDATA);

    userdata->name      = name;
    userdata->data      = data;
    userdata->finalizer = finalizer;

    return userdata;
}
