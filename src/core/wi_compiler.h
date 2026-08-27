#ifndef WI_COMPILER_H
#define WI_COMPILER_H

#include <stdint.h>

#include "../include/wi_conf.h"
#include "wi_box.h"
#include "wi_parser.h"
#include "wi_table.h"

/*
    a whole variables attributes system just for one silly shallow @const?
    NO! there are 3!! @unused and @deprecated too!
*/
enum wi_attr {
    WI_ATTR_CONST,
    WI_ATTR_UNUSED,
    WI_ATTR_DEPRECATED,
};

typedef uint8_t wi_attrs;

/* basically copy-pasted code from wi_conf.h */
#define WI_DEFAULT_ATTRS 0

WI_INLINE void
wi_attr_set(wi_attrs* attrs, enum wi_attr attr) {
    *attrs |= (wi_attrs)1 << attr;
}

WI_INLINE bool
wi_attr_is_set(wi_attrs attrs, enum wi_attr attr) {
    return attrs & ((wi_attrs)1 << attr);
}

struct wi_compiler_local {
    struct wi_token name;
    int             depth; /* -1 = uninitialized */
    bool            is_captured;
    bool            used;
    wi_attrs        attrs;
};

struct wi_compiler_upvalue {
    uint8_t index;
    bool    is_local;
};

struct wi_compiler {
    struct wi_compiler* outer;
    struct wi_state*    state;
    struct wi_gc*       gc;
    struct wi_parser*   parser;
    struct wi_token     var_name;

    struct wi_table*     global_attrs;
    struct wi_prototype* prototype;
    int                  slot_count;
    struct wi_map*       constants;

    struct wi_compiler_local   locals[WI_LOCAL_MAX];
    struct wi_compiler_upvalue upvalues[WI_UPVALUE_MAX];
    int                        local_count;
    int                        scope_depth;

    int innermost_loop_start;
    int innermost_loop_scope_depth;
    int last_call_offset;
};

struct wi_compiler*
wi_new_compiler(struct wi_compiler* outer, struct wi_state* state, struct wi_parser* parser,
                struct wi_table* global_attrs);
void
wi_delete_compiler(struct wi_compiler* compiler);
void
wi_compiler_init(struct wi_compiler* compiler, struct wi_compiler* outer, struct wi_state* state,
                 struct wi_parser* parser, struct wi_table* globals);
struct wi_prototype*
wi_compile(struct wi_state* state, const char* file_path, const char* src, struct wi_table* globals);

#endif
