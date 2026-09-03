#ifndef WI_UTIL_H
#define WI_UTIL_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../../include/wi.h"

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ > 4)
#define WI_NORETURN __attribute__((noreturn))
#define WI_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define WI_LIKELY(x) __builtin_expect(!!(x), 1)
#define WI_INLINE __attribute__((always_inline)) static inline
#define WI_UNREACHABLE() __builtin_unreachable()
#else
#define WI_NORETURN
#define WI_UNLIKELY(x) (x)
#define WI_LIKELY(x) (x)
#define WI_INLINE static inline
#define WI_UNREACHABLE()
#endif

#define WI_UNUSED(x) (void)x

char*
wi_vasprintf(const char* format, va_list args);
char*
wi_sprintf(const char* format, ...);
void
wi_vprintf(wi_print_fn print, const char* format, va_list args);

char*
wi_strdup(const char* src);

int
wi_utf8_len(const char* buf, int count);
int
wi_utf8_cp_offset(const char* buf, int count, int cp_index);
size_t
wi_utf8_cp_len(char cp_start);
bool
wi_utf8_validate(const char* buf, int count);

char*
wi_read_stream(FILE* stream);
bool
wi_read_line(char** line, const char* prompt);

#endif
