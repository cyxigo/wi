#include "wi_lexer.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

const struct wi_token WI_BLANK_TOKEN = {
    .kind  = WI_TOKEN_BLANK,
    .start = "",
    .count = 0,
    .line  = 0,
    .col   = 0,
};

const char*
wi_token_kind_to_string(enum wi_token_kind kind) {
    switch (kind) {
        case WI_TOKEN_BLANK:
            return "";
        case WI_TOKEN_NAME:
            return "name";
        case WI_TOKEN_REAL:
            return "real";
        case WI_TOKEN_STRING:
            return "string";
        case WI_TOKEN_INTERP:
            return "string interpolation";
        case WI_TOKEN_OPEN_PAREN:
            return "(";
        case WI_TOKEN_CLOSE_PAREN:
            return ")";
        case WI_TOKEN_OPEN_BRACKET:
            return "[";
        case WI_TOKEN_CLOSE_BRACKET:
            return "]";
        case WI_TOKEN_OPEN_BRACE:
            return "{";
        case WI_TOKEN_CLOSE_BRACE:
            return "}";
        case WI_TOKEN_SEMICOLON:
            return ";";
        case WI_TOKEN_COMMA:
            return ",";
        case WI_TOKEN_DOT:
            return ".";
        case WI_TOKEN_DOT_DOT_DOT:
            return "...";
        case WI_TOKEN_AT:
            return "@";
        case WI_TOKEN_HASH:
            return "#";
        case WI_TOKEN_ARROW:
            return "->";
        case WI_TOKEN_FAT_ARROW:
            return "=>";
        case WI_TOKEN_PERCENT:
            return "%";
        case WI_TOKEN_PLUS:
            return "+";
        case WI_TOKEN_MINUS:
            return "-";
        case WI_TOKEN_STAR:
            return "*";
        case WI_TOKEN_STAR_STAR:
            return "**";
        case WI_TOKEN_SLASH:
            return "/";
        case WI_TOKEN_AMPER:
            return "&";
        case WI_TOKEN_AMPER_AMPER:
            return "&&";
        case WI_TOKEN_PIPE:
            return "|";
        case WI_TOKEN_PIPE_PIPE:
            return "||";
        case WI_TOKEN_CARET:
            return "^";
        case WI_TOKEN_TILDE:
            return "~";
        case WI_TOKEN_EQUAL:
            return "=";
        case WI_TOKEN_EQUAL_EQUAL:
            return "==";
        case WI_TOKEN_BANG:
            return "!";
        case WI_TOKEN_BANG_EQUAL:
            return "!=";
        case WI_TOKEN_COLON:
            return ":";
        case WI_TOKEN_COLON_EQUAL:
            return ":=";
        case WI_TOKEN_GREATER:
            return ">";
        case WI_TOKEN_GREATER_GREATER:
            return ">>";
        case WI_TOKEN_GREATER_EQUAL:
            return ">=";
        case WI_TOKEN_LESS:
            return "<";
        case WI_TOKEN_LESS_LESS:
            return "<<";
        case WI_TOKEN_LESS_EQUAL:
            return "<=";
        case WI_TOKEN_IF:
            return "if";
        case WI_TOKEN_ELSE:
            return "else";
        case WI_TOKEN_NULL:
            return "null";
        case WI_TOKEN_TRUE:
            return "true";
        case WI_TOKEN_FALSE:
            return "false";
        case WI_TOKEN_WHILE:
            return "while";
        case WI_TOKEN_FOR:
            return "for";
        case WI_TOKEN_BREAK:
            return "break";
        case WI_TOKEN_CONTINUE:
            return "continue";
        case WI_TOKEN_RETURN:
            return "return";
        case WI_TOKEN_OBJECT:
            return "object";
        case WI_TOKEN_NEW:
            return "new";
        case WI_TOKEN_REQUIRE:
            return "require";
        case WI_TOKEN_LOAD:
            return "load";
        case WI_TOKEN_EOF:
            return "end of file";
        case WI_TOKEN_ERROR:
            return "error";
    }

    return "unknown";
}

void
wi_lexer_init(struct wi_lexer* lexer, const char* file_path, const char* src) {
    lexer->file_path    = file_path;
    lexer->src          = src;
    lexer->start        = src;
    lexer->curr         = src;
    lexer->line         = 1;
    lexer->start_col    = 1;
    lexer->curr_col     = 1;
    lexer->interp_depth = 0;
}

static struct wi_token
_lexer_make_token(struct wi_lexer* lexer, enum wi_token_kind kind) {
    struct wi_token token = {
        .kind = kind,
        .line = lexer->line,
    };

    if (token.kind == WI_TOKEN_EOF) {
        token.start = "<eof>";
        token.count = 5;
        token.col   = lexer->curr_col;
    } else {
        token.start = lexer->start;
        token.count = (int)(lexer->curr - lexer->start);
        token.col   = lexer->start_col;
    }

    return token;
}

