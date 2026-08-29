/* making clangd shut up */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <stddef.h>
#include <stdlib.h>

#include "../core/wi_util.h"
#include "../include/wi.h"
#include "../include/wi_conf.h"

/* while yes this API is dead simple, it's used only in "Try Wi online!" thingy */
static wi_state* _g_state = NULL;
static wi_conf   _g_conf  = WI_DEFAULT_CONF;

#ifdef __EMSCRIPTEN__
EM_JS(void, _print_out, (const char* text), {
    if (Module.print) {
        Module.print(UTF8ToString(text));
    }
})

EM_JS(void, _print_err, (const char* text), {
    if (Module.printErr) {
        Module.printErr(UTF8ToString(text));
    }
})
#else
static void
_print_out(const char* text) {
    WI_UNUSED(text);
}

static void
_print_err(const char* text) {
    WI_UNUSED(text);
}
#endif

static void
_wasm_print(void (*js_fn)(const char* text), const char* format, va_list args) {
    char* buf = wi_vasprintf(format, args);

    if (!buf) {
        return;
    }

    js_fn(buf);
    free(buf);
}

static void
_wasm_out(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _wasm_print(_print_out, format, args);
    va_end(args);
}

static void
_wasm_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _wasm_print(_print_err, format, args);
    va_end(args);
}

EMSCRIPTEN_KEEPALIVE void
wi_wasm_init(void) {
    if (_g_state) {
        wi_delete_state(_g_state);
    }

    _g_state = wi_new_state(&_g_conf);
    wi_state_set_callbacks(_g_state, _wasm_out, _wasm_error, NULL, NULL, NULL);

    wi_def_stm(_g_state);
    wi_def_std(_g_state);
}

EMSCRIPTEN_KEEPALIVE int
wi_wasm_run(const char* src) {
    return (int)wi_state_run(_g_state, "<web>", src);
}
