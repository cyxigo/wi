#include <stdbool.h>
#include <string.h>

#include "../std/wi_array.h"
#include "../std/wi_base.h"
#include "../std/wi_map.h"
#include "../std/wi_math.h"
#include "../std/wi_os.h"
#include "../std/wi_string.h"
#include "../std/wi_utf8.h"
#include "wi_box.h"
#include "wi_gc.h"
#include "wi_state.h"
#include "wi_table.h"
#include "wi_value.h"

void
wi_def_std(wi_state_t* state) {
    wi_state_def_base_foreign(state);
    wi_state_def_os_foreign(state);
    wi_state_def_math_foreign(state);
    wi_state_def_utf8_foreign(state);
    wi_state_def_string_foreign(state);
    wi_state_def_array_foreign(state);
    wi_state_def_map_foreign(state);
}

static void
_def_foreign(wi_state_t* state, wi_table_t* table, const char* name, wi_foreign_fn_t fn, int arity,
             bool is_variadic) {
    wi_string_t* name_box = wi_copy_cstring(state->gc, name, (int)strlen(name));
    wi_gc_push_root(state->gc, (wi_box_t*)name_box);

    wi_foreign_t* foreign = wi_new_foreign(state->gc, fn, name_box, arity, is_variadic);
    wi_gc_push_root(state->gc, (wi_box_t*)(foreign));

    wi_table_set(table, WI_MAKE_BOX_VALUE(name_box), WI_MAKE_BOX_VALUE(foreign));

    wi_gc_pop_root(state->gc);
    wi_gc_pop_root(state->gc);
}

void
wi_def_foreign(wi_state_t* state, const char* name, wi_foreign_fn_t fn, int arity, bool is_variadic) {
    _def_foreign(state, &state->foreign, name, fn, arity, is_variadic);
}

wi_object_t*
wi_def_object(wi_state_t* state, const char* name) {
    wi_string_t* name_box = wi_copy_cstring(state->gc, name, (int)strlen(name));
    wi_gc_push_root(state->gc, (wi_box_t*)name_box);

    wi_object_t* object = wi_new_object(state->gc, name_box);
    wi_gc_push_root(state->gc, (wi_box_t*)object);

    wi_table_set(&state->foreign, WI_MAKE_BOX_VALUE(name_box), WI_MAKE_BOX_VALUE(object));

    wi_gc_pop_root(state->gc);
    wi_gc_pop_root(state->gc);

    return object;
}

static void
_set_field(wi_state_t* state, wi_object_t* object, const char* name, wi_value_t value) {
    bool is_box = wi_value_is_box(value);

    if (is_box) {
        wi_gc_push_root(state->gc, wi_value_as_box(value));
    }

    wi_string_t* name_box = wi_copy_cstring(state->gc, name, (int)strlen(name));
    wi_gc_push_root(state->gc, (wi_box_t*)name_box);
    wi_table_set(&object->fields, WI_MAKE_BOX_VALUE(name_box), value);

    if (is_box) {
        wi_gc_pop_root(state->gc);
    }

    wi_gc_pop_root(state->gc);
}

void
wi_set_field_real(wi_state_t* state, wi_object_t* object, const char* name, wi_real_t real) {
    _set_field(state, object, name, wi_make_real_value(real));
}

void
wi_set_field_bool(wi_state_t* state, wi_object_t* object, const char* name, bool boolean) {
    _set_field(state, object, name, wi_make_bool_value(boolean));
}

void
wi_set_field_string(wi_state_t* state, wi_object_t* object, const char* name, char* string) {
    wi_string_t* box = wi_copy_cstring(state->gc, string, (int)strlen(string));
    _set_field(state, object, name, WI_MAKE_BOX_VALUE(box));
}

void
wi_set_field_userdata(wi_state_t* state, wi_object_t* object, const char* field_name, const char* name,
                      void* userdata, wi_userdata_finalizer_fn_t finalizer) {
    wi_string_t* name_box = wi_copy_cstring(state->gc, name, (int)strlen(name));
    wi_gc_push_root(state->gc, (wi_box_t*)name_box);

    wi_userdata_t* box = wi_new_userdata(state->gc, name_box, userdata, finalizer);
    wi_gc_pop_root(state->gc);

    _set_field(state, object, field_name, WI_MAKE_BOX_VALUE(box));
}

