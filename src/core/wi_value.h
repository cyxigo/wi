#ifndef WI_VALUE_H
#define WI_VALUE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../include/wi.h"
#include "wi_buf.h"

struct wi_box;

#define WI_QNAN 0x7ffc000000000000
#define WI_SIGN_BIT 0x8000000000000000

enum {
    WI_TAG_EMPTY = 3,
    WI_TAG_NULL  = 2,
    WI_TAG_TRUE  = 1,
    WI_TAG_FALSE = 0,

    WI_NULL_HASH  = 7,
    WI_TRUE_HASH  = 9,
    WI_FALSE_HASH = 11,
};

typedef uint64_t wi_value;

WI_INLINE wi_value
wi_make_real_value(wi_real real) {
    wi_value value;
    memcpy(&value, &real, sizeof(wi_real));
    return value;
}

WI_INLINE wi_value
wi_make_empty_value(void) {
    return WI_QNAN | WI_TAG_EMPTY;
}

WI_INLINE wi_value
wi_make_null_value(void) {
    return WI_QNAN | WI_TAG_NULL;
}

WI_INLINE wi_value
wi_make_true_value(void) {
    return WI_QNAN | WI_TAG_TRUE;
}

WI_INLINE wi_value
wi_make_false_value(void) {
    return WI_QNAN | WI_TAG_FALSE;
}

WI_INLINE wi_value
wi_make_bool_value(bool boolean) {
    return boolean ? wi_make_true_value() : wi_make_false_value();
}

WI_INLINE wi_value
wi_make_box_value(struct wi_box* box) {
    return WI_SIGN_BIT | WI_QNAN | (uintptr_t)box;
}

#define WI_MAKE_BOX_VALUE(box) wi_make_box_value((struct wi_box*)box)

WI_INLINE bool
wi_value_is_real(wi_value value) {
    return (value & WI_QNAN) != WI_QNAN;
}

WI_INLINE bool
wi_value_is_empty(wi_value value) {
    return value == wi_make_empty_value();
}

WI_INLINE bool
wi_value_is_null(wi_value value) {
    return value == wi_make_null_value();
}

WI_INLINE bool
wi_value_is_bool(wi_value value) {
    return (value | WI_TAG_TRUE) == wi_make_true_value();
}

WI_INLINE bool
wi_value_is_box(wi_value value) {
    return (value & (WI_QNAN | WI_SIGN_BIT)) == (WI_QNAN | WI_SIGN_BIT);
}

WI_INLINE wi_real
wi_value_as_real(wi_value value) {
    wi_real real;
    memcpy(&real, &value, sizeof(wi_value));
    return real;
}

WI_INLINE bool
wi_value_as_bool(wi_value value) {
    return value == wi_make_true_value();
}

WI_INLINE struct wi_box*
wi_value_as_box(wi_value value) {
    return (struct wi_box*)(value & ~(WI_SIGN_BIT | WI_QNAN));
}

WI_INLINE bool
wi_value_is_falsy(wi_value value) {
    return (wi_value_is_bool(value) && !wi_value_as_bool(value)) || wi_value_is_null(value) ||
           (wi_value_is_real(value) && wi_value_as_real(value) == 0.0);
}

WI_INLINE bool
wi_values_equal(wi_value a, wi_value b) {
    if (wi_value_is_real(a) && wi_value_is_real(b)) {
        return wi_value_as_real(a) == wi_value_as_real(b);
    }

    return a == b;
}

void
wi_value_print(struct wi_state* state, wi_value value);
uint32_t
wi_value_hash(wi_value value);
const char*
wi_value_type(wi_value value);
char*
wi_value_to_string(wi_value value);

WI_DECL_BUF(wi_value, value)

wi_real
wi_string_to_real(const char* string, int len, char** end_ptr);

#endif
