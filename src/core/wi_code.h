#ifndef WI_CODE_H
#define WI_CODE_H

#include <stdint.h>
#include <string.h>

#include "wi_buf.h"

struct wi_code {
    struct wi_byte_buf bytes;
    struct wi_int_buf  lines;
};

WI_INLINE void
wi_code_init(struct wi_code* code, struct wi_gc* gc) {
    wi_byte_buf_init(&code->bytes, gc);
    wi_int_buf_init(&code->lines, gc);
}

WI_INLINE void
wi_code_free(struct wi_code* code) {
    wi_byte_buf_free(&code->bytes);
    wi_int_buf_free(&code->lines);
}

WI_INLINE void
wi_code_add(struct wi_code* code, uint8_t byte, int line) {
    wi_byte_buf_add(&code->bytes, byte);
    wi_int_buf_add(&code->lines, line);
}

WI_INLINE void
wi_code_append(struct wi_code* src, struct wi_code* dest) {
    if (src->bytes.count == 0 || src == dest) {
        return;
    }

    wi_byte_buf_reserve(&dest->bytes, src->bytes.count);
    memcpy(dest->bytes.data + dest->bytes.count, src->bytes.data, (size_t)src->bytes.count);
    dest->bytes.count += src->bytes.count;

    wi_int_buf_reserve(&dest->lines, src->lines.count);
    memcpy(dest->lines.data + dest->lines.count, src->lines.data, sizeof(int) * (size_t)src->lines.count);
    dest->lines.count += src->lines.count;
}

#endif
