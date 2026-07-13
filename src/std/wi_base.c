#include "wi_base.h"

#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "../core/wi_state.h"
#include "../include/wi.h"

static void
_base_print(wi_state_t* state, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        wi_value_print(state->api_stack[i + 1]);
        printf("\n");
    }

    wi_slot_set_null(state, 0);
}

static void
_base_input(wi_state_t* state, int arg_count) {
    char buf[2048];

    if (!fgets(buf, sizeof(buf), stdin)) {
        wi_slot_set_null(state, 0);
        return;
    }

    buf[strcspn(buf, "\n")] = 0;
    state->api_stack[0]     = WI_MAKE_BOX_VALUE(wi_copy_cstring(state->gc, buf, (int)strlen(buf)));
}

static void
_base_load_foreign(wi_state_t* state, int arg_count) {
    wi_call_frame_t* frame = wi_state_frame(state);

    if (frame->closure->is_required) {
        wi_state_error(state, "can only use load_foreign from the main script");
    }

    char*   path = wi_slot_check_string(state, 1);
    HMODULE lib  = LoadLibraryA(path);

    if (!lib) {
        DWORD error = GetLastError();
        char* msg   = NULL;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, (char*)&msg, 0,
                       NULL);

        fprintf(stderr, "load_foreign(): failed to load foreign %s: %s", path, msg ? msg : "unknown error");
        wi_state_print_backtrace(state);

        if (msg) {
            LocalFree(msg);
        }

        wi_slot_set_bool(state, 0, false);
        return;
    }

    typedef void (*wi_foreign_init_fn_t)(wi_state_t* state);
    wi_foreign_init_fn_t init = (wi_foreign_init_fn_t)GetProcAddress(lib, "wi_foreign_init");

    if (!init) {
        FreeLibrary(lib);
        fprintf(stderr, "load_foreign(): foreign %s does not export wi_foreign_init", path);
        wi_slot_set_bool(state, 0, false);
        return;
    }

    init(state);
    wi_state_add_foreign_handle(state, lib);
    wi_slot_set_bool(state, 0, true);
}

static void
_base_is_main(wi_state_t* state, int arg_count) {
    wi_call_frame_t* frame = wi_state_frame(state);
    wi_slot_set_bool(state, 0, !frame->closure->is_required);
}

typedef bool(_is_type_fn_t)(wi_value_t value);

static void
_is_type_function(wi_state_t* state, _is_type_fn_t fn) {
    wi_slot_set_bool(state, 0, fn(state->api_stack[1]));
}

static void
_base_is_real(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_real);
}

static void
_base_is_null(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_null);
}

static void
_base_is_bool(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_bool);
}

static void
_base_is_string(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_string);
}

static void
_base_is_array(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_array);
}

static void
_base_is_map(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_map);
}

static void
_base_is_foreign(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_foreign);
}

static void
_base_is_function(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_closure);
}

static void
_base_is_object(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_object);
}

static void
_base_is_userdata(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_userdata);
}

static void
_base_is_falsy(wi_state_t* state, int arg_count) {
    _is_type_function(state, wi_value_is_falsy);
}

static wi_object_t*
_check_arg1_object(wi_state_t* state) {
    if (!wi_value_is_object(state->api_stack[1])) {
        wi_state_error(state, "bad argument 1 - expected a value of type object but got %s",
                       wi_value_type(state->api_stack[1]));
    }

    return wi_value_as_object(state->api_stack[1]);
}

static void
_base_has_field(wi_state_t* state, int arg_count) {
    wi_object_t* object = _check_arg1_object(state);
    wi_slot_set_bool(state, 0, wi_table_get(&object->fields, state->api_stack[2], NULL));
}

void
wi_state_def_base_foreign(wi_state_t* state) {
    wi_state_def_foreign(state, "print", _base_print, -1);
    wi_state_def_foreign(state, "input", _base_input, 0);
    wi_state_def_foreign(state, "load_foreign", _base_load_foreign, 1);
    wi_state_def_foreign(state, "is_main", _base_is_main, 0);

    wi_state_def_foreign(state, "is_real", _base_is_real, 1);
    wi_state_def_foreign(state, "is_null", _base_is_null, 1);
    wi_state_def_foreign(state, "is_bool", _base_is_bool, 1);
    wi_state_def_foreign(state, "is_string", _base_is_string, 1);
    wi_state_def_foreign(state, "is_array", _base_is_array, 1);
    wi_state_def_foreign(state, "is_map", _base_is_map, 1);
    wi_state_def_foreign(state, "is_foreign", _base_is_foreign, 1);
    wi_state_def_foreign(state, "is_function", _base_is_function, 1);
    wi_state_def_foreign(state, "is_object", _base_is_object, 1);
    wi_state_def_foreign(state, "is_userdata", _base_is_userdata, 1);
    wi_state_def_foreign(state, "is_falsy", _base_is_falsy, 1);

    wi_state_def_foreign(state, "has_field", _base_has_field, 2);
}
