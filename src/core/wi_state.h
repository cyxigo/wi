#ifndef WI_STATE_H
#define WI_STATE_H

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
typedef HMODULE wi_lib_handle_t;
#else
typedef void* wi_lib_handle_t;
#include <dlfcn.h>
#endif

#include "../include/wi_conf.h"
#include "wi_box.h"
#include "wi_gc.h"
#include "wi_table.h"
#include "wi_value.h"

static inline void
wi_lib_handle_close(wi_lib_handle_t lib) {
#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
}

typedef enum {
#define WI_OPCODE(name, _) WI_OP_##name,
#include "wi_opcodes.h"
#undef WI_OPCODE
} wi_opcode_t;

typedef struct wi_foreign_handle {
    struct wi_foreign_handle* next;
    wi_lib_handle_t           lib;
} wi_foreign_handle_t;

typedef struct {
    wi_closure_t* closure;
    uint8_t*      ip;
    wi_value_t*   slots;
} wi_call_frame_t;

typedef struct wi_state {
    wi_conf_t conf;
    wi_gc_t*  gc;

    jmp_buf error_jmp;

    wi_call_frame_t frames[WI_CALL_FRAMES_COUNT];
    int             frame_count;

    wi_value_t  stack[WI_STACK_COUNT];
    wi_value_t* stack_top;
    wi_value_t* api_stack;

    wi_table_t    globals;
    wi_table_t    foreign;
    wi_table_t    required;
    wi_upvalue_t* open_upvalues;

    wi_foreign_handle_t* foreign_handles;
} wi_state_t;

static inline void
wi_state_push(wi_state_t* state, wi_value_t value) {
    *state->stack_top++ = value;
}

static inline void
wi_state_drop(wi_state_t* state) {
    state->stack_top--;
}

static inline wi_value_t
wi_state_pop(wi_state_t* state) {
    return *--state->stack_top;
}

static inline wi_value_t
wi_state_peek(wi_state_t* state, int distance) {
    return state->stack_top[-distance - 1];
}

static inline wi_value_t
wi_state_top(wi_state_t* state) {
    return wi_state_peek(state, 0);
}

static inline wi_call_frame_t*
wi_state_frame(wi_state_t* state) {
    return &state->frames[state->frame_count - 1];
}

wi_state_t*
wi_new_state(wi_conf_t conf);
void
wi_delete_state(wi_state_t* state);

bool
wi_state_add_foreign_handle(wi_state_t* state, wi_lib_handle_t lib);

void
wi_state_print_backtrace(wi_state_t* state);
void
wi_state_error(wi_state_t* state, const char* format, ...);

bool
wi_state_run(wi_state_t* state, const char* file_path, const char* src);

#endif
