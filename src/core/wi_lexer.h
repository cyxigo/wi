#ifndef WI_LEXER_H
#define WI_LEXER_H

#include <stdbool.h>
#include <string.h>

static inline bool
wi_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool
wi_is_bin_digit(char c) {
    return c == '0' || c == '1';
}

static inline bool
wi_is_oct_digit(char c) {
    return c >= '0' && c <= '7';
}

static inline bool
wi_is_hex_digit(char c) {
    return wi_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline bool
wi_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '@';
}

static inline bool
wi_is_alnum(char c) {
    return wi_is_digit(c) || wi_is_alpha(c);
}

typedef enum {
    WI_TOKEN_BLANK,
    WI_TOKEN_NAME,

    WI_TOKEN_LIT_REAL,
    WI_TOKEN_LIT_STRING,

    WI_TOKEN_OPEN_PAREN,
    WI_TOKEN_CLOSE_PAREN,
    WI_TOKEN_OPEN_BRACKET,
    WI_TOKEN_CLOSE_BRACKET,
    WI_TOKEN_OPEN_BRACE,
    WI_TOKEN_CLOSE_BRACE,

    WI_TOKEN_COLON,
    WI_TOKEN_SEMICOLON,
    WI_TOKEN_COMMA,
    WI_TOKEN_DOT,
    WI_TOKEN_DOT_DOT,
    WI_TOKEN_DOT_DOT_DOT,
    WI_TOKEN_HASH,
    WI_TOKEN_ARROW,
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
    WI_TOKEN_GREATER,
    WI_TOKEN_GREATER_GREATER,
    WI_TOKEN_GREATER_EQUAL,
    WI_TOKEN_LESS,
    WI_TOKEN_LESS_LESS,
    WI_TOKEN_LESS_EQUAL,

    WI_TOKEN_KW_VAR,
    WI_TOKEN_KW_IF,
    WI_TOKEN_KW_ELSE,
    WI_TOKEN_KW_NULL,
    WI_TOKEN_KW_TRUE,
    WI_TOKEN_KW_FALSE,
    WI_TOKEN_KW_WHILE,
    WI_TOKEN_KW_FOR,
    WI_TOKEN_KW_BREAK,
    WI_TOKEN_KW_CONTINUE,
    WI_TOKEN_KW_FUNCTION,
    WI_TOKEN_KW_RETURN,
    WI_TOKEN_KW_NEW,
    WI_TOKEN_KW_OBJECT,
    WI_TOKEN_KW_REQUIRE,

    WI_TOKEN_EOF,
    WI_TOKEN_ERROR,
} wi_token_kind_t;

typedef struct {
    wi_token_kind_t kind;
    const char*     start;
    int             len;
    int             line;
    int             col;
} wi_token_t;

extern const wi_token_t WI_BLANK_TOKEN;

static inline bool
wi_token_lexemes_equal(wi_token_t a, wi_token_t b) {
    return a.len == b.len && memcmp(a.start, b.start, (size_t)a.len) == 0;
}

static inline wi_token_t
wi_token_from_string(const char* string) {
    return (wi_token_t){
        .kind  = WI_TOKEN_NAME,
        .start = string,
        .len   = (int)strlen(string),
        .line  = 1,
    };
}

const char*
wi_token_kind_to_string(wi_token_kind_t kind);

typedef struct {
    const char* file_path;
    const char* src;
    const char* start;
    const char* curr;
    int         line;
    int         col;
} wi_lexer_t;

void
wi_lexer_init(wi_lexer_t* lexer, const char* file_path, const char* src);
wi_token_t
wi_lexer_next(wi_lexer_t* lexer);

#endif
