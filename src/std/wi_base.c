#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "wi_base.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

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

    char*  raw_path     = wi_slot_check_string(state, 1);
    size_t raw_path_len = strlen(raw_path);
    char   path[4096];  // i assume 4kb is enough for this mess.
    size_t path_size = sizeof(path);

    // platform specific code is a legitimate way of torturing.
#ifdef _WIN32
    DWORD len = GetModuleFileName(NULL, path, (DWORD)path_size);

    if (len < 1 || len >= path_size) {
        wi_state_error(state, "call to GetModuleFileName failed or path truncated");
    }

    char* last_slash = strrchr(path, '\\');

    if (last_slash) {
        *last_slash = '\0';
    }

    size_t path_len  = strlen(path);
    size_t remaining = path_size - path_len;

    // 14: '\foreign\' + '.dll' + '\0'
    if (remaining < 14 || raw_path_len > (remaining - 14)) {
        wi_state_error(state, "foreign path too long for fallback directory");
    }

    snprintf(path + path_len, remaining, "\\foreign\\%s.dll", raw_path);
    HMODULE lib = LoadLibraryA(path);
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", path, path_size - 1);

    if (len == -1) {
        wi_state_error(state, "call to readlink failed");
    }

    path[len] = '\0';

    char* last_slash = strrchr(path, '/');

    if (last_slash) {
        *last_slash = '\0';
    }

    size_t path_len  = strlen(path);
    size_t remaining = path_size - path_len;

    // 13: '/foreign/' + '.so' + '\0'
    if (remaining < 13 || raw_path_len > (remaining - 13)) {
        wi_state_error(state, "foreign path too long for fallback directory");
    }

    snprintf(path + path_len, remaining, "/foreign/%s.so", raw_path);
    void* lib = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
#else
#error platform not supported by load_foreign()
#endif

    if (!lib) {
        wi_state_error(state, "failed to load foreign %s\nattempted path: %s", raw_path, path);
    }

    typedef void (*_foreign_init_fn_t)(wi_state_t* state);

#ifdef _WIN32
    _foreign_init_fn_t init = (_foreign_init_fn_t)GetProcAddress(lib, "wi_foreign_init");
#else
    _foreign_init_fn_t init = (_foreign_init_fn_t)dlsym(lib, "wi_foreign_init");
#endif

    if (!init) {
        wi_lib_handle_close(lib);
        wi_state_error(state, "foreign %s does not export wi_foreign_init", raw_path);
    }

    if (wi_state_add_foreign_handle(state, lib)) {
        init(state);
    }

    wi_slot_set_null(state, 0);
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
    wi_def_foreign(state, "print", _base_print, -1);
    wi_def_foreign(state, "input", _base_input, 0);
    wi_def_foreign(state, "load_foreign", _base_load_foreign, 1);
    wi_def_foreign(state, "is_main", _base_is_main, 0);

    wi_def_foreign(state, "is_real", _base_is_real, 1);
    wi_def_foreign(state, "is_null", _base_is_null, 1);
    wi_def_foreign(state, "is_bool", _base_is_bool, 1);
    wi_def_foreign(state, "is_string", _base_is_string, 1);
    wi_def_foreign(state, "is_array", _base_is_array, 1);
    wi_def_foreign(state, "is_map", _base_is_map, 1);
    wi_def_foreign(state, "is_foreign", _base_is_foreign, 1);
    wi_def_foreign(state, "is_function", _base_is_function, 1);
    wi_def_foreign(state, "is_object", _base_is_object, 1);
    wi_def_foreign(state, "is_userdata", _base_is_userdata, 1);
    wi_def_foreign(state, "is_falsy", _base_is_falsy, 1);

    wi_def_foreign(state, "has_field", _base_has_field, 2);
}
