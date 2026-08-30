#ifndef WI_H
#define WI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wi_conf.h"

/**
 * Platform-specific API export macros
 * WI_API: Used to mark all public API functions
 * WI_FOREIGN_INIT: Used to mark foreign library entry point (wi_foreign_init)
 */
#ifdef _WIN32
#define WI_API __declspec(dllexport)
#define WI_FOREIGN_INIT __declspec(dllexport)
#else
#define WI_API
#define WI_FOREIGN_INIT
#endif

/**
 * Wi's number type
 */
typedef double wi_real;
/**
 * Opaque Wi object handle
 */
typedef struct wi_object wi_object;

/**
 * Opaque Wi state handle
 */
typedef struct wi_state wi_state;

/**
 * The result of running Wi code
 */
typedef enum wi_run_result {
    WI_RUN_OK,    /* No errors occurred */
    WI_RUN_ERROR, /* A runtime error or a compile error occurred, also used when out of memory */
    WI_RUN_ABORT, /* Execution was aborted early via `wi_state_abort` */
} wi_run_result;

/* Callback to print... something. Used by Wi for standard/error output printing */
typedef void (*wi_print_fn)(const char* format, ...);

/**
 * Function called right after a successful compilation of a script.
 */
typedef void (*wi_on_compile_fn)(wi_state* state);

/**
 * Function called in the `require` statement. Use this in a custom virtual filesystem (your app, for example).
 * Must return Wi code
 */
typedef char* (*wi_load_require_fn)(wi_state* state, const char* path);

/**
 * Function used to check whether a `require`d file exists, called at compile time.
 * Must return whether a file exists
 */
typedef bool (*wi_require_exists_fn)(wi_state* state, const char* path);

/**
 * Foreign (C) function pointer, called from Wi scripts
 */
typedef void (*wi_foreign_fn)(wi_state* state, int arg_count);
/**
 * Userdata finalizer - function called when the userdata gets collected by GC
 */
typedef void (*wi_userdata_finalizer_fn)(void* data);

/**
 * Create a new Wi state instance
 *
 * @param conf Wi configuration, see `wi_conf.h` for more
 * @return Created Wi state instance
 * @note Must be freed via `wi_delete_state`
 */
WI_API wi_state*
wi_new_state(wi_conf* conf);

/**
 * Delete a Wi state instance and free all associated memory
 *
 * @param state Wi state instance
 */
WI_API void
wi_delete_state(wi_state* state);

/**
 * Tune the garbage collector's generational thresholds. Call right after `wi_new_state`, before
 * any `wi_state_run`. For default settings, see `wi_conf.h`
 *
 * @param state Wi state instance
 * @param min_heap Total heap size before the first major collection
 * @param heap_grow_factor Heap growth factor per major collection
 * @param young_max Young generation size before a minor collection
 */
WI_API void
wi_state_tune_gc(wi_state* state, size_t min_heap, size_t heap_grow_factor, size_t young_max);

/**
 * Checks if the last compile error occurred at EOF
 *
 * @param state Wi state instance
 */
WI_API bool
wi_state_was_eof_error(wi_state* state);

/**
 * Set the state callbacks. Safe to pass `NULL` for each. For more info about callbacks, check their definitions
 *
 * @param state Wi state instance
 * @param out_fn Standard output callback
 * @param error_fn Error output callback
 * @param on_compile_fn On compile callback
 * @param load_require_fn Load require callback
 * @param require_exists_fn Require existence check callback
 */
WI_API void
wi_state_set_callbacks(struct wi_state* state, wi_print_fn out_fn, wi_print_fn error_fn,
                       wi_on_compile_fn on_compile_fn, wi_load_require_fn load_require_fn,
                       wi_require_exists_fn require_exists_fn);

/**
 * Set the command line arguments that will be available to Wi scripts via os.args
 *
 * @param state Wi state instance
 * @param argc Number of arguments
 * @param argv Array of argument strings (**must** be valid UTF-8, invalid - undefined behaviour)
 */
WI_API void
wi_state_set_args(wi_state* state, int argc, const char** argv);

/**
 * Throw a runtime error in the state
 *
 * @param state Wi state instance
 * @param format Format string
 * @param ... Format arguments
 */
WI_API void
wi_state_error(wi_state* state, const char* format, ...);

/**
 * Request the state to stop execution, returning `WI_RUN_ABORT` from `wi_state_run`.
 *
 * Must only be called while a script is running (e.g., from a foreign (C) function).
 * Calling it outside `wi_state_run` is undefined behavior
 *
 * @param state Wi state instance
 */
