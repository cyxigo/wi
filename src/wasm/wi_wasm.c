#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <stddef.h>

#include "../include/wi.h"
#include "../include/wi_conf.h"

static wi_state* g_state = NULL;

EMSCRIPTEN_KEEPALIVE void
wi_wasm_init(void) {
    if (g_state) {
        wi_delete_state(g_state);
    }

    g_state = wi_new_state(WI_DEFAULT_CONF);
    wi_def_std(g_state);
}

EMSCRIPTEN_KEEPALIVE int
wi_wasm_run(const char* src) {
    return (int)wi_state_run(g_state, "<web>", src);
}

EMSCRIPTEN_KEEPALIVE const char*
wi_wasm_get_error(void) {
    const char* err = wi_state_get_error(g_state);
    return err ? err : "";
}
