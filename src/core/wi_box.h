#ifndef WI_BOX_H
#define WI_BOX_H

#include <stdbool.h>
#include <stdint.h>

#include "../../include/wi.h"
#include "wi_buf.h"
#include "wi_table.h"
#include "wi_value.h"

enum wi_box_kind {
    WI_BOX_STRING,
    WI_BOX_ARRAY,
    WI_BOX_MAP,
    WI_BOX_PROTOTYPE,
    WI_BOX_FOREIGN,
    WI_BOX_CLOSURE,
    WI_BOX_UPVALUE,
    WI_BOX_OBJECT,
    WI_BOX_USERDATA,
};

struct wi_box {
    enum wi_box_kind kind;
    struct wi_box*   next;
    bool             is_marked;
    bool             is_old;
    bool             is_remembered;
};

WI_INLINE bool
wi_value_is_box_kind(wi_value value, enum wi_box_kind kind) {
    return wi_value_is_box(value) && wi_value_as_box(value)->kind == kind;
}

struct wi_box*
wi_new_box(struct wi_gc* gc, size_t size, enum wi_box_kind kind);

#define WI_NEW_BOX(gc, type, kind) (type*)wi_new_box(gc, sizeof(type), kind)

struct wi_string {
    struct wi_box box;
    char*         buf;
    int           count;
    int           len;
    uint32_t      hash;
};

WI_INLINE bool
wi_value_is_string(wi_value value) {
    return wi_value_is_box_kind(value, WI_BOX_STRING);
}

WI_INLINE struct wi_string*
wi_value_as_string(wi_value value) {
    return (struct wi_string*)wi_value_as_box(value);
}

WI_INLINE char*
wi_value_as_cstring(wi_value value) {
    return wi_value_as_string(value)->buf;
}

WI_INLINE uint32_t
wi_string_hash(const char* buf, int len) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)buf[i];
        hash *= 16777619u;
    }

    return hash;
}

struct wi_string*
wi_new_string(struct wi_gc* gc, char* buf, int count, uint32_t hash);
struct wi_string*
wi_copy_cstring(struct wi_gc* gc, const char* buf, int count);
struct wi_string*
wi_take_cstring(struct wi_gc* gc, char* buf, int count);

WI_INLINE struct wi_string*
wi_make_string(struct wi_gc* gc, const char* string) {
    return wi_copy_cstring(gc, string, (int)strlen(string));
}

struct wi_array {
    struct wi_box       box;
    struct wi_value_buf items;
};

WI_INLINE bool
wi_value_is_array(wi_value value) {
    return wi_value_is_box_kind(value, WI_BOX_ARRAY);
}

WI_INLINE struct wi_array*
wi_value_as_array(wi_value value) {
    return (struct wi_array*)wi_value_as_box(value);
}

struct wi_array*
wi_new_array(struct wi_gc* gc);

struct wi_map {
    struct wi_box   box;
    struct wi_table items;
};

WI_INLINE bool
wi_value_is_map(wi_value value) {
    return wi_value_is_box_kind(value, WI_BOX_MAP);
}

WI_INLINE struct wi_map*
wi_value_as_map(wi_value value) {
    return (struct wi_map*)wi_value_as_box(value);
}

struct wi_map*
wi_new_map(struct wi_gc* gc);

struct wi_prototype {
    struct wi_box       box;
    const char*         file_path;
    struct wi_string*   name;
    struct wi_byte_buf  bytes;
    struct wi_int_buf   lines;
    struct wi_value_buf constants;
    bool                is_main;
    bool                is_variadic;
    uint8_t             arity;
    uint8_t             upvalue_count;
    int                 max_slot_count;
};

WI_INLINE bool
wi_value_is_prototype(wi_value value) {
    return wi_value_is_box_kind(value, WI_BOX_PROTOTYPE);
}

WI_INLINE struct wi_prototype*
wi_value_as_prototype(wi_value value) {
    return (struct wi_prototype*)wi_value_as_box(value);
}

struct wi_prototype*
wi_new_prototype(struct wi_gc* gc, const char* fpath);
int
wi_prototype_instr_size(struct wi_prototype* prototype, int offset);

struct wi_foreign {
    struct wi_box box;
    wi_foreign_fn fn;
    int           arity;
    bool          is_variadic;
};

WI_INLINE bool
wi_value_is_foreign(wi_value value) {
    return wi_value_is_box_kind(value, WI_BOX_FOREIGN);
}

WI_INLINE struct wi_foreign*
wi_value_as_foreign(wi_value value) {
    return (struct wi_foreign*)wi_value_as_box(value);
}

struct wi_foreign*
wi_new_foreign(struct wi_gc* gc, wi_foreign_fn fn, int arity, bool is_variadic);

struct wi_upvalue;

struct wi_closure {
    struct wi_box        box;
    struct wi_prototype* prototype;
    struct wi_upvalue**  upvalues;
    uint8_t              upvalue_count;
    struct wi_table*     globals;
    /*
        may be confusing so i'll explain:
        when a script is required, we compile and run it just like the main script -
        compiling it into a closure *and* setting this field because required scripts
        turn into objects, so if this field is set - script is not main (was required)
    */
    struct wi_object* required;
    bool              is_main; /* is this closure a main closure? (the one that runs the script) */
};

WI_INLINE bool
wi_value_is_closure(wi_value value) {
    return wi_value_is_box_kind(value, WI_BOX_CLOSURE);
}

WI_INLINE struct wi_closure*
wi_value_as_closure(wi_value value) {
    return (struct wi_closure*)wi_value_as_box(value);
}

struct wi_closure*
wi_new_closure(struct wi_gc* gc, struct wi_prototype* prototype, struct wi_table* globals);

struct wi_upvalue {
    struct wi_box      box;
    struct wi_upvalue* next;
    wi_value*          location;
    wi_value           closed;
};

WI_INLINE bool
wi_value_is_upvalue(wi_value value) {
    return wi_value_is_box_kind(value, WI_BOX_UPVALUE);
}

WI_INLINE struct wi_upvalue*
wi_value_as_upvalue(wi_value value) {
    return (struct wi_upvalue*)wi_value_as_box(value);
}

struct wi_upvalue*
wi_new_upvalue(struct wi_gc* gc, wi_value* slot);

struct wi_object {
    struct wi_box   box;
    struct wi_table fields;
};

WI_INLINE bool
wi_value_is_object(wi_value value) {
    return wi_value_is_box_kind(value, WI_BOX_OBJECT);
}

WI_INLINE struct wi_object*
wi_value_as_object(wi_value value) {
    return (struct wi_object*)wi_value_as_box(value);
}

struct wi_object*
wi_new_object(struct wi_gc* gc);

struct wi_userdata {
    struct wi_box            box;
    struct wi_string*        name;
    void*                    data;
    wi_userdata_finalizer_fn finalizer;
};

WI_INLINE bool
wi_value_is_userdata(wi_value value) {
    return wi_value_is_box_kind(value, WI_BOX_USERDATA);
}

WI_INLINE struct wi_userdata*
wi_value_as_userdata(wi_value value) {
    return (struct wi_userdata*)wi_value_as_box(value);
}

struct wi_userdata*
wi_new_userdata(struct wi_gc* gc, struct wi_string* name, void* data, wi_userdata_finalizer_fn finalizer);

#endif