WI_API void
wi_state_abort(wi_state* state);

/**
 * Request the state to stop execution as soon as possible, returning `WI_RUN_ABORT` from `wi_state_run`.
 *
 * In contrast to `wi_state_abort`, this function is safe to call
 * asynchronously (e.g., from a signal handler or another thread)
 *
 * @param state Wi state instance
 */
WI_API void
wi_state_interrupt(wi_state* state);

/**
 * Execute Wi code
 *
 * @param state Wi state instance
 * @param file_path Path to the script, used for error messages
 * @param src Code string
 * @return Run result
 */
WI_API wi_run_result
wi_state_run(wi_state* state, const char* file_path, const char* src);

/**
 * Define the standard library (STD) in a state
 *
 * @param state Wi state instance
 */
WI_API void
wi_def_std(wi_state* state);

/**
 * Define the standard method library (STM) in a state - methods for builtin types:
 * string, array, map. Only reachable via the `->` method-call operator
 *
 * @param state Wi state instance
 */
WI_API void
wi_def_stm(wi_state* state);

/**
 * Define a foreign (C) function in the state (global)
 *
 * @param state Wi state instance
 * @param name Function name
 * @param fn Pointer to the C function implementation
 * @param arity Function's arity (number of arguments it expects)
 * @param is_variadic Whether function is variadic or not
 */
WI_API void
wi_def_foreign(wi_state* state, const char* name, wi_foreign_fn fn, int arity, bool is_variadic);

/**
 * Define an object in the state (global)
 *
 * @param state Wi state instance
 * @param name Object name
 * @return Pointer to the created object
 */
WI_API wi_object*
wi_def_object(wi_state* state, const char* name);

/**
 * Set a real field on an object
 *
 * @param state Wi state instance
 * @param object Target object
 * @param name Field name
 * @param real Value to set
 */
WI_API void
wi_object_set_real(wi_state* state, wi_object* object, const char* name, wi_real real);

/**
 * Set a boolean field on an object
 *
 * @param state Wi state instance
 * @param object Target object
 * @param name Field name
 * @param boolean Value to set
 */
WI_API void
wi_object_set_bool(wi_state* state, wi_object* object, const char* name, bool boolean);

/**
 * Set a string field on an object
 *
 * @param state Wi state instance
 * @param object Target object
 * @param name Field name
 * @param string String (**must** be valid UTF-8, invalid - undefined behaviour)
 */
WI_API void
wi_object_set_string(wi_state* state, wi_object* object, const char* name, const char* string);

/**
 * Set userdata as a field on an object
 *
 * @param state Wi state instance
 * @param object Target object
 * @param field_name Field name
 * @param name Userdata name, used for type-checking
 * @param userdata Pointer to userdata
 * @param finalizer Userdata finalizer
 */
WI_API void
wi_object_set_userdata(wi_state* state, wi_object* object, const char* field_name, const char* name,
                       void* userdata, wi_userdata_finalizer_fn finalizer);

/**
 * Set a foreign (C) function as a field on an object
 *
 * @param state Wi state instance
 * @param object Target object
 * @param name Field name
 * @param fn Pointer to the C function implementation
 * @param arity Function's arity (number of arguments it expects)
 * @param is_variadic Whether function is variadic or not
 */
WI_API void
wi_object_set_foreign(wi_state* state, wi_object* object, const char* name, wi_foreign_fn fn, int arity,
                      bool is_variadic);

/**
 * Find a *global* function and push it onto the stack
 *
 * @param state Wi state instance
 * @param name Function name
 */
WI_API bool
wi_find_function(wi_state* state, const char* name);

/**
 * Call a Wi function that is *at the stack top*
 *
 * @param state Wi state instance
 * @param arg_count Argument count
 * @param drop Whether to leave the return value at the stack or not
 */
WI_API void
wi_call(wi_state* state, uint8_t arg_count, bool drop);

/**
 * Call a Wi function that is *at the stack top*; is protected
 *
 * @param state Wi state instance
 * @param arg_count Argument count
 * @param drop Whether to leave the return value at the stack or not
 * @param error Optional pointer to store the error message (if any), can be `NULL`, must be freed manually
 */
WI_API bool
wi_pcall(wi_state* state, uint8_t arg_count, bool drop, char** error);

/**
 * Check if the value at the stack top is a real value
 *
 * @param state Wi state instance
 */
WI_API bool
wi_is_real(wi_state* state);

/**
 * Check if the value at the stack top is a null value
 *
 * @param state Wi state instance
 */
WI_API bool
wi_is_null(wi_state* state);

