#include "fuse/preview.h"
#include "fuse/parser.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool span_equals(
    const char *source,
    SourceSpan span,
    const char *expected
)
{
    size_t length = strlen(expected);

    return span.length == length
        && memcmp(source + span.offset, expected, length) == 0;
}

static bool result_stage_equals(
    const Program *program,
    const char *source,
    const char *expected
)
{
    const Expr *result = program->result;

    return result != NULL
        && result->kind == EXPR_PIPELINE
        && result->as.pipeline.stage != NULL
        && result->as.pipeline.stage->kind == EXPR_CALL
        && span_equals(
            source,
            result->as.pipeline.stage->as.call.name,
            expected
        );
}

static bool select_line(
    const Program *syntax,
    const char *source,
    size_t line,
    Program **out_preview
)
{
    PreviewError error;

    if (!preview_program_at_line(
            syntax,
            source,
            line,
            out_preview,
            &error
        )) {
        fprintf(
            stderr,
            "preview test setup failed at line %zu: %s\n",
            line,
            error.message == NULL ? "unknown error" : error.message
        );
        return false;
    }

    return true;
}

static bool test_binding_and_result_selection(void)
{
    const char *source =
        "let p = uv\n"
        "    .center()\n"
        "    .aspect();\n"
        "\n"
        "p\n"
        ".length()\n"
        ".colorize(rgb(1.0, 1.0, 1.0))";
    Program *syntax = NULL;
    ParseError parse_error;

    if (!parse_program(
            source,
            strlen(source),
            &syntax,
            &parse_error
        )) {
        fprintf(stderr, "preview test parse failed: %s\n", parse_error.message);
        return false;
    }

    Program *binding_preview = NULL;
    Program *result_preview = NULL;
    bool passed = select_line(
        syntax,
        source,
        2,
        &binding_preview
    ) && binding_preview->binding_count == 0
        && result_stage_equals(
            binding_preview,
            source,
            "center"
        )
        && select_line(
            syntax,
            source,
            6,
            &result_preview
        )
        && result_preview->binding_count == 1
        && result_stage_equals(
            result_preview,
            source,
            "length"
        );

    if (!passed) {
        fprintf(stderr, "FAIL preview binding/result selection\n");
    } else {
        printf("PASS preview binding/result selection\n");
    }

    program_destroy(result_preview);
    program_destroy(binding_preview);
    program_destroy(syntax);
    return passed;
}

static bool test_last_point_on_line(void)
{
    const char *source = "uv.center().aspect()";
    Program *syntax = NULL;
    ParseError parse_error;

    if (!parse_program(
            source,
            strlen(source),
            &syntax,
            &parse_error
        )) {
        return false;
    }

    Program *preview = NULL;
    bool passed = select_line(syntax, source, 1, &preview)
        && result_stage_equals(preview, source, "aspect");

    if (!passed) {
        fprintf(stderr, "FAIL preview last point on line\n");
    } else {
        printf("PASS preview last point on line\n");
    }

    program_destroy(preview);
    program_destroy(syntax);
    return passed;
}

static bool test_line_without_point(void)
{
    const char *source = "uv\n\n.center()";
    Program *syntax = NULL;
    ParseError parse_error;

    if (!parse_program(
            source,
            strlen(source),
            &syntax,
            &parse_error
        )) {
        return false;
    }

    Program *preview = NULL;
    PreviewError error;
    bool selected = preview_program_at_line(
        syntax,
        source,
        2,
        &preview,
        &error
    );
    bool passed = !selected
        && preview == NULL
        && error.message != NULL
        && strcmp(
            error.message,
            "line has no previewable expression"
        ) == 0;

    if (!passed) {
        fprintf(stderr, "FAIL preview empty line rejection\n");
    } else {
        printf("PASS preview empty line rejection\n");
    }

    program_destroy(preview);
    program_destroy(syntax);
    return passed;
}

int main(void)
{
    size_t passed = 0;

    if (test_binding_and_result_selection()) {
        passed += 1;
    }

    if (test_last_point_on_line()) {
        passed += 1;
    }

    if (test_line_without_point()) {
        passed += 1;
    }

    printf("%zu/3 preview tests passed\n", passed);
    return passed == 3 ? EXIT_SUCCESS : EXIT_FAILURE;
}