static struct wi_token
_lexer_error(struct wi_lexer* lexer, const char* msg, int line, int col) {
    WI_UNUSED(lexer);
    return (struct wi_token){
        .kind  = WI_TOKEN_ERROR,
        .start = msg,
        .count = 1,
        .line  = line,
        .col   = col,
    };
}

static char
_lexer_peek(struct wi_lexer* lexer) {
    return *lexer->curr;
}

static char
_lexer_advance(struct wi_lexer* lexer) {
    char c = *lexer->curr++;

    if (c == '\n') {
        lexer->line++;
        lexer->curr_col = 1;
    } else if ((c & 0xc0) != 0x80) {
        lexer->curr_col++;
    }

    return c;
}

static bool
_lexer_check(struct wi_lexer* lexer, char c) {
    return _lexer_peek(lexer) == c;
}

static bool
_lexer_is_at_end(struct wi_lexer* lexer) {
    return _lexer_check(lexer, '\0');
}

static char
_lexer_peek_next(struct wi_lexer* lexer) {
    if (_lexer_is_at_end(lexer)) {
        return '\0';
    }

    return lexer->curr[1];
}

static bool
_lexer_match(struct wi_lexer* lexer, char c) {
    if (!_lexer_check(lexer, c)) {
        return false;
    }

    _lexer_advance(lexer);
    return true;
}

static enum wi_token_kind
_lexer_check_kw(struct wi_lexer* lexer, int start, int len, const char* rest, enum wi_token_kind kind) {
    if (lexer->curr - lexer->start == start + len && memcmp(lexer->start + start, rest, (size_t)len) == 0) {
        return kind;
    }

    return WI_TOKEN_NAME;
}

static enum wi_token_kind
_lexer_name_kind(struct wi_lexer* lexer) {
    switch (lexer->start[0]) {
        case 'i':
            return _lexer_check_kw(lexer, 1, 1, "f", WI_TOKEN_IF);
        case 'e':
            return _lexer_check_kw(lexer, 1, 3, "lse", WI_TOKEN_ELSE);
        case 'n':
            if (lexer->curr - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'u':
                        return _lexer_check_kw(lexer, 2, 2, "ll", WI_TOKEN_NULL);
                    case 'e':
                        return _lexer_check_kw(lexer, 2, 1, "w", WI_TOKEN_NEW);
                }
            }

            break;
        case 'f':
            if (lexer->curr - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'a':
                        return _lexer_check_kw(lexer, 2, 3, "lse", WI_TOKEN_FALSE);
                    case 'o':
                        return _lexer_check_kw(lexer, 2, 1, "r", WI_TOKEN_FOR);
                }
            }

            break;
        case 'w':
            return _lexer_check_kw(lexer, 1, 4, "hile", WI_TOKEN_WHILE);
        case 't':
            return _lexer_check_kw(lexer, 1, 3, "rue", WI_TOKEN_TRUE);
        case 'b':
            return _lexer_check_kw(lexer, 1, 4, "reak", WI_TOKEN_BREAK);
        case 'c':
            return _lexer_check_kw(lexer, 1, 7, "ontinue", WI_TOKEN_CONTINUE);
        case 'r':
            if (lexer->curr - lexer->start > 2 && lexer->start[1] == 'e') {
                switch (lexer->start[2]) {
                    case 't':
                        return _lexer_check_kw(lexer, 3, 3, "urn", WI_TOKEN_RETURN);
                    case 'q':
                        return _lexer_check_kw(lexer, 3, 4, "uire", WI_TOKEN_REQUIRE);
                }
            }

            break;
        case 'o':
            return _lexer_check_kw(lexer, 1, 5, "bject", WI_TOKEN_OBJECT);
        case 'l':
            return _lexer_check_kw(lexer, 1, 3, "oad", WI_TOKEN_LOAD);
    }

    return WI_TOKEN_NAME;
}

static struct wi_token
_lexer_name(struct wi_lexer* lexer) {
    while (wi_is_alnum(_lexer_peek(lexer))) {
        _lexer_advance(lexer);
    }

    return _lexer_make_token(lexer, _lexer_name_kind(lexer));
}

