#ifndef WI_H
#define WI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wi_conf.h"

/**
 * Platform-specific API export macros
 * WI_API: Used to mark all public API functions
 * WI_MODULE_EXPORT: Used to mark a foreign library's entry point (`wi_module_init`), which **must** return a Wi
 * module handle
 */
#ifdef _WIN32
#define WI_API __declspec(dllexport)
#define WI_MODULE_EXPORT __declspec(dllexport)
#elif defined(__clang__) || (defined(__GNUC__) && __GNUC__ > 4)
#define WI_API __attribute__((visibility("default")))
#define WI_MODULE_EXPORT __attribute__((visibility("default")))
#else
#define WI_API
#define WI_MODULE_EXPORT
#endif

/**
 * Define a table of foreign (C) functions as globals in one call.
 * Equivalent to calling `wi_push_foreign` + `wi_def` for each `wi_foreign_entry` in `functions`
 *
 * @param state Wi state instance
 * @param functions A C array of `wi_foreign_entry`
 */
#define WI_DEF_FOREIGN_ALL(state, functions)                                 \
    for (size_t i = 0; i < sizeof(functions) / sizeof(functions[0]); i++) {  \
        wi_foreign_entry* entry = &functions[i];                             \
        wi_push_foreign(state, entry->fn, entry->arity, entry->is_variadic); \
        wi_def(state, entry->name);                                          \
    }

/**
 * Set a table of foreign (C) functions as fields on an object in one call.
 * Equivalent to calling `wi_push_foreign` + `wi_object_set` for each `wi_foreign_entry` in `functions`
 *
 * @param state Wi state instance
 * @param object Target object
 * @param functions A C array of `wi_foreign_entry`
 */
#define WI_OBJECT_SET_FOREIGN_ALL(state, object, functions)                  \
    for (size_t i = 0; i < sizeof(functions) / sizeof(functions[0]); i++) {  \
        wi_foreign_entry* entry = &functions[i];                             \
        wi_push_foreign(state, entry->fn, entry->arity, entry->is_variadic); \
        wi_object_set(state, object, entry->name);                           \
    }

/**
 * Export a table of foreign (C) functions as exported variables in a module in one call.
 * Equivalent to calling `wi_push_foreign` + `wi_module_set` for each `wi_foreign_entry` in `functions`
 *
 * @param state Wi state instance
 * @param module Target module
 * @param functions A C array of `wi_foreign_entry`
 */
#define WI_MODULE_EXPORT_FOREIGN_ALL(state, module, functions)               \
    for (size_t i = 0; i < sizeof(functions) / sizeof(functions[0]); i++) {  \
        wi_foreign_entry* entry = &functions[i];                             \
        wi_push_foreign(state, entry->fn, entry->arity, entry->is_variadic); \
        wi_module_set(state, module, entry->name);                           \
    }

/**
 * Wi's number type
 */
typedef double wi_real;

/**
 * Handle to a persistent Wi value reference, see `wi_ref_create`
 */
typedef int wi_ref;

/**
 * Opaque Wi array handle
 */
typedef struct wi_array wi_array;

/**
 * Opaque Wi map handle
 */
typedef struct wi_map wi_map;

/**
 * Opaque Wi object handle
 */
typedef struct wi_object wi_object;

/**
 * Opaque Wi module handle
 */
typedef struct wi_module wi_module;

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
typedef void (*wi_print_fn)(struct wi_state* state, const char* text);

/**
 * Function called right after a successful compilation of a script.
 */
typedef void (*wi_on_compile_fn)(wi_state* state);

/**
 * Function called in the `import` statement. Use this in a custom virtual filesystem (your app, for example).
 * Must return Wi code
 */
typedef char* (*wi_import_load_fn)(wi_state* state, const char* path);

/**
 * Function used to check whether a `import`ed file exists, called at compile time. If it returns `false`, `import`
 * falls back to treating the path as a foreign library.
 * Must return whether a file exists
 */
typedef bool (*wi_import_exists_fn)(wi_state* state, const char* path);

/**
 * Foreign (C) function pointer, called from Wi scripts
 */
typedef void (*wi_foreign_fn)(wi_state* state, uint8_t arg_count);

/**
 * Userdata finalizer - function called when the userdata gets collected by GC
 */
