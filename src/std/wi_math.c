#include "wi_math.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../../include/wi.h"

#define M_E 2.7182818284590452354
#define M_PI 3.14159265358979323846

static void
_math_single_arg_function(struct wi_state* state, double (*fn)(double x)) {
    wi_push_real(state, fn(wi_arg_real(state, 1)));
}

static void
_math_abs(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, fabs);
}

static void
_math_acos(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, acos);
}

static void
_math_asin(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, asin);
}

static void
_math_atan(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, atan);
}

static void
_math_ceil(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, ceil);
}

static void
_math_clamp(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_real real = wi_arg_real(state, 1);
    wi_real min  = wi_arg_real(state, 2);
    wi_real max  = wi_arg_real(state, 3);
    wi_push_real(state, real < min ? min : real > max ? max : real);
}

static void
_math_cos(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, cos);
}

static void
_math_deg(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_real rad = wi_arg_real(state, 1);
    wi_push_real(state, rad * (180.0 / M_PI));
}

static void
_math_exp(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, exp);
}

static void
_math_floor(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, floor);
}

static void
_math_mod(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_real a = wi_arg_real(state, 1);
    wi_real b = wi_arg_real(state, 2);
    wi_push_real(state, fmod(a, b));
}

static void
_math_log(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_real value = wi_arg_real(state, 1);
    wi_real base  = wi_arg_real(state, 2);
    wi_push_real(state, log(value) / log(base));
}

static void
_math_log10(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, log10);
}

static void
_math_ln(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, log);
}

static void
_math_max(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_real max = wi_arg_real(state, 1);

    for (int i = 1; i < arg_count; i++) {
        wi_real arg = wi_arg_real(state, i + 1);

        if (arg > max) {
            max = arg;
        }
    }

    wi_push_real(state, max);
}

static void
_math_min(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_real min = wi_arg_real(state, 1);

    for (int i = 1; i < arg_count; i++) {
        wi_real arg = wi_arg_real(state, i + 1);

        if (arg < min) {
            min = arg;
        }
    }

    wi_push_real(state, min);
}

static void
_math_pow(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_real base = wi_arg_real(state, 1);
    wi_real exp  = wi_arg_real(state, 2);
    wi_push_real(state, pow(base, exp));
}

static void
_math_rad(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_real deg = wi_arg_real(state, 1);
    wi_push_real(state, deg * (M_PI / 180.0));
}

static void
_math_random(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_push_real(state, (wi_real)rand() / ((wi_real)RAND_MAX + 1.0));
}

static void
_math_round(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, round);
}

static void
_math_sign(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    wi_real real = wi_arg_real(state, 1);
    wi_push_real(state, real > 0 ? 1 : real < 0 ? -1 : 0);
}

static void
_math_sin(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, sin);
}

static void
_math_sqrt(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, sqrt);
}

static void
_math_tan(struct wi_state* state, int arg_count) {
    WI_UNUSED(arg_count);
    _math_single_arg_function(state, tan);
}

void
wi_state_def_std_math(struct wi_state* state) {
    struct wi_object* object = wi_push_object(state);
    wi_def(state, "math");
    wi_foreign_entry functions[] = {
        {"abs",    _math_abs,    1, false},
        {"acos",   _math_acos,   1, false},
        {"asin",   _math_asin,   1, false},
        {"atan",   _math_atan,   1, false},
        {"ceil",   _math_ceil,   1, false},
        {"clamp",  _math_clamp,  3, false},
        {"cos",    _math_cos,    1, false},
        {"deg",    _math_deg,    1, false},
        {"exp",    _math_exp,    1, false},
        {"floor",  _math_floor,  1, false},
        {"mod",    _math_mod,    2, false},
        {"log",    _math_log,    2, false},
        {"log10",  _math_log10,  1, false},
        {"ln",     _math_ln,     1, false},
        {"max",    _math_max,    2, true },
        {"min",    _math_min,    2, true },
        {"pow",    _math_pow,    2, false},
        {"rad",    _math_rad,    1, false},
        {"random", _math_random, 0, false},
        {"round",  _math_round,  1, false},
        {"sin",    _math_sin,    1, false},
        {"sign",   _math_sign,   1, false},
        {"sqrt",   _math_sqrt,   1, false},
        {"tan",    _math_tan,    1, false},
    };

    WI_OBJECT_SET_FOREIGN_ALL(state, object, functions);

    wi_push_real(state, M_E);
    wi_object_set(state, object, "E");

    wi_push_real(state, M_PI);
    wi_object_set(state, object, "PI");

    wi_push_real(state, HUGE_VAL);
    wi_object_set(state, object, "HUGE");
}
