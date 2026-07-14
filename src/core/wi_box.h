#ifndef WI_BOX_H
#define WI_BOX_H

#include <stdbool.h>
#include <stdint.h>

#include "../include/wi.h"
#include "wi_buf.h"
#include "wi_table.h"
#include "wi_value.h"

typedef struct wi_state wi_state_t;

typedef enum {
    WI_BOX_STRING,
    WI_BOX_ARRAY,
    WI_BOX_MAP,
    WI_BOX_PROTOTYPE,
    WI_BOX_FOREIGN,
    WI_BOX_CLOSURE,
    WI_BOX_UPVALUE,
    WI_BOX_OBJECT,
    WI_BOX_USERDATA,
} wi_box_kind_t;

typedef struct wi_box {
    wi_box_kind_t  kind;
    struct wi_box* next;
    bool           is_marked;
} wi_box_t;

static inline bool
wi_value_is_box_kind(wi_value_t value, wi_box_kind_t kind) {
    return wi_value_is_box(value) && wi_value_as_box(value)->kind == kind;
}

wi_box_t*
wi_new_box(wi_gc_t* gc, size_t size, wi_box_kind_t kind);

#define WI_NEW_BOX(gc, type, kind) (type*)wi_new_box(gc, sizeof(type), kind)

typedef struct wi_string {
    wi_box_t box;
    char*    chars;
    int32_t  len;
    uint32_t hash;
} wi_string_t;

static inline bool
wi_value_is_string(wi_value_t value) {
    return wi_value_is_box_kind(value, WI_BOX_STRING);
}

static inline wi_string_t*
wi_value_as_string(wi_value_t value) {
    return (wi_string_t*)wi_value_as_box(value);
}
static inline char*
wi_value_as_cstring(wi_value_t value) {
    return wi_value_as_string(value)->chars;
}

wi_string_t*
wi_new_string(wi_gc_t* gc, char* chars, int len, uint32_t hash);
wi_string_t*
wi_copy_cstring(wi_gc_t* gc, const char* chars, int len);
wi_string_t*
wi_take_cstring(wi_gc_t* gc, char* chars, int len);

typedef struct {
    wi_box_t       box;
    wi_value_buf_t items;
} wi_array_t;

static inline bool
wi_value_is_array(wi_value_t value) {
    return wi_value_is_box_kind(value, WI_BOX_ARRAY);
}

static inline wi_array_t*
wi_value_as_array(wi_value_t value) {
    return (wi_array_t*)wi_value_as_box(value);
}

wi_array_t*
wi_new_array(wi_gc_t* gc);

typedef struct {
    wi_box_t   box;
    wi_table_t items;
} wi_map_t;

static inline bool
wi_value_is_map(wi_value_t value) {
    return wi_value_is_box_kind(value, WI_BOX_MAP);
}

static inline wi_map_t*
wi_value_as_map(wi_value_t value) {
    return (wi_map_t*)wi_value_as_box(value);
}

wi_map_t*
wi_new_map(wi_gc_t* gc);

typedef struct {
    wi_box_t       box;
    const char*    file_path;
    wi_string_t*   name;
    wi_byte_buf_t  bytes;
    wi_int_buf_t   lines;
    wi_value_buf_t constants;
    bool           is_main;
    int            arity;
    int            upvalue_count;
} wi_prototype_t;

static inline bool
wi_value_is_prototype(wi_value_t value) {
    return wi_value_is_box_kind(value, WI_BOX_PROTOTYPE);
}

static inline wi_prototype_t*
wi_value_as_prototype(wi_value_t value) {
    return (wi_prototype_t*)wi_value_as_box(value);
}

wi_prototype_t*
wi_new_prototype(wi_gc_t* gc, const char* fpath);
void
wi_prototype_add_byte(wi_prototype_t* prototype, uint8_t byte, int line);
int
wi_prototype_add_constant(wi_prototype_t* prototype, wi_value_t value);
int
wi_prototype_instr_size(wi_prototype_t* prototype, int offset);

typedef struct {
    wi_box_t        box;
    wi_foreign_fn_t fn;
    wi_string_t*    name;
    int             arity;
} wi_foreign_t;

static inline bool
wi_value_is_foreign(wi_value_t value) {
    return wi_value_is_box_kind(value, WI_BOX_FOREIGN);
}

static inline wi_foreign_t*
wi_value_as_foreign(wi_value_t value) {
    return (wi_foreign_t*)wi_value_as_box(value);
}

wi_foreign_t*
wi_new_foreign(wi_gc_t* gc, wi_foreign_fn_t fn, wi_string_t* name, int arity);

typedef struct wi_upvalue wi_upvalue_t;

typedef struct {
    wi_box_t        box;
    wi_prototype_t* prototype;
    wi_upvalue_t**  upvalues;
    int             upvalue_count;
    wi_table_t*     globals;
    bool            is_required;
} wi_closure_t;

static inline bool
wi_value_is_closure(wi_value_t value) {
    return wi_value_is_box_kind(value, WI_BOX_CLOSURE);
}

static inline wi_closure_t*
wi_value_as_closure(wi_value_t value) {
    return (wi_closure_t*)wi_value_as_box(value);
}

wi_closure_t*
wi_new_closure(wi_gc_t* gc, wi_prototype_t* prototype, wi_table_t* globals);

typedef struct wi_upvalue {
    wi_box_t           box;
    struct wi_upvalue* next;
    wi_value_t*        location;
    wi_value_t         closed;
} wi_upvalue_t;

static inline bool
wi_value_is_upvalue(wi_value_t value) {
    return wi_value_is_box_kind(value, WI_BOX_UPVALUE);
}

static inline wi_upvalue_t*
wi_value_as_upvalue(wi_value_t value) {
    return (wi_upvalue_t*)wi_value_as_box(value);
}

wi_upvalue_t*
wi_new_upvalue(wi_gc_t* gc, wi_value_t* slot);

typedef struct wi_object {
    wi_box_t     box;
    wi_string_t* name;
    wi_table_t   fields;
} wi_object_t;

static inline bool
wi_value_is_object(wi_value_t value) {
    return wi_value_is_box_kind(value, WI_BOX_OBJECT);
}

static inline wi_object_t*
wi_value_as_object(wi_value_t value) {
    return (wi_object_t*)wi_value_as_box(value);
}

wi_object_t*
wi_new_object(wi_gc_t* gc, wi_string_t* name);

typedef struct {
    wi_box_t                   box;
    wi_string_t*               name;
    void*                      data;
    wi_userdata_finalizer_fn_t finalizer;
} wi_userdata_t;

static inline bool
wi_value_is_userdata(wi_value_t value) {
    return wi_value_is_box_kind(value, WI_BOX_USERDATA);
}

static inline wi_userdata_t*
wi_value_as_userdata(wi_value_t value) {
    return (wi_userdata_t*)wi_value_as_box(value);
}

wi_userdata_t*
wi_new_userdata(wi_gc_t* gc, wi_string_t* name, void* data, wi_userdata_finalizer_fn_t finalizer);

#endif
