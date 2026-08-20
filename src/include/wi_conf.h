#ifndef WI_CONF_H
#define WI_CONF_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Wi version as a string
 */
#define WI_VERSION_STRING "7.5.1-beta"

/**
 * Format that Wi uses to print any real
 */
#define WI_REAL_FORMAT "%.15g"

enum {
    /**
     * Version numbers
     */
    WI_VERSION_MAJOR = 7,
    WI_VERSION_MINOR = 5,
    WI_VERSION_PATCH = 1,

    /**
     * Compiler limits
     */
    WI_CONSTANT_MAX  = 65535, /* Maximum number of constants in a function */
    WI_JUMP_MAX      = 65535, /* Maximum jump offset */
    WI_LOOP_MAX      = 65535, /* Maximum loop offset */
    WI_LOCAL_MAX     = 255,   /* Maximum number of local variables in a function */
    WI_UPVALUE_MAX   = 255,   /* Maximum number of upvalues in a function (closure) */
    WI_PARAMETER_MAX = 255,   /* Maximum number of parameters in a function and arguments in a call */

    /**
     * Garbage Collector settings
     */
    WI_GC_MIN_HEAP         = 10485760, /* Initial heap size before first collection (`10MB`) */
    WI_GC_HEAP_GROW_FACTOR = 2,        /* Heap growth factor per garbage collection run */

    /**
     * VM limits
     */
    WI_CSTACK_MAX = 200,     /* Maximum depth of nested `wi_state_call` and of recoveries */
    WI_STACK_MIN  = 20,      /* Minimum number of values on the VM stack */
    WI_STACK_MAX  = 1000000, /* Maximum number of values on the VM stack */
};

/**
 * Configuration flags for the Wi state
 */
typedef enum wi_conf_flag {
    WI_CONF_PRINT_CODE,  /* Print bytecode after compilation */
    WI_CONF_STRESS_GC,   /* Run garbage collection on every allocation */
    WI_CONF_LOG_GC,      /* Log garbage collection */
    WI_CONF_NO_WARNINGS, /* Suppress compiler warnings */
} wi_conf_flag;

/**
 * Configuration bitmask type
 */
typedef uint64_t wi_conf;

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
wi_conf_set(wi_conf* conf, wi_conf_flag flag) {
    *conf |= (wi_conf)1 << flag;
}

/**
 * Check if a configuration flag is set
 *
 * @param conf Configuration bitmask
 * @param flag Configuration flag
 * @return `true` if the flag is set, `false` otherwise
 */
static inline bool
wi_conf_is_set(wi_conf conf, wi_conf_flag flag) {
    return conf & ((wi_conf)1 << flag);
}

#endif
