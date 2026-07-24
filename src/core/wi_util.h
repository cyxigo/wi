#ifndef WI_UTIL_H
#define WI_UTIL_H

#include <stdio.h>

#define WI_UNLIKELY(x) __builtin_expect(!!(x), 0)

char*
wi_read_stream(FILE* stream);

#endif
