#ifndef WI_COMPILER_H
#define WI_COMPILER_H

#include <stdint.h>

#include "wi_buf.h"
#include "wi_parser.h"

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

struct wi_loop {
    struct wi_loop* enclosing;
    int             start;
    int             scope_depth;
    /*
        offset to which continue will jump to
        for - increment start
        while - loop opcode
        -1 - not yet set but must be unreachable
    */
    int continue_start;
    /*
        we need to keep increment part of the for-loop... somewhere. we keep it here!
        the reason we need to even do that is because increment is parsed before the body
        and should be executed... after.
    */
    struct wi_byte_buf incr_bytes;
    struct wi_int_buf  incr_lines;
};

WI_INLINE void
wi_delete_loop(struct wi_loop* loop) {
    wi_byte_buf_free(&loop->incr_bytes);
    wi_int_buf_free(&loop->incr_lines);
    free(loop);
}

struct wi_compiler {
    struct wi_compiler* outer;
    struct wi_state*    state;
    struct wi_gc*       gc;
    struct wi_parser*   parser;
    struct wi_token     var_name;

    struct wi_module*    module;
    struct wi_prototype* prototype;
    int                  slot_count;
    struct wi_map*       constants;

    struct wi_compiler_local* locals;
    int                       local_count;
    int                       local_capacity;
    int                       scope_depth;

    struct wi_compiler_upvalue* upvalues;
    int                         upvalue_capacity; /* count is prototype->upvalue_count */

    struct wi_loop* innermost_loop;
    int             last_call_offset;
};

struct wi_compiler*
wi_new_compiler(struct wi_compiler* outer, struct wi_state* state, struct wi_parser* parser,
                struct wi_module* module);
void
wi_delete_compiler(struct wi_compiler* compiler);
struct wi_prototype*
wi_compile(struct wi_state* state, const char* file_path, const char* src, struct wi_module* module);

#endif
