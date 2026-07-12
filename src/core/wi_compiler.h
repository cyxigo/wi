#ifndef WI_COMPILER_H
#define WI_COMPILER_H

#include "../include/wi_conf.h"
#include "wi_box.h"
#include "wi_parser.h"

typedef struct {
    wi_token_t name;
    int        depth;
    bool       is_captured;
} wi_compiler_local_t;

typedef struct {
    uint8_t index;
    bool    is_local;
} wi_compiler_upvalue_t;

typedef struct wi_compiler {
    struct wi_compiler* outer;
    wi_state_t*         state;
    wi_parser_t*        parser;
    wi_token_t          var_name;

    wi_prototype_t* prototype;
    wi_map_t*       constants;

    wi_compiler_local_t   locals[WI_LOCALS_MAX];
    wi_compiler_upvalue_t upvalues[WI_UPVALUES_MAX];
    int                   local_count;
    int                   scope_depth;

    int innermost_loop_start;
    int innermost_loop_scope_depth;
    int last_call_offset;
} wi_compiler_t;

wi_compiler_t*
wi_new_compiler(wi_compiler_t* outer, wi_state_t* state, wi_parser_t* parser);
void
wi_delete_compiler(wi_compiler_t* compiler);
void
wi_compiler_init(wi_compiler_t* compiler, wi_compiler_t* outer, wi_state_t* state, wi_parser_t* parser);
wi_prototype_t*
wi_compile(wi_state_t* state, const char* file_path, const char* src);

#endif
