#include "wi_lexer.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

const wi_token_t WI_BLANK_TOKEN = {
    .kind  = WI_TOKEN_BLANK,
    .start = "",
    .len   = 0,
    .line  = 0,
};

const char*
wi_token_kind_to_string(wi_token_kind_t kind) {
    switch (kind) {
        case WI_TOKEN_BLANK:
            return "";
        case WI_TOKEN_NAME:
            return "name";
        case WI_TOKEN_LIT_REAL:
            return "real";
        case WI_TOKEN_LIT_STRING:
            return "string";
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
        case WI_TOKEN_COLON:
            return ":";
        case WI_TOKEN_SEMICOLON:
            return ";";
        case WI_TOKEN_COMMA:
            return ",";
        case WI_TOKEN_DOT:
            return ".";
        case WI_TOKEN_DOT_DOT:
            return "..";
        case WI_TOKEN_HASH:
            return "#";
        case WI_TOKEN_ARROW:
            return "->";
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
        case WI_TOKEN_KW_VAR:
            return "var";
        case WI_TOKEN_KW_IF:
            return "if";
        case WI_TOKEN_KW_ELSE:
            return "else";
        case WI_TOKEN_KW_NULL:
            return "null";
        case WI_TOKEN_KW_TRUE:
            return "true";
        case WI_TOKEN_KW_FALSE:
            return "false";
        case WI_TOKEN_KW_WHILE:
            return "while";
        case WI_TOKEN_KW_FOR:
            return "for";
        case WI_TOKEN_KW_BREAK:
            return "break";
        case WI_TOKEN_KW_CONTINUE:
            return "continue";
        case WI_TOKEN_KW_FUNCTION:
            return "function";
        case WI_TOKEN_KW_RETURN:
            return "return";
        case WI_TOKEN_KW_NEW:
            return "new";
        case WI_TOKEN_KW_OBJECT:
            return "object";
        case WI_TOKEN_KW_REQUIRE:
            return "require";
        case WI_TOKEN_EOF:
            return "end of file";
        case WI_TOKEN_ERROR:
            return "error";
    }

    return "unknown";
}

void
wi_lexer_init(wi_lexer_t* lexer, const char* file_path, const char* src) {
    lexer->file_path = file_path;
    lexer->src       = src;
    lexer->start     = src;
    lexer->curr      = src;
    lexer->line      = 1;
    lexer->col       = 1;
}

static wi_token_t
_lexer_make_token(wi_lexer_t* lexer, wi_token_kind_t kind) {
    wi_token_t token = {
        .kind = kind,
        .line = lexer->line,
    };

    if (token.kind == WI_TOKEN_EOF) {
        token.start = "<eof>";
        token.len   = 5;
        token.col   = lexer->col;
    } else {
        token.start = lexer->start;
        token.len   = (int)(lexer->curr - lexer->start);
        token.col   = lexer->col - token.len;
    }

    return token;
}

static wi_token_t
_lexer_error(wi_lexer_t* lexer, const char* msg, int line, int col) {
    return (wi_token_t){
        .kind  = WI_TOKEN_ERROR,
        .start = msg,
        .len   = 1,
        .line  = line,
        .col   = col,
    };
}

static char
_lexer_peek(wi_lexer_t* lexer) {
    return *lexer->curr;
}

static char
_lexer_advance(wi_lexer_t* lexer) {
    char c = *lexer->curr++;

    if (c == '\n') {
        lexer->line++;
        lexer->col = 1;
    } else {
        lexer->col++;
    }

    return c;
}

static bool
_lexer_check(wi_lexer_t* lexer, char c) {
    return _lexer_peek(lexer) == c;
}

static bool
_lexer_is_at_end(wi_lexer_t* lexer) {
    return _lexer_check(lexer, '\0');
}

static char
_lexer_peek_next(wi_lexer_t* lexer) {
    if (_lexer_is_at_end(lexer)) {
        return '\0';
    }

    return lexer->curr[1];
}

static bool
_lexer_match(wi_lexer_t* lexer, char c) {
    if (!_lexer_check(lexer, c)) {
        return false;
    }

    _lexer_advance(lexer);
    return true;
}

static wi_token_kind_t
_lexer_check_kw(wi_lexer_t* lexer, int start, int len, const char* rest, wi_token_kind_t kind) {
    if (lexer->curr - lexer->start == start + len && memcmp(lexer->start + start, rest, (size_t)len) == 0) {
        return kind;
    }

    return WI_TOKEN_NAME;
}

