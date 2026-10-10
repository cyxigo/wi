#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "wi_io.h"

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct _file {
    FILE* ptr;
    char* path;
    char* mode;
    bool  updating;
    bool  is_std;
};

static struct _file*
_file_new(FILE* ptr, const char* path, const char* mode, bool updating, bool is_std) {
    struct _file* file = (struct _file*)malloc(sizeof(struct _file));

    if (!file) {
        return NULL;
    }

    file->path = wi_strdup(path);
    file->mode = wi_strdup(mode);

    if (!file->path || !file->mode) {
        free(file->path);
        free(file->mode);
        free(file);
        return NULL;
    }

    file->ptr      = ptr;
    file->updating = updating;
    file->is_std   = is_std;
    return file;
}

static void
_file_close(struct _file* file) {
    if (!file->ptr) {
        return;
    }

    if (file->is_std) {
        if (file->mode[0] != 'r') {
            fflush(file->ptr);
        }

        return;
    }

    fclose(file->ptr);
    file->ptr = NULL;
}

static void
_file_finalizer(void* data) {
    struct _file* file = data;
    _file_close(file);
    free(file->path);
    free(file->mode);
    free(file);
}

static void
_file_check_open(struct wi_state* state, struct _file* file) {
    if (!file->ptr) {
        wi_state_error(state, "file %s is closed", file->path);
    }
}

static void
_file_check_read(struct wi_state* state, struct _file* file) {
    _file_check_open(state, file);

    if (file->mode[0] != 'r' && !file->updating) {
        wi_state_error(state, "file %s was not opened for reading (mode %s)", file->path, file->mode);
    }
}

static void
_file_check_write(struct wi_state* state, struct _file* file) {
    _file_check_open(state, file);

    if (file->mode[0] != 'w' && file->mode[0] != 'a' && !file->updating) {
        wi_state_error(state, "file %s was not opened for writing (mode %s)", file->path, file->mode);
    }
}

static bool
_file_write(struct wi_state* state, struct _file* file, const char* buf, size_t count) {
    if (!file->is_std) {
        return fwrite(buf, sizeof(char), count, file->ptr) == count;
    }

    (file->ptr == stderr ? state->error : state->out)(state, buf);
    return true;
}

static char*
_file_read_all(struct wi_state* state, struct _file* file, int* count) {
    _file_check_read(state, file);
    char* content = wi_read_stream(file->ptr, count);

    if (!content) {
        wi_state_error(state, "failed to read file %s", file->path);
    }

    return content;
}

static void
_io_exists(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    char* path = wi_arg_string(state, 1, NULL, NULL);
    wi_push_bool(state, wi_file_exists(path));
}

static void
_io_open(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);

    char* file_path = wi_arg_string(state, 1, NULL, NULL);
    int   mode_count;
    char* mode     = wi_arg_string(state, 2, &mode_count, NULL);
    bool  updating = mode_count == 2 && mode[1] == '+';

    /* r w a r+ w+ a+ */
    if (!(mode_count == 1 || updating) || (mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a')) {
        wi_state_error(state, "invalid file mode %s", mode);
    }

    char binary_mode[4];
    snprintf(binary_mode, sizeof(binary_mode), "%sb", mode);
    FILE* ptr = fopen(file_path, binary_mode);

    if (!ptr) {
        wi_state_error(state, "failed to open file %s: %s", file_path, strerror(errno));
    }

    struct _file* file = _file_new(ptr, file_path, mode, updating, false);

    if (!file) {
        fclose(ptr);
        wi_state_oom(state, "failed to allocate a file handle (_io_open)");
    }

    wi_push_userdata(state, "file", file, _file_finalizer);
}

static void
_io_close(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct _file* file = wi_arg_userdata(state, 1, "file");
    _file_close(file);
    wi_push_null(state);
}

static void
_io_flush(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct _file* file = wi_arg_userdata(state, 1, "file");
    _file_check_open(state, file);
    fflush(file->ptr);
    wi_push_null(state);
}

static void
_io_write(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct _file* file = wi_arg_userdata(state, 1, "file");
    _file_check_write(state, file);
    int   count;
    bool  owned;
    char* content = wi_value_to_buf(state->ffi_stack[2], &count, &owned);

    if (!content) {
        wi_state_oom(state, "failed to allocate file contents (_io_write)");
    }

    bool written = _file_write(state, file, content, (size_t)count);

    if (owned) {
        free(content);
    }

    if (!written) {
        wi_state_error(state, "failed to write file %s", file->path);
    }

    wi_push_null(state);
}

