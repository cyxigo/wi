#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/wi_conf.h"
#include "wi_gc.h"
#include "wi_state.h"

static void
_version(void) {
    printf("wi " WI_VERSION_STRING "\n");
}

static void
_repl(void) {
    _version();

    char        line[2048];
    wi_state_t* state = wi_new_state(WI_DEFAULT_CONF);
    wi_def_std(state);

    for (;;) {
        printf("wi> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        wi_state_run(state, "<stdin>", line);
    }

    wi_delete_state(state);
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
    *file_path = NULL;

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
    if (argc == 1) {
        _repl();
        return EXIT_SUCCESS;
    }

    wi_conf_t   conf      = 0;
    const char* file_path = NULL;
    _parse_flags(argc, argv, &conf, &file_path);

    char*       src   = _read_file(file_path);
    wi_state_t* state = wi_new_state(conf);
    wi_def_std(state);

    if (!state) {
        free(src);
        fprintf(stderr, "memory error: failed to allocate a state\n");
        return EXIT_FAILURE;
    }

    int result = wi_state_run(state, file_path, src) ? EXIT_SUCCESS : EXIT_FAILURE;

    free(src);
    wi_delete_state(state);

    return result;
}
