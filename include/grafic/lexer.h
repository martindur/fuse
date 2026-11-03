#ifndef GRAFIC_LEXER_H
#define GRAFIC_LEXER_H

#include "grafic/source.h"

#include <stddef.h>

typedef enum {
  TOKEN_EOF,
  TOKEN_INVALID,
  TOKEN_LET,
  TOKEN_IDENTIFIER,
  TOKEN_NUMBER,
  TOKEN_DOT,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_COMMA,
  TOKEN_EQUAL,
  TOKEN_SEMICOLON
} TokenKind;

typedef struct {
  TokenKind kind;
  SourceSpan span;
} Token;

typedef struct {
  const char *source;
  size_t length;
  size_t cursor;
} Lexer;

void lexer_init(Lexer *lexer, const char *source, size_t length);
Token lexer_next(Lexer *lexer);
const char *token_kind_name(TokenKind kind);

#endif
