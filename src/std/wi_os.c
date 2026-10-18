#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#endif

#include "wi_os.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/wait.h>
#endif

#include "../../include/wi.h"

static void
_os_clock(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_push_real(state, (wi_real)clock() / (wi_real)CLOCKS_PER_SEC);
}

static void
_os_monotonic(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    wi_push_real(state, (wi_real)counter.QuadPart / (wi_real)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    wi_push_real(state, (wi_real)ts.tv_sec + (wi_real)ts.tv_nsec / 1e9);
#endif
}

static void
_os_time(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_push_real(state, (wi_real)time(NULL));
}

static void
_os_date(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    wi_real   time_real = wi_arg_real(state, 1);
    time_t    time      = (time_t)wi_state_real_to_int(state, time_real);
    struct tm tm;

#ifdef _WIN32
    if (gmtime_s(&tm, &time) != 0) {
        wi_state_error(state, "date out of range: " WI_REAL_FORMAT, time_real);
    }
#else
    if (!gmtime_r(&time, &tm)) {
        wi_state_error(state, "date out of range: " WI_REAL_FORMAT, time_real);
    }
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
_os_setenv(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* name      = wi_arg_string(state, 1, NULL, NULL);
    char* value     = wi_arg_string(state, 2, NULL, NULL);
    bool  overwrite = wi_arg_bool(state, 3);
    wi_push_bool(state, wi_setenv(name, value, overwrite));
}

static void
_os_getenv(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* value = wi_getenv(wi_arg_string(state, 1, NULL, NULL));

    if (!value) {
        wi_push_null(state);
        return;
    }

    wi_state_ppush_utf8(state, value, "environment variable");
}

static void
_os_args(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* result = wi_push_array(state);
    /* +1 since we also need to insert the main script path */
    wi_value_buf_reserve(&result->items, state->script_argc + 1);

    const char* path = state->main_module->path->buf;

    if (!wi_utf8_validate(path, (int)strlen(path))) {
        wi_state_error(state, "invalid utf-8 sequence in script path");
    }

    wi_push_string(state, path);
    wi_array_add(state, result);

    for (int i = 0; i < state->script_argc; i++) {
        const char* arg = state->script_argv[i];

        if (!wi_utf8_validate(arg, (int)strlen(arg))) {
            wi_state_error(state, "invalid utf-8 sequence in script argument %i", i);
        }

        wi_push_string(state, arg);
        wi_array_add(state, result);
    }
}

static void
_os_system(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* command = wi_arg_string(state, 1, NULL, NULL);
    int   status  = wi_system(command);

    /* on POSIX, system() returns a wait code instead of an exit code, we need the latter */
#ifndef _WIN32
    if (status != -1 && WIFEXITED(status)) {
        status = WEXITSTATUS(status);
    }
#endif

    wi_push_real(state, status);
}

static void
_os_remove(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* path = wi_arg_string(state, 1, NULL, NULL);
    wi_push_bool(state, wi_remove(path) == 0);
}

static void
_os_rename(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* old = wi_arg_string(state, 1, NULL, NULL);
    char* new = wi_arg_string(state, 2, NULL, NULL);
    wi_push_bool(state, wi_rename(old, new) == 0);
}

static void
_os_isdir(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* path = wi_arg_string(state, 1, NULL, NULL);
    wi_push_bool(state, wi_is_dir(path));
}

static void
_os_cwd(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* cwd = wi_getcwd();

    if (!cwd) {
        wi_state_error(state, "failed to get current working directory");
    }

    wi_state_ppush_utf8(state, cwd, "current working directory");
}

static void
_os_mkdir(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* path = wi_arg_string(state, 1, NULL, NULL);
    wi_push_bool(state, wi_mkdir(path));
}

static void
_os_listdir(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char*            path   = wi_arg_string(state, 1, NULL, NULL);
    struct wi_array* result = wi_push_array(state);

#ifdef _WIN32
    char*    pattern  = wi_sprintf("%s\\*", path);
    wchar_t* wpattern = pattern ? wi_utf8_to_wide(pattern) : NULL;
    free(pattern);

    if (!wpattern) {
        wi_state_oom(state, "failed to allocate a directory pattern (_os_listdir)");
    }

    WIN32_FIND_DATAW data;
    HANDLE           hfind = FindFirstFileW(wpattern, &data);
    DWORD            error = GetLastError();
    free(wpattern);

    if (hfind == INVALID_HANDLE_VALUE) {
        wi_state_error(state, "failed to list directory %s (error %lu)", path, error);
    }

    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }

        char* name = wi_wide_to_utf8(data.cFileName);

        if (!name) {
            FindClose(hfind);
            wi_state_oom(state, "failed to allocate a directory entry (_os_listdir)");
        }

        struct wi_string* box = wi_make_calloc_string(state->gc, name);
        wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
        wi_array_add(state, result);
    } while (FindNextFileW(hfind, &data));

    FindClose(hfind);
#else
    DIR* dir = opendir(path);

    if (!dir) {
        wi_state_error(state, "failed to list directory %s: %s", path, strerror(errno));
    }

    struct dirent* entry;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (!wi_utf8_validate(entry->d_name, (int)strlen(entry->d_name))) {
            closedir(dir);
            wi_state_error(state, "invalid utf-8 sequence in directory entry");
        }

        wi_push_string(state, entry->d_name);
        wi_array_add(state, result);
    }

    closedir(dir);
#endif
}

static void
_os_sleep(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int64_t ms = wi_state_real_to_int(state, wi_arg_real(state, 1));

    if (ms < 0) {
        wi_state_error(state, "sleep time must not be negative: %lld", ms);
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
    struct wi_module* module = wi_push_module(state, "os");
    wi_def(state, "os");
    wi_foreign_entry functions[] = {
        {"clock",     _os_clock,     0, false},
        {"monotonic", _os_monotonic, 0, false},
        {"time",      _os_time,      0, false},
        {"date",      _os_date,      1, false},
        {"setenv",    _os_setenv,    3, false},
        {"getenv",    _os_getenv,    1, false},
        {"args",      _os_args,      0, false},
        {"system",    _os_system,    1, false},
        {"remove",    _os_remove,    1, false},
        {"rename",    _os_rename,    2, false},
        {"isdir",     _os_isdir,     1, false},
        {"cwd",       _os_cwd,       0, false},
        {"mkdir",     _os_mkdir,     1, false},
        {"listdir",   _os_listdir,   1, false},
        {"sleep",     _os_sleep,     1, false},
    };
    WI_MODULE_EXPORT_FOREIGN_ALL(state, module, functions);
}
