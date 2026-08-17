#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "../include/wi.h"
#include "../include/wi_conf.h"
#include "wi_util.h"

static wi_state* _g_state = NULL;

static bool
_init_g_state(wi_conf conf) {
    _g_state = wi_new_state(conf);

    if (!_g_state) {
        return false;
    }

    wi_def_std(_g_state);
    return true;
}

static void
_delete_g_state(void) {
    wi_delete_state(_g_state);
    _g_state = NULL;
}

static void
_print_error(void) {
    printf("%s", wi_state_get_error(_g_state));
}

static void
_sigint_handler(int sig) {
    WI_UNUSED(sig);

    if (_g_state) {
        wi_state_interrupt(_g_state);
    }
}

static void
_version(void) {
    printf("Wi " WI_VERSION_STRING " Copyright (C) 2026 cyxigo\n");
}

static char*
_repl_append_line(char* buf, size_t* buf_len, char* line) {
    size_t line_len = strlen(line);
    /*
        offset where to start writing new line
        *buf_len > 0 checks whether we need a '\n' to glue onto the previous line
    */
    size_t new_buf_offset = *buf_len + (*buf_len > 0);
    char*  new_buf        = realloc(buf, new_buf_offset + line_len + 1);

    if (!new_buf) {
        fprintf(stderr, "out of memory: failed to allocate the repl input buffer\n");
        _delete_g_state();
        exit(EXIT_FAILURE);
    }

    if (new_buf_offset) {
        new_buf[*buf_len] = '\n';
    }

    /* write new line to the buffer! */
    memcpy(new_buf + new_buf_offset, line, line_len + 1);
    *buf_len = new_buf_offset + line_len;
    free(line);

    return new_buf;
}

static void
_repl(void) {
    _version();

    /* buffer for the whole repl input, multiline and not */
    char*  buf     = NULL;
    size_t buf_len = 0;

    for (;;) {
        char* line;

        if (!wi_read_line(&line, buf ? "... " : "> ")) {
            printf("\n");
            free(buf);
            break;
        }

        buf                  = _repl_append_line(buf, &buf_len, line);
        wi_run_result result = wi_state_run(_g_state, "<stdin>", buf);

        if (result == WI_RUN_ERROR) {
            if (wi_state_was_eof_error(_g_state)) {
                /* missing ';' or unclosed '('/'['/'{' */
                continue;
            } else {
                _print_error();
            }
        }

        free(buf);
        buf     = NULL;
        buf_len = 0;

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
    printf("    --                        treat all remaining arguments as script arguments\n");
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
_parse_flags(int argc, const char* argv[], wi_conf* conf, const char** file_path, int* script_argc,
             const char*** script_argv) {
    bool script_args = false;

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if (script_args) {
            *script_argc = argc - i;
            *script_argv = argv + i;
            break;
        }

        if (strcmp(arg, "--") == 0) {
            script_args = true;
            continue;
        }

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

    for (int j = 0; j < *script_argc; j++) {
        const char* arg = (*script_argv)[j];

        if (!wi_utf8_validate(arg, (int)strlen(arg))) {
            _flag_parse_error(argv[0], "invalid utf-8 sequence in script argument %i", j);
        }
    }
}

extern int
main(int argc, const char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    signal(SIGINT, _sigint_handler);
    wi_conf      conf        = WI_DEFAULT_CONF;
    const char*  file_path   = NULL;
    int          script_argc = 0;
    const char** script_argv = NULL;
    _parse_flags(argc, argv, &conf, &file_path, &script_argc, &script_argv);

    if (!_init_g_state(conf)) {
        fprintf(stderr, "memory error: failed to allocate a state\n");
        return EXIT_FAILURE;
    }

    if (!file_path) {
        _repl();
        return EXIT_SUCCESS;
    }

    char* src = _read_file(file_path);

    wi_state_set_args(_g_state, script_argc, script_argv);
    wi_run_result result = wi_state_run(_g_state, file_path, src);

    if (result == WI_RUN_ERROR) {
        _print_error();
    }

    free(src);
    _delete_g_state();

    return result == WI_RUN_ERROR ? EXIT_FAILURE : EXIT_SUCCESS;
}
