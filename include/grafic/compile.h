#ifndef GRAFIC_COMPILE_H
#define GRAFIC_COMPILE_H

#include "grafic/dialect.h"
#include "grafic/semantic.h"

typedef struct {
    Program *program;
    SemanticInfo semantic_info;
    /* Complete-program attributes retained for position-only vertex inspection. */
    Program *context_program;
    SemanticInfo context_semantic_info;
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
 * complete-program result type for previews too, unless an alternate is declared.
 * Alternate previews may retain a valid complete program as attribute context.
 *
 * Initialize out to zero before first use. Success replaces its previous result;
 * failure preserves it. Source storage is borrowed and must outlive the result.
 */
bool prepare_program(
    const char *source,
    size_t length,
    const GraficDialect *dialect,
    size_t preview_line,
    PreparedProgram *out,
    CompileError *error
);

void prepared_program_destroy(PreparedProgram *prepared);

/* Viewer policy: explicit line, otherwise last show(), otherwise full program. */
bool prepare_view_program(const char *source, size_t length, const GraficDialect *dialect,
    size_t preview_line, PreparedProgram *out, CompileError *error);

#endif
