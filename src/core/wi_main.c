#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/wi_conf.h"
#include "wi_gc.h"
#include "wi_state.h"
#include "wi_util.h"

static wi_state_t* g_state = NULL;

static bool
_init_g_state(wi_conf_t conf) {
    g_state = wi_new_state(conf);

    if (!g_state) {
        return false;
    }

    wi_def_std(g_state);
    return true;
}

static void
_delete_g_state(void) {
    wi_delete_state(g_state);
    g_state = NULL;
}

static void
_sigint_handler(int sig) {
    if (g_state) {
        wi_state_interrupt(g_state);
    }
}

static void
_version(void) {
    printf("Wi " WI_VERSION_STRING " Copyright (C) 2026 cyxigo\n");
}

static void
_repl(void) {
    _version();

    char line[2048];

    if (!_init_g_state(WI_DEFAULT_CONF)) {
        fprintf(stderr, "memory error: failed to allocate a state\n");
        return;
    }

    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        wi_run_result_t result = wi_state_run(g_state, "<stdin>", line);

        if (result == WI_RUN_ABORT) {
            break;
        }
    }

    _delete_g_state();
}

static void
_read_error(const char* format, ...) {
    va_list args;
    va_start(args, format);

    fprintf(stderr, "read error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    va_end(args);
}

static char*
_read_file(const char* file_path) {
    FILE* file = fopen(file_path, "rb");

    if (!file) {
        _read_error("failed to open file %s", file_path);
        exit(EXIT_FAILURE);
    }

    char* buf = wi_read_stream(file);
    fclose(file);

    if (!buf) {
        _read_error("failed to read file %s", file_path);
        exit(EXIT_FAILURE);
    }

    return buf;
}

static void
_help(const char* exec_path) {
    printf("%s [script] [option]\n", exec_path);
    printf("options:\n");
    printf("    -h    --help              show this help message\n");
    printf("    -v    --version           show version information\n");
    printf("    -pc   --print-code        print bytecode after compilation\n");
    printf("    -sgc  --stress-gc         run garbage collection on every allocation\n");
    printf("    -lgc  --log-gc            log garbage collection\n");
}

static void
_flag_parse_error(const char* exec_path, const char* format, ...) {
    va_list args;
    va_start(args, format);

    fprintf(stderr, "error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    fprintf(stderr, "try '%s --help' for more info\n", exec_path);

    va_end(args);
    exit(EXIT_FAILURE);
}

static void
_parse_flags(int argc, const char* argv[], wi_conf_t* conf, const char** file_path) {
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if (arg[0] != '-') {
            if (*file_path) {
                fprintf(stderr, "error: multiple script files specified\n");
                exit(EXIT_FAILURE);
            }

            *file_path = arg;
            continue;
        }

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            _help(argv[0]);
            exit(EXIT_SUCCESS);
        }

        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            _version();
            exit(EXIT_SUCCESS);
        }

        if (strcmp(arg, "-pc") == 0 || strcmp(arg, "--print-code") == 0) {
            wi_conf_set(conf, WI_CONF_PRINT_CODE);
            continue;
        }

        if (strcmp(arg, "-sgc") == 0 || strcmp(arg, "--stress-gc") == 0) {
            wi_conf_set(conf, WI_CONF_STRESS_GC);
            continue;
        }

        if (strcmp(arg, "-lgc") == 0 || strcmp(arg, "--log-gc") == 0) {
            wi_conf_set(conf, WI_CONF_LOG_GC);
            continue;
        }

        _flag_parse_error(argv[0], "unknown option");
    }

    if (!*file_path) {
        _flag_parse_error(argv[0], "no script file specified");
    }
}

extern int
main(int argc, const char* argv[]) {
    signal(SIGINT, _sigint_handler);

    if (argc == 1) {
        _repl();
        return EXIT_SUCCESS;
    }

    wi_conf_t   conf      = WI_DEFAULT_CONF;
    const char* file_path = NULL;
    _parse_flags(argc, argv, &conf, &file_path);

    char* src = _read_file(file_path);

    if (!_init_g_state(conf)) {
        free(src);
        fprintf(stderr, "memory error: failed to allocate a state\n");
        return EXIT_FAILURE;
    }

    wi_run_result_t result = wi_state_run(g_state, file_path, src);
    free(src);
    _delete_g_state();

    return result == WI_RUN_ERROR ? EXIT_FAILURE : EXIT_SUCCESS;
}