/**
 * Check if the value at the stack top is a boolean value
 *
 * @param state Wi state instance
 */
WI_API bool
wi_is_bool(wi_state* state);

/**
 * Check if the value at the stack top is a string value
 *
 * @param state Wi state instance
 */
WI_API bool
wi_is_string(wi_state* state);

/**
 * Check if the value at the stack top is userdata
 *
 * @param state Wi state instance
 * @param name Userdata name, used for type-checking
 */
WI_API bool
wi_is_userdata(wi_state* state, const char* name);

/**
 * Push a real value onto the stack
 *
 * @param state Wi state instance
 * @param real Real
 */
WI_API void
wi_push_real(wi_state* state, wi_real real);

/**
 * Push a null value onto the stack
 *
 * @param state Wi state instance
 */
WI_API void
wi_push_null(wi_state* state);

/**
 * Push a boolean value onto the stack
 *
 * @param state Wi state instance
 * @param boolean Boolean
 */
WI_API void
wi_push_bool(wi_state* state, bool boolean);

/**
 * Push a string value onto the stack
 *
 * @param state Wi state instance
 * @param string String (**must** be valid UTF-8, invalid - undefined behaviour)
 */
WI_API void
wi_push_string(wi_state* state, const char* string);

/**
 * Push userdata onto the stack
 *
 * @param state Wi state instance
 * @param name Userdata name, used for type-checking
 * @param userdata Pointer to userdata
 * @param finalizer Userdata finalizer
 */
WI_API void
wi_push_userdata(wi_state* state, const char* name, void* userdata, wi_userdata_finalizer_fn finalizer);

/**
 * Drop a value from the stack
 *
 * @param state Wi state instance
 */
WI_API void
wi_drop(wi_state* state);

/**
 * Pop a real value from the stack with type-checking
 *
 * @param state Wi state instance
 */
WI_API wi_real
wi_pop_real(wi_state* state);

/**
 * Pop a null value from the stack with type-checking
 *
 * @param state Wi state instance
 */
WI_API void
wi_pop_null(wi_state* state);

/**
 * Pop a boolean value from the stack with type-checking
 *
 * @param state Wi state instance
 */
WI_API bool
wi_pop_bool(wi_state* state);

/**
 * Pop a string value from the stack with type-checking
 *
 * @param state Wi state instance
 * @param count Optional pointer to store the string byte count, can be `NULL`
 * @param len Optional pointer to store the string length (codepoint count), can be `NULL`
 */
WI_API char*
wi_pop_string(wi_state* state, int* count, int* len);

/**
 * Pop userdata from the stack with type-checking
 *
 * @param state Wi state instance
 * @param name Userdata name, used for type-checking
 */
WI_API void*
wi_pop_userdata(wi_state* state, const char* name);

/**
 * Check if argument is a real value
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 */
WI_API bool
wi_arg_is_real(wi_state* state, int arg);

/**
 * Check if argument is a null value
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 */
WI_API bool
wi_arg_is_null(wi_state* state, int arg);

/**
 * Check if argument is a boolean value
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 */
WI_API bool
wi_arg_is_bool(wi_state* state, int arg);

/**
 * Check if argument is a string value
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 */
WI_API bool
wi_arg_is_string(wi_state* state, int arg);

/**
 * Check if argument is a function value
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 */
WI_API bool
wi_arg_is_function(wi_state* state, int arg);

/**
 * Check if argument is userdata
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @param name Userdata name, used for type-checking
 */
WI_API bool
wi_arg_is_userdata(wi_state* state, int arg, const char* name);

/**
 * Get a real argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return Real stored in a slot
 */
WI_API wi_real
wi_arg_real(wi_state* state, int arg);

/**
 * Type-check if argument is a null value
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 */
WI_API void
wi_arg_null(wi_state* state, int arg);

/**
 * Get a boolean argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return Boolean argument
 */
WI_API bool
wi_arg_bool(wi_state* state, int arg);

/**
 * Get a string argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @param count Optional pointer to store the string byte count, can be `NULL`
 * @param len Optional pointer to store the string length (codepoint count), can be `NULL`
 * @return String argument
 */
WI_API char*
wi_arg_string(wi_state* state, int arg, int* count, int* len);

/**
 * Check if argument is a function, check it's arity, and push it onto the stack
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @param arity Function arity
 */
WI_API void
wi_arg_function(wi_state* state, int arg, uint8_t arity);

/**
 * Get userdata argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @param name Userdata name, used for type-checking
 * @return Userdata argument
 */
WI_API void*
wi_arg_userdata(wi_state* state, int arg, const char* name);

#endif