void
wi_set_field_foreign(wi_state_t* state, wi_object_t* object, const char* name, wi_foreign_fn_t fn, int arity,
                     bool is_variadic) {
    _def_foreign(state, &object->fields, name, fn, arity, is_variadic);
}

static void
_slot_set(wi_state_t* state, int slot, wi_value_t value) {
    state->api_stack[slot] = value;
}

bool
wi_slot_is_real(wi_state_t* state, int slot) {
    return wi_value_is_real(state->api_stack[slot]);
}

bool
wi_slot_is_null(wi_state_t* state, int slot) {
    return wi_value_is_null(state->api_stack[slot]);
}

bool
wi_slot_is_bool(wi_state_t* state, int slot) {
    return wi_value_is_bool(state->api_stack[slot]);
}

bool
wi_slot_is_string(wi_state_t* state, int slot) {
    return wi_value_is_string(state->api_stack[slot]);
}

bool
wi_slot_is_userdata(wi_state_t* state, int slot) {
    return wi_value_is_userdata(state->api_stack[slot]);
}

void
wi_slot_set_real(wi_state_t* state, int slot, wi_real_t real) {
    _slot_set(state, slot, wi_make_real_value(real));
}

void
wi_slot_set_null(wi_state_t* state, int slot) {
    _slot_set(state, slot, wi_make_null_value());
}

void
wi_slot_set_bool(wi_state_t* state, int slot, bool boolean) {
    _slot_set(state, slot, wi_make_bool_value(boolean));
}

void
wi_slot_set_string(wi_state_t* state, int slot, const char* string) {
    wi_string_t* box = wi_copy_cstring(state->gc, string, (int)strlen(string));
    _slot_set(state, slot, WI_MAKE_BOX_VALUE(box));
}

void
wi_slot_set_userdata(wi_state_t* state, int slot, const char* name, void* userdata,
                     wi_userdata_finalizer_fn_t finalizer) {
    wi_string_t* name_box = wi_copy_cstring(state->gc, name, (int)strlen(name));
    wi_gc_push_root(state->gc, (wi_box_t*)name_box);

    wi_userdata_t* box = wi_new_userdata(state->gc, name_box, userdata, finalizer);
    wi_gc_pop_root(state->gc);

    _slot_set(state, slot, WI_MAKE_BOX_VALUE(box));
}

wi_real_t
wi_slot_get_real(wi_state_t* state, int slot) {
    return wi_value_as_real(state->api_stack[slot]);
}

bool
wi_slot_get_bool(wi_state_t* state, int slot) {
    return wi_value_as_bool(state->api_stack[slot]);
}

char*
wi_slot_get_string(wi_state_t* state, int slot) {
    return wi_value_as_cstring(state->api_stack[slot]);
}

void*
wi_slot_get_userdata(wi_state_t* state, int slot) {
    return wi_value_as_userdata(state->api_stack[slot])->data;
}

wi_real_t
wi_slot_check_real(wi_state_t* state, int slot) {
    if (!wi_slot_is_real(state, slot)) {
        wi_state_error(state, "bad argument %i - expected a value of type real but got %s", slot,
                       wi_value_type(state->api_stack[slot]));
    }

    return wi_slot_get_real(state, slot);
}

bool
wi_slot_check_bool(wi_state_t* state, int slot) {
    if (!wi_slot_is_bool(state, slot)) {
        wi_state_error(state, "bad argument %i - expected a value of type bool but got %s", slot,
                       wi_value_type(state->api_stack[slot]));
    }

    return wi_slot_get_bool(state, slot);
}

char*
wi_slot_check_string(wi_state_t* state, int slot) {
    if (!wi_slot_is_string(state, slot)) {
        wi_state_error(state, "bad argument %i - expected a value of type string but got %s", slot,
                       wi_value_type(state->api_stack[slot]));
    }

    return wi_slot_get_string(state, slot);
}

void*
wi_slot_check_userdata(wi_state_t* state, int slot, const char* name) {
    if (!wi_slot_is_userdata(state, slot)) {
        wi_state_error(state, "bad argument %i - expected a value of type userdata but got %s", slot,
                       wi_value_type(state->api_stack[slot]));
    }

    wi_userdata_t* userdata = wi_value_as_userdata(state->api_stack[slot]);

    if (strcmp(userdata->name->chars, name) != 0) {
        wi_state_error(state, "bad argument %i - expected userdata %s but got %s", slot, name,
                       userdata->name->chars);
    }

    return userdata->data;
}
