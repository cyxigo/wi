#ifndef WI_UTIL_H
#define WI_UTIL_H

#include <stdbool.h>
#include <stdio.h>

#define WI_NORETURN __attribute__((noreturn))

#define WI_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define WI_LIKELY(x) __builtin_expect(!!(x), 1)

char*
wi_read_stream(FILE* stream);
bool
wi_read_line(char** line);

#endif
