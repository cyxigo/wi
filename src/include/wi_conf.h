#ifndef WI_CONF_H
#define WI_CONF_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Wi version as a string
 */
#define WI_VERSION_STRING "1.0.0"

enum {
    /**
     * Version numbers
     */
    WI_VERSION_MAJOR = 1,
    WI_VERSION_MINOR = 0,
    WI_VERSION_PATCH = 0,

    /**
     * Compiler limits
     */
    WI_CONSTANT_MAX = 65535,  // Maximum number of constants in a function
    WI_JUMP_MAX     = 65535,  // Maximum jump offset
    WI_LOOP_MAX     = 65535,  // Maximum loop offset
    WI_LOCALS_MAX   = 255,    // Maximum number of local variables in a function
    WI_UPVALUES_MAX = 255,    // Maximum number of upvalues in a function (closure)

    /**
     * Garbage Collector settings
     */
    WI_GC_MIN_HEAP         = 1048576,  // Initial heap size before first collection
    WI_GC_HEAP_GROW_FACTOR = 2,        // Heap growth factor per garbage collection run
    WI_GC_TEMP_ROOTS_MAX   = 16,       // Maximum number of temporary GC root references

    /**
     * VM limits
     */
    WI_CALL_FRAMES_COUNT = 16384,  // Maximum number of call frames
    WI_STACK_COUNT       = 65535,  // Maximum number of values on the VM stack
    WI_C_CALL_STACK_MAX  = 200,    // Maximum depth of nested wi_state_call
};

/**
 * Configuration flags for the Wi state
 */
typedef enum {
    WI_CONF_PRINT_CODE,  // Print bytecode after compilation
    WI_CONF_STRESS_GC,   // Run garbage collection on every allocation
    WI_CONF_LOG_GC,      // Log garbage collection
} wi_conf_flag_t;

/**
 * Configuration bitmask type
 */
typedef uint64_t wi_conf_t;

/**
 * Default configuration (all flags disabled)
 */
#define WI_DEFAULT_CONF 0

/**
 * Set a configuration flag
 *
 * @param conf Configuration bitmask
 * @param flag Configuration flag
 */
static inline void
wi_conf_set(wi_conf_t* conf, wi_conf_flag_t flag) {
    *conf |= (wi_conf_t)1 << flag;
}

/**
 * Check if a configuration flag is set
 *
 * @param conf Configuration bitmask
 * @param flag Configuration flag
 * @return `true` if the flag is set, `false` otherwise
 */
static inline bool
wi_conf_is_set(wi_conf_t conf, wi_conf_flag_t flag) {
    return conf & ((wi_conf_t)1 << flag);
}

#endif
