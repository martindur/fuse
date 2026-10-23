#ifndef FUSE_PREVIEW_H
#define FUSE_PREVIEW_H

#include "fuse/ast.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    SourceSpan span;
    const char *message;
} PreviewError;

/*
 * Builds an owned program whose result is the last preview point on `line`.
 * Lines are one-based. Pipeline stages are anchored to their function name;
 * the initial expression of each pipeline is also a preview point.
 */
bool preview_program_at_line(
    const Program *syntax,
    const char *source,
    size_t line,
    Program **out_preview,
    PreviewError *out_error
);

#endif
