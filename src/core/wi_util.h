#ifndef WI_UTIL_H
#define WI_UTIL_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../../include/wi.h"

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ > 4)
    #define WI_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define WI_LIKELY(x) __builtin_expect(!!(x), 1)
    #define WI_INLINE __attribute__((always_inline)) static inline
    #define WI_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    #define WI_UNLIKELY(x) (x)
    #define WI_LIKELY(x) (x)
    #define WI_INLINE static __forceinline
    #define WI_UNREACHABLE() __assume(0)
#else
    #define WI_UNLIKELY(x) (x)
    #define WI_LIKELY(x) (x)
    #define WI_INLINE static inline
    #define WI_UNREACHABLE()
#endif /* __clang__ || (__GNUC__ && __GNUC__ > 4) || _MSC_VER || else */

#define WI_UNUSED(x) (void)x
#define WI_PATH_MAX 4096

/*
    splitmix64 finalizer, mixes all 64 bits of [x]
    funny numbers you throwin around man
*/
WI_INLINE uint64_t
wi_mix64(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

char*
wi_vasprintf(const char* format, va_list args);
char*
wi_sprintf(const char* format, ...);
void
wi_vprintf(struct wi_state* state, wi_print_fn print, const char* format, va_list args);
void
wi_printf(struct wi_state* state, wi_print_fn fn, const char* format, ...);

char*
wi_strdup(const char* src);

/* windows only utf-8 <-> utf-16 shenanigans */
#ifdef _WIN32
wchar_t*
wi_utf8_to_wide(const char* str);
char*
wi_wide_to_utf8(const wchar_t* wstr);
#endif

/* platform-specific implementations of useful io/os functions */
bool
wi_file_exists(const char* path);
FILE*
wi_fopen(const char* path, const char* mode);
int
wi_remove(const char* path);
int
wi_rename(const char* old, const char* new);
bool
wi_is_dir(const char* path);
bool
wi_mkdir(const char* path);
int
wi_system(const char* command);
bool
wi_setenv(const char* name, const char* value, bool overwrite);
char*
wi_getenv(const char* name);
char*
wi_getcwd(void);
char*
wi_install_dir(void);

int
wi_utf8_len(const char* buf, int count);
int
wi_utf8_cp_offset(const char* buf, int count, int cp_index);
size_t
wi_utf8_cp_len(char cp_start);
bool
wi_utf8_validate(const char* buf, int count);

char*
wi_read_stream(FILE* stream, int* count);
char*
wi_read_file(const char* path);
bool
wi_read_line(char** line, const char* prompt);

#endif