static struct wi_token
_lexer_non_dec_real(struct wi_lexer* lexer) {
    char prefix               = _lexer_peek(lexer);
    bool (*is_digit_fn)(char) = NULL;

    if (prefix == 'b' || prefix == 'B') {
        is_digit_fn = wi_is_bin_digit;
    } else if (prefix == 'o' || prefix == 'O') {
        is_digit_fn = wi_is_oct_digit;
    } else if (prefix == 'x' || prefix == 'X') {
        is_digit_fn = wi_is_hex_digit;
    }

    int line = lexer->line;
    int col  = lexer->curr_col;

    _lexer_advance(lexer);

    if (!is_digit_fn(_lexer_peek(lexer))) {
        return _lexer_error(lexer, "expected at least one digit after base", line, col);
    }

    while (is_digit_fn(_lexer_peek(lexer))) {
        _lexer_advance(lexer);
    }

    return _lexer_make_token(lexer, WI_TOKEN_REAL);
}

static struct wi_token
_lexer_real(struct wi_lexer* lexer) {
    if (lexer->start[0] == '0') {
        char next = _lexer_peek(lexer);

        if (next == 'b' || next == 'B' || next == 'o' || next == 'O' || next == 'x' || next == 'X') {
            return _lexer_non_dec_real(lexer);
        }
    }

    while (wi_is_digit(_lexer_peek(lexer))) {
        _lexer_advance(lexer);
    }

    if (_lexer_check(lexer, '.') && wi_is_digit(_lexer_peek_next(lexer))) {
        do {
            _lexer_advance(lexer);
        } while (wi_is_digit(_lexer_peek(lexer)));
    }

    return _lexer_make_token(lexer, WI_TOKEN_REAL);
}

static struct wi_token
_lexer_string(struct wi_lexer* lexer) {
    /* skip first character, i.e. " */
    lexer->start     = lexer->curr;
    lexer->start_col = lexer->curr_col;

    int line = lexer->line;
    int col  = lexer->curr_col - 1;

    while (_lexer_peek(lexer) != '"' && !_lexer_is_at_end(lexer)) {
        if (_lexer_check(lexer, '$') && _lexer_peek_next(lexer) == '{') {
            if (lexer->interp_depth == WI_INTERP_MAX) {
                /*
                    we can't format lexer errors so we have to use the ancient technique called
                    "just type the limit in the string"
                */
                return _lexer_error(lexer, "string interpolation nested too deeply (limit is 8)", line, col);
            }

            struct wi_token token = _lexer_make_token(lexer, WI_TOKEN_INTERP);

            _lexer_advance(lexer); /* $ */
            _lexer_advance(lexer); /* { */
            lexer->interp_braces[lexer->interp_depth++] = 1;

            return token;
        }

        if (_lexer_check(lexer, '\\') && _lexer_peek_next(lexer) != '\0') {
            _lexer_advance(lexer);
        }

        _lexer_advance(lexer);
    }

    if (_lexer_is_at_end(lexer)) {
        return _lexer_error(lexer, "unfinished string", line, col);
    }

    struct wi_token token = _lexer_make_token(lexer, WI_TOKEN_STRING);
    _lexer_advance(lexer); /* " */
    return token;
}

static void
_lexer_skip_whitespace(struct wi_lexer* lexer) {
    for (;;) {
        char c = _lexer_peek(lexer);

        switch (c) {
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                _lexer_advance(lexer);
                break;
            case '/':
                if (_lexer_peek_next(lexer) == '/') {
                    while (!_lexer_check(lexer, '\n') && !_lexer_is_at_end(lexer)) {
                        _lexer_advance(lexer);
                    }
                } else if (_lexer_peek_next(lexer) == '*') {
                    _lexer_advance(lexer);
                    _lexer_advance(lexer);

                    while (!_lexer_is_at_end(lexer)) {
                        if (_lexer_check(lexer, '*') && _lexer_peek_next(lexer) == '/') {
                            _lexer_advance(lexer);
                            _lexer_advance(lexer);
                            break;
                        }

                        _lexer_advance(lexer);
                    }
                } else {
                    return;
                }

                break;
            default:
                return;
        }
    }
}

