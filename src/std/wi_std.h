#ifndef WI_STD_H
#define WI_STD_H

#include "wi_array.h"
#include "wi_base.h"
#include "wi_map.h"
#include "wi_math.h"
#include "wi_os.h"
#include "wi_string.h"
#include "wi_utf8.h"

static inline void
wi_state_def_std(wi_state_t* state) {
    wi_state_def_base_foreign(state);
    wi_state_def_os_foreign(state);
    wi_state_def_math_foreign(state);
    wi_state_def_utf8_foreign(state);
    wi_state_def_string_foreign(state);
    wi_state_def_array_foreign(state);
    wi_state_def_map_foreign(state);
}

#endif
