#include "wi_array.h"

#include <stdbool.h>
#include <stdint.h>

#include "../../include/wi.h"
#include "../core/wi_state.h"

static void
_array_copy(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array     = wi_arg_array(state, 1);
    struct wi_array* new_array = wi_push_array(state);

    if (array->items.count > 0) {
        wi_value_buf_reserve(&new_array->items, array->items.count);
        memcpy(new_array->items.data, array->items.data, sizeof(wi_value) * (size_t)array->items.count);
        new_array->items.count = array->items.count;
    }
}

static void
_array_clear(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array     = wi_arg_array(state, 1);
    int              mod_count = array->items.mod_count;
    wi_value_buf_free(&array->items);
    array->items.mod_count = mod_count + 1;
    wi_push_arg(state, 1);
}

static void
_array_capacity(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    wi_push_real(state, array->items.capacity);
}

static void
_array_count(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    wi_push_real(state, array->items.count);
}

static void
_array_reverse(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);

    for (int i = 0, j = array->items.count - 1; i < j; i++, j--) {
        wi_value temp        = array->items.data[i];
        array->items.data[i] = array->items.data[j];
        array->items.data[j] = temp;
    }

    array->items.mod_count++;
    wi_push_arg(state, 1);
}

static void
_array_reversed(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array     = wi_arg_array(state, 1);
    struct wi_array* new_array = wi_push_array(state);

    int count = array->items.count;
    wi_value_buf_reserve(&new_array->items, count);

    for (int i = 0; i < count; i++) {
        new_array->items.data[i] = array->items.data[count - 1 - i];
    }

    new_array->items.count = count;
}

static void
_array_add(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    wi_gc_array_add(state->gc, array, state->ffi_stack[2]);
    wi_push_arg(state, 1);
}

/* find the index of [value], -1 = not found */
static int
_array_index(struct wi_array* array, wi_value value) {
    for (int i = 0; i < array->items.count; i++) {
        if (wi_values_equal(array->items.data[i], value)) {
            return i;
        }
    }

    return -1;
}

/* delete a value at [index] from an array */
static wi_value
_array_delete(struct wi_array* array, int index) {
    wi_value removed = array->items.data[index];
    memmove(array->items.data + index, array->items.data + index + 1,
            sizeof(wi_value) * (size_t)(array->items.count - index - 1));
    array->items.count--;
    array->items.mod_count++;
    return removed;
}

static void
_array_has(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    wi_push_bool(state, _array_index(array, state->ffi_stack[2]) != -1);
}

static void
_array_indexof(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    wi_push_real(state, _array_index(array, state->ffi_stack[2]));
}

static void
_array_remove(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    int              index = _array_index(array, state->ffi_stack[2]);

    if (index != -1) {
        _array_delete(array, index);
    }

    wi_push_bool(state, index != -1);
}

static void
_array_removeat(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    int64_t          index = wi_state_real_to_int(state, wi_arg_real(state, 2));

    if (index < 0 || index >= array->items.count) {
        wi_state_error(state, "array index out of range: %lld", index);
    }

    wi_state_ppush(state, _array_delete(array, (int)index));
}

static void
_array_insertat(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    int64_t          index = wi_state_real_to_int(state, wi_arg_real(state, 2));

    if (index < 0 || index > array->items.count) {
        wi_state_error(state, "array index out of range: %lld", index);
    }

    wi_value value = state->ffi_stack[3];
    wi_gc_array_add(state->gc, array, value);
    memmove(array->items.data + index + 1, array->items.data + index,
            sizeof(wi_value) * (size_t)(array->items.count - index - 1));
    array->items.data[index] = value;
    wi_push_arg(state, 1);
}

static void
_array_pop(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);

    if (array->items.count == 0) {
        wi_state_error(state, "cannot pop from an empty array");
    }

    wi_state_ppush(state, array->items.data[array->items.count - 1]);
    array->items.count--;
    array->items.mod_count++;
}