typedef void (*wi_userdata_finalizer_fn)(void* data);

/**
 * A single { name, function, arity, is_variadic } row, used to register foreign (C) functions via
 * `WI_DEF_FOREIGN_ALL`/`WI_OBJECT_SET_FOREIGN_ALL`/`WI_MODULE_EXPORT_FOREIGN_ALL`
 */
typedef struct wi_foreign_entry {
    const char*   name;
    wi_foreign_fn fn;
    uint8_t       arity;
    bool          is_variadic;
} wi_foreign_entry;

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
 * Check if the last compile error occurred at EOF
 *
 * @param state Wi state instance
 * @return `true` if the last compile error occurred at EOF, `false` otherwise
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
 * @param import_load_fn Load import callback
 * @param import_exists_fn Import existence check callback
 */
WI_API void
wi_state_set_callbacks(wi_state* state, wi_print_fn out_fn, wi_print_fn error_fn, wi_on_compile_fn on_compile_fn,
                       wi_import_load_fn import_load_fn, wi_import_exists_fn import_exists_fn);

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
 * Set an opaque pointer on the state, for the embedder to attach application context to
 * (e.g. a widget, some handle, anything).
 *
 * Extra pointer is not touched by Wi in any way
 *
 * @param state Wi state instance
 * @param extra Pointer to store, or `NULL`
 */
WI_API void
wi_state_set_extra(wi_state* state, void* extra);

/**
 * Get the pointer set by `wi_state_set_extra`
 *
 * @param state Wi state instance
 * @return The extra pointer, or `NULL` if none was set
 */
WI_API void*
wi_state_get_extra(wi_state* state);

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
 * Create a persistent reference to the value at the stack top, popping it.
 *
 * Reference created by this function will survive until it is deleted (`wi_ref_delete`)
 * or the state is (`wi_delete_state`)
 *
 * @param state Wi state instance
 * @return The new reference
 */
WI_API wi_ref
wi_ref_create(wi_state* state);

/**
 * Push the value held by a reference onto the stack
 *
 * @param state Wi state instance
 * @param ref Target reference
 * @return `true` if the reference exists and its value was pushed, `false` if it does not exist (already
 * deleted or invalid)
 */
WI_API bool
wi_ref_push(wi_state* state, wi_ref ref);

/**
 * Release a reference, letting the garbage collector reclaim its value
 *
 * @param state Wi state instance
 * @param ref Target reference
 */
WI_API void
wi_ref_delete(wi_state* state, wi_ref ref);

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
 * Define the value at the stack top as a foreign (global) variable, popping it.
 * If the variable already exists, it is overwritten
 *
 * @param state Wi state instance
 * @param name Variable name
 */
WI_API void
wi_def(wi_state* state, const char* name);

/**
 * Find a global variable and push it onto the stack
 *
 * @param state Wi state instance
 * @param name Variable name
 * @return `true` if the variable was found and pushed, `false` otherwise
 */
WI_API bool
wi_find(wi_state* state, const char* name);

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
 * @return `true` if the call succeeded, `false` if it raised an error
 */
WI_API bool
wi_pcall(wi_state* state, uint8_t arg_count, bool drop, char** error);

/**
 * Get the type of the value at the stack top
 *
 * @param state Wi state instance
 * @return Type name
 */
WI_API const char*
wi_type(wi_state* state);

/**
 * Check if the value at the stack top is a real
 *
 * @param state Wi state instance
 * @return `true` if the value is a real, `false` otherwise
 */
WI_API bool
wi_is_real(wi_state* state);

/**
 * Check if the value at the stack top is null
 *
 * @param state Wi state instance
 * @return `true` if the value is null, `false` otherwise
 */
WI_API bool
wi_is_null(wi_state* state);

/**
 * Check if the value at the stack top is a bool
 *
 * @param state Wi state instance
 * @return `true` if the value is a bool, `false` otherwise
 */
WI_API bool
wi_is_bool(wi_state* state);

/**
 * Check if the value at the stack top is a string
 *
 * @param state Wi state instance
 * @return `true` if the value is a string, `false` otherwise
 */
WI_API bool
wi_is_string(wi_state* state);

/**
 * Check if the value at the stack top is an array
 *
 * @param state Wi state instance
 * @return `true` if the value is an array, `false` otherwise
 */