static void
_io_read(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct _file* file = wi_arg_userdata(state, 1, "file");

    int   count;
    char* content = _file_read_all(state, file, &count);

    if (!wi_utf8_validate(content, count)) {
        free(content);
        wi_state_error(state, "invalid utf-8 sequence in file %s", file->path);
    }

    struct wi_string* box = wi_take_calloc_string(state->gc, content, count);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

static void
_io_writebytes(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct _file* file = wi_arg_userdata(state, 1, "file");
    _file_check_write(state, file);

    struct wi_array* bytes = wi_arg_array(state, 2);
    uint8_t*         buf   = (uint8_t*)malloc((size_t)bytes->items.count + 1);

    if (!buf) {
        wi_state_oom(state, "failed to allocate a byte buffer (_io_writebytes)");
    }

    for (int i = 0; i < bytes->items.count; i++) {
        wi_value value = bytes->items.data[i];

        if (WI_UNLIKELY(!wi_value_is_real(value))) {
            free(buf);
            wi_state_error(state, "cannot write a value of type %s as a byte", wi_value_type(value));
        }

        wi_real real = wi_value_as_real(value);

        if (real != trunc(real) || real < 0 || real > 255) {
            free(buf);
            wi_state_error(state, "real " WI_REAL_FORMAT " has no byte representation", real);
        }

        buf[i] = (uint8_t)real;
    }

    buf[bytes->items.count] = '\0';
    bool written            = _file_write(state, file, (const char*)buf, (size_t)bytes->items.count);
    free(buf);

    if (!written) {
        wi_state_error(state, "failed to write file %s", file->path);
    }

    wi_push_null(state);
}

static void
_io_readbytes(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct _file* file = wi_arg_userdata(state, 1, "file");
    int           count;
    char*         content = _file_read_all(state, file, &count);

    struct wi_array* result = wi_push_array(state);
    wi_value_buf_reserve(&result->items, count);

    for (int i = 0; i < count; i++) {
        result->items.data[i] = wi_make_real_value((uint8_t)content[i]);
    }

    result->items.count = count;
    free(content);
}

static void
_io_seek(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct _file* file = wi_arg_userdata(state, 1, "file");
    _file_check_open(state, file);

    int64_t offset = wi_state_real_to_int(state, wi_arg_real(state, 2));
    int64_t whence = wi_state_real_to_int(state, wi_arg_real(state, 3));

    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        wi_state_error(state, "invalid seek origin: %lld", whence);
    }

#ifdef _WIN32
    if (_fseeki64(file->ptr, offset, (int)whence) != 0) {
        wi_state_error(state, "failed to seek file %s", file->path);
    }

    wi_push_real(state, (wi_real)_ftelli64(file->ptr));
#else
    if (fseeko(file->ptr, offset, (int)whence) != 0) {
        wi_state_error(state, "failed to seek file %s", file->path);
    }

    wi_push_real(state, (wi_real)ftello(file->ptr));
#endif
}

static void
_io_set_std(struct wi_state* state, struct wi_module* module, const char* name, FILE* ptr, const char* mode) {
    struct _file* file = _file_new(ptr, name, mode, false, true);

    if (!file) {
        wi_state_oom(state, "failed to allocate a standard stream handle (_io_set_std)");
    }

    wi_push_userdata(state, "file", file, _file_finalizer);
    wi_module_set(state, module, name);
}

void
wi_state_def_std_io(struct wi_state* state) {
    struct wi_module* module = wi_push_module(state);
    wi_def(state, "io");
    wi_foreign_entry functions[] = {
        {"exists",     _io_exists,     1, false},
        {"open",       _io_open,       2, false},
        {"close",      _io_close,      1, false},
        {"flush",      _io_flush,      1, false},
        {"write",      _io_write,      2, false},
        {"read",       _io_read,       1, false},
        {"writebytes", _io_writebytes, 2, false},
        {"readbytes",  _io_readbytes,  1, false},
        {"seek",       _io_seek,       3, false},
    };
    WI_MODULE_EXPORT_FOREIGN_ALL(state, module, functions);

    _io_set_std(state, module, "stdin", stdin, "r");
    _io_set_std(state, module, "stdout", stdout, "w");
    _io_set_std(state, module, "stderr", stderr, "w");

    wi_push_real(state, SEEK_SET);
    wi_module_set(state, module, "SEEK_SET");

    wi_push_real(state, SEEK_CUR);
    wi_module_set(state, module, "SEEK_CUR");

    wi_push_real(state, SEEK_END);
    wi_module_set(state, module, "SEEK_END");
}
