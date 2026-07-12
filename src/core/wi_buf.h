#ifndef WI_BUF_H
#define WI_BUF_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>  // IWYU pragma: keep

typedef struct wi_gc wi_gc_t;

enum {
    WI_BUF_DEFAULT_CAPACITY = 8,
    WI_BUF_CAPACITY_FACTOR  = 2,
};

#define WI_GROW_CAPACITY(capacity) \
    ((capacity) < WI_BUF_DEFAULT_CAPACITY ? WI_BUF_DEFAULT_CAPACITY : (capacity) * WI_BUF_CAPACITY_FACTOR)

#define WI_DECL_BUF(type, name)                                      \
    typedef struct wi_##name##_buf {                                 \
        wi_gc_t* gc;                                                 \
        type*    data;                                               \
        int      capacity;                                           \
        int      count;                                              \
    } wi_##name##_buf_t;                                             \
                                                                     \
    void wi_##name##_buf_init(wi_##name##_buf_t* buf, wi_gc_t* gc);  \
    void wi_##name##_buf_free(wi_##name##_buf_t* buf);               \
                                                                     \
    void wi_##name##_buf_reserve(wi_##name##_buf_t* buf, int count); \
    int  wi_##name##_buf_add(wi_##name##_buf_t* buf, type item);

#define WI_DEF_BUF(type, name)                                                                           \
    void wi_##name##_buf_init(wi_##name##_buf_t* buf, wi_gc_t* gc) {                                     \
        buf->gc       = gc;                                                                              \
        buf->data     = NULL;                                                                            \
        buf->capacity = 0;                                                                               \
        buf->count    = 0;                                                                               \
    }                                                                                                    \
                                                                                                         \
    void wi_##name##_buf_free(wi_##name##_buf_t* buf) {                                                  \
        WI_GC_FREE_ARRAY(buf->gc, type, buf->data, buf->capacity);                                       \
        wi_##name##_buf_init(buf, buf->gc);                                                              \
    }                                                                                                    \
                                                                                                         \
    void wi_##name##_buf_reserve(wi_##name##_buf_t* buf, int count) {                                    \
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
    int wi_##name##_buf_add(wi_##name##_buf_t* buf, type item) {                                         \
        if (buf->count + 1 > buf->capacity) {                                                            \
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

// yes, it's here. i have no idea where else to put it.
char*
wi_read_stream(FILE* stream);

#endif
