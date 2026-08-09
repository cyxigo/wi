#ifdef _WIN32
#define strdup _strdup
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include "wi_util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

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
    DWORD   err      = 0;

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

    *line = strdup(buf);

    if (!*line) {
        return false;
    }

    return true;
#endif
}
