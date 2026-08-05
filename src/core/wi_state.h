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

#include "../include/wi_conf.h"
#include "wi_box.h"
#include "wi_gc.h"
#include "wi_table.h"
#include "wi_value.h"

static inline void
wi_lib_handle_close(wi_lib_handle lib) {
#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
}

enum {
#define WI_OPCODE(name, _, __) WI_OP_##name,
#include "wi_opcode.h"
#undef WI_OPCODE
};

struct wi_foreign_handle {
    struct wi_foreign_handle* next;
    wi_lib_handle             lib;
};

struct wi_call_frame {
    struct wi_closure* closure;
    uint8_t*           ip;
    wi_value*          slots;
};

/* mirrors wi_run_result */
enum {
    WI_JMP_OK    = 0,
    WI_JMP_ERROR = 1,
    WI_JMP_ABORT = 2,
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
    char* error;
    /*
        this is separated because... we are out of memory, what would we do? allocate MORE memory?
        no, instead we use this little static string thingy
    */
    const char* oom;

    wi_conf       conf;
    struct wi_gc* gc;

    wi_load_require_fn   load_require;
    wi_require_exists_fn require_exists;

    int          script_argc;
    const char** script_argv;

    jmp_buf               jmp;
    volatile sig_atomic_t interrupted;

    struct wi_recovery* recoveries;
    uint8_t             recovery_count;

    struct wi_call_frame* frames;
    int                   frame_capacity;
    int                   frame_count;
    uint8_t               c_call_depth;

    wi_value* stack;
    int       stack_capacity;
    wi_value* stack_end;
    wi_value* stack_top;
    wi_value* ffi_stack;

    struct wi_table    globals;
    struct wi_table    foreign;
    struct wi_table    required;
    struct wi_upvalue* open_upvalues;

    struct wi_foreign_handle* foreign_handles;

    struct wi_object* string_obj;
    struct wi_object* array_obj;
    struct wi_object* map_obj;

    /* these are used in `base.try` so we don't need to push 3 gc roots every time we need to call it */
    struct wi_string* ok_str;
    struct wi_string* value_str;
    struct wi_string* error_str;
};

static inline void
wi_state_push(struct wi_state* state, wi_value value) {
    *state->stack_top++ = value;
}

static inline void
wi_state_drop(struct wi_state* state) {
    state->stack_top--;
}

static inline wi_value
wi_state_pop(struct wi_state* state) {
    return *--state->stack_top;
}

static inline wi_value
wi_state_peek(struct wi_state* state, int distance) {
    return state->stack_top[-distance - 1];
}

static inline wi_value
wi_state_top(struct wi_state* state) {
    return wi_state_peek(state, 0);
}

static inline struct wi_call_frame*
wi_state_frame(struct wi_state* state) {
    return &state->frames[state->frame_count - 1];
}

struct wi_state*
wi_new_state(wi_conf conf);
void
wi_delete_state(struct wi_state* state);

static inline void
wi_state_reset_error(struct wi_state* state) {
    free(state->error);
    state->error = NULL;
    state->oom   = NULL;
}

void
wi_state_append_error_va(struct wi_state* state, const char* format, va_list args);

static inline void
wi_state_append_error(struct wi_state* state, const char* format, ...) {
    va_list args;
    va_start(args, format);
    wi_state_append_error_va(state, format, args);
    va_end(args);
}

const char*
wi_state_get_error(struct wi_state* state);

void
wi_state_set_require_load_fn(struct wi_state* state, wi_load_require_fn fn);
void
wi_state_set_require_exists_fn(struct wi_state* state, wi_require_exists_fn fn);

void
wi_state_set_args(struct wi_state* state, int argc, const char** argv);

bool
wi_state_add_foreign_handle(struct wi_state* state, wi_lib_handle lib);

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

void
wi_state_check_arity(struct wi_state* state, int arity, uint8_t arg_count, bool is_variadic);
void
wi_state_call_foreign(struct wi_state* state, struct wi_foreign* foreign, uint8_t arg_count);
enum wi_run_result
wi_state_call(struct wi_state* state, struct wi_closure* closure, uint8_t arg_count, bool drop_result);

enum wi_run_result
wi_state_run(struct wi_state* state, const char* file_path, const char* src);

/* we cannot really expose this to the public API, so.. it stays here. */
struct wi_closure*
wi_slot_check_function(struct wi_state* state, int slot, int arity);

#endif
