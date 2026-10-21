#ifndef _WIN32
    #define _POSIX_C_SOURCE 200809L
#endif

#include "wi_util.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <io.h>
    #include <windows.h>
#else
    #include <sys/stat.h>
    #include <unistd.h>
#endif /* _WIN32 */

#ifdef WI_USE_READLINE /* wi is linked with -lreadline */
    #include <readline/history.h>
    #include <readline/readline.h>
#endif /* WI_USE_READLINE */

#ifdef _WIN32
    #define _PATH_SEP '\\'
#else
    #define _PATH_SEP '/'
#endif

/*
    a bit modified vasprintf for wi needs
    e.g. returns char* instead of taking char** parameter and returning an int
    also creates a copy of args so it's safe to pass them directly
    and is a bit safer
*/
char*
wi_vasprintf(const char* format, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (WI_UNLIKELY(len < 0)) {
        return NULL;
    }

    char* buf = (char*)malloc((size_t)(len + 1));

    if (WI_UNLIKELY(!buf)) {
        return NULL;
    }

    va_copy(args_copy, args);
    int written = vsnprintf(buf, (size_t)(len + 1), format, args_copy);
    va_end(args_copy);

    if (written < 0) {
        free(buf);
        return NULL;
    }

    return buf;
}

/*
    a bit modified sprintf for wi needs
    e.g. returns char* instead of taking char** parameter and returning an int
    and is a bit safer (uses wi_vasprintf)
*/
char*
wi_sprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char* buf = wi_vasprintf(format, args);
    va_end(args);
    return buf;
}

/* a little wrapper around printing callbacks so we can pass va_list directly (uses wi_vasprintf) */
void
wi_vprintf(struct wi_state* state, wi_print_fn fn, const char* format, va_list args) {
    char* buf = wi_vasprintf(format, args);

    if (!buf) {
        return;
    }

    fn(state, buf);
    free(buf);
}

/* a little wrapper around printing callbacks so we can use them just like printf (uses wi_vprintf) */
void
wi_printf(struct wi_state* state, wi_print_fn fn, const char* format, ...) {
    va_list args;
    va_start(args, format);
    wi_vprintf(state, fn, format, args);
    va_end(args);
}

/*
    got tired from using tons of macros to provide strdup...
    so here's this little function thingy! plain as day, simple as it gets!
*/
char*
wi_strdup(const char* src) {
    size_t len = strlen(src) + 1;
    void* new  = malloc(len);

    if (!new) {
        return NULL;
    }

    return memcpy(new, src, len);
}

/*
    welcome to windows and utf-8 hell
    wi is utf-8 everywhere: strings, paths, env, argv, whatever the user hands us
    linux and macos take bytes directly so we just don't care
    windows has two sets of apis: "A" (fopen, getenv, CreateDirectoryA...) which read bytes in the current ANSI
    codepage (almost never utf-8), and "W" which take utf-16. give an "A" function utf-8 and "héllo.txt" turns
    into garbage, a different file, or nothing at all
    so the rule is simple:
    everything going TO windows (paths, names, commands) goes through wi_utf8_to_wide into a "W" function
    everything coming BACK (argv, env, cwd, dir entries, console input) goes through wi_wide_to_utf8
    the wrappers below hide these absolute horrors for the rest of wi
*/

#ifdef _WIN32
wchar_t*
wi_utf8_to_wide(const char* str) {
    int      len  = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    wchar_t* wstr = len > 0 ? (wchar_t*)malloc(sizeof(wchar_t) * (size_t)len) : NULL;

    if (wstr) {
        MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, len);
    }

    return wstr;
}

char*
wi_wide_to_utf8(const wchar_t* wstr) {
    int   len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* str = len > 0 ? (char*)malloc((size_t)len) : NULL;

    if (str) {
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
    }

    return str;
}
#endif /* _WIN32 */

bool
wi_file_exists(const char* path) {
#ifdef _WIN32
    wchar_t* wpath  = wi_utf8_to_wide(path);
    bool     exists = wpath && _waccess(wpath, 0) == 0;
    free(wpath);
    return exists;
#else
    return access(path, F_OK) == 0;
#endif /* _WIN32 */
}

