#include "wi_utf8.h"

#include <stddef.h>

#include "../include/wi.h"

static void
_utf8_len(struct wi_state* state, int arg_count) {
    int   len;
    char* string   = wi_slot_check_string(state, 1, &len);
    int   utf8_len = 0;

    for (int i = 0; i < len; i++) {
        if ((string[i] & 0xC0) != 0x80) {
            utf8_len++;
        }
    }

    wi_slot_set_real(state, 0, utf8_len);
}

static void
_utf8_at(struct wi_state* state, int arg_count) {
    int   len;
    char* string = wi_slot_check_string(state, 1, &len);
    int   index  = (int)wi_slot_check_real(state, 2);
    int   count  = 0;
    int   i      = 0;

    while (i < len) {
        if ((string[i] & 0xC0) != 0x80) {
            if (count == index) {
                break;
            }

            count++;
        }

        i++;
    }

    if (i >= len || count != index) {
        wi_state_error(state, "string index out of range: %i", index);
        return;
    }

    size_t cp_len = 1;
    char   c      = string[i];

    if ((c & 0xE0) == 0xC0) {
        cp_len = 2;
    } else if ((c & 0xF0) == 0xE0) {
        cp_len = 3;
    } else if ((c & 0xF8) == 0xF0) {
        cp_len = 4;
    }

    if (i + (int)cp_len > len) {
        wi_state_error(state, "malformed utf-8 sequence at index %i", index);
        return;
    }

    char buf[5] = {0};
    memcpy(buf, string + i, cp_len);
    wi_slot_set_string(state, 0, buf);
}

void
wi_state_def_utf8_foreign(struct wi_state* state) {
    struct wi_object* object = wi_def_object(state, "utf8");

    wi_object_set_field_foreign(state, object, "len", _utf8_len, 1, false);
    wi_object_set_field_foreign(state, object, "at", _utf8_at, 2, false);
}
