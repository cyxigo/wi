#ifndef WI_BUF_H
#define WI_BUF_H

#include <stdint.h>
#include <stdlib.h> /* IWYU pragma: export */

#include "wi_util.h"

struct wi_gc;

enum {
    WI_BUF_DEFAULT_CAPACITY = 8,
    WI_BUF_CAPACITY_FACTOR  = 2,
};

#define WI_GROW_CAPACITY(capacity) \
    ((capacity) < WI_BUF_DEFAULT_CAPACITY ? WI_BUF_DEFAULT_CAPACITY : (capacity) * WI_BUF_CAPACITY_FACTOR)

#define WI_DECL_BUF(type, name)                                               \
    struct wi_##name##_buf {                                                  \
        struct wi_gc* gc;                                                     \
        type*         data;                                                   \
        int           capacity;                                               \
        int           count;                                                  \
    };                                                                        \
                                                                              \
    void wi_##name##_buf_init(struct wi_##name##_buf* buf, struct wi_gc* gc); \
    void wi_##name##_buf_free(struct wi_##name##_buf* buf);                   \
                                                                              \
    void wi_##name##_buf_reserve(struct wi_##name##_buf* buf, int count);     \
    int  wi_##name##_buf_add(struct wi_##name##_buf* buf, type item);

#define WI_DEF_BUF(type, name)                                                                           \
    void wi_##name##_buf_init(struct wi_##name##_buf* buf, struct wi_gc* gc) {                           \
        buf->gc       = gc;                                                                              \
        buf->data     = NULL;                                                                            \
        buf->capacity = 0;                                                                               \
        buf->count    = 0;                                                                               \
    }                                                                                                    \
                                                                                                         \
    void wi_##name##_buf_free(struct wi_##name##_buf* buf) {                                             \
        WI_GC_FREE_BUF(buf->gc, type, buf->data, buf->capacity);                                         \
        wi_##name##_buf_init(buf, buf->gc);                                                              \
    }                                                                                                    \
                                                                                                         \
    void wi_##name##_buf_reserve(struct wi_##name##_buf* buf, int count) {                               \
        int needed = buf->count + count;                                                                 \
                                                                                                         \
        if (needed <= buf->capacity) {                                                                   \
            return;                                                                                      \
        }                                                                                                \
                                                                                                         \
        int old_capacity = buf->capacity;                                                                \
        buf->capacity    = needed;                                                                       \
        buf->data        = WI_GC_ALLOC_ARRAY(buf->gc, type, buf->data, old_capacity, buf->capacity);     \
    }                                                                                                    \
                                                                                                         \
    int wi_##name##_buf_add(struct wi_##name##_buf* buf, type item) {                                    \
        if (WI_UNLIKELY(buf->count + 1 > buf->capacity)) {                                               \
            int old_capacity = buf->capacity;                                                            \
            buf->capacity    = WI_GROW_CAPACITY(buf->capacity);                                          \
            buf->data        = WI_GC_ALLOC_ARRAY(buf->gc, type, buf->data, old_capacity, buf->capacity); \
        }                                                                                                \
                                                                                                         \
        buf->data[buf->count++] = item;                                                                  \
        return buf->count - 1;                                                                           \
    }

WI_DECL_BUF(int, int)
WI_DECL_BUF(char, char)
WI_DECL_BUF(uint8_t, byte)

#endif