FILE*
wi_fopen(const char* path, const char* mode) {
#ifdef _WIN32
    wchar_t  wmode[8] = {0};
    wchar_t* wpath    = wi_utf8_to_wide(path);
    FILE*    file     = NULL;

    /* modes are always ascii anyways, so we just convert them directly */
    for (int i = 0; i < 7 && mode[i]; i++) {
        wmode[i] = (wchar_t)mode[i];
    }

    if (wpath) {
        file = _wfopen(wpath, wmode);
    } else {
        errno = ENOMEM;
    }

    free(wpath);
    return file;
#else
    return fopen(path, mode);
#endif /* _WIN32 */
}

int
wi_remove(const char* path) {
#ifdef _WIN32
    wchar_t* wpath  = wi_utf8_to_wide(path);
    DWORD    attrs  = wpath ? GetFileAttributesW(wpath) : INVALID_FILE_ATTRIBUTES;
    bool     is_dir = attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    int      result = -1;

    /* unlike POSIX remove, _wremove can't delete directories, so we pick the right call ourselves */
    if (wpath) {
        result = (is_dir ? RemoveDirectoryW(wpath) : DeleteFileW(wpath)) ? 0 : -1;
    }

    free(wpath);
    return result;
#else
    return remove(path);
#endif /* _WIN32 */
}

