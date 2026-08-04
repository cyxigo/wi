#ifndef WI_H
#define WI_H

#include <stdbool.h>
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
typedef double wi_real_t;
/**
 * Opaque Wi object handle
 */
typedef struct wi_object wi_object_t;

/**
 * The result of running Wi code
 */
typedef enum {
    WI_RUN_OK,    /* No errors occurred */
    WI_RUN_ERROR, /* A runtime error or a compile error occurred, also used when out of memory */
    WI_RUN_ABORT, /* Execution was aborted early via `wi_state_abort` */
} wi_run_result_t;

/**
 * Opaque Wi state handle
 */
typedef struct wi_state wi_state_t;

/**
 * Foreign (C) function pointer, called from Wi scripts
 */
typedef void (*wi_foreign_fn_t)(wi_state_t* state, int arg_count);
/**
 * Userdata finalizer - function called when the userdata gets collected by GC
 */
typedef void (*wi_userdata_finalizer_fn_t)(void* data);

/**
 * Function called in the `require` statement. Use this in a custom virtual filesystem (your app, for example).
 * Must return Wi code
 */
typedef char* (*wi_load_require_fn_t)(wi_state_t* state, const char* path);

/**
 * Function used to check whether a `require`d file exists, called at compile time.
 * Must return whether a file exists
 */
typedef bool (*wi_require_exists_fn_t)(wi_state_t* state, const char* path);

/**
 * Define the standard library in a state
 *
 * @param state Wi state instance
 */
WI_API void
wi_def_std(wi_state_t* state);

/**
 * Define a foreign (C) function in the state
 *
 * @param state Wi state instance
 * @param name Function name
 * @param fn Pointer to the C function implementation
 * @param arity Function's arity (number of arguments it expects)
 * @param is_variadic Whether function is variadic or not
 */
WI_API void
wi_def_foreign(wi_state_t* state, const char* name, wi_foreign_fn_t fn, int arity, bool is_variadic);

/**
 * Define an object in the state (global)
 *
 * @param state Wi state instance
 * @param name Object name
 * @return Pointer to the created object
 */
WI_API wi_object_t*
wi_def_object(wi_state_t* state, const char* name);

/**
 * Set a real field on an object
 *
 * @param state Wi state instance
 * @param object Target object
 * @param name Field name
 * @param real Value to set
 */
WI_API void
wi_object_set_field_real(wi_state_t* state, wi_object_t* object, const char* name, wi_real_t real);

/**
 * Set a boolean field on an object
 *
 * @param state Wi state instance
 * @param object Target object
 * @param name Field name
 * @param boolean Value to set
 */
WI_API void
wi_object_set_field_bool(wi_state_t* state, wi_object_t* object, const char* name, bool boolean);

/**
 * Set a string field on an object
 *
 * @param state Wi state instance
 * @param object Target object
 * @param name Field name
 * @param string Value to set
 */
WI_API void
wi_object_set_field_string(wi_state_t* state, wi_object_t* object, const char* name, char* string);

/**
 * Set userdata as a field on an object
 *
 * @param state Wi state instance
 * @param object Target object
 * @param field_name Field name
 * @param name Userdata name, used for type checking
 * @param userdata Pointer to userdata
 * @param finalizer Userdata finalizer
 */
WI_API void
wi_object_set_field_userdata(wi_state_t* state, wi_object_t* object, const char* field_name, const char* name,
                             void* userdata, wi_userdata_finalizer_fn_t finalizer);

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
wi_object_set_field_foreign(wi_state_t* state, wi_object_t* object, const char* name, wi_foreign_fn_t fn,
                            int arity, bool is_variadic);

/**
 * Create a new Wi state instance
 *
 * @param conf Wi configuration, see `wi_conf.h` for more
 * @return Created Wi state instance
 * @note Must be freed via `wi_delete_state`
 */
WI_API wi_state_t*
wi_new_state(wi_conf_t conf);

/**
 * Delete a Wi state instance and free all associated memory
 *
 * @param state Wi state instance
 */
WI_API void
wi_delete_state(wi_state_t* state);

/**
 * Get the error message from the last compile or runtime error
 *
 * @param state Wi state instance
 * @return Error message, `NULL` if none occurred
 */
WI_API const char*
wi_state_get_error(wi_state_t* state);

/**
 * Set the `require` load callback
 *
 * @param state Wi state instance
 * @param fn Load callback function
 */
WI_API void
wi_state_set_require_load_fn(wi_state_t* state, wi_load_require_fn_t fn);

/**
 * Set the `require` existence check callback
 *
 * @param state Wi state instance
 * @param fn Existence check callback function
 */
WI_API void
wi_state_set_require_exists_fn(wi_state_t* state, wi_require_exists_fn_t fn);

