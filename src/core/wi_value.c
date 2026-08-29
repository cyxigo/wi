#include "wi_value.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "wi_box.h"
#include "wi_buf.h"
#include "wi_gc.h" /* IWYU pragma: keep */
#include "wi_state.h"
#include "wi_table.h"
#include "wi_util.h"

static void
_print_bytes(struct wi_state* state, char* buf, int count) {
    char* ptr = buf;
    char* end = buf + count;

    while (ptr < end) {
        char* nul = memchr(ptr, '\0', (size_t)(end - ptr));

        if (!nul) {
            state->out("%.*s", end - ptr, ptr);
            return;
        }

        if (nul > ptr) {
            state->out("%.*s", nul - ptr, ptr);
        }

        state->out(" ");
        ptr = nul + 1;
    }
}

static void
_print_function(struct wi_state* state, struct wi_prototype* prototype) {
    if (prototype->is_main) {
        state->out("<main function %p (%s)>", (void*)prototype, prototype->file_path);
    } else {
        state->out("<function %p>", (void*)prototype);
    }
}

void
wi_value_print(struct wi_state* state, wi_value value) {
    if (wi_value_is_real(value)) {
        state->out(WI_REAL_FORMAT, wi_value_as_real(value));
    } else if (wi_value_is_null(value)) {
        state->out("null");
    } else if (wi_value_is_bool(value)) {
        state->out(wi_value_as_bool(value) ? "true" : "false");
    } else if (wi_value_is_string(value)) {
        struct wi_string* string = wi_value_as_string(value);
        _print_bytes(state, string->buf, string->count);
    } else if (wi_value_is_array(value)) {
        state->out("<array %p>", (void*)wi_value_as_array(value));
    } else if (wi_value_is_map(value)) {
        state->out("<map %p>", (void*)wi_value_as_map(value));
    } else if (wi_value_is_prototype(value)) {
        _print_function(state, wi_value_as_prototype(value));
    } else if (wi_value_is_foreign(value)) {
        state->out("<foreign %p>", (void*)wi_value_as_foreign(value));
    } else if (wi_value_is_closure(value)) {
        _print_function(state, wi_value_as_closure(value)->prototype);
    } else if (wi_value_is_upvalue(value)) {
        state->out("<upvalue %p>", (void*)wi_value_as_upvalue(value));
    } else if (wi_value_is_object(value)) {
        state->out("<object %p>", (void*)wi_value_as_object(value));
    } else if (wi_value_is_userdata(value)) {
        struct wi_userdata* userdata = wi_value_as_userdata(value);
        state->out("<%s %p>", userdata->name->buf, (void*)userdata);
    } else {
        state->out("<unknown>");
    }
}

static uint32_t
_real_hash(wi_real real) {
    uint64_t bits;
    memcpy(&bits, &real, sizeof(bits));

    /* -0.0 -> 0.0 */
    if (bits == 0x8000000000000000ull) {
        bits = 0;
    }

    bits ^= bits >> 30;
    bits *= 0xbf58476d1ce4e5b9ull;
    bits ^= bits >> 27;
    bits *= 0x94d049bb133111ebull;
    bits ^= bits >> 31;

    return (uint32_t)bits ^ (uint32_t)(bits >> 32);
}

uint32_t
wi_value_hash(wi_value value) {
    if (wi_value_is_real(value)) {
        return _real_hash(wi_value_as_real(value));
    }

    if (wi_value_is_box(value)) {
        struct wi_box* box = wi_value_as_box(value);

        if (WI_LIKELY(box->kind == WI_BOX_STRING)) {
            return ((struct wi_string*)box)->hash;
        }

        return (uint32_t)((uintptr_t)box >> 2);
    }

    if (WI_UNLIKELY(wi_value_is_null(value))) {
        return WI_NULL_HASH;
    }

    if (WI_UNLIKELY(wi_value_is_bool(value))) {
        return wi_value_as_bool(value) ? WI_TRUE_HASH : WI_FALSE_HASH;
    }

    return 0;
}

const char*
wi_value_type(wi_value value) {
    if (wi_value_is_real(value)) {
        return "real";
    }

    if (wi_value_is_null(value)) {
        return "null";
    }

    if (wi_value_is_bool(value)) {
        return "bool";
    }

    if (wi_value_is_string(value)) {
        return "string";
    }

    if (wi_value_is_array(value)) {
        return "array";
    }

    if (wi_value_is_map(value)) {
        return "map";
    }

    if (wi_value_is_prototype(value) || wi_value_is_closure(value)) {
        return "function";
    }

    if (wi_value_is_foreign(value)) {
        return "foreign";
    }

    if (wi_value_is_upvalue(value)) {
        return "upvalue";
    }

    if (wi_value_is_object(value)) {
        return "object";
    }

    if (wi_value_is_userdata(value)) {
        return wi_value_as_userdata(value)->name->buf;
    }

    return "unknown";
}

static char*
_function_to_string(struct wi_prototype* prototype) {
    if (prototype->is_main) {
        return wi_sprintf("<main function %p (%s)>", (void*)prototype, prototype->file_path);
    }

    return wi_sprintf("<function %p>", (void*)prototype);
}

char*
wi_value_to_string(wi_value value) {
    if (wi_value_is_real(value)) {
        return wi_sprintf(WI_REAL_FORMAT, wi_value_as_real(value));
    }

    if (wi_value_is_null(value)) {
        return wi_strdup("null");
    }

    if (wi_value_is_bool(value)) {
        return wi_strdup(wi_value_as_bool(value) ? "true" : "false");
    }

    if (wi_value_is_string(value)) {
        return wi_strdup(wi_value_as_cstring(value));
    }

    if (wi_value_is_array(value)) {
        return wi_sprintf("<array %p>", (void*)wi_value_as_array(value));
    }

    if (wi_value_is_map(value)) {
        return wi_sprintf("<map %p>", (void*)wi_value_as_map(value));
    }

    if (wi_value_is_prototype(value)) {
        return _function_to_string(wi_value_as_prototype(value));
    }

    if (wi_value_is_foreign(value)) {
        return wi_sprintf("<foreign %p>", (void*)wi_value_as_foreign(value));
    }

    if (wi_value_is_closure(value)) {
        return _function_to_string(wi_value_as_closure(value)->prototype);
    }

    if (wi_value_is_upvalue(value)) {
        return wi_sprintf("<upvalue %p>", (void*)wi_value_as_upvalue(value));
    }

    if (wi_value_is_object(value)) {
        return wi_sprintf("<object %p>", (void*)wi_value_as_object(value));
    }

    if (wi_value_is_userdata(value)) {
        struct wi_userdata* userdata = wi_value_as_userdata(value);
        return wi_sprintf("<%s %p>", userdata->name->buf, (void*)userdata);
    }

    return wi_strdup("<unknown>");
}

wi_real
wi_string_to_real(const char* string, int len, char** end_ptr) {
    if (len > 2 && string[0] == '0') {
        char c = string[1];

        if (c == 'x' || c == 'X') {
            return (wi_real)strtoll(string + 2, end_ptr, 16);
        }

        if (c == 'o' || c == 'O') {
            return (wi_real)strtoll(string + 2, end_ptr, 8);
        }

        if (c == 'b' || c == 'B') {
            return (wi_real)strtoll(string + 2, end_ptr, 2);
        }
    }

    return strtod(string, end_ptr);
}
