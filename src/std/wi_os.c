#include "wi_os.h"

#include <stdlib.h>

#include "../include/wi.h"
#include "time.h"

static void
_os_clock(wi_state_t* state, int arg_count) {
    wi_slot_set_real(state, 0, (wi_real_t)clock() / (wi_real_t)CLOCKS_PER_SEC);
}

static void
_os_time(wi_state_t* state, int arg_count) {
    wi_slot_set_real(state, 0, (wi_real_t)time(NULL));
}

static void
_os_get_env(wi_state_t* state, int arg_count) {
    char* value = getenv(wi_slot_check_string(state, 1));

    if (!value) {
        wi_slot_set_null(state, 0);
        return;
    }

    wi_slot_set_string(state, 0, value);
}

void
wi_state_def_os_foreign(wi_state_t* state) {
    wi_object_t* object = wi_def_object(state, "os");

    wi_set_field_foreign(state, object, "clock", _os_clock, 0, false);
    wi_set_field_foreign(state, object, "time", _os_time, 0, false);
    wi_set_field_foreign(state, object, "get_env", _os_get_env, 1, false);
}
