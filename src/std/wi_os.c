#include "wi_os.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/wi.h"
#include "../core/wi_gc.h"
#include "time.h"

static void
_os_clock(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_push_real(state, (wi_real)clock() / (wi_real)CLOCKS_PER_SEC);
}

static void
_os_time(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_push_real(state, (wi_real)time(NULL));
}

static void
_os_get_env(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* value = getenv(wi_arg_string(state, 1, NULL, NULL));

    if (!value) {
        wi_push_null(state);
        return;
    }

    if (!wi_utf8_validate(value, (int)strlen(value))) {
        wi_state_error(state, "invalid utf-8 sequence from os.get_env()");
    }

    wi_push_string(state, value);
}

static void
_os_args(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* result = wi_new_array(state->gc);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(result));
    wi_value_buf_reserve(&result->items, state->script_argc);

    for (int i = 0; i < state->script_argc; i++) {
        const char* arg = state->script_argv[i];

        if (!wi_utf8_validate(arg, (int)strlen(arg))) {
            wi_state_error(state, "invalid utf-8 sequence in script argument %i", i);
        }

        struct wi_string* arg_box = wi_make_string(state->gc, arg);

        WI_GC_PUSH_ROOT(state->gc, arg_box);
        wi_value_buf_add(&result->items, WI_MAKE_BOX_VALUE(arg_box));
        WI_GC_WRITE_BARRIER(state->gc, result, WI_MAKE_BOX_VALUE(arg_box));
        wi_gc_pop_root(state->gc);
    }
}

static void
_os_system(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* command = wi_arg_string(state, 1, NULL, NULL);
    wi_push_real(state, system(command));
}

static void
_os_remove(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* file_path = wi_arg_string(state, 1, NULL, NULL);
    wi_push_bool(state, remove(file_path) == 0);
}

static void
_os_rename(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* old = wi_arg_string(state, 1, NULL, NULL);
    char* new = wi_arg_string(state, 2, NULL, NULL);
    wi_push_bool(state, rename(old, new) == 0);
}

void
wi_state_def_std_os(struct wi_state* state) {
    struct wi_module* module = wi_push_module(state);
    wi_def(state, "os");
    wi_foreign_entry functions[] = {
        {"clock",   _os_clock,   0, false},
        {"time",    _os_time,    0, false},
        {"get_env", _os_get_env, 1, false},
        {"args",    _os_args,    0, false},
        {"system",  _os_system,  1, false},
        {"remove",  _os_remove,  1, false},
        {"rename",  _os_rename,  2, false},
    };

    WI_MODULE_EXPORT_FOREIGN_ALL(state, module, functions);
}