/**
 * Set the command line arguments that will be available to Wi scripts via os.args
 *
 * @param state Wi state instance
 * @param argc Number of arguments
 * @param argv Array of argument strings
 */
WI_API void
wi_state_set_args(wi_state_t* state, int argc, const char** argv);

/**
 * Throw a runtime error in the state
 *
 * @param state Wi state instance
 * @param format `printf` format string
 * @param ... Format arguments
 */
WI_API void
wi_state_error(wi_state_t* state, const char* format, ...);

/**
 * Request the state to stop execution, returning `WI_RUN_ABORT` from `wi_state_run`.
 *
 * Must only be called while a script is running (e.g., from a foreign (C) function).
 * Calling it outside `wi_state_run` is undefined behavior
 *
 * @param state Wi state instance
 */
WI_API void
wi_state_abort(wi_state_t* state);

/**
 * Request the state to stop execution as soon as possible, returning `WI_RUN_ABORT` from `wi_state_run`.
 *
 * In contrast to `wi_state_abort`, this function is safe to call
 * asynchronously (e.g., from a signal handler or another thread)
 *
 * @param state Wi state instance
 */
WI_API void
wi_state_interrupt(wi_state_t* state);

/**
 * Execute Wi code
 *
 * @param state Wi state instance
 * @param file_path Path to the script, used for error messages
 * @param src Code string
 * @return Run result
 */
WI_API wi_run_result_t
wi_state_run(wi_state_t* state, const char* file_path, const char* src);

/**
 * Function calling API. Example:
 * ```c
 * wi_find_function(state, "sum"); - Find the global function "sum" and push it onto the stack
 *
 * wi_push_real(state, 10); - Push argument 1
 * wi_push_real(state, 20); - Push argument 2
 * wi_call(state, 2); - Call function with `2` arguments, is protected
 *
 * wi_real_t sum = wi_check_real(state); - Get the function result, with type checking
 * printf("%g\n", sum); - Print it
 * ```
 */

/**
 * Find a *global* function and push it onto the stack
 *
 * @param state Wi state instance
 * @param name Function name
 */
WI_API bool
wi_find_function(wi_state_t* state, const char* name);

/**
 * Check if the value at the stack top is a real value
 *
 * @param state Wi state instance
 */
WI_API bool
wi_is_real(wi_state_t* state);

/**
 * Check if the value at the stack top is a null value
 *
 * @param state Wi state instance
 */
WI_API bool
wi_is_null(wi_state_t* state);

/**
 * Check if the value at the stack top is a boolean value
 *
 * @param state Wi state instance
 */
WI_API bool
wi_is_bool(wi_state_t* state);

/**
 * Check if the value at the stack top is a string value
 *
 * @param state Wi state instance
 */
WI_API bool
wi_is_string(wi_state_t* state);

/**
 * Check if the value at the stack top is userdata
 *
 * @param state Wi state instance
 */
WI_API bool
wi_is_userdata(wi_state_t* state, const char* name);

/**
 * Push a real value onto the stack
 *
 * @param state Wi state instance
 * @param real Real
 */
WI_API void
wi_push_real(wi_state_t* state, wi_real_t real);

/**
 * Push a null value onto the stack
 *
 * @param state Wi state instance
 */
WI_API void
wi_push_null(wi_state_t* state);

/**
 * Push a boolean value onto the stack
 *
 * @param state Wi state instance
 * @param boolean Boolean
 */
WI_API void
wi_push_bool(wi_state_t* state, bool boolean);

/**
 * Push a string value onto the stack
 *
 * @param state Wi state instance
 * @param string string
 */
WI_API void
wi_push_string(wi_state_t* state, const char* string);

/**
 * Push userdata onto the stack
 *
 * @param state Wi state instance
 * @param name Userdata name, used for type checking
 * @param userdata Pointer to userdata
 * @param finalizer Userdata finalizer
 */
WI_API void
wi_push_userdata(wi_state_t* state, const char* name, void* userdata, wi_userdata_finalizer_fn_t finalizer);

/**
 * Pop a real value from the stack
 *
 * @param state Wi state instance
 */
WI_API wi_real_t
wi_pop_real(wi_state_t* state);

/**
 * Pop a null value from the stack
 *
 * @param state Wi state instance
 */
WI_API void
wi_pop_null(wi_state_t* state);

/**
 * Pop a boolean value from the stack
 *
 * @param state Wi state instance
 */
WI_API bool
wi_pop_bool(wi_state_t* state);

/**
 * Pop a string value from the stack
 *
 * @param state Wi state instance
 * @param len Optional pointer to store the string length, can be `NULL`
 */
WI_API char*
wi_pop_string(wi_state_t* state, int* len);

