/* making clangd shut up */
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#else
    #define EMSCRIPTEN_KEEPALIVE
#endif /* __EMSCRIPTEN__ */

#include <stddef.h>

#include "../../include/wi.h"
#include "../../include/wi_conf.h"
#include "../core/wi_util.h"

/* while yes this "API" is dead simple, it's used only by that little Wi playground */
static wi_state* _g_state = NULL;
static wi_conf   _g_conf  = WI_DEFAULT_CONF;

#ifdef __EMSCRIPTEN__
EM_JS(void, _print_out, (struct wi_state * state, const char* text), {
    if (Module.print) {
        Module.print(UTF8ToString(text));
    }
})

EM_JS(void, _print_err, (struct wi_state * state, const char* text), {
    if (Module.printErr) {
        Module.printErr(UTF8ToString(text));
    }
})
#else
static void
_print_out(struct wi_state* state, const char* text) {
    WI_UNUSED(state);
    WI_UNUSED(text);
}

static void
_print_err(struct wi_state* state, const char* text) {
    WI_UNUSED(state);
    WI_UNUSED(text);
}
#endif /* __EMSCRIPTEN__ */

EMSCRIPTEN_KEEPALIVE void
wi_wasm_init(void) {
    if (_g_state) {
        wi_delete_state(_g_state);
    }

    _g_state = wi_new_state(&_g_conf);

    if (!_g_state) {
        return;
    }

    wi_state_set_callbacks(_g_state, _print_out, _print_err, NULL, NULL, NULL);

    if (wi_def_stm(_g_state) != WI_RUN_OK || wi_def_std(_g_state) != WI_RUN_OK) {
        wi_delete_state(_g_state);
        _g_state = NULL;
    }
}

EMSCRIPTEN_KEEPALIVE int
wi_wasm_run(const char* src) {
    if (!_g_state) {
        return WI_RUN_OOM;
    }

    int result = (int)wi_state_run(_g_state, "<web>", src);

    if (result == WI_RUN_OOM) {
        wi_delete_state(_g_state);
        _g_state = NULL;
    }

    return result;
}
