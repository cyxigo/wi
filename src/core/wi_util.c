#include "wi_util.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

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

char*
wi_read_stream(FILE* stream) {
    fseek(stream, 0L, SEEK_END);
    long fsize = ftell(stream);
    rewind(stream);

    if (fsize < 0) {
        return NULL;
    }

    char* buf = malloc((size_t)fsize + 1);

    if (!buf) {
        return NULL;
    }

    size_t bytes_read = fread(buf, 1, (size_t)fsize, stream);

    if (bytes_read < fsize) {
        free(buf);
        return NULL;
    }

    buf[bytes_read] = '\0';
    return buf;
}

bool
wi_read_line(char** line) {
    /* remember what i said about platform-specific code? */
#ifdef _WIN32
    /* this mess wouldn't exist if windows api wasn't so complicated for NO reason!!! */
    HANDLE  hstdin = GetStdHandle(STD_INPUT_HANDLE);
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

    int   len = WideCharToMultiByte(CP_UTF8, 0, wbuf, (int)read, NULL, 0, NULL, NULL);
    char* buf = malloc((size_t)(len + 1));

    if (!buf) {
        return false;
    }

    WideCharToMultiByte(CP_UTF8, 0, wbuf, (int)read, buf, len, NULL, NULL);

    buf[len] = '\0';
    *line    = buf;

    return true;
#else
    char buf[2048];

    if (!fgets(buf, sizeof(buf), stdin)) {
        *line = NULL;
        return false;
    }

    *line = wi_strdup(buf);

    if (!*line) {
        return false;
    }

    return true;
#endif
}