WI_API bool
wi_is_array(wi_state* state);

/**
 * Check if the value at the stack top is a map
 *
 * @param state Wi state instance
 * @return `true` if the value is a map, `false` otherwise
 */
WI_API bool
wi_is_map(wi_state* state);

/**
 * Check if the value at the stack top is a function (Wi or C)
 *
 * @param state Wi state instance
 * @return `true` if the value is a function (Wi or C), `false` otherwise
 */
WI_API bool
wi_is_function(wi_state* state);

/**
 * Check if the value at the stack top is an object
 *
 * @param state Wi state instance
 * @return `true` if the value is an object, `false` otherwise
 */
WI_API bool
wi_is_object(wi_state* state);

/**
 * Check if the value at the stack top is userdata
 *
 * @param state Wi state instance
 * @param name Userdata name, used for type-checking
 * @return `true` if the value is userdata of the given name, `false` otherwise
 */
WI_API bool
wi_is_userdata(wi_state* state, const char* name);

/**
 * Check if the value at the stack top is a module
 *
 * @param state Wi state instance
 * @return `true` if the value is a module, `false` otherwise
 */
WI_API bool
wi_is_module(wi_state* state);

/**
 * Push a real onto the stack
 *
 * @param state Wi state instance
 * @param real Real
 */
WI_API void
wi_push_real(wi_state* state, wi_real real);

/**
 * Push null onto the stack
 *
 * @param state Wi state instance
 */
WI_API void
wi_push_null(wi_state* state);

/**
 * Push a bool onto the stack
 *
 * @param state Wi state instance
 * @param bool_ Bool
 */
WI_API void
wi_push_bool(wi_state* state, bool bool_);

/**
 * Push a string onto the stack
 *
 * @param state Wi state instance
 * @param string String (**must** be valid UTF-8, invalid - undefined behaviour)
 */
WI_API void
wi_push_string(wi_state* state, const char* string);

/**
 * Push a new, empty array onto the stack
 *
 * @param state Wi state instance
 * @return Pointer to the created array
 */
WI_API wi_array*
wi_push_array(wi_state* state);

/**
 * Push a new, empty map onto the stack
 *
 * @param state Wi state instance
 * @return Pointer to the created map
 */
WI_API wi_map*
wi_push_map(wi_state* state);

/**
 * Push a foreign (C) function onto the stack
 *
 * @param state Wi state instance
 * @param fn Pointer to the C function implementation
 * @param arity Function's arity (number of arguments it expects)
 * @param is_variadic Whether function is variadic or not
 */
WI_API void
wi_push_foreign(wi_state* state, wi_foreign_fn fn, uint8_t arity, bool is_variadic);

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
 * Push a new, empty object onto the stack
 *
 * @param state Wi state instance
 * @return Pointer to the created object
 */
WI_API wi_object*
wi_push_object(wi_state* state);

/**
 * Push a new, empty module onto the stack.
 * `path` of said module will be just "foreign"
 *
 * @param state Wi state instance
 * @return Pointer to the created module
 */
WI_API wi_module*
wi_push_module(wi_state* state);

/**
 * Drop a value from the stack
 *
 * @param state Wi state instance
 */
WI_API void
wi_drop(wi_state* state);

/**
 * Pop a real from the stack with type-checking
 *
 * @param state Wi state instance
 * @return The popped real
 */
WI_API wi_real
wi_pop_real(wi_state* state);

/**
 * Pop null from the stack with type-checking
 *
 * @param state Wi state instance
 */
WI_API void
wi_pop_null(wi_state* state);

/**
 * Pop a bool from the stack with type-checking
 *
 * @param state Wi state instance
 * @return The popped bool
 */
WI_API bool
wi_pop_bool(wi_state* state);

/**
 * Pop a string from the stack with type-checking
 *
 * @param state Wi state instance
 * @param count Optional pointer to store the string byte count, can be `NULL`
 * @param len Optional pointer to store the string length (codepoint count), can be `NULL`
 * @return The popped string
 */
WI_API char*
wi_pop_string(wi_state* state, int* count, int* len);

/**
 * Pop an array from the stack with type-checking
 *
 * @param state Wi state instance
 * @return The popped array
 */