static wi_token_kind_t
_lexer_name_kind(wi_lexer_t* lexer) {
    switch (lexer->start[0]) {
        case 'v':
            return _lexer_check_kw(lexer, 1, 2, "ar", WI_TOKEN_KW_VAR);
        case 'i':
            return _lexer_check_kw(lexer, 1, 1, "f", WI_TOKEN_KW_IF);
        case 'e':
            return _lexer_check_kw(lexer, 1, 3, "lse", WI_TOKEN_KW_ELSE);
        case 'n':
            if (lexer->curr - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'u':
                        return _lexer_check_kw(lexer, 2, 2, "ll", WI_TOKEN_KW_NULL);
                    case 'e':
                        return _lexer_check_kw(lexer, 2, 1, "w", WI_TOKEN_KW_NEW);
                }
            }

            break;
        case 'f':
            if (lexer->curr - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'a':
                        return _lexer_check_kw(lexer, 2, 3, "lse", WI_TOKEN_KW_FALSE);
                    case 'o':
                        return _lexer_check_kw(lexer, 2, 1, "r", WI_TOKEN_KW_FOR);
                    case 'u':
                        return _lexer_check_kw(lexer, 2, 6, "nction", WI_TOKEN_KW_FUNCTION);
                }
            }

            break;
        case 'w':
            return _lexer_check_kw(lexer, 1, 4, "hile", WI_TOKEN_KW_WHILE);
        case 't':
            return _lexer_check_kw(lexer, 1, 3, "rue", WI_TOKEN_KW_TRUE);
        case 'b':
            return _lexer_check_kw(lexer, 1, 4, "reak", WI_TOKEN_KW_BREAK);
        case 'o':
            return _lexer_check_kw(lexer, 1, 5, "bject", WI_TOKEN_KW_OBJECT);
        case 'c':
            return _lexer_check_kw(lexer, 1, 7, "ontinue", WI_TOKEN_KW_CONTINUE);
        case 'r':
            if (lexer->curr - lexer->start > 2 && lexer->start[1] == 'e') {
                switch (lexer->start[2]) {
                    case 't':
                        return _lexer_check_kw(lexer, 3, 3, "urn", WI_TOKEN_KW_RETURN);
                    case 'q':
                        return _lexer_check_kw(lexer, 3, 4, "uire", WI_TOKEN_KW_REQUIRE);
                }
            }

            break;
    }

    return WI_TOKEN_NAME;
}

static wi_token_t
_lexer_name(wi_lexer_t* ls) {
    while (wi_is_alnum(_lexer_peek(ls))) {
        _lexer_advance(ls);
    }

    return _lexer_make_token(ls, _lexer_name_kind(ls));
}

static wi_token_t
_lexer_real(wi_lexer_t* lexer) {
    if (lexer->start[0] == '0' && (_lexer_peek(lexer) == 'b' || _lexer_peek(lexer) == 'B')) {
        _lexer_advance(lexer);

        while (_lexer_peek(lexer) == '0' || _lexer_peek(lexer) == '1') {
            _lexer_advance(lexer);
        }

        return _lexer_make_token(lexer, WI_TOKEN_LIT_REAL);
    }

    if (lexer->start[0] == '0' && (_lexer_check(lexer, 'x') || _lexer_check(lexer, 'X'))) {
        _lexer_advance(lexer);

        while (wi_is_hex_digit(_lexer_peek(lexer))) {
            _lexer_advance(lexer);
        }

        return _lexer_make_token(lexer, WI_TOKEN_LIT_REAL);
    }

    while (wi_is_digit(_lexer_peek(lexer))) {
        _lexer_advance(lexer);
    }

    if (_lexer_check(lexer, '.') && wi_is_digit(_lexer_peek_next(lexer))) {
        do {
            _lexer_advance(lexer);
        } while (wi_is_digit(_lexer_peek(lexer)));
    }

    return _lexer_make_token(lexer, WI_TOKEN_LIT_REAL);
}

static wi_token_t
_lexer_string(wi_lexer_t* lexer) {
    int line = lexer->line;
    int col  = lexer->col - 1;

    while (_lexer_peek(lexer) != '"' && !_lexer_is_at_end(lexer)) {
        if (_lexer_check(lexer, '\\') && _lexer_peek_next(lexer) != '\0') {
            _lexer_advance(lexer);
        }

        _lexer_advance(lexer);
    }

    if (_lexer_is_at_end(lexer)) {
        return _lexer_error(lexer, "unfinished string", line, col);
    }

    _lexer_advance(lexer);
    return _lexer_make_token(lexer, WI_TOKEN_LIT_STRING);
}

static void
_lexer_skip_whitespace(wi_lexer_t* lexer) {
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

wi_token_t
wi_lexer_next(wi_lexer_t* lexer) {
    _lexer_skip_whitespace(lexer);
    lexer->start = lexer->curr;

    if (_lexer_is_at_end(lexer)) {
        return _lexer_make_token(lexer, WI_TOKEN_EOF);
    }

    char c = _lexer_advance(lexer);

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
            return _lexer_make_token(lexer, WI_TOKEN_OPEN_BRACE);
        case '}':
            return _lexer_make_token(lexer, WI_TOKEN_CLOSE_BRACE);
        case ':':
            return _lexer_make_token(lexer, WI_TOKEN_COLON);
        case ';':
            return _lexer_make_token(lexer, WI_TOKEN_SEMICOLON);
        case ',':
            return _lexer_make_token(lexer, WI_TOKEN_COMMA);
        case '.':
            return _lexer_make_token(lexer, _lexer_match(lexer, '.') ? WI_TOKEN_DOT_DOT : WI_TOKEN_DOT);
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
            return _lexer_make_token(lexer, _lexer_match(lexer, '=') ? WI_TOKEN_EQUAL_EQUAL : WI_TOKEN_EQUAL);
        case '!':
            return _lexer_make_token(lexer, _lexer_match(lexer, '=') ? WI_TOKEN_BANG_EQUAL : WI_TOKEN_BANG);
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

    return _lexer_error(lexer, "unexpected character", lexer->line, lexer->col);
}
