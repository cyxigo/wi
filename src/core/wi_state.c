#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "wi_state.h"

#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "../include/wi_conf.h"
#include "wi_box.h"
#include "wi_buf.h"
#include "wi_compiler.h"
#include "wi_gc.h"
#include "wi_table.h"
#include "wi_util.h"
#include "wi_value.h"

static void
_state_reset(struct wi_state* state) {
    struct wi_recovery* recovery = state->recoveries;

    while (recovery) {
        struct wi_recovery* next = recovery->next;
        free(recovery);
        recovery = next;
    }

    state->recoveries     = NULL;
    state->recovery_count = 0;
    state->stack_end      = state->stack + state->stack_capacity;
    state->stack_top      = state->stack;
    state->ffi_stack      = NULL;
    state->frame_count    = 0;
    state->c_depth        = 0;
    state->open_upvalues  = NULL;
}

static void
_state_out(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

static void
_state_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

static void
_state_on_compile(struct wi_state* state) {
    WI_UNUSED(state);
    /* do nothing */
}

static char*
_state_read_file(struct wi_state* state, const char* file_path) {
    FILE* file = fopen(file_path, "rb");

    if (!file) {
        wi_state_error(state, "failed to open file %s", file_path);
    }

    char* buf = wi_read_stream(file);
    fclose(file);

    if (!buf) {
        wi_state_error(state, "failed to read file %s", file_path);
    }

    return buf;
}

static bool
_state_require_exists(struct wi_state* state, const char* path) {
    WI_UNUSED(state);
#ifdef _WIN32
    return _access(path, 0) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}

struct wi_state*
wi_new_state(wi_conf* conf) {
    struct wi_state* state = malloc(sizeof(struct wi_state));

    if (!state) {
        return NULL;
    }

    state->conf = conf;
    state->gc   = wi_new_gc(state->conf);

    if (!state->gc) {
        free(state);
        return NULL;
    }

    state->gc->state = state;

    state->was_eof_error = false;
    state->out           = _state_out;
    state->error         = _state_error;

    state->on_compile     = _state_on_compile;
    state->load_require   = _state_read_file;
    state->require_exists = _state_require_exists;

    state->script_argc = 0;
    state->script_argv = NULL;

    state->interrupted = 0;

    state->recoveries = NULL;

    state->frames         = NULL;
    state->frame_capacity = 0;

    state->stack          = malloc(sizeof(wi_value) * WI_STACK_MIN);
    state->stack_capacity = WI_STACK_MIN;

    if (!state->stack) {
        wi_delete_gc(state->gc);
        free(state);
        return NULL;
    }

    _state_reset(state);

    wi_table_init(&state->globals, state->gc);
    wi_table_init(&state->global_attrs, state->gc);
    wi_table_init(&state->foreign, state->gc);
    wi_table_init(&state->required, state->gc);

    state->libs = NULL;

    wi_table_init(&state->stm_string, state->gc);
    wi_table_init(&state->stm_array, state->gc);
    wi_table_init(&state->stm_map, state->gc);

    state->ok_str    = NULL;
    state->value_str = NULL;
    state->error_str = NULL;

    state->ok_str    = wi_copy_cstring(state->gc, "ok", 2);
    state->value_str = wi_copy_cstring(state->gc, "value", 5);
    state->error_str = wi_copy_cstring(state->gc, "error", 5);

    srand((unsigned)time(NULL));

    return state;
}

void
wi_delete_state(struct wi_state* state) {
    free(state->frames);
    free(state->stack);

    wi_table_free(&state->globals);
    wi_table_free(&state->global_attrs);
    wi_table_free(&state->foreign);
    wi_table_free(&state->required);

    wi_table_free(&state->stm_string);
    wi_table_free(&state->stm_array);
    wi_table_free(&state->stm_map);

    wi_delete_gc(state->gc);

    wi_state_close_libs(state);
    free(state);
}

void
wi_state_tune_gc(struct wi_state* state, size_t min_heap, size_t heap_grow_factor, size_t young_max) {
    struct wi_gc* gc     = state->gc;
    gc->min_heap         = min_heap;
    gc->heap_grow_factor = heap_grow_factor < 1 ? 1 : heap_grow_factor;
    gc->young_max        = young_max;
    gc->next_major       = min_heap;
}

bool
wi_state_was_eof_error(wi_state* state) {
    return state->was_eof_error;
}

void
wi_state_set_callbacks(struct wi_state* state, wi_print_fn out_fn, wi_print_fn error_fn,
                       wi_on_compile_fn on_compile_fn, wi_load_require_fn load_require_fn,
                       wi_require_exists_fn require_exists_fn) {
    if (out_fn) {
        state->out = out_fn;
    }

    if (error_fn) {
        state->error = error_fn;
    }

    if (on_compile_fn) {
        state->on_compile = on_compile_fn;
    }

    if (load_require_fn) {
        state->load_require = load_require_fn;
    }

    if (require_exists_fn) {
        state->require_exists = require_exists_fn;
    }
}

void
wi_state_set_args(struct wi_state* state, int argc, const char** argv) {
    state->script_argc = argc;
    state->script_argv = argv;
}

bool
wi_state_add_lib(struct wi_state* state, wi_lib_handle handle) {
    struct wi_lib_node* lib = state->libs;

    while (lib) {
        if (lib->handle == handle) {
            /*
                we expect a handle to be opened by the point of calling this function
                so we use wi_lib_close if we are trying to load it again
                safe because both FreeLibrary and dlclose just decrement the reference count
                of the handle
            */
            wi_lib_close(handle);
            return false;
        }

        lib = lib->next;
    }

    struct wi_lib_node* new_lib = malloc(sizeof(struct wi_lib_node));

    if (!new_lib) {
        wi_lib_close(handle);
        return false;
    }

    new_lib->handle = handle;
    new_lib->next   = state->libs;
    state->libs     = new_lib;

    return true;
}

void
wi_state_close_libs_from(struct wi_state* state, struct wi_lib_node* from) {
    struct wi_lib_node* lib = state->libs;

    while (lib != from) {
        struct wi_lib_node* next = lib->next;
        wi_lib_close(lib->handle);
        free(lib);
        lib = next;
    }

    state->libs = from;
}

void
wi_state_close_libs(struct wi_state* state) {
    /* since wi_state_close_libs_from checks until lib == from we can use NULL here */
    wi_state_close_libs_from(state, NULL);
}

struct wi_recovery*
wi_state_push_recovery(struct wi_state* state) {
    if (state->recovery_count == WI_CSTACK_MAX) {
        wi_state_error(state, "too many error buffers (limit is %i)", WI_CSTACK_MAX);
    }

    struct wi_recovery* recovery = malloc(sizeof(struct wi_recovery));

    if (!recovery) {
        wi_state_oom(state, "out of memory: failed to allocate an error buffer");
    }

    recovery->next    = state->recoveries;
    state->recoveries = recovery;

    recovery->frame_count     = state->frame_count;
    recovery->c_call_depth    = state->c_depth;
    recovery->stack_top       = state->stack_top;
    recovery->ffi_stack       = state->ffi_stack;
    recovery->temp_root_count = state->gc->temp_root_count;
    recovery->error           = NULL;

    state->recovery_count++;
    return recovery;
}

void
wi_state_pop_recovery(struct wi_state* state) {
    struct wi_recovery* recovery = state->recoveries;

    if (WI_UNLIKELY(!recovery)) {
        return;
    }

    state->recoveries = recovery->next;
    free(recovery);
    state->recovery_count--;
}

WI_INLINE void
_state_close_upvalues(struct wi_state* state, wi_value* last) {
    while (state->open_upvalues && state->open_upvalues->location >= last) {
        struct wi_upvalue* upvalue = state->open_upvalues;

        upvalue->closed   = *upvalue->location;
        upvalue->location = &upvalue->closed;
        WI_GC_WRITE_BARRIER(state->gc, upvalue, upvalue->closed);

        state->open_upvalues = upvalue->next;
    }
}

WI_NORETURN void
wi_state_error(struct wi_state* state, const char* format, ...) {
    va_list args;
    va_start(args, format);

    if (state->recoveries) {
        struct wi_recovery* recovery = state->recoveries;
        _state_close_upvalues(state, recovery->stack_top);

        state->frame_count         = recovery->frame_count;
        state->c_depth             = recovery->c_call_depth;
        state->stack_top           = recovery->stack_top;
        state->ffi_stack           = recovery->ffi_stack;
        state->gc->temp_root_count = recovery->temp_root_count;

        char* error = wi_vasprintf(format, args);

        if (WI_UNLIKELY(!error)) {
            wi_state_oom(state, "failed to allocate an error message");
        }

        recovery->error = wi_take_cstring(state->gc, error, (int)strlen(error));
        va_end(args);
        longjmp(recovery->jmp, WI_RUN_ERROR);
    }

    state->error("runtime error: ");
    wi_vprintf(state->error, format, args);
    va_end(args);
    state->error("\n");

    for (int i = state->frame_count - 1; i >= 0; i--) {
        struct wi_call_frame* frame     = &state->frames[i];
        struct wi_prototype*  prototype = frame->closure->prototype;
        int                   line      = prototype->lines.data[frame->ip - prototype->bytes.data - 1];
        state->error("   --> %s:%i", prototype->file_path, line);

        if (prototype->is_main) {
            state->error(" in main function\n");
        } else if (prototype->name) {
            state->error(" in %s()\n", prototype->name->buf);
        } else {
            state->error(" in anonymous function\n");
        }
    }

    _state_reset(state);
    wi_gc_reset_roots(state->gc);
    longjmp(state->jmp, WI_RUN_ERROR);

#undef _APPEND_FORMAT
}

WI_NORETURN void
wi_state_oom(struct wi_state* state, const char* what) {
    state->error("out of memory: %s\n", what);
    _state_reset(state);
    wi_gc_reset_roots(state->gc);
    longjmp(state->jmp, WI_RUN_ABORT);
}

WI_NORETURN void
wi_state_abort(struct wi_state* state) {
    _state_reset(state);
    wi_gc_reset_roots(state->gc);
    longjmp(state->jmp, WI_RUN_ABORT);
}

void
wi_state_interrupt(struct wi_state* state) {
    state->interrupted = 1;
}

static void
_state_concat(struct wi_state* state) {
    wi_value a = wi_state_peek(state, 1);
    wi_value b = wi_state_top(state);

    char* a_buf;
    char* b_buf;
    int   a_count;
    int   b_count;
    bool  a_owned = false;
    bool  b_owned = false;

    if (wi_value_is_string(a)) {
        struct wi_string* string = wi_value_as_string(a);
        a_buf                    = string->buf;
        a_count                  = string->count;
    } else {
        a_buf = wi_value_to_string(a);

        if (!a_buf) {
            wi_state_oom(state, "out of memory: failed to allocate a string for concatenation");
        }

        a_count = (int)strlen(a_buf);
        a_owned = true;
    }

    if (wi_value_is_string(b)) {
        struct wi_string* string = wi_value_as_string(b);

        b_buf   = string->buf;
        b_count = string->count;
    } else {
        b_buf = wi_value_to_string(b);

        if (!b_buf) {
            wi_state_oom(state, "out of memory: failed to allocate a string for concatenation");
        }

        b_count = (int)strlen(b_buf);
        b_owned = true;
    }

    int   len = a_count + b_count;
    char* buf = WI_GC_ALLOC(state->gc, char, len + 1);

    memcpy(buf, a_buf, (size_t)a_count);
    memcpy(buf + a_count, b_buf, (size_t)b_count);
    buf[len] = '\0';

    if (a_owned) {
        free(a_buf);
    }

    if (b_owned) {
        free(b_buf);
    }

    struct wi_string* result = wi_take_cstring(state->gc, buf, len);

    wi_state_drop(state);
    wi_state_drop(state);
    wi_state_push(state, WI_MAKE_BOX_VALUE(result));
}

static void
_state_push_array(struct wi_state* state, int item_count) {
    struct wi_array* array = wi_new_array(state->gc);
    WI_GC_PUSH_ROOT(state->gc, array);
    wi_value_buf_reserve(&array->items, item_count);

    wi_value* item_start = state->stack_top - item_count;

    if (item_count > 0) {
        memcpy(array->items.data, item_start, sizeof(wi_value) * (size_t)item_count);
    }

    array->items.count = item_count;
    state->stack_top   = item_start;
    wi_state_push(state, WI_MAKE_BOX_VALUE(array));
    wi_gc_pop_root(state->gc);
}

WI_INLINE int
_state_validate_index(struct wi_state* state, const char* target, wi_value value, int count) {
    if (WI_UNLIKELY(!wi_value_is_real(value))) {
        wi_state_error(state, "%s index must be a real but got %s", target, wi_value_type(value));
    }

    int index = (int)wi_value_as_real(value);

    if (WI_UNLIKELY(index < 0 || index >= count)) {
        wi_state_error(state, "%s index out of range: %i", target, index);
    }

    return index;
}

static void
_state_subscript_set(struct wi_state* state, wi_value target, wi_value index, wi_value value) {
    if (wi_value_is_array(target)) {
        struct wi_array* array = wi_value_as_array(target);
        int              i     = _state_validate_index(state, "array", index, array->items.count);
        array->items.data[i]   = value;
        WI_GC_WRITE_BARRIER(state->gc, array, value);
        return;
    }

    if (wi_value_is_map(target)) {
        struct wi_map* map = wi_value_as_map(target);

        if (wi_table_set(&map->items, index, value)) {
            WI_GC_WRITE_BARRIER(state->gc, map, index);
        }

        WI_GC_WRITE_BARRIER(state->gc, map, value);
        return;
    }

    wi_state_error(state, "cannot use operator '[]' on a value of type %s", wi_value_type(target));
}

static wi_value
_state_subscript_get(struct wi_state* state, wi_value target, wi_value index) {
    if (WI_UNLIKELY(wi_value_is_string(target))) {
        struct wi_string* string = wi_value_as_string(target);
        int               i      = _state_validate_index(state, "string", index, string->len);
        i                        = wi_utf8_cp_offset(string->buf, string->count, i);
        size_t cp_len            = wi_utf8_cp_len(string->buf[i]);

        char buf[5] = {0};
        memcpy(buf, string->buf + i, cp_len);

        return WI_MAKE_BOX_VALUE(wi_copy_cstring(state->gc, buf, (int)cp_len));
    }

    if (wi_value_is_array(target)) {
        struct wi_array* array = wi_value_as_array(target);
        int              i     = _state_validate_index(state, "array", index, array->items.count);
        return array->items.data[i];
    }

    if (wi_value_is_map(target)) {
        struct wi_map* map = wi_value_as_map(target);
        wi_value       value;

        if (WI_LIKELY(wi_table_get(&map->items, index, &value))) {
            return value;
        }

        char* key;

        if (wi_value_is_string(index)) {
            key = wi_value_as_cstring(index);
        } else {
            key = wi_value_to_string(index);

            if (WI_UNLIKELY(!key)) {
                wi_state_oom(state, "out of memory: failed to allocate key string for the error message");
            }

            /*
                same trick as in _state_require: use gc boxing for cleanup, since wi_state_error longjmps
                why assign to "->buf" you may ask? because wi_take_cstring frees passed to it buffer if it's
                interned
            */
            key = wi_take_cstring(state->gc, key, strlen(key))->buf;
        }

        wi_state_error(state, "map has no key %s", key);
    }

    wi_state_error(state, "cannot use operator '[]' on a value of type %s", wi_value_type(target));
}

static struct wi_upvalue*
_state_capture_upvalue(struct wi_state* state, wi_value* local) {
    struct wi_upvalue* prev    = NULL;
    struct wi_upvalue* upvalue = state->open_upvalues;

    while (upvalue && upvalue->location > local) {
        prev    = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue && upvalue->location == local) {
        return upvalue;
    }

    struct wi_upvalue* new_upvalue = wi_new_upvalue(state->gc, local);
    new_upvalue->next              = upvalue;

    if (!prev) {
        state->open_upvalues = new_upvalue;
    } else {
        prev->next = new_upvalue;
    }

    return new_upvalue;
}

/*
    so! there are many things (a.k.a pointers) to correct after we obliterate the
    old stack memory with realloc...
*/
static void
_state_correct_stack(struct wi_state* state, wi_value* old_stack, wi_value* new_stack) {
    /*
        this is safe as long as we don't try, and, obliviously,
        dereference the old stack... why would we need to anyway?
    */
    ptrdiff_t diff = new_stack - old_stack;
    state->stack   = new_stack;

    state->stack_top += diff;
    state->stack_end = new_stack + state->stack_capacity;

    /* correct all the recoveries */
    struct wi_recovery* recovery = state->recoveries;

    while (recovery) {
        recovery->stack_top += diff;

        if (recovery->ffi_stack) {
            recovery->ffi_stack += diff;
        }

        recovery = recovery->next;
    }

    /* correct all the call frames */
    for (int i = 0; i < state->frame_count; i++) {
        state->frames[i].slots += diff;
    }

    /* correct even the ffi stack */
    if (state->ffi_stack) {
        state->ffi_stack += diff;
    }

    /* correct all the open upvalues */
    for (struct wi_upvalue* upvalue = state->open_upvalues; upvalue; upvalue = upvalue->next) {
        upvalue->location += diff;
    }
}

/*
    ensures [needed] more slots fit past [base], growing (and correcting) the stack
    why do we need [base]? because of the tail calls!
    let's trace the things... when we call a function, we add a new frame right? stack is:
    [function] [arg1] [arg2]
    cool! so, when we call a NEW function...
    [function] [arg1] [arg2] [new_function] [arg1]
                            ^------ calculate from here (the stack top)
    see where i'm going? tail calls don't need that old function, so we don't need to calculate
    from the stack_top, but from overwritten frame's slots
    [function] [arg1] [arg2] <--- BOOM! TAIL CALL
    [new_function] [arg1]
    ^------ calculate from here (where previous frame's slots started)
    i hope that clears up things...

    p.s: this comment exists because after i made this function with the [base] parameter and took a nap
    i couldn't understand why it exists for a looooooong time. welp, turns out comments are really important.
    - cyxigo, 08.05.2026
*/
static bool
_state_grow_stack(struct wi_state* state, wi_value* base, int needed) {
    int required = (int)(base - state->stack) + needed;

    if (required > WI_STACK_MAX) {
        return false;
    }

    int capacity = state->stack_capacity;

    while (capacity < required) {
        capacity = WI_GROW_CAPACITY(capacity);
    }

    if (capacity > WI_STACK_MAX) {
        capacity = WI_STACK_MAX;
    }

    wi_value* old_stack = state->stack;
    wi_value* new_stack = realloc(state->stack, sizeof(wi_value) * (size_t)capacity);

    if (WI_UNLIKELY(!new_stack)) {
        wi_state_oom(state, "out of memory: failed to allocate the value stack");
    }

    state->stack_capacity = capacity;
    /* reallocating memory usually means breaking many many things... */
    _state_correct_stack(state, old_stack, new_stack);

    return true;
}

WI_INLINE bool
_state_reserve_stack(struct wi_state* state, wi_value* base, int needed) {
    if (WI_UNLIKELY(base + needed > state->stack_end)) {
        return _state_grow_stack(state, base, needed);
    }

    return true;
}

static void
_state_capture_overflow_ctx(struct wi_state* state) {
    /*
        this is useful considering that without this call stack overflow will show you
        exactly WI_STACK_MAX amount of functions in the backtrace
        now that's NOT very useful isn't it?
    */
    if (state->frame_count > 0) {
        state->frames[0]   = state->frames[state->frame_count - 1];
        state->frame_count = 1;
    } else {
        state->frame_count = 0;
    }
}

WI_INLINE void
_state_call(struct wi_state* state, struct wi_closure* closure, uint8_t arg_count) {
    struct wi_prototype* prototype = closure->prototype;
    wi_state_check_arity(state, prototype->arity, arg_count, prototype->is_variadic);

    /* grow the frames if needed */
    if (WI_UNLIKELY(state->frame_count == state->frame_capacity)) {
        int capacity  = WI_GROW_CAPACITY(state->frame_capacity);
        state->frames = realloc(state->frames, sizeof(struct wi_call_frame) * (size_t)capacity);

        if (WI_UNLIKELY(!state->frames)) {
            wi_state_oom(state, "out of memory: failed to allocate call frames");
        }

        state->frame_capacity = capacity;
    }

    /* calculate and, if needed, grow the slots starting from the stack top */
    if (WI_UNLIKELY(!_state_reserve_stack(state, state->stack_top, prototype->max_slot_count))) {
        _state_capture_overflow_ctx(state);
        wi_state_error(state, "stack overflow (limit is %i)", WI_STACK_MAX);
    }

    struct wi_call_frame* frame = &state->frames[state->frame_count++];
    frame->closure              = closure;
    frame->ip                   = prototype->bytes.data;

    /*
        for variadic functions, stack looks like:
        [function] [fixed_args] [var_args array]
        here, [fixed_args] is prototype->arity, and -2 is var_args array and the function itself
        for simple functions, stack is:
        [function] [fixed_args]
        so [fixed_args] is simply arg_count and -1 is the function

        we set frame->slots to point to the start of the arguments so function can access them
        as locals! (first slot, which is a function, is a local too - so recursive functions can use it)
    */
    if (prototype->is_variadic) {
        _state_push_array(state, arg_count - prototype->arity);
        frame->slots = state->stack_top - prototype->arity - 2;
    } else {
        frame->slots = state->stack_top - arg_count - 1;
    }
}

WI_INLINE void
_state_tail_call(struct wi_state* state, struct wi_call_frame* frame, struct wi_closure* closure,
                 uint8_t arg_count) {
    struct wi_prototype* prototype = closure->prototype;
    wi_state_check_arity(state, prototype->arity, arg_count, prototype->is_variadic);

    /* calculate and, if needed, grow the slots starting from the reused frame slots */
    if (WI_UNLIKELY(!_state_reserve_stack(state, frame->slots, prototype->max_slot_count))) {
        _state_capture_overflow_ctx(state);
        wi_state_error(state, "stack overflow (limit is %i)", WI_STACK_MAX);
    }

    _state_close_upvalues(state, frame->slots);

    /*
        move new arguments in the place of the old ones
    */
    if (prototype->is_variadic) {
        _state_push_array(state, arg_count - prototype->arity);
        wi_value* callee_slots = state->stack_top - prototype->arity - 2;
        memmove(frame->slots, callee_slots, sizeof(wi_value) * (size_t)(prototype->arity + 2));
        state->stack_top = frame->slots + prototype->arity + 2;
    } else {
        wi_value* callee_slots = state->stack_top - arg_count - 1;
        memmove(frame->slots, callee_slots, sizeof(wi_value) * (size_t)(arg_count + 1));
        state->stack_top = frame->slots + arg_count + 1;
    }

    frame->closure = closure;
    frame->ip      = prototype->bytes.data;
}

WI_INLINE void
_state_resolve_field(struct wi_state* state, struct wi_object* object, wi_value name, wi_value* value) {
    if (wi_table_get(&object->fields, name, value)) {
        return;
    }

    wi_state_error(state, "object has no field %s", wi_value_as_cstring(name));
}

static wi_value
_state_resolve_method(struct wi_state* state, wi_value receiver, wi_value name) {
    wi_value function;

    if (wi_value_is_object(receiver)) {
        _state_resolve_field(state, wi_value_as_object(receiver), name, &function);
        return function;
    }

    struct wi_table* methods;

    if (wi_value_is_string(receiver)) {
        methods = &state->stm_string;
    } else if (wi_value_is_array(receiver)) {
        methods = &state->stm_array;
    } else if (wi_value_is_map(receiver)) {
        methods = &state->stm_map;
    } else {
        wi_state_error(state, "value type %s has no methods", wi_value_type(receiver));
    }

    if (WI_UNLIKELY(!wi_table_get(methods, name, &function))) {
        wi_state_error(state, "%s has no method %s", wi_value_type(receiver), wi_value_as_cstring(name));
    }

    return function;
}

WI_INLINE void
_state_set_field(struct wi_state* state, wi_value name, wi_value target) {
    if (WI_UNLIKELY(!wi_value_is_object(target))) {
        wi_state_error(state, "cannot use operator '.' on a value of type %s", wi_value_type(target));
    }

    struct wi_object* object = wi_value_as_object(target);
    wi_value          value  = wi_state_top(state);
    /* literal black magic to just make the compiler optimize the hell out of write barriers */
    bool is_new_key = wi_table_set(&object->fields, name, value);

    if (WI_LIKELY(!object->box.is_old)) {
        return;
    }

    if (is_new_key) {
        WI_GC_WRITE_BARRIER(state->gc, object, name);
    }

    WI_GC_WRITE_BARRIER(state->gc, object, value);
}

static struct wi_closure*
_state_require(struct wi_state* state, wi_value path_value) {
    char* path = wi_value_as_cstring(path_value);
    char* src  = state->load_require(state, path);

    struct wi_object* object = wi_new_object(state->gc);
    WI_GC_PUSH_ROOT(state->gc, object);

    /*
        we wrap src in a box in case wi_compile fails and causes oom error
        gc will have a reference to src and will be able to free it
    */
    struct wi_string* src_box = wi_take_cstring(state->gc, src, (int)strlen(src));
    WI_GC_PUSH_ROOT(state->gc, src_box);

    /*
        here, object->fields acts as state->global_attrs, it's temporary yes, but we won't need
        to go back to the required script (i.e. recompile it)
    */
    struct wi_prototype* prototype = wi_compile(state, path, src_box->buf, &object->fields);

    if (!prototype) {
        wi_gc_pop_root(state->gc); /* src_box */
        wi_gc_pop_root(state->gc); /* object */
        wi_state_error(state, "failed to compile script %s", path);
    }

    wi_gc_pop_root(state->gc); /* src_box */
    WI_GC_PUSH_ROOT(state->gc, prototype);

    state->on_compile(state);

    wi_table_set(&state->required, path_value, WI_MAKE_BOX_VALUE(object));

    struct wi_closure* closure = wi_new_closure(state->gc, prototype, &object->fields);
    closure->required          = object;
    closure->is_main           = true;

    wi_gc_pop_root(state->gc); /* prototype */
    wi_gc_pop_root(state->gc); /* object */

    return closure;
}

/*
    here's where the MAGIC happens..
*/
static enum wi_run_result
_state_interpreter_loop(struct wi_state* state, int base_frame_count, bool drop_result) {
    struct wi_call_frame* frame = wi_state_frame(state);
    uint8_t               opcode;

    register wi_value* constants = frame->closure->prototype->constants.data;
    register uint8_t*  ip        = frame->ip;

#define _UPDATE_FRAME()                                    \
    frame     = wi_state_frame(state);                     \
    constants = frame->closure->prototype->constants.data; \
    ip        = frame->ip

#define _ERROR(...)                         \
    do {                                    \
        frame->ip = ip;                     \
        wi_state_error(state, __VA_ARGS__); \
    } while (false)

#if defined(__GNUC__) || defined(__clang__)
    static void* dispatch_table[] = {
#define WI_OPCODE(name, _, __) __extension__ &&LABEL_##name,
#include "wi_opcode.h"
#undef WI_OPCODE
    };

#define _INTERPRET _DISPATCH();
#define _DISPATCH() __extension__({ goto* dispatch_table[(opcode = _READ_BYTE())]; })
#define _OPCODE_LABEL(name) LABEL_##name
#else
#define _INTERPRET \
    loop:          \
    switch (opcode = _READ_BYTE())
#define _DISPATCH() goto loop
#define _OPCODE_LABEL(name) case WI_OP_##name
#endif

#define _CHECK_INTERRUPT()                 \
    if (WI_UNLIKELY(state->interrupted)) { \
        state->interrupted = 0;            \
        frame->ip          = ip;           \
        wi_state_abort(state);             \
    }

#define _READ_BYTE() *ip++
#define _READ_SHORT() (ip += 2, (uint16_t)(ip[-2] << 8 | ip[-1]))
#define _READ_CONSTANT() constants[_READ_SHORT()]

#define _BINARY_OP(op, maker)                                                                     \
    do {                                                                                          \
        wi_value b = wi_state_pop(state);                                                         \
        wi_value a = wi_state_pop(state);                                                         \
                                                                                                  \
        if (WI_UNLIKELY(!wi_value_is_real(a) || !wi_value_is_real(b))) {                          \
            _ERROR("cannot use operator '" #op "' on values of type %s and %s", wi_value_type(a), \
                   wi_value_type(b));                                                             \
        }                                                                                         \
                                                                                                  \
        wi_real a_real = wi_value_as_real(a);                                                     \
        wi_real b_real = wi_value_as_real(b);                                                     \
                                                                                                  \
        wi_state_push(state, maker(a_real op b_real));                                            \
    } while (false)
#define _BIT_OP(op)                                                                               \
    do {                                                                                          \
        wi_value b = wi_state_pop(state);                                                         \
        wi_value a = wi_state_pop(state);                                                         \
                                                                                                  \
        if (WI_UNLIKELY(!wi_value_is_real(a) || !wi_value_is_real(b))) {                          \
            _ERROR("cannot use operator '" #op "' on values of type %s and %s", wi_value_type(a), \
                   wi_value_type(b));                                                             \
        }                                                                                         \
                                                                                                  \
        int64_t a_int = (int64_t)wi_value_as_real(a);                                             \
        int64_t b_int = (int64_t)wi_value_as_real(b);                                             \
                                                                                                  \
        wi_state_push(state, wi_make_real_value((wi_real)(a_int op b_int)));                      \
    } while (false)

    _INTERPRET {
        _OPCODE_LABEL(PUSH) : {
            wi_state_push(state, _READ_CONSTANT());
            _DISPATCH();
        }
        _OPCODE_LABEL(PUSH_NULL) : {
            wi_state_push(state, wi_make_null_value());
            _DISPATCH();
        }
        _OPCODE_LABEL(PUSH_TRUE) : {
            wi_state_push(state, wi_make_true_value());
            _DISPATCH();
        }
        _OPCODE_LABEL(PUSH_FALSE) : {
            wi_state_push(state, wi_make_false_value());
            _DISPATCH();
        }
        _OPCODE_LABEL(POP) : {
            wi_state_drop(state);
            _DISPATCH();
        }
        _OPCODE_LABEL(DEF_GLOBAL) : {
            wi_value name = _READ_CONSTANT();
            wi_table_set(frame->closure->globals, name, wi_state_top(state));

            if (WI_UNLIKELY(frame->closure->required)) {
                WI_GC_WRITE_BARRIER(state->gc, frame->closure->required, name);
                WI_GC_WRITE_BARRIER(state->gc, frame->closure->required, wi_state_top(state));
            }

            wi_state_drop(state);
            _DISPATCH();
        }
        _OPCODE_LABEL(SET_GLOBAL) : {
            wi_value name = _READ_CONSTANT();
            wi_table_set(frame->closure->globals, name, wi_state_top(state));

            if (WI_UNLIKELY(frame->closure->required)) {
                WI_GC_WRITE_BARRIER(state->gc, frame->closure->required, wi_state_top(state));
            }

            _DISPATCH();
        }
        _OPCODE_LABEL(GET_GLOBAL) : {
            wi_value name = _READ_CONSTANT();
            wi_value value;

            wi_table_get(frame->closure->globals, name, &value);
            wi_state_push(state, value);
            _DISPATCH();
        }
        _OPCODE_LABEL(STORE_LOCAL) : {
            frame->slots[_READ_BYTE()] = wi_state_top(state);
            _DISPATCH();
        }
        _OPCODE_LABEL(LOAD_LOCAL) : {
            wi_state_push(state, frame->slots[_READ_BYTE()]);
            _DISPATCH();
        }
        /* clang-format off */
        _OPCODE_LABEL(LOAD_LOCAL_0) :
        _OPCODE_LABEL(LOAD_LOCAL_1) :
        _OPCODE_LABEL(LOAD_LOCAL_2) :
        _OPCODE_LABEL(LOAD_LOCAL_3) :
        _OPCODE_LABEL(LOAD_LOCAL_4) :
        _OPCODE_LABEL(LOAD_LOCAL_5) : 
        _OPCODE_LABEL(LOAD_LOCAL_6) : 
        _OPCODE_LABEL(LOAD_LOCAL_7) :
        _OPCODE_LABEL(LOAD_LOCAL_8) : {
            wi_state_push(state, frame->slots[opcode - WI_OP_LOAD_LOCAL_0]);
            _DISPATCH();
        }
        /* clang-format on */
        _OPCODE_LABEL(ADD) : {
            _BINARY_OP(+, wi_make_real_value);
            _DISPATCH();
        }
        _OPCODE_LABEL(SUBTRACT) : {
            _BINARY_OP(-, wi_make_real_value);
            _DISPATCH();
        }
        _OPCODE_LABEL(MULTIPLY) : {
            _BINARY_OP(*, wi_make_real_value);
            _DISPATCH();
        }
        _OPCODE_LABEL(DIVIDE) : {
            _BINARY_OP(/, wi_make_real_value);
            _DISPATCH();
        }
        _OPCODE_LABEL(NEGATE) : {
            wi_value a = wi_state_pop(state);

            if (!wi_value_is_real(a)) {
                _ERROR("cannot use operator '-' on a value of type %s", wi_value_type(a));
            }

            wi_state_push(state, wi_make_real_value(-wi_value_as_real(a)));
            _DISPATCH();
        }
        _OPCODE_LABEL(POWER) : {
            wi_value b = wi_state_pop(state);
            wi_value a = wi_state_pop(state);

            if (!wi_value_is_real(a) || !wi_value_is_real(b)) {
                _ERROR("cannot use operator '**' on values of type %s and %s", wi_value_type(a), wi_value_type(b));
            }

            wi_state_push(state, wi_make_real_value(pow(wi_value_as_real(a), wi_value_as_real(b))));
            _DISPATCH();
        }
        _OPCODE_LABEL(MODULO) : {
            wi_value b = wi_state_pop(state);
            wi_value a = wi_state_pop(state);

            if (!wi_value_is_real(a) || !wi_value_is_real(b)) {
                _ERROR("cannot use operator '%%' on values of type %s and %s", wi_value_type(a), wi_value_type(b));
            }

            wi_state_push(state, wi_make_real_value(fmod(wi_value_as_real(a), wi_value_as_real(b))));
            _DISPATCH();
        }
        _OPCODE_LABEL(GREATER) : {
            _BINARY_OP(>, wi_make_bool_value);
            _DISPATCH();
        }
        _OPCODE_LABEL(GREATER_EQUAL) : {
            _BINARY_OP(>=, wi_make_bool_value);
            _DISPATCH();
        }
        _OPCODE_LABEL(LESS) : {
            _BINARY_OP(<, wi_make_bool_value);
            _DISPATCH();
        }
        _OPCODE_LABEL(LESS_EQUAL) : {
            _BINARY_OP(<=, wi_make_bool_value);
            _DISPATCH();
        }
        _OPCODE_LABEL(EQUAL) : {
            wi_value b = wi_state_pop(state);
            wi_value a = wi_state_pop(state);
            wi_state_push(state, wi_make_bool_value(wi_values_equal(a, b)));
            _DISPATCH();
        }
        _OPCODE_LABEL(NOT_EQUAL) : {
            wi_value b = wi_state_pop(state);
            wi_value a = wi_state_pop(state);
            wi_state_push(state, wi_make_bool_value(!wi_values_equal(a, b)));
            _DISPATCH();
        }
        _OPCODE_LABEL(LOG_NOT) : {
            wi_value value = wi_state_pop(state);
            wi_state_push(state, wi_make_bool_value(wi_value_is_falsy(value)));
            _DISPATCH();
        }
        _OPCODE_LABEL(BIT_AND) : {
            _BIT_OP(&);
            _DISPATCH();
        }
        _OPCODE_LABEL(BIT_OR) : {
            _BIT_OP(|);
            _DISPATCH();
        }
        _OPCODE_LABEL(BIT_XOR) : {
            _BIT_OP(^);
            _DISPATCH();
        }
        _OPCODE_LABEL(BIT_NOT) : {
            wi_value a = wi_state_pop(state);

            if (!wi_value_is_real(a)) {
                _ERROR("cannot use operator '~' on a value of type %s", wi_value_type(a));
            }

            wi_state_push(state, wi_make_real_value((wi_real) ~(int64_t)wi_value_as_real(a)));
            _DISPATCH();
        }
        _OPCODE_LABEL(BIT_SHL) : {
            _BIT_OP(<<);
            _DISPATCH();
        }
        _OPCODE_LABEL(BIT_SHR) : {
            _BIT_OP(>>);
            _DISPATCH();
        }
        _OPCODE_LABEL(LEN) : {
            wi_value a = wi_state_pop(state);

            if (wi_value_is_string(a)) {
                wi_state_push(state, wi_make_real_value(wi_value_as_string(a)->len));
                _DISPATCH();
            }

            if (wi_value_is_array(a)) {
                wi_state_push(state, wi_make_real_value(wi_value_as_array(a)->items.count));
                _DISPATCH();
            }

            if (wi_value_is_map(a)) {
                struct wi_table* table = &wi_value_as_map(a)->items;
                wi_state_push(state, wi_make_real_value(table->live_count));
                _DISPATCH();
            }

            _ERROR("cannot use operator '#' on a value of type %s", wi_value_type(a));
        }
        _OPCODE_LABEL(CONCAT) : {
            _state_concat(state);
            _DISPATCH();
        }
        _OPCODE_LABEL(JUMP) : {
            uint16_t offset = _READ_SHORT();
            ip += offset;
            _DISPATCH();
        }
        _OPCODE_LABEL(JUMP_IF_FALSE) : {
            uint16_t offset = _READ_SHORT();
            wi_value cond   = wi_state_pop(state);

            if (wi_value_is_falsy(cond)) {
                ip += offset;
            }

            _DISPATCH();
        }
        _OPCODE_LABEL(AND) : {
            uint16_t offset = _READ_SHORT();
            wi_value cond   = wi_state_top(state);

            if (wi_value_is_falsy(cond)) {
                ip += offset;
            } else {
                wi_state_drop(state);
            }

            _DISPATCH();
        }
        _OPCODE_LABEL(OR) : {
            uint16_t offset = _READ_SHORT();
            wi_value cond   = wi_state_top(state);

            if (wi_value_is_falsy(cond)) {
                wi_state_drop(state);
            } else {
                ip += offset;
            }

            _DISPATCH();
        }
        _OPCODE_LABEL(LOOP) : {
            uint16_t offset = _READ_SHORT();
            ip -= offset;
            _CHECK_INTERRUPT();
            _DISPATCH();
        }
        _OPCODE_LABEL(LOOP_END) : {
            _ERROR("invalid opcode");
        }
        _OPCODE_LABEL(PUSH_ARRAY) : {
            uint16_t count = _READ_SHORT();
            _state_push_array(state, (int)count);
            _DISPATCH();
        }
        _OPCODE_LABEL(PUSH_MAP) : {
            uint16_t       count = _READ_SHORT();
            struct wi_map* map   = wi_new_map(state->gc);
            WI_GC_PUSH_ROOT(state->gc, map);
            wi_table_reserve(&map->items, count);

            wi_value* item_start = state->stack_top - count * 2;

            for (int i = 0; i < count; i++) {
                wi_value key   = item_start[i * 2];
                wi_value value = item_start[i * 2 + 1];
                wi_table_set(&map->items, key, value);
            }

            state->stack_top = item_start;
            wi_state_push(state, WI_MAKE_BOX_VALUE(map));
            wi_gc_pop_root(state->gc);
            _DISPATCH();
        }
        _OPCODE_LABEL(SUBSCRIPT_SET) : {
            wi_value value  = wi_state_top(state);
            wi_value index  = wi_state_peek(state, 1);
            wi_value target = wi_state_peek(state, 2);
            frame->ip       = ip;

            _state_subscript_set(state, target, index, value);

            wi_state_drop(state);
            wi_state_drop(state);
            wi_state_drop(state);
            wi_state_push(state, value);

            _DISPATCH();
        }
        _OPCODE_LABEL(SUBSCRIPT_GET) : {
            wi_value index  = wi_state_pop(state);
            wi_value target = wi_state_pop(state);
            frame->ip       = ip;

            wi_value result = _state_subscript_get(state, target, index);
            wi_state_push(state, result);
            _DISPATCH();
        }
        _OPCODE_LABEL(PUSH_CLOSURE) : {
            struct wi_prototype* prototype = wi_value_as_prototype(_READ_CONSTANT());
            struct wi_closure*   closure   = wi_new_closure(state->gc, prototype, frame->closure->globals);
            closure->required              = frame->closure->required;
            wi_state_push(state, WI_MAKE_BOX_VALUE(closure));

            for (int i = 0; i < closure->upvalue_count; i++) {
                uint8_t index    = _READ_BYTE();
                uint8_t is_local = _READ_BYTE();

                if (is_local) {
                    closure->upvalues[i] = _state_capture_upvalue(state, frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }

                WI_GC_WRITE_BARRIER(state->gc, closure, WI_MAKE_BOX_VALUE(closure->upvalues[i]));
            }

            _DISPATCH();
        }
        _OPCODE_LABEL(STORE_UPVALUE) : {
            struct wi_upvalue* upvalue = frame->closure->upvalues[_READ_BYTE()];
            wi_value           value   = wi_state_top(state);
            *upvalue->location         = value;
            WI_GC_WRITE_BARRIER(state->gc, upvalue, value);
            _DISPATCH();
        }
        _OPCODE_LABEL(LOAD_UPVALUE) : {
            wi_state_push(state, *frame->closure->upvalues[_READ_BYTE()]->location);
            _DISPATCH();
        }
        _OPCODE_LABEL(CLOSE_UPVALUE) : {
            _state_close_upvalues(state, state->stack_top - 1);
            wi_state_drop(state);
            _DISPATCH();
        }
        _OPCODE_LABEL(CALL) : {
            uint8_t  arg_count = _READ_BYTE();
            wi_value value     = wi_state_peek(state, arg_count);
            frame->ip          = ip;

            if (wi_value_is_foreign(value)) {
                wi_state_call_foreign(state, wi_value_as_foreign(value), arg_count);
                _DISPATCH();
            }

            if (WI_UNLIKELY(!wi_value_is_closure(value))) {
                _ERROR("cannot use operator '()' on a value of type %s", wi_value_type(value));
            }

            _state_call(state, wi_value_as_closure(value), arg_count);
            _UPDATE_FRAME();
            _CHECK_INTERRUPT();
            _DISPATCH();
        }
        _OPCODE_LABEL(TAIL_CALL) : {
            uint8_t  arg_count = _READ_BYTE();
            wi_value value     = wi_state_peek(state, arg_count);
            frame->ip          = ip;

            if (wi_value_is_foreign(value)) {
                wi_state_call_foreign(state, wi_value_as_foreign(value), arg_count);
                /*
                    we can't reuse the call frame because well... it does not exist to begin with
                    so we use WI_OP_RETURN, which, removes the frame!
                */
                goto _op_return;
            }

            if (WI_UNLIKELY(!wi_value_is_closure(value))) {
                _ERROR("cannot use operator '()' on a value of type %s", wi_value_type(value));
            }

            _state_tail_call(state, frame, wi_value_as_closure(value), arg_count);
            _UPDATE_FRAME();
            _CHECK_INTERRUPT();

            _DISPATCH();
        }
    _op_return:
        _OPCODE_LABEL(RETURN) : {
            wi_value result = wi_state_pop(state);
            state->frame_count--;
            _state_close_upvalues(state, frame->slots);

            state->stack_top = frame->slots;

            if (frame->closure->required && frame->closure->is_main) {
                wi_state_push(state, WI_MAKE_BOX_VALUE(frame->closure->required));
            } else {
                wi_state_push(state, result);
            }

            if (state->frame_count == base_frame_count) {
                if (drop_result) {
                    wi_state_drop(state);
                }

                return WI_RUN_OK;
            }

            _UPDATE_FRAME();
            _DISPATCH();
        }
        _OPCODE_LABEL(PUSH_OBJECT) : {
            uint16_t          field_count = _READ_SHORT();
            struct wi_object* object      = wi_new_object(state->gc);
            WI_GC_PUSH_ROOT(state->gc, object);
            wi_table_reserve(&object->fields, field_count);

            wi_value* field_start = state->stack_top - field_count * 2;

            for (uint16_t i = 0; i < field_count; i++) {
                wi_value name  = field_start[i * 2];
                wi_value value = field_start[i * 2 + 1];
                wi_table_set(&object->fields, name, value);
            }

            state->stack_top = field_start;
            wi_state_push(state, WI_MAKE_BOX_VALUE(object));
            wi_gc_pop_root(state->gc);
            _DISPATCH();
        }
        _OPCODE_LABEL(INIT_FIELD) : {
            wi_value name   = _READ_CONSTANT();
            wi_value target = wi_state_peek(state, 1);
            frame->ip       = ip;
            _state_set_field(state, name, target);

            wi_state_drop(state);
            _DISPATCH();
        }
        _OPCODE_LABEL(SET_FIELD) : {
            wi_value name   = _READ_CONSTANT();
            wi_value target = wi_state_peek(state, 1);
            frame->ip       = ip;
            _state_set_field(state, name, target);

            wi_value value = wi_state_pop(state);
            wi_state_drop(state);
            wi_state_push(state, value);
            _DISPATCH();
        }
        _OPCODE_LABEL(GET_FIELD) : {
            wi_value name   = _READ_CONSTANT();
            wi_value target = wi_state_top(state);

            if (WI_UNLIKELY(!wi_value_is_object(target))) {
                _ERROR("cannot use operator '.' on a value of type %s", wi_value_type(target));
            }

            struct wi_object* object = wi_value_as_object(target);
            wi_value          value;
            frame->ip = ip;
            _state_resolve_field(state, object, name, &value);

            wi_state_drop(state);
            wi_state_push(state, value);
            _DISPATCH();
        }
        _OPCODE_LABEL(LOAD_METHOD) : {
            wi_value name     = _READ_CONSTANT();
            wi_value receiver = wi_state_top(state);
            frame->ip         = ip;
            wi_value method   = _state_resolve_method(state, receiver, name);

            state->stack_top[-1] = method;
            wi_state_push(state, receiver);
            _DISPATCH();
        }
        _OPCODE_LABEL(NEW) : {
            uint16_t          count = _READ_SHORT();
            wi_value*         start = state->stack_top - count;
            struct wi_object* clone = wi_new_object(state->gc);
            WI_GC_PUSH_ROOT(state->gc, clone);

            /*
                most of the time we clone a single object rather than merge multiple ones
                so a simple optimization here is to use wi_table_copy for simple cloning
            */
            if (WI_LIKELY(count == 1)) {
                wi_value value = start[0];

                if (WI_UNLIKELY(!wi_value_is_object(value))) {
                    _ERROR("cannot use operator 'new' on a value of type %s", wi_value_type(value));
                }

                wi_table_copy(&wi_value_as_object(value)->fields, &clone->fields);
            } else {
                for (uint16_t i = 0; i < count; i++) {
                    wi_value value = start[i];

                    if (WI_UNLIKELY(!wi_value_is_object(value))) {
                        _ERROR("cannot use operator 'new' on a value of type %s", wi_value_type(value));
                    }

                    struct wi_table* fields = &wi_value_as_object(value)->fields;

                    for (int j = 0; j < fields->capacity; j++) {
                        struct wi_entry* entry = &fields->entries[j];

                        if (!wi_value_is_empty(entry->key)) {
                            wi_table_set(&clone->fields, entry->key, entry->value);
                        }
                    }
                }
            }

            state->stack_top = start;
            wi_state_push(state, WI_MAKE_BOX_VALUE(clone));
            wi_gc_pop_root(state->gc);
            _DISPATCH();
        }
        _OPCODE_LABEL(REQUIRE) : {
            wi_value path_value = _READ_CONSTANT();
            wi_value loaded;

            if (wi_table_get(&state->required, path_value, &loaded)) {
                wi_state_push(state, loaded);
                _DISPATCH();
            }

            frame->ip                  = ip;
            struct wi_closure* closure = _state_require(state, path_value);
            wi_state_push(state, WI_MAKE_BOX_VALUE(closure));
            _state_call(state, closure, 0);

            _UPDATE_FRAME();
            _DISPATCH();
        }
    }

#undef _UPDATE_FRAME

#undef _ERROR

#undef _INTERPRET
#undef _DISPATCH
#undef _OPCODE_LABEL
#undef _CHECK_INTERRUPT

#undef _READ_BYTE
#undef _READ_SHORT
#undef _READ_CONSTANT

#undef _BINARY_OP
#undef _BIT_OP

    return WI_RUN_OK;
}

void
wi_state_call_foreign(struct wi_state* state, struct wi_foreign* foreign, uint8_t arg_count) {
    wi_state_check_arity(state, foreign->arity, arg_count, foreign->is_variadic);
    ptrdiff_t ffi_stack_offset = state->ffi_stack ? state->ffi_stack - state->stack : -1;

    state->ffi_stack = state->stack_top - arg_count - 1;
    foreign->fn(state, arg_count);

    state->stack_top = state->ffi_stack + 1;
    state->ffi_stack = ffi_stack_offset == -1 ? NULL : state->stack + ffi_stack_offset;
}

enum wi_run_result
wi_state_call(struct wi_state* state, wi_value callable, uint8_t arg_count, bool drop_result) {
    if (wi_value_is_foreign(callable)) {
        wi_state_call_foreign(state, wi_value_as_foreign(callable), arg_count);

        if (drop_result) {
            wi_state_drop(state);
        }

        return WI_RUN_OK;
    }

    if (state->c_depth == WI_CSTACK_MAX) {
        wi_state_error(state, "C stack overflow (limit is %i)", WI_CSTACK_MAX);
    }

    struct wi_closure* closure = wi_value_as_closure(callable);

    /* same thing as wi_call! but instead storing where the ffi stack starts */
    ptrdiff_t ffi_stack_offset = state->ffi_stack ? state->ffi_stack - state->stack : -1;
    int       base_frame_count = state->frame_count;
    _state_call(state, closure, arg_count);

    state->c_depth++;
    enum wi_run_result result = _state_interpreter_loop(state, base_frame_count, drop_result);
    state->c_depth--;

    state->ffi_stack = ffi_stack_offset == -1 ? NULL : state->stack + ffi_stack_offset;
    return result;
}

enum wi_run_result
wi_state_run(struct wi_state* state, const char* file_path, const char* src) {
    state->was_eof_error = false;
    state->interrupted   = 0;
    /* set early so we can catch compiler/parser oom */
    int jmp_result = setjmp(state->jmp);

    if (jmp_result == WI_RUN_ABORT) {
        return WI_RUN_ABORT;
    }

    if (jmp_result != WI_RUN_OK) {
        return WI_RUN_ERROR;
    }

    struct wi_prototype* prototype = wi_compile(state, file_path, src, &state->global_attrs);

    if (!prototype) {
        return WI_RUN_ERROR;
    }

    WI_GC_PUSH_ROOT(state->gc, prototype);
    state->on_compile(state);

    struct wi_closure* closure = wi_new_closure(state->gc, prototype, &state->globals);
    closure->is_main           = true;
    wi_gc_pop_root(state->gc);

    wi_state_push(state, WI_MAKE_BOX_VALUE(closure));
    _state_call(state, closure, 0);

    return _state_interpreter_loop(state, 0, true);
}

wi_value
wi_slot_check_callback(struct wi_state* state, int slot, uint8_t arg_count) {
    wi_value callback = state->ffi_stack[slot];

    if (!wi_value_is_foreign(callback) && !wi_value_is_closure(callback)) {
        wi_state_error(state, "bad argument %i - cannot use a value of type %s as a callback", slot,
                       wi_value_type(callback));
    }

    if (wi_value_is_closure(callback)) {
        struct wi_prototype* prototype = wi_value_as_closure(callback)->prototype;
        wi_state_check_arity(state, prototype->arity, arg_count, prototype->is_variadic);
    } else {
        struct wi_foreign* foreign = wi_value_as_foreign(callback);
        wi_state_check_arity(state, foreign->arity, arg_count, foreign->is_variadic);
    }

    return callback;
}
