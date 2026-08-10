#include "wi_string.h"

#include <ctype.h>
#include <stddef.h>

#include "../include/wi.h"

static void
_string_sub(struct wi_state* state, int arg_count) {
    int   len;
    char* string = wi_slot_check_string(state, 1, &len, NULL);
    int   start  = (int)wi_slot_check_real(state, 2);
    int   end    = (int)wi_slot_check_real(state, 3);

    if (start < 0 || start > len || end < 0 || end > len || start > end) {
        wi_state_error(state, "string sub bounds out of range: %i to %i", start, end);
    }

    state->ffi_stack[0] = WI_MAKE_BOX_VALUE(wi_copy_cstring(state->gc, string + start, end - start));
}

static void
_string_upper(struct wi_state* state, int arg_count) {
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
    int   len;
    char* string = wi_slot_check_string(state, 1, &len, NULL);
    int   start  = 0;
    int   end    = len;

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
    int   len;
    char* string = wi_slot_check_string(state, 1, &len, NULL);
    int   target_len;
    char* target = wi_slot_check_string(state, 2, &target_len, NULL);
    bool  found  = target_len == 0;

    for (int i = 0; !found && i + target_len <= len; i++) {
        if (memcmp(string + i, target, (size_t)target_len) == 0) {
            found = true;
        }
    }

    wi_slot_set_bool(state, 0, found);
}

static void
_string_starts_with(struct wi_state* state, int arg_count) {
    int   len;
    char* string = wi_slot_check_string(state, 1, &len, NULL);
    int   prefix_len;
    char* prefix = wi_slot_check_string(state, 2, &prefix_len, NULL);
    bool  result = prefix_len <= len && memcmp(string, prefix, (size_t)prefix_len) == 0;

    wi_slot_set_bool(state, 0, result);
}

static void
_string_ends_with(struct wi_state* state, int arg_count) {
    int   len;
    char* string = wi_slot_check_string(state, 1, &len, NULL);
    int   suffix_len;
    char* suffix = wi_slot_check_string(state, 2, &suffix_len, NULL);
    bool  result = suffix_len <= len && memcmp(string + (len - suffix_len), suffix, (size_t)suffix_len) == 0;

    wi_slot_set_bool(state, 0, result);
}

static void
_string_replace(struct wi_state* state, int arg_count) {
    int   len;
    char* string = wi_slot_check_string(state, 1, &len, NULL);
    int   old_len;
    char* old = wi_slot_check_string(state, 2, &old_len, NULL);
    int   new_len;
    char* new = wi_slot_check_string(state, 3, &new_len, NULL);

    if (old_len == 0) {
        state->ffi_stack[0] = state->ffi_stack[1];
        return;
    }

    struct wi_char_buf result;
    wi_char_buf_init(&result, state->gc);

    int i = 0;

    while (i < len) {
        if (i + old_len <= len && memcmp(string + i, old, (size_t)old_len) == 0) {
            for (int j = 0; j < new_len; j++) {
                wi_char_buf_add(&result, new[j]);
            }

            i += old_len;
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
    int   len;
    char* string = wi_slot_check_string(state, 1, &len, NULL);
    int   sep_len;
    char* sep = wi_slot_check_string(state, 2, &sep_len, NULL);

    struct wi_array* result = wi_new_array(state->gc);
    state->ffi_stack[0]     = WI_MAKE_BOX_VALUE(result);

    if (sep_len == 0) {
        wi_value_buf_add(&result->items, state->ffi_stack[1]);
        return;
    }

    int start = 0;
    int i     = 0;

    while (i + sep_len <= len) {
        if (memcmp(string + i, sep, (size_t)sep_len) == 0) {
            struct wi_string* part = wi_copy_cstring(state->gc, string + start, i - start);

            WI_GC_PUSH_ROOT(state->gc, part);
            wi_value_buf_add(&result->items, WI_MAKE_BOX_VALUE(part));
            wi_gc_pop_root(state->gc);

            i += sep_len;
            start = i;
        } else {
            i++;
        }
    }

    struct wi_string* last = wi_copy_cstring(state->gc, string + start, len - start);

    WI_GC_PUSH_ROOT(state->gc, last);
    wi_value_buf_add(&result->items, WI_MAKE_BOX_VALUE(last));
    wi_gc_pop_root(state->gc);
}

static void
_string_reverse(struct wi_state* state, int arg_count) {
    int   len;
    char* string = wi_slot_check_string(state, 1, &len, NULL);
    char* buf    = WI_GC_ALLOC(state->gc, char, len + 1);

    for (int i = 0; i < len; i++) {
        buf[i] = string[len - 1 - i];
    }

    buf[len]            = '\0';
    state->ffi_stack[0] = WI_MAKE_BOX_VALUE(wi_take_cstring(state->gc, buf, len));
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
