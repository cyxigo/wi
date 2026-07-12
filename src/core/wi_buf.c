#include "wi_buf.h"

#include "wi_gc.h"

WI_DEF_BUF(int, int)
WI_DEF_BUF(char, char)
WI_DEF_BUF(uint8_t, byte)

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
