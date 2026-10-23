#ifndef FUSE_PARSER_H
#define FUSE_PARSER_H

#include "fuse/ast.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  SourceSpan span;
  const char *message;
} ParseError;

bool parse_source(
    const char *source,
    size_t length,
    Expr **out_expression,
    ParseError *out_error
);

bool parse_program(
    const char *source,
    size_t length,
    Program **out_program,
    ParseError *out_error
);

#endif
