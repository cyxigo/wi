#include "wi_string.h"

#include <ctype.h>
#include <stddef.h>

#include "../include/wi.h"

static void
_string_sub(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    int   len;
    int   count;
    char* string = wi_slot_check_string(state, 1, &count, &len);
    int   start  = (int)wi_slot_check_real(state, 2);
    int   end    = (int)wi_slot_check_real(state, 3);

    if (start < 0 || start > len || end < 0 || end > len || start > end) {
        wi_state_error(state, "string sub bounds out of range: %i to %i", start, end);
    }

    int byte_start = wi_utf8_cp_offset(string, count, start);
    int byte_end   = wi_utf8_cp_offset(string, count, end);

    struct wi_string* result = wi_copy_cstring(state->gc, string + byte_start, byte_end - byte_start);
    state->ffi_stack[0]      = WI_MAKE_BOX_VALUE(result);
}

static void
_string_upper(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    int   len;
    char* string = wi_slot_check_string(state, 1, &len, NULL);
    char* buf    = WI_GC_ALLOC(state->gc, char, len + 1);

    for (int i = 0; i < len; i++) {
        buf[i] = (char)toupper(string[i]);
    }

    buf[len]            = '\0';
    state->ffi_stack[0] = WI_MAKE_BOX_VALUE(wi_take_cstring(state->gc, buf, len));
}

static void
_string_lower(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    int   len;
    char* string = wi_slot_check_string(state, 1, &len, NULL);
    char* buf    = WI_GC_ALLOC(state->gc, char, len + 1);

    for (int i = 0; i < len; i++) {
        buf[i] = (char)tolower(string[i]);
    }

    buf[len]            = '\0';
    state->ffi_stack[0] = WI_MAKE_BOX_VALUE(wi_take_cstring(state->gc, buf, len));
}

static void
_string_trim(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_slot_check_string(state, 1, &count, NULL);
    int   start  = 0;
    int   end    = count;

    while (start < end && isspace(string[start])) {
        start++;
    }

    while (end > start && isspace(string[end - 1])) {
        end--;
    }

    state->ffi_stack[0] = WI_MAKE_BOX_VALUE(wi_copy_cstring(state->gc, string + start, end - start));
}

static void
_string_has(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_slot_check_string(state, 1, &count, NULL);
    int   target_count;
    char* target = wi_slot_check_string(state, 2, &target_count, NULL);
    bool  found  = target_count == 0;

    for (int i = 0; !found && i + target_count <= count; i++) {
        if (memcmp(string + i, target, (size_t)target_count) == 0) {
            found = true;
        }
    }

    wi_slot_set_bool(state, 0, found);
}

static void
_string_starts_with(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_slot_check_string(state, 1, &count, NULL);
    int   pref_count;
    char* pref   = wi_slot_check_string(state, 2, &pref_count, NULL);
    bool  result = pref_count <= count && memcmp(string, pref, (size_t)pref_count) == 0;

    wi_slot_set_bool(state, 0, result);
}

static void
_string_ends_with(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_slot_check_string(state, 1, &count, NULL);
    int   suff_count;
    char* suff   = wi_slot_check_string(state, 2, &suff_count, NULL);
    bool  result = suff_count <= count && memcmp(string + (count - suff_count), suff, (size_t)suff_count) == 0;

    wi_slot_set_bool(state, 0, result);
}

static void
_string_replace(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_slot_check_string(state, 1, &count, NULL);
    int   old_count;
    char* old = wi_slot_check_string(state, 2, &old_count, NULL);
    int   new_count;
    char* new = wi_slot_check_string(state, 3, &new_count, NULL);

    if (old_count == 0) {
        state->ffi_stack[0] = state->ffi_stack[1];
        return;
    }

    struct wi_char_buf result;
    wi_char_buf_init(&result, state->gc);

    int i = 0;

    while (i < count) {
        if (i + old_count <= count && memcmp(string + i, old, (size_t)old_count) == 0) {
            for (int j = 0; j < new_count; j++) {
                wi_char_buf_add(&result, new[j]);
            }

            i += old_count;
        } else {
            wi_char_buf_add(&result, string[i]);
            i++;
        }
    }

    struct wi_string* replaced = wi_copy_cstring(state->gc, result.data, result.count);
    wi_char_buf_free(&result);

    state->ffi_stack[0] = WI_MAKE_BOX_VALUE(replaced);
}

