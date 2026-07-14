#include "wi_math.h"

#include <math.h>

#include "../include/wi.h"

#define M_E 2.7182818284590452354
#define M_PI 3.14159265358979323846

typedef double (*_math_single_arg_fn_t)(double x);

static void
_math_single_arg_function(wi_state_t* state, _math_single_arg_fn_t fn) {
    wi_slot_set_real(state, 0, fn(wi_slot_check_real(state, 1)));
}

static void
_math_abs(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, fabs);
}

static void
_math_acos(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, acos);
}

static void
_math_asin(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, asin);
}

static void
_math_atan(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, atan);
}

static void
_math_ceil(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, ceil);
}

static void
_math_cos(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, cos);
}

static void
_math_deg(wi_state_t* state, int arg_count) {
    wi_real_t rad = wi_slot_check_real(state, 1);
    wi_slot_set_real(state, 0, rad * (180.0 / M_PI));
}

static void
_math_exp(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, exp);
}

static void
_math_floor(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, floor);
}

static void
_math_mod(wi_state_t* state, int arg_count) {
    wi_real_t a = wi_slot_check_real(state, 1);
    wi_real_t b = wi_slot_check_real(state, 2);
    wi_slot_set_real(state, 0, fmod(a, b));
}

static void
_math_log(wi_state_t* state, int arg_count) {
    wi_real_t value = wi_slot_check_real(state, 1);
    wi_real_t base  = wi_slot_check_real(state, 2);
    wi_slot_set_real(state, 0, log(value) / log(base));
}

static void
_math_log10(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, log10);
}

static void
_math_ln(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, log);
}

static void
_math_max(wi_state_t* state, int arg_count) {
    if (arg_count == 0) {
        wi_slot_set_real(state, 0, 0);
        return;
    }

    wi_real_t max = wi_slot_check_real(state, 1);

    for (int i = 0; i < arg_count; i++) {
        wi_real_t arg = wi_slot_check_real(state, i + 1);

        if (arg > max) {
            max = arg;
        }
    }

    wi_slot_set_real(state, 0, max);
}

static void
_math_min(wi_state_t* state, int arg_count) {
    if (arg_count == 0) {
        wi_slot_set_real(state, 0, 0);
        return;
    }

    wi_real_t min = wi_slot_check_real(state, 1);

    for (int i = 0; i < arg_count; i++) {
        wi_real_t arg = wi_slot_check_real(state, i + 1);

        if (arg < min) {
            min = arg;
        }
    }

    wi_slot_set_real(state, 0, min);
}

static void
_math_pow(wi_state_t* state, int arg_count) {
    wi_real_t base = wi_slot_check_real(state, 1);
    wi_real_t exp  = wi_slot_check_real(state, 2);
    wi_slot_set_real(state, 0, pow(base, exp));
}

static void
_math_rad(wi_state_t* state, int arg_count) {
    wi_real_t deg = wi_slot_check_real(state, 1);
    wi_slot_set_real(state, 0, deg * (M_PI / 180.0));
}

static void
_math_sin(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, sin);
}

static void
_math_sqrt(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, sqrt);
}

static void
_math_tan(wi_state_t* state, int arg_count) {
    _math_single_arg_function(state, tan);
}

void
wi_state_def_math_foreign(wi_state_t* state) {
    wi_object_t* object = wi_state_def_object(state, "math");

    wi_set_field_real(state, object, "e", M_E);
    wi_set_field_real(state, object, "pi", M_PI);
    wi_set_field_real(state, object, "huge", HUGE_VAL);

    wi_set_field_foreign(state, object, "abs", _math_abs, 1);
    wi_set_field_foreign(state, object, "acos", _math_acos, 1);
    wi_set_field_foreign(state, object, "asin", _math_asin, 1);
    wi_set_field_foreign(state, object, "atan", _math_atan, 1);
    wi_set_field_foreign(state, object, "ceil", _math_ceil, 1);
    wi_set_field_foreign(state, object, "cos", _math_cos, 1);
    wi_set_field_foreign(state, object, "deg", _math_deg, 1);
    wi_set_field_foreign(state, object, "exp", _math_exp, 1);
    wi_set_field_foreign(state, object, "floor", _math_floor, 1);
    wi_set_field_foreign(state, object, "mod", _math_mod, 2);
    wi_set_field_foreign(state, object, "log", _math_log, 2);
    wi_set_field_foreign(state, object, "log10", _math_log10, 1);
    wi_set_field_foreign(state, object, "ln", _math_ln, 1);
    wi_set_field_foreign(state, object, "max", _math_max, -1);
    wi_set_field_foreign(state, object, "min", _math_min, -1);
    wi_set_field_foreign(state, object, "pow", _math_pow, 2);
    wi_set_field_foreign(state, object, "rad", _math_rad, 1);
    wi_set_field_foreign(state, object, "sin", _math_sin, 1);
    wi_set_field_foreign(state, object, "sqrt", _math_sqrt, 1);
    wi_set_field_foreign(state, object, "tan", _math_tan, 1);
}
