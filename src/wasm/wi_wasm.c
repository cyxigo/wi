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

#ifdef __EMSCRIPTEN__
EM_JS(void, _print_warnings, (const char* warnings), {
    if (Module.printWarn) {
        Module.printWarn(UTF8ToString(warnings));
    }
})
#else
static void
_print_warnings(const char* warnings) {
    (void)warnings; /* WI_UNUSED */
}
#endif

static void
_on_compile(wi_state* state) {
    const char* warnings = wi_state_get_warnings(state);

    if (warnings) {
        _print_warnings(warnings);
    }
}

EMSCRIPTEN_KEEPALIVE void
wi_wasm_init(void) {
    if (_g_state) {
        wi_delete_state(_g_state);
    }

    _g_state = wi_new_state(WI_DEFAULT_CONF);
    wi_state_set_on_compile_fn(_g_state, _on_compile);

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
