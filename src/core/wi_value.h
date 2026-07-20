#ifndef WI_VALUE_H
#define WI_VALUE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../include/wi.h"
#include "wi_buf.h"

typedef struct wi_gc  wi_gc_t;
typedef struct wi_box wi_box_t;

#define WI_REAL_FORMAT "%.15g"
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

typedef uint64_t wi_value_t;

static inline wi_value_t
wi_make_real_value(wi_real_t real) {
    wi_value_t value;
    memcpy(&value, &real, sizeof(wi_real_t));
    return value;
}

static inline wi_value_t
wi_make_empty_value(void) {
    return WI_QNAN | WI_TAG_EMPTY;
}

static inline wi_value_t
wi_make_null_value(void) {
    return WI_QNAN | WI_TAG_NULL;
}

static inline wi_value_t
wi_make_true_value(void) {
    return WI_QNAN | WI_TAG_TRUE;
}

static inline wi_value_t
wi_make_false_value(void) {
    return WI_QNAN | WI_TAG_FALSE;
}

static inline wi_value_t
wi_make_bool_value(bool boolean) {
    return boolean ? wi_make_true_value() : wi_make_false_value();
}

static inline wi_value_t
wi_make_box_value(wi_box_t* box) {
    return WI_SIGN_BIT | WI_QNAN | (uintptr_t)box;
}

#define WI_MAKE_BOX_VALUE(box) wi_make_box_value((wi_box_t*)box)

static inline bool
wi_value_is_real(wi_value_t value) {
    return (value & WI_QNAN) != WI_QNAN;
}

static inline bool
wi_value_is_empty(wi_value_t value) {
    return value == wi_make_empty_value();
}

static inline bool
wi_value_is_null(wi_value_t value) {
    return value == wi_make_null_value();
}

static inline bool
wi_value_is_bool(wi_value_t value) {
    return value == wi_make_true_value() || value == wi_make_false_value();
}

static inline bool
wi_value_is_box(wi_value_t value) {
    return (value & (WI_QNAN | WI_SIGN_BIT)) == (WI_QNAN | WI_SIGN_BIT);
}

static inline wi_real_t
wi_value_as_real(wi_value_t value) {
    wi_real_t real;
    memcpy(&real, &value, sizeof(wi_value_t));
    return real;
}

static inline bool
wi_value_as_bool(wi_value_t value) {
    return value == wi_make_true_value();
}

static inline wi_box_t*
wi_value_as_box(wi_value_t value) {
    return (wi_box_t*)(value & ~(WI_SIGN_BIT | WI_QNAN));
}

static inline bool
wi_value_is_falsy(wi_value_t value) {
    return wi_value_is_null(value) || (wi_value_is_bool(value) && !wi_value_as_bool(value)) ||
           (wi_value_is_real(value) && wi_value_as_real(value) == 0.0);
}

static inline bool
wi_values_equal(wi_value_t a, wi_value_t b) {
    if (wi_value_is_real(a) && wi_value_is_real(b)) {
        return wi_value_as_real(a) == wi_value_as_real(b);
    }

    return a == b;
}

void
wi_value_print(wi_value_t value);
uint32_t
wi_value_hash(wi_value_t value);
const char*
wi_value_type(wi_value_t value);
char*
wi_value_to_string(wi_value_t value);

WI_DECL_BUF(wi_value_t, value)

wi_real_t
wi_string_to_real(const char* string, int len, char** end_ptr);

#endif
