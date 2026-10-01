#ifndef WI_COMPILER_H
#define WI_COMPILER_H

#include <stdint.h>

#include "wi_buf.h"
#include "wi_code.h"
#include "wi_parser.h"
#include "wi_value.h"

enum wi_attr {
    WI_ATTR_CONST,
    WI_ATTR_UNUSED,
    WI_ATTR_DEPRECATED,
};

/* attrs are in the low 16 bits, a global's slot index (if any) in the high 16 */
typedef uint32_t wi_vardata;

#define WI_DEFAULT_VARDATA 0
#define WI_VARDATA_INDEX_SHIFT 16

WI_INLINE void
wi_attr_set(wi_vardata* vardata, enum wi_attr attr) {
    *vardata |= (wi_vardata)1 << attr;
}

WI_INLINE bool
wi_attr_is_set(wi_vardata vardata, enum wi_attr attr) {
    return vardata & ((wi_vardata)1 << attr);
}

WI_INLINE void
wi_vardata_set_index(wi_vardata* vardata, uint16_t index) {
    *vardata = (*vardata & 0xffff) | ((wi_vardata)index << WI_VARDATA_INDEX_SHIFT);
}

WI_INLINE uint16_t
wi_vardata_index(wi_vardata vardata) {
    return (uint16_t)(vardata >> WI_VARDATA_INDEX_SHIFT);
}

WI_INLINE wi_vardata
wi_value_as_vardata(wi_value value) {
    return (wi_vardata)wi_value_as_real(value);
}

struct wi_local {
    struct wi_token name;
    int             depth; /* -1 = uninitialized */
    bool            is_captured;
    bool            used;
    wi_vardata      vardata;
};

/*
    compiler upvalue.
    ...nah it's obviously cup value
*/
struct wi_cupvalue {
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
    struct wi_code incr;
};

WI_INLINE void
wi_delete_loop(struct wi_loop* loop) {
    wi_code_free(&loop->incr);
    free(loop);
}

struct wi_switch {
    struct wi_switch* enclosing;
    struct wi_int_buf end_jumps; /* each case needs to jump over the whole switch */
    bool              has_default;
};

WI_INLINE void
wi_delete_switch(struct wi_switch* switch_) {
    wi_int_buf_free(&switch_->end_jumps);
    free(switch_);
}

struct wi_compiler {
    struct wi_compiler* outer;
    struct wi_state*    state;
    struct wi_gc*       gc;
    struct wi_parser*   parser;
    struct wi_token     var_name;

    struct wi_module*    module;
    struct wi_prototype* prototype;
    struct wi_code*      code;
    int                  slot_count;
    struct wi_map*       constants;

    struct wi_local* locals;
    int              local_count;
    int              local_capacity;
    int              scope_depth;

    struct wi_cupvalue* upvalues;
    int                 upvalue_capacity; /* count is prototype->upvalue_count */

    struct wi_loop*   loop;
    struct wi_switch* switch_;

    int last_call;
};

struct wi_compiler*
wi_new_compiler(struct wi_compiler* outer, struct wi_state* state, struct wi_parser* parser,
                struct wi_module* module);
void
wi_delete_compiler(struct wi_compiler* compiler);
struct wi_prototype*
wi_compile(struct wi_state* state, const char* file_path, const char* src, struct wi_module* module);

#endif
