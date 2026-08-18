/* making clangd shut up */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <stddef.h>

#include "../include/wi.h"
#include "../include/wi_conf.h"

/* while yes this API is dead simple, it's used only in "Try Wi online!" thingy */
static wi_state* _g_state = NULL;

EMSCRIPTEN_KEEPALIVE void
wi_wasm_init(void) {
    if (_g_state) {
        wi_delete_state(_g_state);
    }

    _g_state = wi_new_state(WI_DEFAULT_CONF);

    wi_def_stm(_g_state);
    wi_def_std(_g_state);
}

EMSCRIPTEN_KEEPALIVE int
wi_wasm_run(const char* src) {
    return (int)wi_state_run(_g_state, "<web>", src);
}

EMSCRIPTEN_KEEPALIVE const char*
wi_wasm_get_error(void) {
    const char* error = wi_state_get_error(_g_state);
    return error ? error : "";
}