static void
_array_concat(struct wi_state* state, uint8_t arg_count) {
    struct wi_array* result = wi_push_array(state);

    for (int i = 0; i < arg_count; i++) {
        struct wi_array* array = wi_arg_array(state, (uint8_t)(i + 1));
        int              count = array->items.count;

        if (count == 0) {
            continue;
        }

        wi_value_buf_reserve(&result->items, count);
        memcpy(result->items.data + result->items.count, array->items.data, sizeof(wi_value) * (size_t)count);
        result->items.count += count;
    }
}

static void
_array_slice(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    int64_t          start = wi_state_real_to_int(state, wi_arg_real(state, 2));
    int64_t          end   = wi_state_real_to_int(state, wi_arg_real(state, 3));

    if (start < 0 || start > array->items.count || end < 0 || end > array->items.count || start > end) {
        wi_state_error(state, "array slice bounds out of range: %lld to %lld", start, end);
    }

    struct wi_array* result = wi_push_array(state);
    int64_t          count  = end - start;

    if (count <= 0) {
        return;
    }

    wi_value_buf_reserve(&result->items, (int)count);
    memcpy(result->items.data, array->items.data + start, sizeof(wi_value) * (size_t)count);
    result->items.count = (int)count;
}

static void
_array_join(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    int              sep_count;
    char*            sep = wi_arg_string(state, 2, &sep_count, NULL);

    struct wi_char_buf buf;
    wi_char_buf_init(&buf, state->gc);

    for (int i = 0; i < array->items.count; i++) {
        if (i > 0) {
            wi_char_buf_append(&buf, sep, sep_count);
        }

        wi_value item = array->items.data[i];
        int      item_count;
        bool     owned;
        char*    item_buf = wi_value_to_buf(item, &item_count, &owned);

        if (!item_buf) {
            wi_char_buf_free(&buf);
            wi_state_oom(state, "failed to allocate a string for join (_array_join)");
        }

        wi_char_buf_append(&buf, item_buf, item_count);

        if (owned) {
            free(item_buf);
        }
    }

    wi_push_lstring(state, buf.data, buf.count);
    wi_char_buf_free(&buf);
}

static void
_array_call(struct wi_state* state, struct wi_array* array, int mod_count, wi_value item, bool drop) {
    wi_arg_function(state, 2, 1);
    wi_state_ppush(state, item);
    wi_call(state, 1, drop);
    wi_state_check_mod_count(state, "array", array->items.mod_count, mod_count);
}

static void
_array_each(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array     = wi_arg_array(state, 1);
    int              mod_count = array->items.mod_count;
    wi_arg_check_function(state, 2, 1);

    for (int i = 0; i < array->items.count; i++) {
        _array_call(state, array, mod_count, array->items.data[i], true);
    }

    wi_push_arg(state, 1);
}

static void
_array_select(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array     = wi_arg_array(state, 1);
    int              mod_count = array->items.mod_count;
    wi_arg_check_function(state, 2, 1);
    struct wi_array* result = wi_push_array(state);
    wi_value_buf_reserve(&result->items, array->items.count);

    for (int i = 0; i < array->items.count; i++) {
        _array_call(state, array, mod_count, array->items.data[i], false);
        wi_gc_array_add(state->gc, result, wi_state_pop(state));
    }
}

static void
_array_where(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array     = wi_arg_array(state, 1);
    int              mod_count = array->items.mod_count;
    wi_arg_check_function(state, 2, 1);
    struct wi_array* result = wi_push_array(state);
    wi_value_buf_reserve(&result->items, array->items.count);

    for (int i = 0; i < array->items.count; i++) {
        wi_value item = array->items.data[i];
        _array_call(state, array, mod_count, item, false);

        if (!wi_value_is_falsy(wi_state_pop(state))) {
            wi_gc_array_add(state->gc, result, item);
        }
    }
}

