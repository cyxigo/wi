#include "wi_math.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "../include/wi.h"

#define M_E 2.7182818284590452354
#define M_PI 3.14159265358979323846

static void
_math_single_arg_function(struct wi_state* state, double (*fn)(double x)) {
    wi_slot_set_real(state, 0, fn(wi_slot_check_real(state, 1)));
}

static void
_math_abs(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, fabs);
}

static void
_math_acos(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, acos);
}

static void
_math_asin(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, asin);
}

static void
_math_atan(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, atan);
}

static void
_math_ceil(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, ceil);
}

static void
_math_cos(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, cos);
}

static void
_math_deg(struct wi_state* state, int arg_count) {
    wi_real rad = wi_slot_check_real(state, 1);
    wi_slot_set_real(state, 0, rad * (180.0 / M_PI));
}

static void
_math_exp(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, exp);
}

static void
_math_floor(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, floor);
}

static void
_math_mod(struct wi_state* state, int arg_count) {
    wi_real a = wi_slot_check_real(state, 1);
    wi_real b = wi_slot_check_real(state, 2);
    wi_slot_set_real(state, 0, fmod(a, b));
}

static void
_math_log(struct wi_state* state, int arg_count) {
    wi_real value = wi_slot_check_real(state, 1);
    wi_real base  = wi_slot_check_real(state, 2);
    wi_slot_set_real(state, 0, log(value) / log(base));
}

static void
_math_log10(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, log10);
}

static void
_math_ln(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, log);
}

static void
_math_max(struct wi_state* state, int arg_count) {
    wi_real max = wi_slot_check_real(state, 1);

    for (int i = 1; i < arg_count; i++) {
        wi_real arg = wi_slot_check_real(state, i + 1);

        if (arg > max) {
            max = arg;
        }
    }

    wi_slot_set_real(state, 0, max);
}

static void
_math_min(struct wi_state* state, int arg_count) {
    wi_real min = wi_slot_check_real(state, 1);

    for (int i = 1; i < arg_count; i++) {
        wi_real arg = wi_slot_check_real(state, i + 1);

        if (arg < min) {
            min = arg;
        }
    }

    wi_slot_set_real(state, 0, min);
}

static void
_math_pow(struct wi_state* state, int arg_count) {
    wi_real base = wi_slot_check_real(state, 1);
    wi_real exp  = wi_slot_check_real(state, 2);
    wi_slot_set_real(state, 0, pow(base, exp));
}

static void
_math_rad(struct wi_state* state, int arg_count) {
    wi_real deg = wi_slot_check_real(state, 1);
    wi_slot_set_real(state, 0, deg * (M_PI / 180.0));
}

static void
_math_random(struct wi_state* state, int arg_count) {
    wi_slot_set_real(state, 0, (wi_real)rand() / ((wi_real)RAND_MAX + 1.0));
}

static void
_math_round(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, round);
}

static void
_math_sin(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, sin);
}

static void
_math_sqrt(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, sqrt);
}

static void
_math_tan(struct wi_state* state, int arg_count) {
    _math_single_arg_function(state, tan);
}

void
wi_state_def_math_foreign(struct wi_state* state) {
    srand((unsigned)time(NULL));
    struct wi_object* object = wi_def_object(state, "math");

    wi_object_set_field_real(state, object, "E", M_E);
    wi_object_set_field_real(state, object, "PI", M_PI);
    wi_object_set_field_real(state, object, "HUGE", HUGE_VAL);

    wi_object_set_field_foreign(state, object, "abs", _math_abs, 1, false);
    wi_object_set_field_foreign(state, object, "acos", _math_acos, 1, false);
    wi_object_set_field_foreign(state, object, "asin", _math_asin, 1, false);
    wi_object_set_field_foreign(state, object, "atan", _math_atan, 1, false);
    wi_object_set_field_foreign(state, object, "ceil", _math_ceil, 1, false);
    wi_object_set_field_foreign(state, object, "cos", _math_cos, 1, false);
    wi_object_set_field_foreign(state, object, "deg", _math_deg, 1, false);
    wi_object_set_field_foreign(state, object, "exp", _math_exp, 1, false);
    wi_object_set_field_foreign(state, object, "floor", _math_floor, 1, false);
    wi_object_set_field_foreign(state, object, "mod", _math_mod, 2, false);
    wi_object_set_field_foreign(state, object, "log", _math_log, 2, false);
    wi_object_set_field_foreign(state, object, "log10", _math_log10, 1, false);
    wi_object_set_field_foreign(state, object, "ln", _math_ln, 1, false);
    wi_object_set_field_foreign(state, object, "max", _math_max, 2, true);
    wi_object_set_field_foreign(state, object, "min", _math_min, 2, true);
    wi_object_set_field_foreign(state, object, "pow", _math_pow, 2, false);
    wi_object_set_field_foreign(state, object, "rad", _math_rad, 1, false);
    wi_object_set_field_foreign(state, object, "random", _math_random, 0, false);
    wi_object_set_field_foreign(state, object, "round", _math_round, 1, false);
    wi_object_set_field_foreign(state, object, "sin", _math_sin, 1, false);
    wi_object_set_field_foreign(state, object, "sqrt", _math_sqrt, 1, false);
    wi_object_set_field_foreign(state, object, "tan", _math_tan, 1, false);
}
