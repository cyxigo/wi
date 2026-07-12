#ifndef WI_CONF_H
#define WI_CONF_H

#include <stdbool.h>
#include <stdint.h>

#define WI_VERSION_STRING "1.0.0"

enum {
    WI_VERSION_MAJOR = 1,
    WI_VERSION_MINOR = 0,
    WI_VERSION_PATCH = 0,

    WI_CONSTANT_MAX = 1 << 16,
    WI_JUMP_MAX     = 1 << 16,
    WI_LOOP_MAX     = 1 << 16,

    WI_LOCALS_MAX   = 1 << 8,
    WI_UPVALUES_MAX = 1 << 8,

    WI_GC_HEAP_GROW_FACTOR = 2,
    WI_GC_TEMP_ROOTS_MAX   = 1 << 4,

    WI_CALL_FRAMES_COUNT = 1 << 14,
    WI_STACK_COUNT       = 1 << 16,
};

typedef enum {
    WI_CONF_PRINT_CODE,
    WI_CONF_STRESS_GC,
    WI_CONF_LOG_GC,
} wi_conf_flag_t;

typedef uint64_t wi_conf_t;

static inline void
wi_conf_set(wi_conf_t* conf, wi_conf_flag_t flag) {
    *conf |= (wi_conf_t)1 << flag;
}

static inline bool
wi_conf_is_set(wi_conf_t* conf, wi_conf_flag_t flag) {
    return *conf & ((wi_conf_t)1 << flag);
}

#endif
