#include "wi_parser.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#include "wi_buf.h"
#include "wi_gc.h"
#include "wi_lexer.h"
#include "wi_state.h"

wi_parser_t*
wi_new_parser(wi_lexer_t* lexer, wi_gc_t* gc) {
    wi_parser_t* parser = malloc(sizeof(wi_parser_t));

    if (!parser) {
        return NULL;
    }

    parser->lexer = lexer;
    parser->gc    = gc;

    parser->prev = WI_BLANK_TOKEN;
    parser->curr = wi_lexer_next(parser->lexer);
    parser->next = wi_lexer_next(parser->lexer);
    parser->last = WI_BLANK_TOKEN;

    return parser;
}

void
wi_delete_parser(wi_parser_t* parser) {
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
_parser_append_token_line(wi_parser_t* parser, wi_token_t token) {
    if (token.kind == WI_TOKEN_BLANK) {
        return;
    }

    wi_state_t* state      = parser->gc->state;
    const char* src        = parser->lexer->src;
    const char* line_start = src;
    int         line       = 1;
    const char* ptr        = src;

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

    wi_state_append_error(state, " %*s | \n", line_width, "");
    wi_state_append_error(state, " %*i | %.*s\n", line_width, token.line, (int)(line_end - line_start),
                          line_start);
    wi_state_append_error(state, " %*s | %*s", line_width, "", token.col - 1, "");

    for (int i = 0; i < token.len; i++) {
        wi_state_append_error(state, "^");
    }

    wi_state_append_error(state, "\n");
}

static void
_parser_error_va(wi_parser_t* parser, wi_token_t token, const char* format, va_list args) {
    wi_state_t* state = parser->gc->state;

    wi_state_reset_error(state);
    wi_state_append_error(state, "compile error: ");

    if (token.kind == WI_TOKEN_ERROR) {
        wi_state_append_error(state, "%s\n", token.start);
    } else {
        wi_state_append_error_va(state, format, args);
        wi_state_append_error(state, "\n");
    }

    _parser_append_token_line(parser, token.kind == WI_TOKEN_EOF ? parser->last : token);
    wi_state_append_error(state, "   --> %s:%i:%i\n", parser->lexer->file_path, token.line, token.col);

    wi_gc_reset_roots(parser->gc);
    longjmp(parser->error_jmp, 1);
}

void
wi_parser_error_at(wi_parser_t* parser, wi_token_t token, const char* format, ...) {
    va_list args;
    va_start(args, format);
    _parser_error_va(parser, token, format, args);
    va_end(args);
}

void
wi_parser_error_at_prev(wi_parser_t* parser, const char* format, ...) {
    va_list args;
    va_start(args, format);
    _parser_error_va(parser, parser->prev, format, args);
    va_end(args);
}

void
wi_parser_error_at_curr(wi_parser_t* parser, const char* format, ...) {
    va_list args;
    va_start(args, format);
    _parser_error_va(parser, parser->curr, format, args);
    va_end(args);
}

void
wi_parser_advance(wi_parser_t* parser) {
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
wi_parser_check(wi_parser_t* parser, wi_token_kind_t kind) {
    return parser->curr.kind == kind;
}

bool
wi_parser_match(wi_parser_t* parser, wi_token_kind_t kind) {
    if (!wi_parser_check(parser, kind)) {
        return false;
    }

    wi_parser_advance(parser);
    return true;
}

bool
wi_parser_is_at_end(wi_parser_t* parser) {
    return wi_parser_check(parser, WI_TOKEN_EOF);
}

wi_token_t
wi_parser_expect(wi_parser_t* parser, wi_token_kind_t kind) {
    if (wi_parser_match(parser, kind)) {
        return parser->prev;
    }

    wi_token_t* prev = &parser->prev;
    prev->col += prev->len;
    prev->len = 1;
    wi_parser_error_at_prev(parser, "expected %s", wi_token_kind_to_string(kind));

    return WI_BLANK_TOKEN;
}
