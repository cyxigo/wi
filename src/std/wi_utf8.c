#include "wi_utf8.h"

#include <stddef.h>

#include "../include/wi.h"

static void
_utf8_len(wi_state_t* state, int arg_count) {
    char* string = wi_slot_check_string(state, 1);
    int   len    = 0;

    while (*string) {
        if ((*string & 0xc0) != 0x80) {
            len++;
        }

        string++;
    }

    wi_slot_set_real(state, 0, len);
}

static void
_utf8_at(wi_state_t* state, int arg_count) {
    char* string = wi_slot_check_string(state, 1);
    int   index  = (int)wi_slot_check_real(state, 2);
    int   count  = 0;

    while (*string) {
        if ((*string & 0xC0) != 0x80) {
            if (count == index) {
                break;
            }

            count++;
        }

        string++;
    }

    if (!*string || count != index) {
        wi_state_error(state, "string index out of range: %i", index);
        return;
    }

    size_t cp_len = 1;

    if ((*string & 0xE0) == 0xC0) {
        cp_len = 2;
    } else if ((*string & 0xF0) == 0xE0) {
        cp_len = 3;
    } else if ((*string & 0xF8) == 0xF0) {
        cp_len = 4;
    }

    for (size_t i = 0; i < cp_len; i++) {
        if (string[i] == '\0') {
            wi_state_error(state, "malformed utf-8 sequence at index %i", index);
            return;
        }
    }

    char buf[5] = {0};
    memcpy(buf, string, cp_len);

    wi_slot_set_string(state, 0, buf);
}

void
wi_state_def_utf8_foreign(wi_state_t* state) {
    wi_object_t* object = wi_state_def_object(state, "utf8");

    wi_set_field_foreign(state, object, "len", _utf8_len, 1);
    wi_set_field_foreign(state, object, "at", _utf8_at, 2);
}