/**
 * Pop userdata from the stack
 *
 * @param state Wi state instance
 */
WI_API void*
wi_pop_userdata(wi_state_t* state);

/**
 * Pop a real value from the stack with type-checking
 *
 * @param state Wi state instance
 */
WI_API wi_real_t
wi_check_real(wi_state_t* state);

/**
 * Pop a boolean value from the stack with type-checking
 *
 * @param state Wi state instance
 */
WI_API bool
wi_check_bool(wi_state_t* state);

/**
 * Pop a string value from the stack with type-checking
 *
 * @param state Wi state instance
 * @param len Optional pointer to store the string length, can be `NULL`
 */
WI_API char*
wi_check_string(wi_state_t* state, int* len);

/**
 * Pop userdata from the stack with type-checking
 *
 * @param state Wi state instance
 * @param name Userdata name, used for type checking
 */
WI_API void*
wi_check_userdata(wi_state_t* state, const char* name);

/**
 * Call a Wi function, is protected.
 *
 * Leaves the result on the stack top, which needs to be explicitly popped via one of `wi_pop_X` or `wi_check_X`
 * functions
 *
 * @param state Wi state instance
 * @param arg_count Argument count
 * @param error Optional pointer to store the error message (if any), can be `NULL`, must be freed manually
 */
WI_API bool
wi_call(wi_state_t* state, uint8_t arg_count, char** error);

/**
 * Slot functions are used in C functions to get arguments from the Wi caller
 * and to set the return value. Slot 0 is reserved for the return value.
 */

/**
 * Check if a slot contains a real value
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 */
WI_API bool
wi_slot_is_real(wi_state_t* state, int slot);

/**
 * Check if a slot contains a null value
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 */
WI_API bool
wi_slot_is_null(wi_state_t* state, int slot);

/**
 * Check if a slot contains a boolean value
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 */
WI_API bool
wi_slot_is_bool(wi_state_t* state, int slot);

/**
 * Check if a slot contains a string value
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 */
WI_API bool
wi_slot_is_string(wi_state_t* state, int slot);

/**
 * Check if a slot contains userdata
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 */
WI_API bool
wi_slot_is_userdata(wi_state_t* state, int slot);

/**
 * Store a real value in a slot
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 */
WI_API void
wi_slot_set_real(wi_state_t* state, int slot, wi_real_t real);

/**
 * Store a null value in a slot
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 */
WI_API void
wi_slot_set_null(wi_state_t* state, int slot);

/**
 * Store a boolean value in a slot
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 */
WI_API void
wi_slot_set_bool(wi_state_t* state, int slot, bool boolean);

/**
 * Store a string value in a slot
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 */
WI_API void
wi_slot_set_string(wi_state_t* state, int slot, const char* string);

/**
 * Store userdata in a slot
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 * @param name Userdata name, used for type checking
 * @param userdata Pointer to userdata
 * @param finalizer Userdata finalizer
 */
WI_API void
wi_slot_set_userdata(wi_state_t* state, int slot, const char* name, void* userdata,
                     wi_userdata_finalizer_fn_t finalizer);

/**
 * Get a real value from a slot
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 * @return Real stored in a slot
 */
WI_API wi_real_t
wi_slot_get_real(wi_state_t* state, int slot);

/**
 * Get a boolean value from a slot
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 * @return Boolean stored in a slot
 */
WI_API bool
wi_slot_get_bool(wi_state_t* state, int slot);

/**
 * Get a string value and its length from a slot
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 * @param len Optional pointer to store the string length, can be `NULL`
 * @return String stored in a slot
 */
WI_API char*
wi_slot_get_string(wi_state_t* state, int slot, int* len);

/**
 * Get userdata from a slot
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 * @return Userdata stored in a slot
 */
WI_API void*
wi_slot_get_userdata(wi_state_t* state, int slot);

/**
 * Get a real value from a slot with type-checking
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 * @return Real stored in a slot
 */
WI_API wi_real_t
wi_slot_check_real(wi_state_t* state, int slot);

/**
 * Get a boolean value from a slot with type-checking
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 * @return Boolean stored in a slot
 */
WI_API bool
wi_slot_check_bool(wi_state_t* state, int slot);

/**
 * Get a string value from a slot with type-checking
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 * @param len Optional pointer to store the string length, can be `NULL`
 * @return String stored in a slot
 */
WI_API char*
wi_slot_check_string(wi_state_t* state, int slot, int* len);

/**
 * Get userdata from a slot with type-checking
 *
 * @param state Wi state instance
 * @param slot Slot index (0-[arg_count])
 * @param name Userdata name, used for type checking
 * @return Userdata stored in a slot
 */
WI_API void*
wi_slot_check_userdata(wi_state_t* state, int slot, const char* name);

#endif
