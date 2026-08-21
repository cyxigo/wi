#ifndef WI_BUF_H
#define WI_BUF_H

#include <stdint.h>
#include <stdlib.h> /* IWYU pragma: export */

#include "wi_util.h"

struct wi_gc;

void*
wi_gc_realloc(struct wi_gc* gc, void* ptr, size_t old_size, size_t new_size);

enum {
    WI_BUF_DEFAULT_CAPACITY = 8,
    WI_BUF_CAPACITY_FACTOR  = 2,
};

#define WI_GROW_CAPACITY(capacity) \
    ((capacity) < WI_BUF_DEFAULT_CAPACITY ? WI_BUF_DEFAULT_CAPACITY : (capacity) * WI_BUF_CAPACITY_FACTOR)

#define WI_DECL_BUF(type, name)                                                                       \
    struct wi_##name##_buf {                                                                          \
        struct wi_gc* gc;                                                                             \
        type*         data;                                                                           \
        int           capacity;                                                                       \
        int           count;                                                                          \
    };                                                                                                \
                                                                                                      \
    WI_INLINE void wi_##name##_buf_init(struct wi_##name##_buf* buf, struct wi_gc* gc) {              \
        buf->gc       = gc;                                                                           \
        buf->data     = NULL;                                                                         \
        buf->capacity = 0;                                                                            \
        buf->count    = 0;                                                                            \
    }                                                                                                 \
                                                                                                      \
    WI_INLINE void wi_##name##_buf_free(struct wi_##name##_buf* buf) {                                \
        wi_gc_realloc(buf->gc, buf->data, sizeof(type) * (size_t)buf->capacity, 0);                   \
        wi_##name##_buf_init(buf, buf->gc);                                                           \
    }                                                                                                 \
                                                                                                      \
    WI_INLINE void wi_##name##_buf_reserve(struct wi_##name##_buf* buf, int count) {                  \
        int needed = buf->count + count;                                                              \
                                                                                                      \
        if (WI_UNLIKELY(needed <= buf->capacity)) {                                                   \
            return;                                                                                   \
        }                                                                                             \
                                                                                                      \
        int old_capacity = buf->capacity;                                                             \
        buf->capacity    = needed;                                                                    \
        buf->data        = wi_gc_realloc(buf->gc, buf->data, sizeof(type) * (size_t)old_capacity,     \
                                         sizeof(type) * (size_t)buf->capacity);                       \
    }                                                                                                 \
                                                                                                      \
    WI_INLINE int wi_##name##_buf_add(struct wi_##name##_buf* buf, type item) {                       \
        if (WI_UNLIKELY(buf->count + 1 > buf->capacity)) {                                            \
            int old_capacity = buf->capacity;                                                         \
            buf->capacity    = WI_GROW_CAPACITY(buf->capacity);                                       \
            buf->data        = wi_gc_realloc(buf->gc, buf->data, sizeof(type) * (size_t)old_capacity, \
                                             sizeof(type) * (size_t)buf->capacity);                   \
        }                                                                                             \
                                                                                                      \
        buf->data[buf->count++] = item;                                                               \
        return buf->count - 1;                                                                        \
    }

WI_DECL_BUF(int, int)
WI_DECL_BUF(char, char)
WI_DECL_BUF(uint8_t, byte)

#endif
