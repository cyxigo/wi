#ifndef WI_PARSER_H
#define WI_PARSER_H

#include <setjmp.h>

#include "wi_buf.h"
#include "wi_lexer.h"
#include "wi_util.h"

struct wi_parser {
    struct wi_lexer* lexer;
    struct wi_gc*    gc;
    jmp_buf          error_jmp;

    struct wi_token prev;
    struct wi_token curr;
    struct wi_token next;
    struct wi_token last;
};

struct wi_parser*
wi_new_parser(struct wi_lexer* lexer, struct wi_gc* gc);
void
wi_delete_parser(struct wi_parser* parser);

WI_NORETURN void
wi_parser_error_at(struct wi_parser* parser, struct wi_token token, const char* format, ...);
WI_NORETURN void
wi_parser_error_at_prev(struct wi_parser* parser, const char* format, ...);
WI_NORETURN void
wi_parser_error_at_curr(struct wi_parser* parser, const char* format, ...);

void
wi_parser_warning_at(struct wi_parser* parser, struct wi_token token, const char* format, ...);

void
wi_parser_advance(struct wi_parser* parser);
bool
wi_parser_check(struct wi_parser* parser, enum wi_token_kind kind);
bool
wi_parser_match(struct wi_parser* parser, enum wi_token_kind kind);
bool
wi_parser_is_at_end(struct wi_parser* parser);
struct wi_token
wi_parser_expect(struct wi_parser* parser, enum wi_token_kind kind);

#endif
