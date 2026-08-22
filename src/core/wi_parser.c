#include "wi_parser.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#include "wi_buf.h"
#include "wi_gc.h"
#include "wi_lexer.h"
#include "wi_state.h"
#include "wi_util.h"

struct wi_parser*
wi_new_parser(struct wi_lexer* lexer, struct wi_gc* gc) {
    struct wi_parser* parser = malloc(sizeof(struct wi_parser));

    if (!parser) {
        return NULL;
    }

    parser->lexer = lexer;
    parser->gc    = gc;

    parser->prev = WI_BLANK_TOKEN;
    parser->curr = wi_lexer_next(parser->lexer);
    parser->next = wi_lexer_next(parser->lexer);
    parser->last = WI_BLANK_TOKEN;

    parser->c_depth = 0;

    return parser;
}

void
wi_delete_parser(struct wi_parser* parser) {
    free(parser);
}

static int
_digit_count(int n) {
    if (n <= 0) {
        return 1;
    }

    int count = 0;

    while (n > 0) {
        count++;
        n /= 10;
    }

    return count;
}

static void
_parser_append_token_line(struct wi_parser* parser, char** t_buf, struct wi_token token) {
    if (token.kind == WI_TOKEN_BLANK) {
        return;
    }

    struct wi_state* state      = parser->gc->state;
    const char*      src        = parser->lexer->src;
    const char*      line_start = src;
    int              line       = 1;
    const char*      ptr        = src;

    while (*ptr && line < token.line) {
        if (*ptr == '\n') {
            line++;
            line_start = ptr + 1;
        }

        ptr++;
    }

    const char* line_end = line_start;

    while (*line_end && *line_end != '\n') {
        line_end++;
    }

    int line_width = _digit_count(token.line);

    wi_state_append_to(state, t_buf, " %*s | \n", line_width, "");
    wi_state_append_to(state, t_buf, " %*i | %.*s\n", line_width, token.line, (int)(line_end - line_start),
                       line_start);
    wi_state_append_to(state, t_buf, " %*s | %*s", line_width, "", token.col - 1, "");

    int caret_count = wi_utf8_len(token.start, token.count);

    for (int i = 0; i < caret_count; i++) {
        wi_state_append_to(state, t_buf, "^");
    }

    wi_state_append_to(state, t_buf, "\n");
}

static void
_parser_error_va(struct wi_parser* parser, struct wi_token token, const char* format, va_list args) {
    struct wi_state* state = parser->gc->state;
    wi_state_reset_error(state);

    if (token.kind == WI_TOKEN_EOF) {
        state->was_eof_error = true;
    }

    wi_state_append_error(state, "compile error: ");

    if (token.kind == WI_TOKEN_ERROR) {
        wi_state_append_error(state, "%s\n", token.start);
    } else {
        wi_state_append_error_va(state, format, args);
        wi_state_append_error(state, "\n");
    }

    _parser_append_token_line(parser, &state->error, token.kind == WI_TOKEN_EOF ? parser->last : token);
    wi_state_append_error(state, "   --> %s:%i:%i\n", parser->lexer->file_path, token.line, token.col);

    wi_gc_reset_roots(parser->gc);
}

WI_NORETURN void
wi_parser_error_at(struct wi_parser* parser, struct wi_token token, const char* format, ...) {
    va_list args;
    va_start(args, format);
    _parser_error_va(parser, token, format, args);
    va_end(args);
    longjmp(parser->error_jmp, 1);
}

WI_NORETURN void
wi_parser_error_at_prev(struct wi_parser* parser, const char* format, ...) {
    va_list args;
    va_start(args, format);
    _parser_error_va(parser, parser->prev, format, args);
    va_end(args);
    longjmp(parser->error_jmp, 1);
}

WI_NORETURN void
wi_parser_error_at_curr(struct wi_parser* parser, const char* format, ...) {
    va_list args;
    va_start(args, format);
    _parser_error_va(parser, parser->curr, format, args);
    va_end(args);
    longjmp(parser->error_jmp, 1);
}

void
wi_parser_warning_at(struct wi_parser* parser, struct wi_token token, const char* format, ...) {
    struct wi_state* state = parser->gc->state;

    if (wi_conf_is_set(state->conf, WI_CONF_NO_WARNINGS)) {
        return;
    }

    wi_state_append_warning(state, "compile warning: ");

    va_list args;
    va_start(args, format);
    wi_state_append_warning_va(state, format, args);
    va_end(args);

    wi_state_append_warning(state, "\n");

    _parser_append_token_line(parser, &state->warnings, token);
    wi_state_append_warning(state, "   --> %s:%i:%i\n", parser->lexer->file_path, token.line, token.col);
}

void
wi_parser_advance(struct wi_parser* parser) {
    if (parser->curr.kind != WI_TOKEN_EOF && parser->curr.kind != WI_TOKEN_ERROR) {
        parser->last = parser->curr;
    }

    parser->prev = parser->curr;
    parser->curr = parser->next;
    parser->next = wi_lexer_next(parser->lexer);

    if (parser->curr.kind != WI_TOKEN_ERROR) {
        return;
    }

    wi_parser_error_at(parser, parser->curr, "%s", parser->curr.start);
}

bool
wi_parser_check(struct wi_parser* parser, enum wi_token_kind kind) {
    return parser->curr.kind == kind;
}

bool
wi_parser_match(struct wi_parser* parser, enum wi_token_kind kind) {
    if (!wi_parser_check(parser, kind)) {
        return false;
    }

    wi_parser_advance(parser);
    return true;
}

bool
wi_parser_is_at_end(struct wi_parser* parser) {
    return wi_parser_check(parser, WI_TOKEN_EOF);
}

struct wi_token
wi_parser_expect(struct wi_parser* parser, enum wi_token_kind kind) {
    if (wi_parser_match(parser, kind)) {
        return parser->prev;
    }

    if (wi_parser_is_at_end(parser)) {
        wi_parser_error_at_curr(parser, "expected %s", wi_token_kind_to_string(kind));
        return WI_BLANK_TOKEN;
    }

    struct wi_token* prev = &parser->prev;
    prev->col += wi_utf8_len(prev->start, prev->count);
    prev->count = 1;
    wi_parser_error_at_prev(parser, "expected %s", wi_token_kind_to_string(kind));

    return WI_BLANK_TOKEN;
}
