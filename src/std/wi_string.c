#include "wi_string.h"

#include <ctype.h>
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

static wi_string_t*
_check_arg2_string(wi_state_t* state) {
    return _check_arg_string(state, 2);
}

static wi_string_t*
_check_arg3_string(wi_state_t* state) {
    return _check_arg_string(state, 3);
}

static void
_string_sub(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    int          start  = (int)wi_slot_check_real(state, 2);
    int          end    = (int)wi_slot_check_real(state, 3);

    if (start < 0 || start > string->len || end < 0 || end > string->len || start > end) {
        wi_state_error(state, "string sub bounds out of range: %i to %i", start, end);
    }

    state->api_stack[0] = WI_MAKE_BOX_VALUE(wi_copy_cstring(state->gc, string->chars + start, end - start));
}

static void
_string_upper(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    char*        buf    = WI_GC_ALLOC(state->gc, char, string->len + 1);

    for (int i = 0; i < string->len; i++) {
        buf[i] = (char)toupper(string->chars[i]);
    }

    buf[string->len]    = '\0';
    state->api_stack[0] = WI_MAKE_BOX_VALUE(wi_take_cstring(state->gc, buf, string->len));
}

static void
_string_lower(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    char*        buf    = WI_GC_ALLOC(state->gc, char, string->len + 1);

    for (int i = 0; i < string->len; i++) {
        buf[i] = (char)tolower(string->chars[i]);
    }

    buf[string->len]    = '\0';
    state->api_stack[0] = WI_MAKE_BOX_VALUE(wi_take_cstring(state->gc, buf, string->len));
}

static void
_string_trim(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    int          start  = 0;
    int          end    = string->len;

    while (start < end && isspace(string->chars[start])) {
        start++;
    }

    while (end > start && isspace(string->chars[end - 1])) {
        end--;
    }

    state->api_stack[0] = WI_MAKE_BOX_VALUE(wi_copy_cstring(state->gc, string->chars + start, end - start));
}

static void
_string_has(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    wi_string_t* target = _check_arg2_string(state);
    bool         found  = target->len == 0;

    for (int i = 0; !found && i + target->len <= string->len; i++) {
        if (memcmp(string->chars + i, target->chars, (size_t)target->len) == 0) {
            found = true;
        }
    }

    wi_slot_set_bool(state, 0, found);
}

static void
_string_starts_with(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    wi_string_t* prefix = _check_arg2_string(state);
    bool result = prefix->len <= string->len && memcmp(string->chars, prefix->chars, (size_t)prefix->len) == 0;

    wi_slot_set_bool(state, 0, result);
}

static void
_string_ends_with(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    wi_string_t* suffix = _check_arg2_string(state);
    bool         result = suffix->len <= string->len &&
                  memcmp(string->chars + (string->len - suffix->len), suffix->chars, (size_t)suffix->len) == 0;

    wi_slot_set_bool(state, 0, result);
}

static void
_string_replace(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    wi_string_t* old    = _check_arg2_string(state);
    wi_string_t* new    = _check_arg3_string(state);

    if (old->len == 0) {
        state->api_stack[0] = WI_MAKE_BOX_VALUE(string);
        return;
    }

    wi_char_buf_t result;
    wi_char_buf_init(&result, state->gc);

    int i = 0;

    while (i < string->len) {
        if (i + old->len <= string->len && memcmp(string->chars + i, old->chars, (size_t)old->len) == 0) {
            for (int j = 0; j < new->len; j++) {
                wi_char_buf_add(&result, new->chars[j]);
            }

            i += old->len;
        } else {
            wi_char_buf_add(&result, string->chars[i]);
            i++;
        }
    }

    wi_string_t* replaced = wi_copy_cstring(state->gc, result.data, result.count);
    wi_char_buf_free(&result);

    state->api_stack[0] = WI_MAKE_BOX_VALUE(replaced);
}

static void
_string_split(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    wi_string_t* sep    = _check_arg2_string(state);

    wi_array_t* result  = wi_new_array(state->gc);
    state->api_stack[0] = WI_MAKE_BOX_VALUE(result);

    if (sep->len == 0) {
        wi_value_buf_add(&result->items, WI_MAKE_BOX_VALUE(string));
        return;
    }

    int start = 0;
    int i     = 0;

    while (i + sep->len <= string->len) {
        if (memcmp(string->chars + i, sep->chars, (size_t)sep->len) == 0) {
            wi_string_t* part = wi_copy_cstring(state->gc, string->chars + start, i - start);

            wi_gc_push_root(state->gc, (wi_box_t*)part);
            wi_value_buf_add(&result->items, WI_MAKE_BOX_VALUE(part));
            wi_gc_pop_root(state->gc);

            i += sep->len;
            start = i;
        } else {
            i++;
        }
    }

    wi_string_t* last = wi_copy_cstring(state->gc, string->chars + start, string->len - start);

    wi_gc_push_root(state->gc, (wi_box_t*)last);
    wi_value_buf_add(&result->items, WI_MAKE_BOX_VALUE(last));
    wi_gc_pop_root(state->gc);
}

static void
_string_reverse(wi_state_t* state, int arg_count) {
    wi_string_t* string = _check_arg1_string(state);
    char*        buf    = WI_GC_ALLOC(state->gc, char, string->len + 1);

    for (int i = 0; i < string->len; i++) {
        buf[i] = string->chars[string->len - 1 - i];
    }

    buf[string->len]    = '\0';
    state->api_stack[0] = WI_MAKE_BOX_VALUE(wi_take_cstring(state->gc, buf, string->len));
}

void
wi_state_def_string_foreign(wi_state_t* state) {
    wi_object_t* object = wi_state_def_object(state, "string");

    wi_state_def_field_foreign(state, "sub", _string_sub, 3, object);
    wi_state_def_field_foreign(state, "upper", _string_upper, 1, object);
    wi_state_def_field_foreign(state, "lower", _string_lower, 1, object);
    wi_state_def_field_foreign(state, "trim", _string_trim, 1, object);
    wi_state_def_field_foreign(state, "has", _string_has, 2, object);
    wi_state_def_field_foreign(state, "starts_with", _string_starts_with, 2, object);
    wi_state_def_field_foreign(state, "ends_with", _string_ends_with, 2, object);
    wi_state_def_field_foreign(state, "replace", _string_replace, 3, object);
    wi_state_def_field_foreign(state, "split", _string_split, 2, object);
    wi_state_def_field_foreign(state, "reverse", _string_reverse, 1, object);
}
