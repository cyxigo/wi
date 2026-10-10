#include "wi_string.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../include/wi.h"

static void
_string_bytes(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int              count;
    char*            string = wi_arg_string(state, 1, &count, NULL);
    struct wi_array* array  = wi_push_array(state);
    wi_value_buf_reserve(&array->items, count);

    for (int i = 0; i < count; i++) {
        wi_push_real(state, (wi_real)(unsigned char)string[i]);
        wi_array_add(state, array);
    }
}

static void
_string_sub(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int     len;
    int     count;
    char*   string = wi_arg_string(state, 1, &count, &len);
    int64_t start  = wi_state_real_to_int(state, wi_arg_real(state, 2));
    int64_t end    = wi_state_real_to_int(state, wi_arg_real(state, 3));

    if (start < 0 || start > len || end < 0 || end > len || start > end) {
        wi_state_error(state, "string sub bounds out of range: %lld to %lld", start, end);
    }

    int byte_start = wi_utf8_cp_offset(string, count, (int)start);
    int byte_end   = wi_utf8_cp_offset(string, count, (int)end);
    wi_push_lstring(state, string + byte_start, byte_end - byte_start);
}

static void
_string_case_mod(struct wi_state* state, int (*mod_fn)(int c)) {
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    char* buf    = WI_GC_ALLOC(state->gc, char, count + 1);

    for (int i = 0; i < count; i++) {
        unsigned char c = (unsigned char)string[i];
        /* skip utf-8 characters... for now at least */
        buf[i] = c < 0x80 ? (char)mod_fn(c) : (char)c;
    }

    buf[count]            = '\0';
    struct wi_string* box = wi_take_cstring(state->gc, buf, count);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

static void
_string_upper(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _string_case_mod(state, toupper);
}

static void
_string_lower(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _string_case_mod(state, tolower);
}

static void
_string_trim(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    int   start  = 0;
    int   end    = count;

    while (start < end && isspace((unsigned char)string[start])) {
        start++;
    }

    while (end > start && isspace((unsigned char)string[end - 1])) {
        end--;
    }

    wi_push_lstring(state, string + start, end - start);
}

static int
_string_index(const char* string, int count, const char* target, int target_count) {
    for (int i = 0; i + target_count <= count; i++) {
        if (memcmp(string + i, target, (size_t)target_count) == 0) {
            return i;
        }
    }

    return -1;
}

static void
_string_has(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    int   target_count;
    char* target = wi_arg_string(state, 2, &target_count, NULL);
    wi_push_bool(state, _string_index(string, count, target, target_count) != -1);
}

static void
_string_indexof(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    int   target_count;
    char* target = wi_arg_string(state, 2, &target_count, NULL);
    int   offset = _string_index(string, count, target, target_count);
    wi_push_real(state, offset == -1 ? -1 : wi_utf8_len(string, offset));
}

static void
_string_startswith(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    int   pref_count;
    char* pref   = wi_arg_string(state, 2, &pref_count, NULL);
    bool  result = pref_count <= count && memcmp(string, pref, (size_t)pref_count) == 0;

    wi_push_bool(state, result);
}

static void
_string_endswith(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    int   suff_count;
    char* suff   = wi_arg_string(state, 2, &suff_count, NULL);
    bool  result = suff_count <= count && memcmp(string + (count - suff_count), suff, (size_t)suff_count) == 0;

    wi_push_bool(state, result);
}

static bool
_is_digit(unsigned char c) {
    return c >= '0' && c <= '9';
}

static bool
_is_alpha(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool
_is_alnum(unsigned char c) {
    return _is_digit(c) || _is_alpha(c);
}

static void
_string_class_check(struct wi_state* state, bool (*fn)(unsigned char c)) {
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    bool  result = count > 0;

    for (int i = 0; result && i < count; i++) {
        result = fn((unsigned char)string[i]);
    }

    wi_push_bool(state, result);
}

static void
_string_isdigit(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _string_class_check(state, _is_digit);
}

static void
_string_isalpha(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _string_class_check(state, _is_alpha);
}

static void
_string_isalnum(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _string_class_check(state, _is_alnum);
}

static void
_string_compare(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   a_count;
    char* a = wi_arg_string(state, 1, &a_count, NULL);
    int   b_count;
    char* b = wi_arg_string(state, 2, &b_count, NULL);

    int cmp = memcmp(a, b, (size_t)(a_count < b_count ? a_count : b_count));
    wi_push_bool(state, (cmp != 0 ? cmp : a_count - b_count) < 0);
}

static void
_string_replace(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    int   old_count;
    char* old = wi_arg_string(state, 2, &old_count, NULL);
    int   new_count;
    char* new = wi_arg_string(state, 3, &new_count, NULL);

    if (old_count == 0) {
        wi_push_arg(state, 1);
        return;
    }

    struct wi_char_buf buf;
    wi_char_buf_init(&buf, state->gc);

    int i = 0;

    while (i < count) {
        if (i + old_count <= count && memcmp(string + i, old, (size_t)old_count) == 0) {
            wi_char_buf_append(&buf, new, new_count);
            i += old_count;
        } else {
            wi_char_buf_add(&buf, string[i]);
            i++;
        }
    }

    wi_push_lstring(state, buf.data, buf.count);
    wi_char_buf_free(&buf);
}

static void
_string_split(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    int   sep_count;
    char* sep = wi_arg_string(state, 2, &sep_count, NULL);

    struct wi_array* result = wi_push_array(state);

    if (sep_count == 0) {
        wi_push_arg(state, 1);
        wi_array_add(state, result);
        return;
    }

    int start = 0;
    int i     = 0;

    while (i + sep_count <= count) {
        if (memcmp(string + i, sep, (size_t)sep_count) == 0) {
            wi_push_lstring(state, string + start, i - start);
            wi_array_add(state, result);

            i += sep_count;
            start = i;
        } else {
            i++;
        }
    }

    wi_push_lstring(state, string + start, count - start);
    wi_array_add(state, result);
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
_string_reverse(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
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

end:;
    struct wi_string* box = wi_take_cstring(state->gc, buf, count);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

static void
_string_repeat(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int     count;
    char*   string = wi_arg_string(state, 1, &count, NULL);
    int64_t times  = wi_state_real_to_int(state, wi_arg_real(state, 2));

    if (times < 0) {
        wi_state_error(state, "string repeat count must not be negative: %lld", times);
    }

    if (times == 0 || count == 0) {
        wi_push_string(state, "");
        return;
    }

    if (times > (INT_MAX - 1) / count) {
        wi_state_error(state, "string repeat result too large: %i bytes times %lld", count, times);
    }

    int   len = count * (int)times;
    char* buf = WI_GC_ALLOC(state->gc, char, len + 1);

    for (int64_t i = 0; i < times; i++) {
        memcpy(buf + i * count, string, (size_t)count);
    }

    buf[len]              = '\0';
    struct wi_string* box = wi_take_cstring(state->gc, buf, len);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

static size_t
_string_call(struct wi_state* state, const char* string, size_t i, char* cp_buf, bool drop) {
    size_t cp_len = wi_utf8_cp_len(string[i]);
    memcpy(cp_buf, string + i, cp_len);
    cp_buf[cp_len] = '\0';

    wi_arg_function(state, 2, 1);
    wi_push_lstring(state, cp_buf, (int)cp_len);
    wi_call(state, 1, drop);

    return cp_len;
}

static void
_string_each(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    wi_arg_check_function(state, 2, 1);

    for (size_t i = 0; i < (size_t)count;) {
        char cp_buf[5];
        i += _string_call(state, string, i, cp_buf, true);
    }

    wi_push_arg(state, 1);
}

static void
_char_buf_finalizer(void* data) {
    struct wi_char_buf* buf = data;
    wi_char_buf_free(buf);
    free(buf);
}

static void
_string_collect(struct wi_state* state, bool select) {
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    wi_arg_check_function(state, 2, 1);

    struct wi_char_buf* buf = (struct wi_char_buf*)malloc(sizeof(struct wi_char_buf));

    if (!buf) {
        wi_state_oom(state, "failed to allocate a string buffer (_string_collect)");
    }

    wi_char_buf_init(buf, state->gc);
    wi_push_userdata(state, "char_buf", buf, _char_buf_finalizer);

    for (size_t i = 0; i < (size_t)count;) {
        char     cp_buf[5];
        size_t   cp_len = _string_call(state, string, i, cp_buf, false);
        wi_value result = wi_state_top(state);

        if (!select) {
            if (!wi_value_is_falsy(result)) {
                wi_char_buf_append(buf, cp_buf, (int)cp_len);
            }
        } else if (wi_value_is_string(result)) {
            struct wi_string* box = wi_value_as_string(result);
            wi_char_buf_append(buf, box->buf, box->count);
        } else {
            wi_state_error(state, "callback must return a string but got %s", wi_value_type(result));
        }

        wi_drop(state);
        i += cp_len;
    }

    wi_push_lstring(state, buf->data, buf->count);
    wi_char_buf_free(buf);
}

static void
_string_select(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _string_collect(state, true);
}

static void
_string_where(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    _string_collect(state, false);
}

static void
_string_find(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    int   count;
    char* string = wi_arg_string(state, 1, &count, NULL);
    wi_arg_check_function(state, 2, 1);

    for (size_t i = 0; i < (size_t)count;) {
        char   cp_buf[5];
        size_t cp_len = _string_call(state, string, i, cp_buf, false);

        if (!wi_value_is_falsy(wi_state_pop(state))) {
            wi_push_lstring(state, cp_buf, (int)cp_len);
            return;
        }

        i += cp_len;
    }

    wi_push_null(state);
}

void
wi_state_def_stm_string(struct wi_state* state) {
    wi_foreign_entry functions[] = {
        {"bytes",      _string_bytes,      1, false},
        {"sub",        _string_sub,        3, false},
        {"upper",      _string_upper,      1, false},
        {"lower",      _string_lower,      1, false},
        {"trim",       _string_trim,       1, false},
        {"has",        _string_has,        2, false},
        {"indexof",    _string_indexof,    2, false},
        {"startswith", _string_startswith, 2, false},
        {"endswith",   _string_endswith,   2, false},
        {"isdigit",    _string_isdigit,    1, false},
        {"isalpha",    _string_isalpha,    1, false},
        {"isalnum",    _string_isalnum,    1, false},
        {"compare",    _string_compare,    2, false},
        {"replace",    _string_replace,    3, false},
        {"split",      _string_split,      2, false},
        {"reverse",    _string_reverse,    1, false},
        {"repeat",     _string_repeat,     2, false},
        {"each",       _string_each,       2, false},
        {"select",     _string_select,     2, false},
        {"where",      _string_where,      2, false},
        {"find",       _string_find,       2, false},
    };
    WI_TABLE_SET_FOREIGN_ALL(&state->stm_string, functions);
}
