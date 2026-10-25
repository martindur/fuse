#include "fuse/lexer.h"

#include <stdbool.h>
#include <string.h>

static bool is_at_end(const Lexer *lexer) {
  return lexer->cursor >= lexer->length;
}

static char peek(const Lexer *lexer) {
  if (is_at_end(lexer)) {
    return '\0';
  }

  return lexer->source[lexer->cursor];
}

static char peek_next(const Lexer *lexer) {
  if (lexer->cursor + 1 >= lexer->length) {
    return '\0';
  }

  return lexer->source[lexer->cursor + 1];
}

static char advance(Lexer *lexer) {
  char current = lexer->source[lexer->cursor];
  lexer->cursor += 1;
  return current;
}

static bool is_digit(char character) {
  return character >= '0' && character <= '9';
}

static bool is_identifier_start(char character) {
  return (character >= 'a' && character <= 'z')
    || (character >= 'A' && character <= 'Z')
    || character == '_';
}

static bool is_identifier_continue(char character) {
  return is_identifier_start(character) ||
    is_digit(character);
}

static void skip_whitespace(Lexer *lexer) {
  for (;;) {
    switch (peek(lexer)) {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
        advance(lexer);
        break;

      default:
        return;
    }
  }
}

static Token make_token(
    const Lexer *lexer,
    TokenKind kind,
    size_t start
) {
  Token token = {
    .kind = kind,
    .span = {
      .offset = start,
      .length = lexer->cursor - start,
    },
  };

  return token;
}

void lexer_init(Lexer *lexer, const char *source, size_t length) {
  lexer->source = source;
  lexer->length = length;
  lexer->cursor = 0;
}

Token lexer_next(Lexer *lexer) {
  skip_whitespace(lexer);

  size_t start = lexer->cursor;

  if (is_at_end(lexer)) {
    return make_token(lexer, TOKEN_EOF, start);
  }

  char current = advance(lexer);

  if (is_identifier_start(current)) {
    while (is_identifier_continue(peek(lexer))) {
      advance(lexer);
    }

    if (lexer->cursor - start == 3
        && memcmp(lexer->source + start, "let", 3) == 0) {
      return make_token(lexer, TOKEN_LET, start);
    }

    return make_token(lexer, TOKEN_IDENTIFIER, start);
  }

  if (is_digit(current)
      || (current == '-' && is_digit(peek(lexer)))) {
    while (is_digit(peek(lexer))) {
      advance(lexer);
    }

    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
      advance(lexer);

      while (is_digit(peek(lexer))) {
        advance(lexer);
      }
    }

    return make_token(lexer, TOKEN_NUMBER, start);
  }

  switch (current) {
    case '(':
      return make_token(lexer, TOKEN_LEFT_PAREN, start);
    case ')':
      return make_token(lexer, TOKEN_RIGHT_PAREN, start);
    case ',':
      return make_token(lexer, TOKEN_COMMA, start);
    case '=':
      return make_token(lexer, TOKEN_EQUAL, start);
    case ';':
      return make_token(lexer, TOKEN_SEMICOLON, start);
    case '.':
      return make_token(lexer, TOKEN_DOT, start);
    default:
      return make_token(lexer, TOKEN_INVALID, start);
  }
}

const char *token_kind_name(TokenKind kind) {
  switch (kind) {
    case TOKEN_EOF:
      return "EOF";
    case TOKEN_INVALID:
      return "INVALID";
    case TOKEN_LET:
      return "LET";
    case TOKEN_IDENTIFIER:
      return "IDENTIFIER";
    case TOKEN_NUMBER:
      return "NUMBER";
    case TOKEN_DOT:
      return "DOT";
    case TOKEN_LEFT_PAREN:
      return "LEFT_PAREN";
    case TOKEN_RIGHT_PAREN:
      return "RIGHT_PAREN";
    case TOKEN_COMMA:
      return "COMMA";
    case TOKEN_EQUAL:
      return "EQUAL";
    case TOKEN_SEMICOLON:
      return "SEMICOLON";
  }

  return "UNKNOWN";
}
