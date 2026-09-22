#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#endif

#include "wi_os.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "../../include/wi.h"
#include "../core/wi_gc.h"

static void
_os_clock(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_push_real(state, (wi_real)clock() / (wi_real)CLOCKS_PER_SEC);
}

static void
_os_date(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    time_t    t = (time_t)wi_arg_real(state, 1);
    struct tm tm;

#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif

    struct wi_object* result = wi_push_object(state);

    wi_push_real(state, tm.tm_year + 1900);
    wi_object_set(state, result, "year");
    wi_push_real(state, tm.tm_mon + 1);
    wi_object_set(state, result, "month");
    wi_push_real(state, tm.tm_mday);
    wi_object_set(state, result, "day");
    wi_push_real(state, tm.tm_hour);
    wi_object_set(state, result, "hour");
    wi_push_real(state, tm.tm_min);
    wi_object_set(state, result, "minute");
    wi_push_real(state, tm.tm_sec);
    wi_object_set(state, result, "second");
    wi_push_real(state, tm.tm_wday);
    wi_object_set(state, result, "weekday");
}

static void
_os_time(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_push_real(state, (wi_real)time(NULL));
}

static void
_os_setenv(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* name      = wi_arg_string(state, 1, NULL, NULL);
    char* value     = wi_arg_string(state, 2, NULL, NULL);
    bool  overwrite = wi_arg_bool(state, 3);
    bool  result;

#ifdef _WIN32
    if (!overwrite && GetEnvironmentVariableA(name, NULL, 0) > 0) {
        result = true;
    } else {
        result = SetEnvironmentVariableA(name, value) != 0;
    }
#else
    result = setenv(name, value, (int)overwrite) == 0;
#endif

    wi_push_bool(state, result);
}

static void
_os_getenv(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* value = getenv(wi_arg_string(state, 1, NULL, NULL));

    if (!value) {
        wi_push_null(state);
        return;
    }

    if (!wi_utf8_validate(value, (int)strlen(value))) {
        wi_state_error(state, "invalid utf-8 sequence from os.getenv()");
    }

    wi_push_string(state, value);
}

static void
_os_args(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* result = wi_push_array(state);
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

static void
_os_sleep(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int64_t ms = wi_state_real_to_int(state, wi_arg_real(state, 1));

    if (ms < 0) {
        wi_state_error(state, "sleep time must be positive: %lld", ms);
    }

#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif /* _WIN32 */

    wi_push_null(state);
}

void
wi_state_def_std_os(struct wi_state* state) {
    struct wi_module* module = wi_push_module(state);
    wi_def(state, "os");
    wi_foreign_entry functions[] = {
        {"clock",  _os_clock,  0, false},
        {"date",   _os_date,   1, false},
        {"time",   _os_time,   0, false},
        {"setenv", _os_setenv, 3, false},
        {"getenv", _os_getenv, 1, false},
        {"args",   _os_args,   0, false},
        {"system", _os_system, 1, false},
        {"remove", _os_remove, 1, false},
        {"rename", _os_rename, 2, false},
        {"sleep",  _os_sleep,  1, false},
    };

    WI_MODULE_EXPORT_FOREIGN_ALL(state, module, functions);
}
