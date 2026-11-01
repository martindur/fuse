#include "fuse/compile.h"
#include "fuse/lower.h"
#include "fuse/parser.h"
#include "fuse/preview.h"

#include <stdio.h>
#include <string.h>

static void set_error(CompileError *error, const char *kind, SourceSpan span, const char *message)
{
    error->kind = kind;
    error->span = span;
    snprintf(error->message, sizeof(error->message), "%s", message);
}

void prepared_program_destroy(PreparedProgram *prepared)
{
    if (prepared != NULL) {
        semantic_info_destroy(&prepared->semantic_info);
        program_destroy(prepared->program);
        *prepared = (PreparedProgram){0};
    }
}

bool prepare_program(
    const char *source,
    size_t length,
    const FuseDialect *dialect,
    size_t preview_line,
    PreparedProgram *out,
    CompileError *error
)
{
    if (error == NULL) {
        return false;
    }
    *error = (CompileError){0};
    if (source == NULL || out == NULL || dialect == NULL || dialect->environment == NULL
        || dialect->required_result_type < VALUE_TYPE_FLOAT
        || dialect->required_result_type > VALUE_TYPE_MESH) {
        set_error(error, "compile", (SourceSpan){0}, "invalid compilation input");
        return false;
    }
    const char *nul = memchr(source, '\0', length);
    if (nul != NULL) {
        set_error(error, "parse", (SourceSpan){(size_t)(nul - source), 1}, "NUL byte in source");
        return false;
    }

    Program *syntax = NULL;
    Program *preview = NULL;
    PreparedProgram prepared = {0};
    bool success = false;
    ParseError parse_error;
    if (!parse_program(source, length, &syntax, &parse_error)) {
        set_error(error, "parse", parse_error.span, parse_error.message);
        goto cleanup;
    }
    const Program *selected = syntax;
    if (preview_line != 0) {
        PreviewError preview_error;
        if (!preview_program_at_line(syntax, source, preview_line, &preview, &preview_error)) {
            set_error(error, "preview", preview_error.span, preview_error.message);
            goto cleanup;
        }
        selected = preview;
    }
    LowerError lower_error;
    if (!lower_program(selected, &prepared.program, &lower_error)) {
        set_error(error, "lowering", lower_error.span, lower_error.message);
        goto cleanup;
    }
    SemanticError semantic_error;
    if (!semantic_analyze(prepared.program, source, dialect->environment,
            &prepared.semantic_info, &semantic_error)) {
        set_error(error, "semantic", semantic_error.span, semantic_error.message);
        goto cleanup;
    }

    ValueType result = prepared.semantic_info.result_type;
    bool numeric_preview = preview_line != 0 && value_type_is_previewable(dialect->required_result_type);
    if (numeric_preview ? !value_type_is_previewable(result) : result != dialect->required_result_type) {
        set_error(error, "semantic", prepared.program->result->span,
            numeric_preview ? "preview result has unsupported type" : "");
        if (!numeric_preview) {
            snprintf(error->message, sizeof(error->message), "program result must be %s",
                value_type_name(dialect->required_result_type));
        }
        goto cleanup;
    }

    prepared_program_destroy(out);
    *out = prepared;
    prepared = (PreparedProgram){0};
    success = true;
cleanup:
    prepared_program_destroy(&prepared);
    program_destroy(preview);
    program_destroy(syntax);
    return success;
}
