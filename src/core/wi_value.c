#include "wi_value.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "wi_box.h"
#include "wi_buf.h"
#include "wi_gc.h"
#include "wi_table.h"

static void
_print_function(wi_prototype_t* prototype) {
    if (prototype->is_main) {
        printf("<main function in %s at %p>", prototype->file_path, (void*)prototype);
    } else if (prototype->name) {
        printf("<function %s at %p>", prototype->name->chars, (void*)prototype);
    } else {
        printf("<anonymous function at %p>", (void*)prototype);
    }
}

void
wi_value_print(wi_value_t value) {
    if (wi_value_is_real(value)) {
        printf(WI_REAL_FORMAT, wi_value_as_real(value));
    } else if (wi_value_is_null(value)) {
        printf("null");
    } else if (wi_value_is_bool(value)) {
        printf(wi_value_as_bool(value) ? "true" : "false");
    } else if (wi_value_is_string(value)) {
        printf("%s", wi_value_as_cstring(value));
    } else if (wi_value_is_array(value)) {
        wi_value_buf_t items = wi_value_as_array(value)->items;
        printf("[");

        for (int i = 0; i < items.count; i++) {
            if (i > 0) {
                printf(", ");
            }

            wi_value_t value = items.data[i];

            if (wi_value_is_string(value)) {
                printf("\"%s\"", wi_value_as_cstring(value));
            } else {
                wi_value_print(value);
            }
        }

        printf("]");
    } else if (wi_value_is_map(value)) {
        printf("{ ");
        wi_table_t* table   = &wi_value_as_map(value)->items;
        int         printed = 0;

        for (int i = 0; i < table->capacity; i++) {
            wi_entry_t* entry = &table->entries[i];

            if (wi_value_is_empty(entry->key)) {
                continue;
            }

            if (wi_value_is_string(entry->key)) {
                printf("\"%s\": ", wi_value_as_cstring(entry->key));
            } else {
                wi_value_print(entry->key);
                printf(": ");
            }

            if (wi_value_is_string(entry->value)) {
                printf("\"%s\"", wi_value_as_cstring(entry->value));
            } else {
                wi_value_print(entry->value);
            }

            printed++;

            if (printed != table->count) {
                printf(", ");
            }
        }

        printf(" }");
    } else if (wi_value_is_prototype(value)) {
        _print_function(wi_value_as_prototype(value));
    } else if (wi_value_is_foreign(value)) {
        wi_foreign_t* foreign = wi_value_as_foreign(value);
        printf("<foreign %s at %p>", foreign->name->chars, (void*)foreign);
    } else if (wi_value_is_closure(value)) {
        _print_function(wi_value_as_closure(value)->prototype);
    } else if (wi_value_is_upvalue(value)) {
        printf("<upvalue at %p>", (void*)wi_value_as_upvalue(value));
    } else if (wi_value_is_object(value)) {
        wi_object_t* object = wi_value_as_object(value);

        if (object->name) {
            printf("<object %s at %p> ", object->name->chars, (void*)object);
        } else {
            printf("<anonymous object at %p> ", (void*)object);
        }
    } else if (wi_value_is_userdata(value)) {
        wi_userdata_t* userdata = wi_value_as_userdata(value);
        printf("<userdata %s at %p>", userdata->name->chars, (void*)userdata);
    } else {
        printf("<unknown>");
    }
}

static uint32_t
_real_hash(wi_real_t real) {
    uint64_t bits;
    memcpy(&bits, &real, sizeof(bits));

    // -0.0 -> 0.0
    if (bits == 0x8000000000000000ull) {
        bits = 0;
    }

    bits ^= bits >> 30;
    bits *= 0xbf58476d1ce4e5b9ull;
    bits ^= bits >> 27;
    bits *= 0x94d049bb133111ebull;
    bits ^= bits >> 31;

    return (uint32_t)bits ^ (uint32_t)(bits >> 32);
}