int
wi_rename(const char* old, const char* new) {
#ifdef _WIN32
    wchar_t* wold = wi_utf8_to_wide(old);
    wchar_t* wnew = wi_utf8_to_wide(new);
    /* POSIX rename replaces the destination, _wrename refuses to, so we do some trickery again... */
    int result = wold && wnew && MoveFileExW(wold, wnew, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
    free(wold);
    free(wnew);
    return result;
#else
    return rename(old, new);
#endif /* _WIN32 */
}

bool
wi_is_dir(const char* path) {
#ifdef _WIN32
    wchar_t* wpath = wi_utf8_to_wide(path);
    DWORD    attrs = wpath ? GetFileAttributesW(wpath) : INVALID_FILE_ATTRIBUTES;
    free(wpath);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif /* _WIN32 */
}

bool
wi_mkdir(const char* path) {
#ifdef _WIN32
    wchar_t* wpath   = wi_utf8_to_wide(path);
    bool     created = wpath && CreateDirectoryW(wpath, NULL) != 0;
    free(wpath);
    return created;
#else
    return mkdir(path, 0777) == 0;
#endif /* _WIN32 */
}

int
wi_system(const char* command) {
#ifdef _WIN32
    wchar_t* wcommand = wi_utf8_to_wide(command);
    int      status   = wcommand ? _wsystem(wcommand) : -1;
    free(wcommand);
    return status;
#else
    return system(command);
#endif /* _WIN32 */
}

bool
wi_setenv(const char* name, const char* value, bool overwrite) {
#ifdef _WIN32
    wchar_t* wname  = wi_utf8_to_wide(name);
    wchar_t* wvalue = wi_utf8_to_wide(value);
    bool     result = false;

    if (wname && wvalue) {
        result = (!overwrite && GetEnvironmentVariableW(wname, NULL, 0) > 0) ||
                 SetEnvironmentVariableW(wname, wvalue) != 0;
    }

    free(wname);
    free(wvalue);
    return result;
#else
    return setenv(name, value, (int)overwrite) == 0;
#endif /* _WIN32 */
}

char*
wi_getenv(const char* name) {
#ifdef _WIN32
    wchar_t* wname  = wi_utf8_to_wide(name);
    DWORD    count  = wname ? GetEnvironmentVariableW(wname, NULL, 0) : 0;
    wchar_t* wvalue = count ? (wchar_t*)malloc(sizeof(wchar_t) * count) : NULL;
    char*    value  = NULL;

    if (wvalue && GetEnvironmentVariableW(wname, wvalue, count) < count) {
        value = wi_wide_to_utf8(wvalue);
    }

    free(wname);
    free(wvalue);
    return value;
#else
    const char* value = getenv(name);
    return value ? wi_strdup(value) : NULL;
#endif /* _WIN32 */
}

char*
wi_getcwd(void) {
#ifdef _WIN32
    wchar_t wbuf[WI_PATH_MAX];
    DWORD   count = GetCurrentDirectoryW(WI_PATH_MAX, wbuf);
    return count > 0 && count < WI_PATH_MAX ? wi_wide_to_utf8(wbuf) : NULL;
#else
    char buf[WI_PATH_MAX];
    return getcwd(buf, sizeof(buf)) ? wi_strdup(buf) : NULL;
#endif /* _WIN32 */
}

/* directory above the one the wi executable lives in (usually "bin"), NULL if it can't be located */
char*
wi_install_dir(void) {
    char* exe = NULL;
#ifdef _WIN32
    wchar_t wexe[WI_PATH_MAX];
    DWORD   count = GetModuleFileNameW(NULL, wexe, WI_PATH_MAX);

    if (count > 0 && count < WI_PATH_MAX) {
        exe = wi_wide_to_utf8(wexe);
    }
#elif defined(__linux__)
    char    buf[WI_PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buf, sizeof(buf) - 1);

    if (count > 0) {
        buf[count] = '\0';
        exe        = wi_strdup(buf);
    }
#endif /* _WIN32 */

    /* cut the executable name and its directory off */
    for (int i = 0; exe && i < 2; i++) {
        char* slash = strrchr(exe, _PATH_SEP);

        if (slash) {
            *slash = '\0';
        }
    }

    return exe;
}

/* returns a codepoint count from a byte count */
int
wi_utf8_len(const char* buf, int count) {
    int len = 0;

    for (int i = 0; i < count; i++) {
        /* not a continuation byte? -> a codepoint start */
        if ((buf[i] & 0xc0) != 0x80) {
            len++;
        }
    }

    return len;
}

/* converts a codepoint index into a byte index */
int
wi_utf8_cp_offset(const char* buf, int count, int cp_index) {
    int offset = 0;
    int cp     = 0;

    while (offset < count && cp < cp_index) {
        offset++; /* lead byte: 11xxxxxx (0xxxxxxx for ASCII) */

        /* continuation bytes (10xxxxxx) */
        while (offset < count && (buf[offset] & 0xc0) == 0x80) {
            offset++;
        }

        cp++;
    }

    return offset;
}

/* returns codepoint length (starting at [cp_start]) */
size_t
wi_utf8_cp_len(char cp_start) {
    size_t cp_len = 1; /* 0xxxxxxx */

    if ((cp_start & 0xe0) == 0xc0) { /* 110xxxxx */
        cp_len = 2;
    } else if ((cp_start & 0xf0) == 0xe0) { /* 1110xxxx */
        cp_len = 3;
    } else if ((cp_start & 0xf8) == 0xf0) { /* 11110xxx */
        cp_len = 4;
    }

    return cp_len;
}

bool
wi_utf8_validate(const char* buf, int count) {
    uint8_t* b   = (uint8_t*)buf;
    uint8_t* end = b + count;

    while (b < end) {
        if (*b < 0x80) {
            /* 0xxxxxxx */
            b++;
        } else if ((b[0] & 0xe0) == 0xc0) {
            /* 110xxxxx 10xxxxxx */
            if (b + 1 >= end) {
                return false;
            }

            if ((b[1] & 0xc0) != 0x80 || /* invalid continuation? */
                (b[0] & 0xfe) == 0xc0 /* overlong? */) {
                return false;
            }

            b += 2;
        } else if ((b[0] & 0xf0) == 0xe0) {
            /* 1110xxxx 10xxxxxx 10xxxxxx */
            if (b + 2 >= end) {
                return false;
            }

            if ((b[1] & 0xc0) != 0x80 || (b[2] & 0xc0) != 0x80 ||
                (b[0] == 0xe0 && (b[1] & 0xe0) == 0x80) || /* overlong? */
                (b[0] == 0xed && (b[1] & 0xe0) == 0xa0) || /* surrogate? */
                (b[0] == 0xef && b[1] == 0xbf && (b[2] & 0xfe) == 0xbe)) {
                return false;
            }

            b += 3;
        } else if ((b[0] & 0xf8) == 0xf0) {
            /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
            if (b + 3 >= end) {
                return false;
            }

            if ((b[1] & 0xc0) != 0x80 || (b[2] & 0xc0) != 0x80 || (b[3] & 0xc0) != 0x80 ||
                (b[0] == 0xf0 && (b[1] & 0xf0) == 0x80) || /* overlong? */
                (b[0] == 0xf4 && b[1] > 0x8f) || b[0] > 0xf4) /* > U+10FFFF? */ {
                return false;
            }

            b += 4;
        } else {
            /* invalid byte */
            return false;
        }
    }

    return true;
}

/* read a FILE* stream in chunks, supports pipes too! */
char*
wi_read_stream(FILE* stream, int* count) {
    int   capacity = 1024;
    int   size     = 0;
    char* buf      = (char*)malloc((size_t)capacity);

    if (!buf) {
        return NULL;
    }

    for (;;) {
        /* we only read capacity - 1 bytes, the last one is reserved for the NUL */
        size += (int)fread(buf + size, sizeof(char), (size_t)(capacity - size - 1), stream);

        /* did we read short? then it means either we hit the EOF or an error */
        if (size < capacity - 1) {
            break;
        }

        /* we double the capacity later, so we need to check half of INT_MAX here */
        if (capacity > INT_MAX / 2) {
            free(buf);
            errno = EFBIG;
            return NULL;
        }

        capacity *= 2;
        char* new_buf = (char*)realloc(buf, (size_t)capacity);

        if (!new_buf) {
            int err = errno;
            free(buf);
            errno = err;
            return NULL;
        }

        buf = new_buf;
    }

    if (ferror(stream)) {
        int err = errno;
        free(buf);
        errno = err;
        return NULL;
    }

    /* try to reallocate to the exact size so we don't waste memory */
    char* actual_buf = (char*)realloc(buf, (size_t)size + 1);

    if (actual_buf) {
        buf = actual_buf;
    }

    buf[size] = '\0';

    if (count) {
        *count = size;
    }

    return buf;
}

char*
wi_read_file(const char* path) {
    FILE* file = wi_fopen(path, "rb");

    if (!file) {
        return NULL;
    }

    char* buf = wi_read_stream(file, NULL);
    int   err = errno;
    fclose(file);
    errno = err;
    return buf;
}

#if defined(_WIN32) || !defined(WI_USE_READLINE)
static bool
_read_line_fgets(char** line) {
    char buf[2048];

    if (!fgets(buf, sizeof(buf), stdin)) {
        *line = NULL;
        return false;
    }

    *line = wi_strdup(buf);

    if (!*line) {
        return false;
    }

    (*line)[strcspn(*line, "\r\n")] = '\0';
    return true;
}
#endif /* _WIN32 || !WI_USE_READLINE */

/*
    this function is split into three versions depending on the platform and definitions:
    on windows: use ReadConsoleW and convert input in utf-16 to utf-8
    on linux: if WI_USE_READLINE is defined, we use the readline library and its features
              else - fallback to fgets (_read_line_fgets)
*/
bool
wi_read_line(char** line, const char* prompt) {
    /* remember what i said about platform-specific code? */
#ifdef _WIN32
    printf("%s", prompt);
    fflush(stdout);

    /* this mess wouldn't exist if windows api wasn't so complicated for NO reason!!! */
    HANDLE hstdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD  mode;

    /* piped/redirected stdin is not a console, ReadConsoleW will fail, use fgets! */
    if (!GetConsoleMode(hstdin, &mode)) {
        return _read_line_fgets(line);
    }

    wchar_t wbuf[2048];
    DWORD   wbuf_len = sizeof(wbuf) / sizeof(wbuf[0]) - 1;
    DWORD   read     = 0;

    if (!ReadConsoleW(hstdin, wbuf, wbuf_len, &read, NULL)) {
        *line = NULL;
        return false;
    }

    while (read > 0 && (wbuf[read - 1] == L'\n' || wbuf[read - 1] == L'\r')) {
        read--;
    }

    wbuf[read] = L'\0';
    *line      = wi_wide_to_utf8(wbuf);
    return *line != NULL;
#elif defined(WI_USE_READLINE)
    char* buf = readline(prompt);

    if (!buf) {
        return false;
    }

    if (strlen(buf) > 0) {
        add_history(buf);
    }

    *line = buf;
    return true;
#else
    printf("%s", prompt);
    fflush(stdout);
    return _read_line_fgets(line);
#endif /* _WIN32 || WI_USE_READLINE || else */
}
