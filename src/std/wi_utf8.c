#include "wi_utf8.h"

#include <stddef.h>

#include "../include/wi.h"

static wi_string_t*
_check_arg_string(wi_state_t* state, int arg) {
    if (!wi_value_is_string(state->api_stack[arg])) {
        wi_state_error(state, "bad argument %i - expected a value of type string but got %s", arg,
                       wi_value_type(state->api_stack[arg]));
    }

    return wi_value_as_string(state->api_stack[arg]);
}

static wi_string_t*
_check_arg1_string(wi_state_t* state) {
    return _check_arg_string(state, 1);
}

static void
_utf8_len(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    int          len    = 0;

    for (int i = 0; i < string->len; i++) {
        if ((string->chars[i] & 0xC0) != 0x80) {
            len++;
        }
    }

    wi_slot_set_real(state, 0, len);
}

static void
_utf8_at(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    int          index  = (int)wi_slot_check_real(state, 2);
    int          count  = 0;
    int          i      = 0;

    while (i < string->len) {
        if ((string->chars[i] & 0xC0) != 0x80) {
            if (count == index) {
                break;
            }

            count++;
        }

        i++;
    }

    if (i >= string->len || count != index) {
        wi_state_error(state, "string index out of range: %i", index);
        return;
    }

    size_t cp_len = 1;
    char   c      = string->chars[i];

    if ((c & 0xE0) == 0xC0) {
        cp_len = 2;
    } else if ((c & 0xF0) == 0xE0) {
        cp_len = 3;
    } else if ((c & 0xF8) == 0xF0) {
        cp_len = 4;
    }

    if (i + (int)cp_len > string->len) {
        wi_state_error(state, "malformed utf-8 sequence at index %i", index);
        return;
    }

    char buf[5] = {0};
    memcpy(buf, string->chars + i, cp_len);
    wi_slot_set_string(state, 0, buf);
}

void
wi_state_def_utf8_foreign(wi_state_t* state) {
    wi_object_t* object = wi_def_object(state, "utf8");

    wi_set_field_foreign(state, object, "len", _utf8_len, 1, false);
    wi_set_field_foreign(state, object, "at", _utf8_at, 2, false);
}
