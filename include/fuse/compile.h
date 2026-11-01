#ifndef FUSE_COMPILE_H
#define FUSE_COMPILE_H

#include "fuse/dialect.h"
#include "fuse/semantic.h"

typedef struct {
    Program *program;
    SemanticInfo semantic_info;
} PreparedProgram;

typedef struct {
    const char *kind;
    SourceSpan span;
    char message[128];
} CompileError;

/*
 * Parse, optionally select a one-based preview line (zero means the full program),
 * lower, type check, and validate the result against the dialect.
 * Numeric dialects allow numeric/vector previews; other dialects require their
 * complete-program result type for previews too.
 *
 * Initialize out to zero before first use. Success replaces its previous result;
 * failure preserves it. Source storage is borrowed and must outlive the result.
 */
bool prepare_program(
    const char *source,
    size_t length,
    const FuseDialect *dialect,
    size_t preview_line,
    PreparedProgram *out,
    CompileError *error
);

void prepared_program_destroy(PreparedProgram *prepared);

#endif