struct wi_token
wi_lexer_next(struct wi_lexer* lexer) {
    _lexer_skip_whitespace(lexer);

    lexer->start     = lexer->curr;
    lexer->start_col = lexer->curr_col;

    if (_lexer_is_at_end(lexer)) {
        return _lexer_make_token(lexer, WI_TOKEN_EOF);
    }

    char c = _lexer_advance(lexer);

    /*
        while yes, this error is not entirely accurate
        (because we check every character, not names only)
        our dear user most likely tried to type a character that is meant to be a name or in a name
    */
    if ((c & 0x80) != 0) {
        return _lexer_error(lexer, "non-ascii character in a name", lexer->line, lexer->curr_col - 1);
    }

    if (wi_is_alpha(c)) {
        return _lexer_name(lexer);
    }

    if (wi_is_digit(c)) {
        return _lexer_real(lexer);
    }

    switch (c) {
        case '"':
            return _lexer_string(lexer);
        case '(':
            return _lexer_make_token(lexer, WI_TOKEN_OPEN_PAREN);
        case ')':
            return _lexer_make_token(lexer, WI_TOKEN_CLOSE_PAREN);
        case '[':
            return _lexer_make_token(lexer, WI_TOKEN_OPEN_BRACKET);
        case ']':
            return _lexer_make_token(lexer, WI_TOKEN_CLOSE_BRACKET);
        case '{':
            /* are we in the middle of string interpolation? then increase amount of the { */
            if (lexer->interp_depth > 0) {
                lexer->interp_braces[lexer->interp_depth - 1]++;
            }

            return _lexer_make_token(lexer, WI_TOKEN_OPEN_BRACE);
        case '}':
            /*
                are we in the middle of string interpolation AND this is the last }?
                then this interpolation ends NOW!!
                + lex the trailing part
            */
            if (lexer->interp_depth > 0 && --lexer->interp_braces[lexer->interp_depth - 1] == 0) {
                lexer->interp_depth--;
                return _lexer_string(lexer);
            }

            return _lexer_make_token(lexer, WI_TOKEN_CLOSE_BRACE);
        case ';':
            return _lexer_make_token(lexer, WI_TOKEN_SEMICOLON);
        case ',':
            return _lexer_make_token(lexer, WI_TOKEN_COMMA);
        case '.':
            if (_lexer_match(lexer, '.')) {
                if (_lexer_match(lexer, '.')) {
                    return _lexer_make_token(lexer, WI_TOKEN_DOT_DOT_DOT);
                }

                break;
            }

            return _lexer_make_token(lexer, WI_TOKEN_DOT);
        case '@':
            return _lexer_make_token(lexer, WI_TOKEN_AT);
        case '#':
            return _lexer_make_token(lexer, WI_TOKEN_HASH);
        case '%':
            return _lexer_make_token(lexer, WI_TOKEN_PERCENT);
        case '+':
            return _lexer_make_token(lexer, WI_TOKEN_PLUS);
        case '-':
            return _lexer_make_token(lexer, _lexer_match(lexer, '>') ? WI_TOKEN_ARROW : WI_TOKEN_MINUS);
        case '*':
            return _lexer_make_token(lexer, _lexer_match(lexer, '*') ? WI_TOKEN_STAR_STAR : WI_TOKEN_STAR);
        case '/':
            return _lexer_make_token(lexer, WI_TOKEN_SLASH);
        case '&':
            return _lexer_make_token(lexer, _lexer_match(lexer, '&') ? WI_TOKEN_AMPER_AMPER : WI_TOKEN_AMPER);
        case '|':
            return _lexer_make_token(lexer, _lexer_match(lexer, '|') ? WI_TOKEN_PIPE_PIPE : WI_TOKEN_PIPE);
        case '^':
            return _lexer_make_token(lexer, WI_TOKEN_CARET);
        case '~':
            return _lexer_make_token(lexer, WI_TOKEN_TILDE);
        case '=':
            if (_lexer_match(lexer, '=')) {
                return _lexer_make_token(lexer, WI_TOKEN_EQUAL_EQUAL);
            } else if (_lexer_match(lexer, '>')) {
                return _lexer_make_token(lexer, WI_TOKEN_FAT_ARROW);
            } else {
                return _lexer_make_token(lexer, WI_TOKEN_EQUAL);
            }

            break;
        case '!':
            return _lexer_make_token(lexer, _lexer_match(lexer, '=') ? WI_TOKEN_BANG_EQUAL : WI_TOKEN_BANG);
        case ':':
            return _lexer_make_token(lexer, _lexer_match(lexer, '=') ? WI_TOKEN_COLON_EQUAL : WI_TOKEN_COLON);
        case '>':
            if (_lexer_match(lexer, '>')) {
                return _lexer_make_token(lexer, WI_TOKEN_GREATER_GREATER);
            } else if (_lexer_match(lexer, '=')) {
                return _lexer_make_token(lexer, WI_TOKEN_GREATER_EQUAL);
            } else {
                return _lexer_make_token(lexer, WI_TOKEN_GREATER);
            }

            break;
        case '<':
            if (_lexer_match(lexer, '<')) {
                return _lexer_make_token(lexer, WI_TOKEN_LESS_LESS);
            } else if (_lexer_match(lexer, '=')) {
                return _lexer_make_token(lexer, WI_TOKEN_LESS_EQUAL);
            } else {
                return _lexer_make_token(lexer, WI_TOKEN_LESS);
            }

            break;
    }

    return _lexer_error(lexer, "unexpected character", lexer->line, lexer->curr_col);
}