static void
_string_split(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_slot_check_string(state, 1, &count, NULL);
    int   sep_count;
    char* sep = wi_slot_check_string(state, 2, &sep_count, NULL);

    struct wi_array* result = wi_new_array(state->gc);
    state->ffi_stack[0]     = WI_MAKE_BOX_VALUE(result);

    if (sep_count == 0) {
        wi_value_buf_add(&result->items, state->ffi_stack[1]);
        return;
    }

    int start = 0;
    int i     = 0;

    while (i + sep_count <= count) {
        if (memcmp(string + i, sep, (size_t)sep_count) == 0) {
            struct wi_string* part = wi_copy_cstring(state->gc, string + start, i - start);

            WI_GC_PUSH_ROOT(state->gc, part);
            wi_value_buf_add(&result->items, WI_MAKE_BOX_VALUE(part));
            wi_gc_pop_root(state->gc);

            i += sep_count;
            start = i;
        } else {
            i++;
        }
    }

    struct wi_string* last = wi_copy_cstring(state->gc, string + start, count - start);

    WI_GC_PUSH_ROOT(state->gc, last);
    wi_value_buf_add(&result->items, WI_MAKE_BOX_VALUE(last));
    wi_gc_pop_root(state->gc);
}

static void
_reverse_bytes(char* start, char* end) {
    while (start < end) {
        char c   = *start;
        *start++ = *end;
        *end--   = c;
    }
}

static char*
_reverse_cp_bytes(char* start, char* buf_end) {
    char* end = start;

    while (end + 1 < buf_end && (end[1] & 0xc0) == 0x80) {
        end++;
    }

    _reverse_bytes(start, end);
    return end + 1;
}

static void
_string_reverse(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_slot_check_string(state, 1, &count, NULL);
    char* buf    = WI_GC_ALLOC(state->gc, char, count + 1);

    memcpy(buf, string, (size_t)count);
    buf[count] = '\0';

    if (count == 0) {
        goto end;
    }

    /*
        so how this thingy works:
        first pass - we reverse each codepoint bytes
        second pass - we reverse the whole string
        see how first pass is synced with second pass? yep! and we get a perfectly
        fine reversed utf-8 string
    */
    char* buf_end = buf + count;
    char* pos     = buf;

    while (pos < buf_end) {
        pos = _reverse_cp_bytes(pos, buf_end);
    }

    _reverse_bytes(buf, buf_end - 1);

end:
    state->ffi_stack[0] = WI_MAKE_BOX_VALUE(wi_take_cstring(state->gc, buf, count));
}

void
wi_state_def_string_foreign(struct wi_state* state) {
    struct wi_object* object = wi_def_object(state, "string");

    wi_object_set_field_foreign(state, object, "sub", _string_sub, 3, false);
    wi_object_set_field_foreign(state, object, "upper", _string_upper, 1, false);
    wi_object_set_field_foreign(state, object, "lower", _string_lower, 1, false);
    wi_object_set_field_foreign(state, object, "trim", _string_trim, 1, false);
    wi_object_set_field_foreign(state, object, "has", _string_has, 2, false);
    wi_object_set_field_foreign(state, object, "starts_with", _string_starts_with, 2, false);
    wi_object_set_field_foreign(state, object, "ends_with", _string_ends_with, 2, false);
    wi_object_set_field_foreign(state, object, "replace", _string_replace, 3, false);
    wi_object_set_field_foreign(state, object, "split", _string_split, 2, false);
    wi_object_set_field_foreign(state, object, "reverse", _string_reverse, 1, false);

    state->string_obj = object;
}
