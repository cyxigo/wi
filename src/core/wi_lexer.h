#ifndef WI_LEXER_H
#define WI_LEXER_H

#include <stdbool.h>
#include <string.h>

#include "wi_util.h"

WI_INLINE bool
wi_is_digit(char c) {
    return c >= '0' && c <= '9';
}

WI_INLINE bool
wi_is_bin_digit(char c) {
    return c == '0' || c == '1';
}

WI_INLINE bool
wi_is_oct_digit(char c) {
    return c >= '0' && c <= '7';
}

WI_INLINE bool
wi_is_hex_digit(char c) {
    return wi_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

WI_INLINE bool
wi_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

WI_INLINE bool
wi_is_alnum(char c) {
    return wi_is_digit(c) || wi_is_alpha(c);
}

enum wi_token_kind {
    WI_TOKEN_BLANK,
    WI_TOKEN_NAME,

    WI_TOKEN_REAL,
    WI_TOKEN_STRING,

    WI_TOKEN_OPEN_PAREN,
    WI_TOKEN_CLOSE_PAREN,
    WI_TOKEN_OPEN_BRACKET,
    WI_TOKEN_CLOSE_BRACKET,
    WI_TOKEN_OPEN_BRACE,
    WI_TOKEN_CLOSE_BRACE,

    WI_TOKEN_SEMICOLON,
    WI_TOKEN_COMMA,
    WI_TOKEN_DOT,
    WI_TOKEN_DOT_DOT,
    WI_TOKEN_DOT_DOT_DOT,
    WI_TOKEN_HASH,
    WI_TOKEN_AT,
    WI_TOKEN_ARROW,     /* -> */
    WI_TOKEN_FAT_ARROW, /* => */
    WI_TOKEN_PERCENT,

    WI_TOKEN_PLUS,
    WI_TOKEN_MINUS,
    WI_TOKEN_STAR,
    WI_TOKEN_STAR_STAR,
    WI_TOKEN_SLASH,

    WI_TOKEN_AMPER,
    WI_TOKEN_AMPER_AMPER,
    WI_TOKEN_PIPE,
    WI_TOKEN_PIPE_PIPE,
    WI_TOKEN_CARET,
    WI_TOKEN_TILDE,
    WI_TOKEN_EQUAL,
    WI_TOKEN_EQUAL_EQUAL,
    WI_TOKEN_BANG,
    WI_TOKEN_BANG_EQUAL,
    WI_TOKEN_COLON,
    WI_TOKEN_COLON_EQUAL,
    WI_TOKEN_GREATER,
    WI_TOKEN_GREATER_GREATER,
    WI_TOKEN_GREATER_EQUAL,
    WI_TOKEN_LESS,
    WI_TOKEN_LESS_LESS,
    WI_TOKEN_LESS_EQUAL,

    WI_TOKEN_KW_IF,
    WI_TOKEN_KW_ELSE,
    WI_TOKEN_KW_NULL,
    WI_TOKEN_KW_TRUE,
    WI_TOKEN_KW_FALSE,
    WI_TOKEN_KW_WHILE,
    WI_TOKEN_KW_FOR,
    WI_TOKEN_KW_BREAK,
    WI_TOKEN_KW_CONTINUE,
    WI_TOKEN_KW_RETURN,
    WI_TOKEN_KW_OBJECT,
    WI_TOKEN_KW_NEW,
    WI_TOKEN_KW_REQUIRE,
    WI_TOKEN_KW_LOAD,

    WI_TOKEN_EOF,
    WI_TOKEN_ERROR,
};

struct wi_token {
    enum wi_token_kind kind;
    const char*        start;
    int                count;
    int                line;
    int                col;
};

extern const struct wi_token WI_BLANK_TOKEN;

WI_INLINE bool
wi_token_lexemes_equal(struct wi_token a, struct wi_token b) {
    return a.count == b.count && memcmp(a.start, b.start, (size_t)a.count) == 0;
}

WI_INLINE struct wi_token
wi_token_from_string(const char* string) {
    return (struct wi_token){
        .kind  = WI_TOKEN_NAME,
        .start = string,
        .count = (int)strlen(string),
        .line  = 1,
        .col   = 1,
    };
}

const char*
wi_token_kind_to_string(enum wi_token_kind kind);

struct wi_lexer {
    const char* file_path;
    const char* src;
    const char* start;
    const char* curr;
    int         line;
    int         start_col;
    int         curr_col;
};

void
wi_lexer_init(struct wi_lexer* lexer, const char* file_path, const char* src);
struct wi_token
wi_lexer_next(struct wi_lexer* lexer);

#endif
