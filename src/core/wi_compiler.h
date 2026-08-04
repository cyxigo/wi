#ifndef WI_COMPILER_H
#define WI_COMPILER_H

#include "../include/wi_conf.h"
#include "wi_box.h"
#include "wi_parser.h"
#include "wi_table.h"

struct wi_compiler_local {
    struct wi_token name;
    int             depth;
    bool            is_captured;
};

struct wi_compiler_upvalue {
    uint8_t index;
    bool    is_local;
};

struct wi_compiler {
    struct wi_compiler* outer;
    struct wi_state*    state;
    struct wi_parser*   parser;
    struct wi_token     var_name;

    struct wi_table*     globals;
    struct wi_prototype* prototype;
    int                  slot_count;
    struct wi_map*       constants;

    struct wi_compiler_local   locals[WI_LOCALS_MAX];
    struct wi_compiler_upvalue upvalues[WI_UPVALUES_MAX];
    int                        local_count;
    int                        scope_depth;

    int innermost_loop_start;
    int innermost_loop_scope_depth;
    int last_call_offset;
};

struct wi_compiler*
wi_new_compiler(struct wi_compiler* outer, struct wi_state* state, struct wi_parser* parser,
                struct wi_table* globals);
void
wi_delete_compiler(struct wi_compiler* compiler);
void
wi_compiler_init(struct wi_compiler* compiler, struct wi_compiler* outer, struct wi_state* state,
                 struct wi_parser* parser, struct wi_table* globals);
struct wi_prototype*
wi_compile(struct wi_state* state, const char* file_path, const char* src, struct wi_table* globals);

#endif