static void
_aqsort_swap(struct wi_array* array, int i, int j) {
    wi_value temp        = array->items.data[i];
    array->items.data[i] = array->items.data[j];
    array->items.data[j] = temp;
}

static int
_aqsort_partition(struct wi_state* state, struct wi_array* array, int lo, int hi, int mod_count) {
    int pii = lo + (int)(wi_state_rand_next(state) % (uint64_t)(hi - lo + 1));
    _aqsort_swap(array, pii, hi);

    wi_value pi        = array->items.data[hi];
    bool     pi_is_box = wi_value_is_box(pi);

    if (pi_is_box) {
        WI_GC_PUSH_ROOT(state->gc, wi_value_as_box(pi));
    }

    int i = lo - 1;

    for (int j = lo; j < hi; j++) {
        wi_arg_function(state, 2, 2);
        wi_state_ppush(state, array->items.data[j]);
        wi_state_ppush(state, pi);
        wi_call(state, 2, false);
        wi_state_check_mod_count(state, "array", array->items.mod_count, mod_count);

        if (!wi_value_is_falsy(wi_state_pop(state))) {
            i++;
            _aqsort_swap(array, i, j);
        }
    }

    if (pi_is_box) {
        wi_gc_pop_root(state->gc);
    }

    _aqsort_swap(array, i + 1, hi);
    return i + 1;
}

static void
_aqsort(struct wi_state* state, struct wi_array* array, int lo, int hi, int mod_count) {
    while (lo < hi) { /* loop for tail recursion */
        int pi = _aqsort_partition(state, array, lo, hi, mod_count);

        if (pi - lo < hi - pi) {
            _aqsort(state, array, lo, pi - 1, mod_count);
            lo = pi + 1;
        } else {
            _aqsort(state, array, pi + 1, hi, mod_count);
            hi = pi - 1;
        }
    }
}

static void
_array_sort(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array = wi_arg_array(state, 1);
    wi_arg_check_function(state, 2, 2);

    if (array->items.count > 1) {
        _aqsort(state, array, 0, array->items.count - 1, array->items.mod_count);
    }

    array->items.mod_count++;
    wi_push_arg(state, 1);
}

static void
_array_find(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_array* array     = wi_arg_array(state, 1);
    int              mod_count = array->items.mod_count;
    wi_arg_check_function(state, 2, 1);

    for (int i = 0; i < array->items.count; i++) {
        wi_value item = array->items.data[i];
        _array_call(state, array, mod_count, item, false);

        if (!wi_value_is_falsy(wi_state_pop(state))) {
            wi_state_ppush(state, item);
            return;
        }
    }

    wi_push_null(state);
}

void
wi_state_def_stm_array(struct wi_state* state) {
    wi_foreign_entry functions[] = {
        {"copy",     _array_copy,     1, false},
        {"clear",    _array_clear,    1, false},
        {"capacity", _array_capacity, 1, false},
        {"count",    _array_count,    1, false},
        {"reverse",  _array_reverse,  1, false},
        {"reversed", _array_reversed, 1, false},
        {"add",      _array_add,      2, false},
        {"has",      _array_has,      2, false},
        {"indexof",  _array_indexof,  2, false},
        {"remove",   _array_remove,   2, false},
        {"removeat", _array_removeat, 2, false},
        {"insertat", _array_insertat, 3, false},
        {"pop",      _array_pop,      1, false},
        {"concat",   _array_concat,   0, true },
        {"slice",    _array_slice,    3, false},
        {"join",     _array_join,     2, false},
        {"each",     _array_each,     2, false},
        {"select",   _array_select,   2, false},
        {"where",    _array_where,    2, false},
        {"sort",     _array_sort,     2, false},
        {"find",     _array_find,     2, false},
    };
    WI_TABLE_SET_FOREIGN_ALL(&state->stm_array, functions);
}
