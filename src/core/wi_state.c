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

#include "../include/wi_conf.h"
#include "wi_box.h"
#include "wi_compiler.h"
#include "wi_gc.h"
#include "wi_table.h"
#include "wi_util.h"
#include "wi_value.h"

static void
_state_reset_stack(wi_state_t* state) {
    state->recovery_count = 0;
    state->stack_top      = state->stack;
    state->api_stack      = NULL;
    state->frame_count    = 0;
    state->c_call_depth   = 0;
    state->open_upvalues  = NULL;
}

wi_state_t*
wi_new_state(wi_conf_t conf) {
    wi_state_t* state = malloc(sizeof(wi_state_t));

    if (!state) {
        return NULL;
    }

    state->conf = conf;
    state->gc   = wi_new_gc(state->conf);

    if (!state->gc) {
        return NULL;
    }

    state->gc->state   = state;
    state->interrupted = 0;

    _state_reset_stack(state);

    wi_table_init(&state->globals, state->gc);
    wi_table_init(&state->foreign, state->gc);
    wi_table_init(&state->required, state->gc);

    state->foreign_handles = NULL;

    state->string_obj = NULL;
    state->array_obj  = NULL;
    state->map_obj    = NULL;

    state->ok_str    = NULL;
    state->value_str = NULL;
    state->error_str = NULL;

    state->ok_str    = wi_copy_cstring(state->gc, "ok", 2);
    state->value_str = wi_copy_cstring(state->gc, "value", 5);
    state->error_str = wi_copy_cstring(state->gc, "error", 5);

    return state;
}

static void
_state_free_foreign_handles(wi_state_t* state) {
    wi_foreign_handle_t* handle = state->foreign_handles;

    while (handle) {
        wi_foreign_handle_t* next = handle->next;

#ifdef _WIN32
        FreeLibrary(handle->lib);
#else
        dlclose(handle->lib);
#endif

        free(handle);
        handle = next;
    }
}

void
wi_delete_state(wi_state_t* state) {
    wi_table_free(&state->globals);
    wi_table_free(&state->foreign);
    wi_table_free(&state->required);

    wi_delete_gc(state->gc);

    _state_free_foreign_handles(state);
    free(state);
}

bool
wi_state_add_foreign_handle(wi_state_t* state, wi_lib_handle_t lib) {
    wi_foreign_handle_t* handle = state->foreign_handles;

    while (handle) {
        if (handle->lib == lib) {
            wi_lib_handle_close(lib);
            return false;
        }

        handle = handle->next;
    }

    wi_foreign_handle_t* new_handle = malloc(sizeof(wi_foreign_handle_t));

    if (!new_handle) {
        wi_lib_handle_close(lib);
        wi_state_error(state, "not enough memory to allocate a foreign handle");
        return false;
    }

    new_handle->lib        = lib;
    new_handle->next       = state->foreign_handles;
    state->foreign_handles = new_handle;

    return true;
}

wi_recovery_t*
wi_state_push_recovery(wi_state_t* state) {
    if (state->recovery_count >= WI_C_CALL_STACK_MAX) {
        wi_state_error(state, "too many error buffers (limit is %i)", WI_C_CALL_STACK_MAX);
    }

    wi_recovery_t* recovery = &state->recoveries[state->recovery_count++];

    recovery->frame_count     = state->frame_count;
    recovery->c_call_depth    = state->c_call_depth;
    recovery->stack_top       = state->stack_top;
    recovery->api_stack       = state->api_stack;
    recovery->temp_root_count = state->gc->temp_root_count;
    recovery->error           = NULL;

    return recovery;
}

void
wi_state_print_backtrace(wi_state_t* state) {
    for (int i = state->frame_count - 1; i >= 0; i--) {
        wi_call_frame_t* frame     = &state->frames[i];
        wi_prototype_t*  prototype = frame->closure->prototype;
        int              line      = prototype->lines.data[frame->ip - prototype->bytes.data - 1];
        fprintf(stderr, "   --> %s:%i", prototype->file_path, line);

        if (prototype->is_main) {
            fprintf(stderr, " in main function\n");
        } else if (prototype->name) {
            fprintf(stderr, " in %s()\n", prototype->name->chars);
        } else {
            fprintf(stderr, " in anonymous function\n");
        }
    }
}

static void
_state_close_upvalues(wi_state_t* state, wi_value_t* last);