WI_API wi_array*
wi_pop_array(wi_state* state);

/**
 * Pop a map from the stack with type-checking
 *
 * @param state Wi state instance
 * @return The popped map
 */
WI_API wi_map*
wi_pop_map(wi_state* state);

/**
 * Pop userdata from the stack with type-checking
 *
 * @param state Wi state instance
 * @param name Userdata name, used for type-checking
 * @return The popped userdata's data pointer
 */
WI_API void*
wi_pop_userdata(wi_state* state, const char* name);

/**
 * Pop an object from the stack with type-checking
 *
 * @param state Wi state instance
 * @return The popped object
 */
WI_API wi_object*
wi_pop_object(wi_state* state);

/**
 * Pop a module from the stack with type-checking
 *
 * @param state Wi state instance
 * @return The popped module
 */
WI_API wi_module*
wi_pop_module(wi_state* state);

/**
 * Get the type name of an argument
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return Type name
 */
WI_API const char*
wi_arg_type(wi_state* state, uint8_t arg);

/**
 * Check if argument is a real
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return `true` if the argument is a real, `false` otherwise
 */
WI_API bool
wi_arg_is_real(wi_state* state, uint8_t arg);

/**
 * Check if argument is null
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return `true` if the argument is null, `false` otherwise
 */
WI_API bool
wi_arg_is_null(wi_state* state, uint8_t arg);

/**
 * Check if argument is a bool
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return `true` if the argument is a bool, `false` otherwise
 */
WI_API bool
wi_arg_is_bool(wi_state* state, uint8_t arg);

/**
 * Check if argument is a string
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return `true` if the argument is a string, `false` otherwise
 */
WI_API bool
wi_arg_is_string(wi_state* state, uint8_t arg);

/**
 * Check if argument is an array
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return `true` if the argument is an array, `false` otherwise
 */
WI_API bool
wi_arg_is_array(wi_state* state, uint8_t arg);

/**
 * Check if argument is a map
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return `true` if the argument is a map, `false` otherwise
 */
WI_API bool
wi_arg_is_map(wi_state* state, uint8_t arg);

/**
 * Check if argument is a function (Wi or C)
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return `true` if the argument is a function (Wi or C), `false` otherwise
 */
WI_API bool
wi_arg_is_function(wi_state* state, uint8_t arg);

/**
 * Check if argument is userdata
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @param name Userdata name, used for type-checking
 * @return `true` if the argument is userdata of the given name, `false` otherwise
 */
WI_API bool
wi_arg_is_userdata(wi_state* state, uint8_t arg, const char* name);

/**
 * Check if argument is an object
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return `true` if the argument is an object, `false` otherwise
 */
WI_API bool
wi_arg_is_object(wi_state* state, uint8_t arg);

/**
 * Check if argument is a module
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return `true` if the argument is a module, `false` otherwise
 */
WI_API bool
wi_arg_is_module(wi_state* state, uint8_t arg);

/**
 * Get a real argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return Real argument
 */
WI_API wi_real
wi_arg_real(wi_state* state, uint8_t arg);

/**
 * Type-check if argument is null
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 */
WI_API void
wi_arg_null(wi_state* state, uint8_t arg);

/**
 * Get a bool argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return Bool argument
 */
WI_API bool
wi_arg_bool(wi_state* state, uint8_t arg);

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
wi_arg_string(wi_state* state, uint8_t arg, int* count, int* len);

/**
 * Get an array argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return Array argument
 */
WI_API wi_array*
wi_arg_array(wi_state* state, uint8_t arg);

/**
 * Get a map argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return Map argument
 */
WI_API wi_map*
wi_arg_map(wi_state* state, uint8_t arg);

/**
 * Check if argument is a function, check it's arity, and push it onto the stack
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @param arity Function arity
 */
WI_API void
wi_arg_function(wi_state* state, uint8_t arg, uint8_t arity);

/**
 * Get userdata argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @param name Userdata name, used for type-checking
 * @return Userdata argument
 */
WI_API void*
wi_arg_userdata(wi_state* state, uint8_t arg, const char* name);

/**
 * Get an object argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return Object argument
 */
WI_API wi_object*
wi_arg_object(wi_state* state, uint8_t arg);

