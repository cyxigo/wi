#ifndef WI_PARSER_H
#define WI_PARSER_H

#include <setjmp.h>

#include "wi_buf.h"
#include "wi_lexer.h"

typedef struct {
    wi_lexer_t* lexer;
    wi_gc_t*    gc;
    jmp_buf     error_jmp;

    wi_token_t prev;
    wi_token_t curr;
    wi_token_t next;
    wi_token_t last;
} wi_parser_t;

wi_parser_t*
wi_new_parser(wi_lexer_t* lexer, wi_gc_t* gc);
void
wi_delete_parser(wi_parser_t* parser);

void
wi_parser_error_at(wi_parser_t* parser, wi_token_t token, const char* format, ...);
void
wi_parser_error_at_prev(wi_parser_t* parser, const char* format, ...);
void
wi_parser_error_at_curr(wi_parser_t* parser, const char* format, ...);

void
wi_parser_advance(wi_parser_t* parser);
bool
wi_parser_check(wi_parser_t* parser, wi_token_kind_t kind);
bool
wi_parser_match(wi_parser_t* parser, wi_token_kind_t kind);
bool
wi_parser_is_at_end(wi_parser_t* parser);
wi_token_t
wi_parser_expect(wi_parser_t* parser, wi_token_kind_t kind);

#endif