uint32_t
wi_value_hash(wi_value_t value) {
    if (wi_value_is_real(value)) {
        return _real_hash(wi_value_as_real(value));
    }

    if (wi_value_is_null(value)) {
        return WI_NULL_HASH;
    }

    if (wi_value_is_bool(value)) {
        return wi_value_as_bool(value) ? WI_TRUE_HASH : WI_FALSE_HASH;
    }

    if (wi_value_is_string(value)) {
        return wi_value_as_string(value)->hash;
    }

    if (wi_value_is_box(value)) {
        return (uint32_t)((uintptr_t)wi_value_as_box(value) >> 2);
    }

    return 0;
}

const char*
wi_value_type(wi_value_t value) {
    if (wi_value_is_real(value)) {
        return "real";
    }

    if (wi_value_is_null(value)) {
        return "null";
    }

    if (wi_value_is_bool(value)) {
        return "bool";
    }

    if (wi_value_is_string(value)) {
        return "string";
    }

    if (wi_value_is_array(value)) {
        return "array";
    }

    if (wi_value_is_map(value)) {
        return "map";
    }

    if (wi_value_is_prototype(value) || wi_value_is_closure(value)) {
        return "function";
    }

    if (wi_value_is_foreign(value)) {
        return "foreign";
    }

    if (wi_value_is_upvalue(value)) {
        return "upvalue";
    }

    if (wi_value_is_object(value)) {
        return "object";
    }

    if (wi_value_is_userdata(value)) {
        return "userdata";
    }

    return "unknown";
}

static char*
_format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (needed < 0) {
        return NULL;
    }

    char* buf = malloc((size_t)needed + 1);

    if (!buf) {
        return NULL;
    }

    va_start(args, format);
    vsnprintf(buf, (size_t)needed + 1, format, args);
    va_end(args);

    return buf;
}

char*
_function_to_string(wi_prototype_t* prototype) {
    if (prototype->is_main) {
        return _format("<main function in %s at %p>", prototype->file_path, (void*)prototype);
    }

    if (prototype->name) {
        return _format("<function %s at %p>", prototype->name->chars, (void*)prototype);
    }

    return _format("<anonymous function %p>", (void*)prototype);
}

char*
wi_value_to_string(wi_value_t value) {
    if (wi_value_is_real(value)) {
        return _format(WI_REAL_FORMAT, wi_value_as_real(value));
    }

    if (wi_value_is_null(value)) {
        return strdup("null");
    }

    if (wi_value_is_bool(value)) {
        return strdup(wi_value_as_bool(value) ? "true" : "false");
    }

    if (wi_value_is_string(value)) {
        return strdup(wi_value_as_cstring(value));
    }

    if (wi_value_is_array(value)) {
        return strdup("<array>");
    }

    if (wi_value_is_map(value)) {
        return strdup("<map>");
    }

    if (wi_value_is_prototype(value)) {
        return _function_to_string(wi_value_as_prototype(value));
    }

    if (wi_value_is_foreign(value)) {
        wi_foreign_t* foreign = wi_value_as_foreign(value);
        return _format("<foreign %s at %p>", foreign->name->chars, (void*)foreign);
    }

    if (wi_value_is_closure(value)) {
        return _function_to_string(wi_value_as_closure(value)->prototype);
    }

    if (wi_value_is_upvalue(value)) {
        return _format("<upvalue at %p>", (void*)wi_value_as_upvalue(value));
    }

    if (wi_value_is_object(value)) {
        wi_object_t* object = wi_value_as_object(value);

        if (object->name) {
            return _format("<object %s at %p>", object->name->chars, (void*)object);
        }

        return _format("<anonymous object at %p>", (void*)object);
    }

    if (wi_value_is_userdata(value)) {
        wi_userdata_t* userdata = wi_value_as_userdata(value);
        return _format("<userdata %s at %p>", userdata->name->chars, (void*)userdata);
    }

    return strdup("<unknown>");
}

WI_DEF_BUF(wi_value_t, value)
