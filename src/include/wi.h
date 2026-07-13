#ifndef WI_H
#define WI_H

#include <stdbool.h>
#include <stdint.h>

#include "wi_conf.h"

#ifdef _WIN32
#define WI_API __declspec(dllexport)
#define WI_FOREIGN_INIT __declspec(dllexport)
#else
#define WI_API
#define WI_FOREIGN_INIT
#endif

typedef double          wi_real_t;
typedef struct wi_state wi_state_t;

typedef void (*wi_foreign_fn_t)(wi_state_t* state, int arg_count);
typedef void (*wi_userdata_finalizer_fn_t)(void* data);

WI_API void
wi_get_global(wi_state_t* state, const char* name, int slot);
WI_API void
wi_set_global(wi_state_t* state, const char* name, int slot);
WI_API bool
wi_has_global(wi_state_t* state, const char* name);

WI_API wi_state_t*
wi_new_state(wi_conf_t* conf);
WI_API void
wi_state_def_foreign(wi_state_t* state, const char* name, wi_foreign_fn_t fn, int arity);
WI_API void
wi_state_print_backtrace(wi_state_t* state);
WI_API void
wi_state_error(wi_state_t* state, const char* format, ...);
WI_API bool
wi_state_run(wi_state_t* state, const char* file_path, const char* src);
WI_API void
wi_delete_state(wi_state_t* state);

WI_API bool
wi_slot_is_real(wi_state_t* state, int slot);
WI_API bool
wi_slot_is_null(wi_state_t* state, int slot);
WI_API bool
wi_slot_is_bool(wi_state_t* state, int slot);
WI_API bool
wi_slot_is_string(wi_state_t* state, int slot);
WI_API bool
wi_slot_is_userdata(wi_state_t* state, int slot);

WI_API void
wi_slot_set_real(wi_state_t* state, int slot, wi_real_t value);
WI_API void
wi_slot_set_null(wi_state_t* state, int slot);
WI_API void
wi_slot_set_bool(wi_state_t* state, int slot, bool boolean);
WI_API void
wi_slot_set_string(wi_state_t* state, int slot, const char* string);
WI_API void
wi_slot_set_userdata(wi_state_t* state, int slot, const char* name, void* userdata,
                     wi_userdata_finalizer_fn_t finalizer);

WI_API wi_real_t
wi_slot_get_real(wi_state_t* state, int slot);
WI_API bool
wi_slot_get_bool(wi_state_t* state, int slot);
WI_API char*
wi_slot_get_string(wi_state_t* state, int slot);
WI_API void*
wi_slot_get_userdata(wi_state_t* state, int slot);

WI_API wi_real_t
wi_slot_check_real(wi_state_t* state, int slot);
WI_API bool
wi_slot_check_bool(wi_state_t* state, int slot);
WI_API char*
wi_slot_check_string(wi_state_t* state, int slot);
WI_API void*
wi_slot_check_userdata(wi_state_t* state, int slot, const char* name);

#endif