void
wi_state_error(wi_state_t* state, const char* format, ...) {
    if (state->recovery_count > 0) {
        wi_recovery_t* recovery = &state->recoveries[--state->recovery_count];
        _state_close_upvalues(state, recovery->stack_top);

        state->frame_count         = recovery->frame_count;
        state->c_call_depth        = recovery->c_call_depth;
        state->stack_top           = recovery->stack_top;
        state->api_stack           = recovery->api_stack;
        state->gc->temp_root_count = recovery->temp_root_count;

        char* error;

        va_list args;
        va_start(args, format);
        int len = vsnprintf(NULL, 0, format, args);
        va_end(args);

        error = WI_GC_ALLOC(state->gc, char, len + 1);

        va_start(args, format);
        vsnprintf(error, (size_t)(len + 1), format, args);
        va_end(args);

        recovery->error = wi_take_cstring(state->gc, error, len);
        longjmp(recovery->jmp, WI_JMP_ERROR);
    }

    va_list args;
    va_start(args, format);

    fprintf(stderr, "runtime error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    va_end(args);
    wi_state_print_backtrace(state);

    _state_reset_stack(state);
    wi_gc_reset_roots(state->gc);
    longjmp(state->jmp, WI_JMP_ERROR);
}

void
wi_state_check_arity(wi_state_t* state, int arity, uint8_t arg_count, bool is_variadic) {
    if (is_variadic) {
        if (arg_count < arity) {
            wi_state_error(state, "expected at least %i arguments but got %hhu", arity, arg_count);
        }
    } else if (arg_count != arity) {
        wi_state_error(state, "expected %i arguments but got %hhu", arity, arg_count);
    }
}

void
wi_state_abort(wi_state_t* state) {
    _state_reset_stack(state);
    wi_gc_reset_roots(state->gc);
    longjmp(state->jmp, WI_JMP_ABORT);
}

void
wi_state_interrupt(wi_state_t* state) {
    state->interrupted = 1;
}

static void
_state_concat(wi_state_t* state) {
    wi_value_t a = wi_state_peek(state, 1);
    wi_value_t b = wi_state_top(state);

    char* a_chars;
    char* b_chars;
    int   a_len;
    int   b_len;
    bool  a_owned = false;
    bool  b_owned = false;

    if (wi_value_is_string(a)) {
        wi_string_t* s = wi_value_as_string(a);
        a_chars        = s->chars;
        a_len          = s->len;
    } else {
        a_chars = wi_value_to_string(a);
        a_len   = (int)strlen(a_chars);
        a_owned = true;
    }

    if (wi_value_is_string(b)) {
        wi_string_t* s = wi_value_as_string(b);
        b_chars        = s->chars;
        b_len          = s->len;
    } else {
        b_chars = wi_value_to_string(b);
        b_len   = (int)strlen(b_chars);
        b_owned = true;
    }

    int   len   = a_len + b_len;
    char* chars = WI_GC_ALLOC(state->gc, char, len + 1);

    memcpy(chars, a_chars, (size_t)a_len);
    memcpy(chars + a_len, b_chars, (size_t)b_len);
    chars[len] = '\0';

    if (a_owned) {
        free(a_chars);
    }

    if (b_owned) {
        free(b_chars);
    }

    wi_string_t* result = wi_take_cstring(state->gc, chars, len);

    wi_state_drop(state);
    wi_state_drop(state);
    wi_state_push(state, WI_MAKE_BOX_VALUE(result));
}

static void
_state_push_array(wi_state_t* state, int item_count) {
    wi_array_t* array = wi_new_array(state->gc);
    wi_gc_push_root(state->gc, (wi_box_t*)array);
    wi_value_buf_reserve(&array->items, item_count);

    wi_value_t* item_start = state->stack_top - item_count;

    if (item_count > 0) {
        memcpy(array->items.data, item_start, sizeof(wi_value_t) * (size_t)item_count);
    }

    array->items.count = item_count;
    state->stack_top   = item_start;
    wi_state_push(state, WI_MAKE_BOX_VALUE(array));
    wi_gc_pop_root(state->gc);
}

static int
_state_validate_index(wi_state_t* state, const char* target, wi_value_t value, int count) {
    if (!wi_value_is_real(value)) {
        wi_state_error(state, "%s index must be a real, got %s", target, wi_value_type(value));
    }

    int index = (int)wi_value_as_real(value);

    if (index < 0 || index >= count) {
        wi_state_error(state, "%s index out of range: %i", target, index);
    }

    return index;
}

static void
_state_subscript_set(wi_state_t* state, wi_value_t target, wi_value_t index, wi_value_t value) {
    if (wi_value_is_array(target)) {
        wi_array_t* array    = wi_value_as_array(target);
        int         i        = _state_validate_index(state, "array", index, array->items.count);
        array->items.data[i] = value;
        return;
    }

    if (wi_value_is_map(target)) {
        wi_map_t* map = wi_value_as_map(target);
        wi_table_set(&map->items, index, value);
        return;
    }

    wi_state_error(state, "cannot use operator '[]' on a value of type %s", wi_value_type(target));
    return;
}

static wi_value_t
_state_subscript_get(wi_state_t* state, wi_value_t target, wi_value_t index) {
    if (wi_value_is_string(target)) {
        wi_string_t* string = wi_value_as_string(target);
        int          i      = _state_validate_index(state, "string", index, string->len);

        char result[2];
        result[0] = string->chars[i];
        result[1] = '\0';

        return WI_MAKE_BOX_VALUE(wi_copy_cstring(state->gc, result, 1));
    }

    if (wi_value_is_array(target)) {
        wi_array_t* array = wi_value_as_array(target);
        int         i     = _state_validate_index(state, "array", index, array->items.count);
        return array->items.data[i];
    }

    if (wi_value_is_map(target)) {
        wi_map_t*  map = wi_value_as_map(target);
        wi_value_t value;

        if (wi_table_get(&map->items, index, &value)) {
            return value;
        }

        wi_state_error(state, "key error");
    }

    wi_state_error(state, "cannot use operator '[]' on a value of type %s", wi_value_type(target));
    return wi_make_null_value();
}

static wi_upvalue_t*
_state_capture_upvalue(wi_state_t* state, wi_value_t* local) {
    wi_upvalue_t* prev    = NULL;
    wi_upvalue_t* upvalue = state->open_upvalues;

    while (upvalue && upvalue->location > local) {
        prev    = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue && upvalue->location == local) {
        return upvalue;
    }

    wi_upvalue_t* new_upvalue = wi_new_upvalue(state->gc, local);
    new_upvalue->next         = upvalue;

    if (!prev) {
        state->open_upvalues = new_upvalue;
    } else {
        prev->next = new_upvalue;
    }

    return new_upvalue;
}

static void
_state_close_upvalues(wi_state_t* state, wi_value_t* last) {
    while (state->open_upvalues && state->open_upvalues->location >= last) {
        wi_upvalue_t* upvalue = state->open_upvalues;

        upvalue->closed   = *upvalue->location;
        upvalue->location = &upvalue->closed;

        state->open_upvalues = upvalue->next;
    }
}

static void
_state_call_foreign(wi_state_t* state, wi_foreign_t* foreign, uint8_t arg_count) {
    wi_state_check_arity(state, foreign->arity, arg_count, foreign->is_variadic);

    state->api_stack = state->stack_top - arg_count - 1;
    foreign->fn(state, arg_count);

    state->stack_top = state->api_stack + 1;
    state->api_stack = NULL;
}

static void
_state_call(wi_state_t* state, wi_closure_t* closure, uint8_t arg_count) {
    wi_prototype_t* prototype = closure->prototype;
    wi_state_check_arity(state, prototype->arity, arg_count, prototype->is_variadic);

    if (state->frame_count == WI_CALL_FRAMES_COUNT) {
        // we try to provide at least *some* context about where the overflow has happened
        state->frames[0]   = state->frames[state->frame_count - 1];
        state->frame_count = 1;
        wi_state_error(state, "stack overflow (limit is %i)", WI_CALL_FRAMES_COUNT);
    }

    wi_call_frame_t* frame = &state->frames[state->frame_count++];
    frame->closure         = closure;
    frame->ip              = prototype->bytes.data;

    if (prototype->is_variadic) {
        _state_push_array(state, arg_count - prototype->arity);
        frame->slots = state->stack_top - prototype->arity - 2;
    } else {
        frame->slots = state->stack_top - arg_count - 1;
    }
}

static void
_state_tail_call(wi_state_t* state, wi_call_frame_t* frame, wi_closure_t* closure, uint8_t arg_count) {
    wi_prototype_t* prototype = closure->prototype;
    wi_state_check_arity(state, prototype->arity, arg_count, prototype->is_variadic);
    _state_close_upvalues(state, frame->slots);

    if (prototype->is_variadic) {
        _state_push_array(state, arg_count - prototype->arity);
        wi_value_t* callee_slots = state->stack_top - prototype->arity - 2;
        memmove(frame->slots, callee_slots, sizeof(wi_value_t) * (size_t)(prototype->arity + 2));
        state->stack_top = frame->slots + prototype->arity + 2;
    } else {
        wi_value_t* callee_slots = state->stack_top - arg_count - 1;
        memmove(frame->slots, callee_slots, sizeof(wi_value_t) * (size_t)(arg_count + 1));
        state->stack_top = frame->slots + arg_count + 1;
    }

    frame->closure = closure;
    frame->ip      = prototype->bytes.data;
}

static void
_state_resolve_field(wi_state_t* state, wi_object_t* object, wi_value_t name, wi_value_t* value);

static wi_value_t
_state_resolve_invoke(wi_state_t* state, wi_value_t receiver, wi_value_t name) {
    wi_value_t function;

    if (wi_value_is_object(receiver)) {
        _state_resolve_field(state, wi_value_as_object(receiver), name, &function);
        return function;
    }

    wi_object_t* object = NULL;

    if (wi_value_is_string(receiver)) {
        object = state->string_obj;
    } else if (wi_value_is_array(receiver)) {
        object = state->array_obj;
    } else if (wi_value_is_map(receiver)) {
        object = state->map_obj;
    }

    if (!object) {
        wi_state_error(state, "value type %s has no functions", wi_value_type(receiver));
    }

    _state_resolve_field(state, object, name, &function);
    return function;
}

static void
_state_shift_receiver(wi_state_t* state, wi_value_t function, uint8_t arg_count) {
    wi_value_t* args = state->stack_top - arg_count;
    memmove(args + 1, args, sizeof(wi_value_t) * arg_count);
    args[0] = function;
    state->stack_top++;
}

static void
_state_resolve_field(wi_state_t* state, wi_object_t* object, wi_value_t name, wi_value_t* value) {
    if (wi_table_get(&object->fields, name, value)) {
        return;
    }

    if (object->name) {
        wi_state_error(state, "object %s has no field %s", object->name->chars, wi_value_as_cstring(name));
        return;
    }

    wi_state_error(state, "anonymous object has no field %s", wi_value_as_cstring(name));
}

static void
_state_set_field(wi_state_t* state, wi_value_t name, wi_value_t target) {
    if (!wi_value_is_object(target)) {
        wi_state_error(state, "cannot access fields on a value of type %s", wi_value_type(target));
    }

    wi_object_t* object = wi_value_as_object(target);
    wi_table_set(&object->fields, name, wi_state_top(state));
}

static char*
_state_read_file(wi_state_t* state, const char* file_path) {
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

static wi_closure_t*
_state_require(wi_state_t* state, wi_value_t path_value, wi_value_t name_value) {
    wi_call_frame_t* frame = wi_state_frame(state);
    wi_string_t*     name  = wi_value_as_string(name_value);
    char*            path  = wi_value_as_cstring(path_value);
    char*            src   = _state_read_file(state, path);

    if (wi_table_get(frame->closure->globals, name_value, NULL)) {
        wi_state_error(state, "variable %s is already defined", name->chars);
    }

    wi_prototype_t* prototype = wi_compile(state, path, src);
    wi_gc_push_root(state->gc, (wi_box_t*)prototype);
    free(src);

    if (!prototype) {
        wi_state_error(state, "failed to compile module %s", path);
    }

    wi_object_t* object = wi_new_object(state->gc, name);
    wi_gc_push_root(state->gc, (wi_box_t*)object);

    wi_table_set(&state->required, path_value, WI_MAKE_BOX_VALUE(object));
    wi_table_set(frame->closure->globals, name_value, WI_MAKE_BOX_VALUE(object));

    wi_gc_pop_root(state->gc);

    wi_closure_t* closure = wi_new_closure(state->gc, prototype, &object->fields);
    closure->is_required  = true;
    wi_gc_pop_root(state->gc);

    return closure;
}

static wi_run_result_t
_state_interpreter_loop(wi_state_t* state, int base_frame_count, bool drop_result) {
    wi_call_frame_t* frame = wi_state_frame(state);
    wi_opcode_t      opcode;

    register wi_value_t* constants = frame->closure->prototype->constants.data;
    register uint8_t*    ip        = frame->ip;

    static void* dispatch_table[] = {
#define WI_OPCODE(name, _) &&LABEL_##name,
#include "wi_opcodes.h"
#undef WI_OPCODE
    };

#define _UPDATE_FRAME(void)                                \
    frame     = wi_state_frame(state);                     \
    constants = frame->closure->prototype->constants.data; \
    ip        = frame->ip

#define _ERROR(...) \
    frame->ip = ip; \
    wi_state_error(state, __VA_ARGS__)

#define _DISPATCH(void)                    \
    if (WI_UNLIKELY(state->interrupted)) { \
        state->interrupted = 0;            \
        frame->ip          = ip;           \
        wi_state_abort(state);             \
    }                                      \
                                           \
    goto* dispatch_table[(opcode = _READ_BYTE())];
#define _OPCODE_LABEL(name) LABEL_##name

#define _READ_BYTE(void) *ip++
#define _READ_SHORT(void) (ip += 2, (uint16_t)(ip[-2] << 8 | ip[-1]))
#define _READ_CONSTANT(void) constants[_READ_SHORT()]

#define _BINARY_OP(op, maker)                                                                     \
    do {                                                                                          \
        wi_value_t b = wi_state_pop(state);                                                       \
        wi_value_t a = wi_state_pop(state);                                                       \
                                                                                                  \
        if (!wi_value_is_real(a) || !wi_value_is_real(b)) {                                       \
            _ERROR("cannot use operator '" #op "' on values of type %s and %s", wi_value_type(a), \
                   wi_value_type(b));                                                             \
        }                                                                                         \
                                                                                                  \
        wi_real_t a_real = wi_value_as_real(a);                                                   \
        wi_real_t b_real = wi_value_as_real(b);                                                   \
                                                                                                  \
        wi_state_push(state, maker(a_real op b_real));                                            \
    } while (false)
#define _BIT_OP(op)                                                                               \
    do {                                                                                          \
        wi_value_t b = wi_state_pop(state);                                                       \
        wi_value_t a = wi_state_pop(state);                                                       \
                                                                                                  \
        if (!wi_value_is_real(a) || !wi_value_is_real(b)) {                                       \
            _ERROR("cannot use operator '" #op "' on values of type %s and %s", wi_value_type(a), \
                   wi_value_type(b));                                                             \
        }                                                                                         \
                                                                                                  \
        int64_t a_int = (int64_t)wi_value_as_real(a);                                             \
        int64_t b_int = (int64_t)wi_value_as_real(b);                                             \
                                                                                                  \
        wi_state_push(state, wi_make_real_value((wi_real_t)(a_int op b_int)));                    \
    } while (false)

    _DISPATCH();
    {
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
            wi_value_t name = _READ_CONSTANT();

            if (wi_table_get(frame->closure->globals, name, NULL)) {
                _ERROR("variable %s is already defined", wi_value_as_cstring(name));
            }

            wi_table_set(frame->closure->globals, name, wi_state_top(state));
            wi_state_drop(state);
            _DISPATCH();
        }
        _OPCODE_LABEL(SET_GLOBAL) : {
            wi_value_t name = _READ_CONSTANT();

            if (wi_table_set(frame->closure->globals, name, wi_state_top(state))) {
                wi_table_delete(frame->closure->globals, name);
                _ERROR("variable %s is used but not defined", wi_value_as_cstring(name));
            }

            _DISPATCH();
        }
        _OPCODE_LABEL(GET_GLOBAL) : {
            wi_value_t name = _READ_CONSTANT();
            wi_value_t value;

            if (wi_table_get(frame->closure->globals, name, &value)) {
                wi_state_push(state, value);
                _DISPATCH();
            }

            if (wi_table_get(&state->foreign, name, &value)) {
                wi_state_push(state, value);
                _DISPATCH();
            }

            _ERROR("variable %s is used but not defined", wi_value_as_cstring(name));
        }
        _OPCODE_LABEL(STORE_LOCAL) : {
            frame->slots[_READ_BYTE()] = wi_state_top(state);
            _DISPATCH();
        }
        _OPCODE_LABEL(LOAD_LOCAL) : {
            wi_state_push(state, frame->slots[_READ_BYTE()]);
            _DISPATCH();
        }
        // clang-format off
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
        // clang-format on
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
            wi_value_t a = wi_state_pop(state);

            if (!wi_value_is_real(a)) {
                _ERROR("cannot use operator '-' on a value of type %s", wi_value_type(a));
            }

            wi_state_push(state, wi_make_real_value(-wi_value_as_real(a)));
            _DISPATCH();
        }
        _OPCODE_LABEL(POWER) : {
            wi_value_t b = wi_state_pop(state);
            wi_value_t a = wi_state_pop(state);

            if (!wi_value_is_real(a) || !wi_value_is_real(b)) {
                _ERROR("cannot use operator '**' on values of type %s and %s", wi_value_type(a), wi_value_type(b));
            }

            wi_state_push(state, wi_make_real_value(pow(wi_value_as_real(a), wi_value_as_real(b))));
            _DISPATCH();
        }
        _OPCODE_LABEL(MODULO) : {
            wi_value_t b = wi_state_pop(state);
            wi_value_t a = wi_state_pop(state);

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
            wi_value_t b = wi_state_pop(state);
            wi_value_t a = wi_state_pop(state);
            wi_state_push(state, wi_make_bool_value(wi_values_equal(a, b)));
            _DISPATCH();
        }
        _OPCODE_LABEL(NOT_EQUAL) : {
            wi_value_t b = wi_state_pop(state);
            wi_value_t a = wi_state_pop(state);
            wi_state_push(state, wi_make_bool_value(!wi_values_equal(a, b)));
            _DISPATCH();
        }
        _OPCODE_LABEL(LOG_NOT) : {
            wi_value_t value = wi_state_pop(state);
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
            wi_value_t a = wi_state_pop(state);

            if (!wi_value_is_real(a)) {
                _ERROR("cannot use operator '~' on a value of type %s", wi_value_type(a));
            }

            wi_state_push(state, wi_make_real_value((wi_real_t) ~(int64_t)wi_value_as_real(a)));
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
            wi_value_t a = wi_state_pop(state);

            if (wi_value_is_string(a)) {
                wi_state_push(state, wi_make_real_value(wi_value_as_string(a)->len));
                _DISPATCH();
            }

            if (wi_value_is_array(a)) {
                wi_state_push(state, wi_make_real_value(wi_value_as_array(a)->items.count));
                _DISPATCH();
            }

            if (wi_value_is_map(a)) {
                wi_table_t* table = &wi_value_as_map(a)->items;
                wi_state_push(state, wi_make_real_value(wi_table_count(table)));
                _DISPATCH();
            }

            _ERROR("cannot use operator '#' on a value of type '%s'", wi_value_type(a));
            _DISPATCH();
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

            if (wi_value_is_falsy(wi_state_top(state))) {
                ip += offset;
            }

            _DISPATCH();
        }
        _OPCODE_LABEL(LOOP) : {
            uint16_t offset = _READ_SHORT();
            ip -= offset;
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
            uint16_t  count = _READ_SHORT();
            wi_map_t* map   = wi_new_map(state->gc);
            wi_gc_push_root(state->gc, (wi_box_t*)map);
            wi_table_reserve(&map->items, count);

            wi_value_t* item_start = state->stack_top - count * 2;

            for (int i = 0; i < count; i++) {
                wi_value_t key   = item_start[i * 2];
                wi_value_t value = item_start[i * 2 + 1];
                wi_table_set(&map->items, key, value);
            }

            state->stack_top = item_start;
            wi_state_push(state, WI_MAKE_BOX_VALUE(map));
            wi_gc_pop_root(state->gc);
            _DISPATCH();
        }
        _OPCODE_LABEL(SUBSCRIPT_SET) : {
            wi_value_t value  = wi_state_top(state);
            wi_value_t index  = wi_state_peek(state, 1);
            wi_value_t target = wi_state_peek(state, 2);
            frame->ip         = ip;

            _state_subscript_set(state, target, index, value);

            wi_state_drop(state);
            wi_state_drop(state);
            wi_state_drop(state);
            wi_state_push(state, value);

            _DISPATCH();
        }
        _OPCODE_LABEL(SUBSCRIPT_GET) : {
            wi_value_t index  = wi_state_pop(state);
            wi_value_t target = wi_state_pop(state);
            frame->ip         = ip;

            wi_value_t result = _state_subscript_get(state, target, index);
            wi_state_push(state, result);
            _DISPATCH();
        }
        _OPCODE_LABEL(PUSH_CLOSURE) : {
            wi_prototype_t* prototype = wi_value_as_prototype(_READ_CONSTANT());
            wi_closure_t*   closure   = wi_new_closure(state->gc, prototype, frame->closure->globals);
            wi_state_push(state, WI_MAKE_BOX_VALUE(closure));

            for (int i = 0; i < closure->upvalue_count; i++) {
                uint8_t index    = _READ_BYTE();
                uint8_t is_local = _READ_BYTE();

                if (is_local) {
                    closure->upvalues[i] = _state_capture_upvalue(state, frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }

            _DISPATCH();
        }
        _OPCODE_LABEL(STORE_UPVALUE) : {
            *frame->closure->upvalues[_READ_BYTE()]->location = wi_state_top(state);
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
            uint8_t    arg_count = _READ_BYTE();
            wi_value_t value     = wi_state_peek(state, arg_count);
            frame->ip            = ip;

            if (wi_value_is_foreign(value)) {
                _state_call_foreign(state, wi_value_as_foreign(value), arg_count);
                _DISPATCH();
            }

            if (!wi_value_is_closure(value)) {
                _ERROR("cannot call a value of type %s", wi_value_type(value));
            }

            _state_call(state, wi_value_as_closure(value), arg_count);
            _UPDATE_FRAME();
            _DISPATCH();
        }
        _OPCODE_LABEL(TAIL_CALL) : {
            uint8_t    arg_count = _READ_BYTE();
            wi_value_t value     = wi_state_peek(state, arg_count);
            frame->ip            = ip;

            if (wi_value_is_foreign(value)) {
                _state_call_foreign(state, wi_value_as_foreign(value), arg_count);
                goto _OPCODE_LABEL(RETURN);
            }

            if (!wi_value_is_closure(value)) {
                _ERROR("cannot call a value of type %s", wi_value_type(value));
            }

            _state_tail_call(state, frame, wi_value_as_closure(value), arg_count);
            _UPDATE_FRAME();
            _DISPATCH();
        }
        _OPCODE_LABEL(INVOKE) : {
            wi_value_t name      = _READ_CONSTANT();
            uint8_t    arg_count = _READ_BYTE();
            wi_value_t receiver  = state->stack_top[-(int)arg_count];
            frame->ip            = ip;

            wi_value_t function = _state_resolve_invoke(state, receiver, name);
            _state_shift_receiver(state, function, arg_count);

            if (wi_value_is_foreign(function)) {
                _state_call_foreign(state, wi_value_as_foreign(function), arg_count);
                _DISPATCH();
            }

            if (!wi_value_is_closure(function)) {
                _ERROR("cannot call a value of type %s", wi_value_type(function));
            }

            _state_call(state, wi_value_as_closure(function), arg_count);
            _UPDATE_FRAME();
            _DISPATCH();
        }
        _OPCODE_LABEL(TAIL_INVOKE) : {
            wi_value_t name      = _READ_CONSTANT();
            uint8_t    arg_count = _READ_BYTE();
            wi_value_t receiver  = state->stack_top[-(int)arg_count];
            frame->ip            = ip;

            wi_value_t function = _state_resolve_invoke(state, receiver, name);
            _state_shift_receiver(state, function, arg_count);

            if (wi_value_is_foreign(function)) {
                _state_call_foreign(state, wi_value_as_foreign(function), arg_count);
                goto _OPCODE_LABEL(RETURN);
            }

            if (!wi_value_is_closure(function)) {
                _ERROR("cannot call a value of type %s", wi_value_type(function));
            }

            _state_tail_call(state, frame, wi_value_as_closure(function), arg_count);
            _UPDATE_FRAME();
            _DISPATCH();
        }
        _OPCODE_LABEL(RETURN) : {
            wi_value_t result = wi_state_pop(state);
            state->frame_count--;
            _state_close_upvalues(state, frame->slots);

            state->stack_top = frame->slots;

            if (!frame->closure->is_required) {
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
            uint16_t field_count = _READ_SHORT();
            uint8_t  has_name    = _READ_BYTE();

            wi_string_t* object_name = NULL;

            if (has_name) {
                object_name = wi_value_as_string(_READ_CONSTANT());
            }

            wi_object_t* object = wi_new_object(state->gc, object_name);
            wi_gc_push_root(state->gc, (wi_box_t*)object);
            wi_table_reserve(&object->fields, field_count);

            wi_value_t* field_start = state->stack_top - field_count * 2;

            for (uint16_t i = 0; i < field_count; i++) {
                wi_value_t name  = field_start[i * 2];
                wi_value_t value = field_start[i * 2 + 1];
                wi_table_set(&object->fields, name, value);
            }

            state->stack_top = field_start;
            wi_state_push(state, WI_MAKE_BOX_VALUE(object));
            wi_gc_pop_root(state->gc);
            _DISPATCH();
        }
        _OPCODE_LABEL(INIT_FIELD) : {
            wi_value_t name   = _READ_CONSTANT();
            wi_value_t target = wi_state_peek(state, 1);
            frame->ip         = ip;
            _state_set_field(state, name, target);

            wi_state_drop(state);
            _DISPATCH();
        }
        _OPCODE_LABEL(SET_FIELD) : {
            wi_value_t name   = _READ_CONSTANT();
            wi_value_t target = wi_state_peek(state, 1);
            frame->ip         = ip;
            _state_set_field(state, name, target);

            wi_value_t value = wi_state_pop(state);
            wi_state_drop(state);
            wi_state_push(state, value);
            _DISPATCH();
        }
        _OPCODE_LABEL(GET_FIELD) : {
            wi_value_t name   = _READ_CONSTANT();
            wi_value_t target = wi_state_top(state);

            if (!wi_value_is_object(target)) {
                _ERROR("cannot access fields on a value of type %s", wi_value_type(target));
            }

            wi_object_t* object = wi_value_as_object(target);
            wi_value_t   value;
            frame->ip = ip;
            _state_resolve_field(state, object, name, &value);

            wi_state_drop(state);
            wi_state_push(state, value);
            _DISPATCH();
        }
        _OPCODE_LABEL(NEW) : {
            wi_value_t target = wi_state_top(state);

            if (!wi_value_is_object(target)) {
                _ERROR("cannot use operator 'new' on a value of type %s", wi_value_type(target));
            }

            wi_object_t* object = wi_value_as_object(target);
            wi_object_t* clone  = wi_new_object(state->gc, object->name);

            wi_gc_push_root(state->gc, (wi_box_t*)clone);
            wi_table_copy(&object->fields, &clone->fields);
            wi_gc_pop_root(state->gc);

            wi_state_pop(state);
            wi_state_push(state, WI_MAKE_BOX_VALUE(clone));
            _DISPATCH();
        }
        _OPCODE_LABEL(REQUIRE) : {
            wi_value_t path_value = _READ_CONSTANT();
            wi_value_t name_value = _READ_CONSTANT();
            wi_value_t loaded;

            if (wi_table_get(&state->required, path_value, &loaded)) {
                if (wi_table_get(frame->closure->globals, name_value, NULL)) {
                    _ERROR("variable %s is already defined", wi_value_as_cstring(name_value));
                }

                wi_table_set(frame->closure->globals, name_value, loaded);
                _DISPATCH();
            }

            frame->ip             = ip;
            wi_closure_t* closure = _state_require(state, path_value, name_value);
            wi_state_push(state, WI_MAKE_BOX_VALUE(closure));
            _state_call(state, closure, 0);

            _UPDATE_FRAME();
            _DISPATCH();
        }
    }

#undef _UPDATE_FRAME

#undef _ERROR

#undef _DISPATCH
#undef _OPCODE_LABEL

#undef _READ_BYTE
#undef _READ_SHORT
#undef _READ_CONSTANT

#undef _BINARY_OP
#undef _BIT_OP
}

wi_run_result_t
wi_state_call(wi_state_t* state, wi_closure_t* closure, uint8_t arg_count, bool drop_result) {
    if (state->c_call_depth >= WI_C_CALL_STACK_MAX) {
        wi_state_error(state, "C call stack overflow (limit is %i)", WI_C_CALL_STACK_MAX);
    }

    wi_value_t* api_stack = state->api_stack;

    int base_frame_count = state->frame_count;
    _state_call(state, closure, arg_count);

    state->c_call_depth++;
    wi_run_result_t result = _state_interpreter_loop(state, base_frame_count, drop_result);
    state->c_call_depth--;

    state->api_stack = api_stack;

    return result;
}

wi_run_result_t
wi_state_run(wi_state_t* state, const char* file_path, const char* src) {
    state->interrupted        = 0;
    wi_prototype_t* prototype = wi_compile(state, file_path, src);

    if (!prototype) {
        return WI_RUN_ERROR;
    }

    wi_gc_push_root(state->gc, (wi_box_t*)prototype);
    wi_closure_t* closure = wi_new_closure(state->gc, prototype, &state->globals);
    wi_gc_pop_root(state->gc);

    wi_state_push(state, WI_MAKE_BOX_VALUE(closure));
    _state_call(state, closure, 0);

    int jmp_result = setjmp(state->jmp);

    if (jmp_result == WI_JMP_OK) {
        wi_run_result_t result = _state_interpreter_loop(state, 0, true);
        return result;
    }

    if (jmp_result == WI_JMP_ABORT) {
        return WI_RUN_ABORT;
    }

    return WI_RUN_ERROR;
}

wi_closure_t*
wi_slot_check_function(wi_state_t* state, int slot, int arity) {
    if (!wi_value_is_closure(state->api_stack[slot])) {
        wi_state_error(state, "bad argument %i - cannot use a value of type %s as a callback", slot,
                       wi_value_type(state->api_stack[slot]));
    }

    wi_closure_t* closure = wi_value_as_closure(state->api_stack[slot]);

    if (arity != -1 && closure->prototype->arity != arity) {
        wi_state_error(state, "callback must take %i arguments but takes %i", arity, closure->prototype->arity);
    }

    return closure;
}
