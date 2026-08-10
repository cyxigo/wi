#ifndef WI_UTIL_H
#define WI_UTIL_H

#include <stdbool.h>
#include <stdio.h>

#define WI_NORETURN __attribute__((noreturn))

#define WI_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define WI_LIKELY(x) __builtin_expect(!!(x), 1)

char*
wi_strdup(const char* src);
int
wi_utf8_len(const char* buf, int count);
int
wi_utf8_cp_offset(const char* buf, int count, int cp_index);

char*
wi_read_stream(FILE* stream);
bool
wi_read_line(char**);

#endif
