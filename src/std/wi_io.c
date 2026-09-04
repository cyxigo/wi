#include "wi_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct wi_file {
    FILE* ptr;
    char* path;
    char* mode;
    bool  updating;
};

static void
_file_close(struct wi_file* file) {
    if (file->ptr) {
        fclose(file->ptr);
        file->ptr = NULL;
    }
}

static void
_file_finalizer(void* data) {
    struct wi_file* file = data;
    _file_close(file);
    free(file->path);
    free(file->mode);
    free(file);
}

static void
_file_check_open(struct wi_state* state, struct wi_file* file) {
    if (!file->ptr) {
        wi_state_error(state, "file %s is closed", file->path);
    }
}

static void
_io_open(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);

    char* file_path = wi_arg_string(state, 1, NULL, NULL);
    int   mode_count;
    char* mode     = wi_arg_string(state, 2, &mode_count, NULL);
    bool  updating = false;

    /* r w a */
    if (mode_count == 0 || (mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a')) {
        wi_state_error(state, "invalid file mode %s", mode);
    }

    /* r+ w+ a+ */
    if (mode_count == 2 && mode[1] == '+') {
        updating = true;
    } else if (mode_count != 1) {
        wi_state_error(state, "invalid file mode %s", mode);
    }

    FILE* ptr = fopen(file_path, mode);

    if (!ptr) {
        wi_state_error(state, "failed to open file %s", file_path);
    }

    struct wi_file* file = malloc(sizeof(struct wi_file));

    if (!file) {
        fclose(ptr);
        wi_state_oom(state, "failed to allocate a file handle (_io_open)");
    }

    file->path = wi_strdup(file_path);
    file->mode = wi_strdup(mode);

    if (!file->path || !file->mode) {
        fclose(ptr);
        free(file->path);
        free(file->mode);
        free(file);
        wi_state_oom(state, "failed to allocate a file handle (_io_open)");
    }

    file->ptr      = ptr;
    file->updating = updating;
    wi_push_userdata(state, "file", file, _file_finalizer);
}

static void
_io_close(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_file* file = wi_arg_userdata(state, 1, "file");
    _file_close(file);
    wi_push_null(state);
}

static void
_io_write(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_file* file = wi_arg_userdata(state, 1, "file");
    _file_check_open(state, file);

    if (file->mode[0] != 'w' && file->mode[0] != 'a' && !file->updating) {
        wi_state_error(state, "file %s was not opened for writing (mode %s)", file->path, file->mode);
    }

    wi_value arg2 = state->ffi_stack[2];
    char*    content;
    int      count;
    bool     owned = false;

    if (wi_value_is_string(arg2)) {
        content = wi_arg_string(state, 2, &count, NULL);
    } else {
        content = wi_value_to_string(arg2);

        if (!content) {
            wi_state_oom(state, "failed to allocate file contents (_io_write)");
        }

        count = (int)strlen(content);
        owned = true;
    }

    size_t written = fwrite(content, sizeof(char), (size_t)count, file->ptr);

    if (owned) {
        free(content);
    }

    if (written < (size_t)count) {
        wi_state_error(state, "failed to write file %s", file->path);
    }

    wi_push_null(state);
}

static void
_io_read(struct wi_state* state, uint8_t arg_count) {
    WI_UNUSED(arg_count);
    struct wi_file* file = wi_arg_userdata(state, 1, "file");
    _file_check_open(state, file);

    if (file->mode[0] != 'r' && !file->updating) {
        wi_state_error(state, "file %s was not opened for reading (mode %s)", file->path, file->mode);
    }

    fseek(file->ptr, 0L, SEEK_END);
    long size = ftell(file->ptr);
    rewind(file->ptr);

    if (size < 0) {
        wi_state_error(state, "failed to get file size (file %s)", file->path);
    }

    char* content = malloc((size_t)size + 1);

    if (!content) {
        wi_state_oom(state, "failed to allocate file contents (_io_read)");
    }

    size_t read = fread(content, sizeof(char), (size_t)size, file->ptr);

    if (read < (size_t)size) {
        free(content);
        wi_state_error(state, "failed to read file %s", file->path);
    }

    content[read] = '\0';
    wi_gc_add_bytes(state->gc, read + 1);
    struct wi_string* box = wi_take_cstring(state->gc, content, (int)read);
    wi_state_ppush(state, WI_MAKE_BOX_VALUE(box));
}

void
wi_state_def_std_io(struct wi_state* state) {
    struct wi_object* object = wi_push_object(state);
    wi_def(state, "io");
    wi_foreign_entry functions[] = {
        {"open",  _io_open,  2, false},
        {"close", _io_close, 1, false},
        {"write", _io_write, 2, false},
        {"read",  _io_read,  1, false},
    };
    WI_OBJECT_SET_FOREIGN_ALL(state, object, functions);
}
