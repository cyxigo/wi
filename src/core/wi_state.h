#ifndef WI_STATE_H
#define WI_STATE_H

#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "wi_util.h"

#ifdef _WIN32
#include <windows.h>
typedef HMODULE wi_lib_handle;
#else
typedef void* wi_lib_handle;
#include <dlfcn.h>
#endif

#include "../../include/wi_conf.h"
#include "wi_box.h"
#include "wi_gc.h"
#include "wi_table.h"
#include "wi_value.h"

WI_INLINE void
wi_lib_close(wi_lib_handle handle) {
#ifdef _WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
}

enum {
#define WI_OPCODE(name, _, __) WI_OP_##name,
#include "wi_opcode.h"
#undef WI_OPCODE
};

struct wi_lib_node {
    struct wi_lib_node* next;
    wi_lib_handle       handle;
};

struct wi_call_frame {
    struct wi_closure* closure;
    uint8_t*           ip;
    wi_value*          slots;
};

struct wi_recovery {
    struct wi_recovery* next;
    jmp_buf             jmp;
    int                 frame_count;
    uint8_t             c_call_depth;
    wi_value*           stack_top;
    wi_value*           ffi_stack;
    int                 temp_root_count;
    struct wi_string*   error;
};

struct wi_state {
    wi_conf*      conf;
    struct wi_gc* gc;

    bool        was_eof_error;
    wi_print_fn out;
    wi_print_fn error;

    wi_on_compile_fn     on_compile;
    wi_load_require_fn   load_require;
    wi_require_exists_fn require_exists;

    int          script_argc;
    const char** script_argv;

    jmp_buf               jmp;
    volatile sig_atomic_t interrupted;

    /*
        recoveries are implemented as a linked list because using a dynamic
        array with realloc on a structure with jmp_buf is undefined behaviour™
    */
    struct wi_recovery* recoveries;
    uint8_t             recovery_count;

    struct wi_call_frame* frames;
    int                   frame_capacity;
    int                   frame_count;
    uint8_t               c_depth;

    wi_value* stack;
    int       stack_capacity;
    wi_value* stack_end;
    wi_value* stack_top;
    wi_value* ffi_stack;

    struct wi_table globals;
    /*
        this table is used by the compiler for two purposes:
        1. track globals definition and redefinition
        2. track globals attributes, such as @const
    */
    struct wi_table    global_attrs;
    struct wi_table    foreign;
    struct wi_table    required;
    struct wi_upvalue* open_upvalues;

    /*
        these are a linked list because:
        1. we won't have THAT many opened handles that a linked list would be an overhead compared
           to a dynamic array with realloc
        2. i love linked lists
    */
    struct wi_lib_node* libs;

    /*
        stm - standard method library
        methods for builtin types
    */
    struct wi_table stm_string;
    struct wi_table stm_array;
    struct wi_table stm_map;
};

WI_INLINE void
wi_state_push(struct wi_state* state, wi_value value) {
    *state->stack_top++ = value;
}

WI_INLINE void
wi_state_drop(struct wi_state* state) {
    state->stack_top--;
}

WI_INLINE wi_value
wi_state_pop(struct wi_state* state) {
    return *--state->stack_top;
}

WI_INLINE wi_value
wi_state_peek(struct wi_state* state, int distance) {
    return state->stack_top[-distance - 1];
}

WI_INLINE wi_value
wi_state_top(struct wi_state* state) {
    return wi_state_peek(state, 0);
}

WI_INLINE struct wi_call_frame*
wi_state_frame(struct wi_state* state) {
    return &state->frames[state->frame_count - 1];
}

struct wi_state*
wi_new_state(wi_conf* conf);
void
wi_delete_state(struct wi_state* state);

void
wi_state_tune_gc(wi_state* state, size_t min_heap, size_t heap_grow_factor, size_t young_max);
bool
wi_state_was_eof_error(wi_state* state);

void
wi_state_set_callbacks(struct wi_state* state, wi_print_fn out_fn, wi_print_fn error_fn,
                       wi_on_compile_fn on_compile_fn, wi_load_require_fn load_require_fn,
                       wi_require_exists_fn require_exists_fn);

void
wi_state_set_args(struct wi_state* state, int argc, const char** argv);

bool
wi_state_add_lib(struct wi_state* state, wi_lib_handle lib);
void
wi_state_close_libs_from(struct wi_state* state, struct wi_lib_node* from);
void
wi_state_close_libs(struct wi_state* state);

struct wi_recovery*
wi_state_push_recovery(struct wi_state* state);
void
wi_state_pop_recovery(struct wi_state* state);

WI_NORETURN void
wi_state_error(struct wi_state* state, const char* format, ...);
WI_NORETURN void
wi_state_oom(struct wi_state* state, const char* what);

WI_NORETURN void
wi_state_abort(struct wi_state* state);
void
wi_state_interrupt(struct wi_state* state);

/* ppush means protected push, calls _state_reserve_stack */
void
wi_state_ppush(struct wi_state* state, wi_value value);

WI_INLINE void
wi_state_check_arity(struct wi_state* state, int arity, uint8_t arg_count, bool is_variadic) {
    if (is_variadic) {
        if (WI_UNLIKELY(arg_count < arity)) {
            wi_state_error(state, "expected at least %i arguments but got %hhu", arity, arg_count);
        }
    } else if (WI_UNLIKELY(arg_count != arity)) {
        wi_state_error(state, "expected %i arguments but got %hhu", arity, arg_count);
    }
}

void
wi_state_call_foreign(struct wi_state* state, struct wi_foreign* foreign, uint8_t arg_count);
enum wi_run_result
wi_state_call(struct wi_state* state, wi_value callable, uint8_t arg_count, bool drop_result);

enum wi_run_result
wi_state_run(struct wi_state* state, const char* file_path, const char* src);

#endif