/**
 * Get a module argument with type-checking
 *
 * @param state Wi state instance
 * @param arg Argument index (1-[arg_count])
 * @return Module argument
 */
WI_API wi_module*
wi_arg_module(wi_state* state, uint8_t arg);

/**
 * Get the number of items in an array
 *
 * @param array Target array
 * @return The item count
 */
WI_API int
wi_array_count(wi_array* array);

/**
 * Append the value at the stack top to an array, popping it
 *
 * @param state Wi state instance
 * @param array Target array
 */
WI_API void
wi_array_add(wi_state* state, wi_array* array);

/**
 * Set the value at the stack top as an array item at index, popping it
 *
 * @param state Wi state instance
 * @param array Target array
 * @param index Item index
 * @return `true` if the index was in range and the item was set, `false` otherwise
 */
WI_API bool
wi_array_set(wi_state* state, wi_array* array, int index);

/**
 * Get an array item by index and push it onto the stack.
 *
 * @param state Wi state instance
 * @param array Target array
 * @param index Item index
 * @return `true` and pushes the item if the index was in range, `false` and pushes nothing otherwise
 */
WI_API bool
wi_array_get(wi_state* state, wi_array* array, int index);

/**
 * Get the number of entries in a map
 *
 * @param map Target map
 * @return The entry count
 */
WI_API int
wi_map_count(wi_map* map);

/**
 * Set map key to a value, both need to be at the stack top pushed in order key -> value
 *
 * @param state Wi state instance
 * @param map Target map
 */
WI_API void
wi_map_set(wi_state* state, wi_map* map);

/**
 * Look up map key which is at the stack top
 *
 * @param state Wi state instance
 * @param map Target map
 * @return `true` and pushes the value if the key was found, `false` and pushes nothing otherwise
 */
WI_API bool
wi_map_get(wi_state* state, wi_map* map);

/**
 * Iterate over a map's entries. Pass *iter = 0 on the first call.
 * Each call that finds a live entry pushes its key then value and advances *iter
 *
 * @param state Wi state instance
 * @param map Target map
 * @param iter Iteration cursor, updated in place
 * @return `true` and pushes key/value, `false` and pushes nothing once every entry has been visited
 */
WI_API bool
wi_map_next(wi_state* state, wi_map* map, int* iter);

/**
 * Set the value at the stack top as a field on an object, popping it
 *
 * @param state Wi state instance
 * @param object Target object
 * @param name Field name
 */
WI_API void
wi_object_set(wi_state* state, wi_object* object, const char* name);

/**
 * Get a field from an object and push it onto the stack
 *
 * @param state Wi state instance
 * @param object Target object
 * @param name Field name
 * @return `true` and pushes the value if the field exists, `false` and pushes nothing otherwise
 */
WI_API bool
wi_object_get(wi_state* state, wi_object* object, const char* name);

/**
 * Iterate over an object's fields. Pass *iter = 0 on the first call.
 * Each call that finds a live field entry pushes its name then value and advances *iter
 *
 * @param state Wi state instance
 * @param object Target object
 * @param iter Iteration cursor, updated in place
 * @return `true` and pushes name/value, `false` and pushes nothing once every field entry has been visited
 */
WI_API bool
wi_object_next(wi_state* state, wi_object* object, int* iter);

/**
 * Export the value at the stack top as a module variable, popping it
 *
 * @param state Wi state instance
 * @param module Target module
 * @param name Export name
 */
WI_API void
wi_module_set(wi_state* state, wi_module* module, const char* name);

/**
 * Get a global variable from a module and push it onto the stack
 *
 * @param state Wi state instance
 * @param module Target module
 * @param name Export name
 * @return `true` and pushes the value if the variable exists, `false` and pushes nothing otherwise
 */
WI_API bool
wi_module_get(wi_state* state, wi_module* module, const char* name);

/**
 * Iterate over a module's variables, including ones that were not exported. Pass *iter = 0 on the first call.
 * Each call that finds a live variable entry pushes its name then value and advances *iter
 *
 * @param state Wi state instance
 * @param module Target module
 * @param iter Iteration cursor, updated in place
 * @return `true` and pushes name/value, `false` and pushes nothing once every variable entry has been visited
 */
WI_API bool
wi_module_next(wi_state* state, wi_module* module, int* iter);

#endif
