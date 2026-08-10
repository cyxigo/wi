#include "wi_os.h"

#include <stdlib.h>

#include "../include/wi.h"
#include "time.h"

static void
_os_clock(struct wi_state* state, int arg_count) {
    wi_slot_set_real(state, 0, (wi_real)clock() / (wi_real)CLOCKS_PER_SEC);
}

static void
_os_time(struct wi_state* state, int arg_count) {
    wi_slot_set_real(state, 0, (wi_real)time(NULL));
}

static void
_os_get_env(struct wi_state* state, int arg_count) {
    char* value = getenv(wi_slot_check_string(state, 1, NULL, NULL));

    if (!value) {
        wi_slot_set_null(state, 0);
        return;
    }

    if (!wi_utf8_validate(value, (int)strlen(value))) {
        wi_state_error(state, "invalid utf-8 sequence from os.get_env()");
    }

    wi_slot_set_string(state, 0, value);
}

static void
_os_args(struct wi_state* state, int arg_count) {
    struct wi_array* result = wi_new_array(state->gc);
    state->ffi_stack[0]     = WI_MAKE_BOX_VALUE(result);
    wi_value_buf_reserve(&result->items, state->script_argc);

    for (int i = 0; i < state->script_argc; i++) {
        const char* arg = state->script_argv[i];

        if (!wi_utf8_validate(arg, (int)strlen(arg))) {
            wi_state_error(state, "invalid utf-8 sequence in script argument %i", i);
        }

        struct wi_string* arg_box = wi_make_string(state->gc, arg);

        WI_GC_PUSH_ROOT(state->gc, arg_box);
        wi_value_buf_add(&result->items, WI_MAKE_BOX_VALUE(arg_box));
        wi_gc_pop_root(state->gc);
    }
}

void
wi_state_def_os_foreign(struct wi_state* state) {
    struct wi_object* object = wi_def_object(state, "os");

    wi_object_set_field_foreign(state, object, "clock", _os_clock, 0, false);
    wi_object_set_field_foreign(state, object, "time", _os_time, 0, false);
    wi_object_set_field_foreign(state, object, "get_env", _os_get_env, 1, false);
    wi_object_set_field_foreign(state, object, "args", _os_args, 0, false);
}
