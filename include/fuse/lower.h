#ifndef FUSE_LOWER_H
#define FUSE_LOWER_H

#include "fuse/ast.h"

#include <stdbool.h>

typedef struct {
    SourceSpan span;
    const char *message;
} LowerError;

/*
 * Builds a new AST without modifying `syntax`.
 *
 * Success:
 *   - returns true
 *   - caller owns *out_lowered
 *   - output contains no EXPR_PIPELINE nodes
 *
 * Failure:
 *   - returns false
 *   - *out_lowered is NULL
 *   - out_error describes the first failure
 */
bool lower_expression(
    const Expr *syntax,
    Expr **out_lowered,
    LowerError *out_error
);

bool lower_program(
    const Program *syntax,
    Program **out_lowered,
    LowerError *out_error
);

#endif
