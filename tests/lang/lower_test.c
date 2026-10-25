#define _POSIX_C_SOURCE 200809L

#include "fuse/ast_dump.h"
#include "fuse/lower.h"
#include "fuse/parser.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char EXPECTED_LOWERED_DUMP[] =
    "(call \"rotate\"\n"
    "  (call \"center\"\n"
    "    (identifier \"uv\"))\n"
    "  (identifier \"time\"))\n";

static bool contains_pipeline(const Expr *expression)
{
    switch (expression->kind) {
    case EXPR_IDENTIFIER:
    case EXPR_NUMBER:
        return false;

    case EXPR_CALL:
        for (size_t index = 0;
             index < expression->as.call.arguments.count;
             index += 1) {
            if (contains_pipeline(
                    expression->as.call.arguments.items[index]
                )) {
                return true;
            }
        }

        return false;

    case EXPR_PIPELINE:
        return true;
    }

    return true;
}

static bool dump_to_string(
    const Expr *expression,
    const char *source,
    char **out_text,
    size_t *out_length
)
{
    FILE *stream = open_memstream(out_text, out_length);

    if (stream == NULL) {
        return false;
    }

    bool dumped = ast_dump(stream, expression, source);
    bool closed = fclose(stream) != EOF;

    return dumped && closed;
}

static bool test_pipeline_lowering(void)
{
    const char *source =
        "uv.center().rotate(time)";

    Expr *syntax = NULL;
    ParseError parse_error;

    if (!parse_source(
            source,
            strlen(source),
            &syntax,
            &parse_error
        )) {
        fprintf(
            stderr,
            "FAIL lowering: parse error at %zu:%zu: %s\n",
            parse_error.span.offset,
            parse_error.span.length,
            parse_error.message
        );
        return false;
    }

    Expr *lowered = NULL;
    LowerError lower_error;

    if (!lower_expression(
            syntax,
            &lowered,
            &lower_error
        )) {
        fprintf(
            stderr,
            "FAIL lowering at %zu:%zu: %s\n",
            lower_error.span.offset,
            lower_error.span.length,
            lower_error.message
        );
        expr_destroy(syntax);
        return false;
    }

    bool passed = true;

    if (syntax->kind != EXPR_PIPELINE) {
        fprintf(
            stderr,
            "FAIL lowering: input AST was modified\n"
        );
        passed = false;
    }

    if (contains_pipeline(lowered)) {
        fprintf(
            stderr,
            "FAIL lowering: output still contains a pipeline\n"
        );
        passed = false;
    }

    char *actual = NULL;
    size_t actual_length = 0;

    if (!dump_to_string(
            lowered,
            source,
            &actual,
            &actual_length
        )) {
        fprintf(
            stderr,
            "FAIL lowering: could not dump lowered AST\n"
        );
        passed = false;
    } else {
        size_t expected_length =
            sizeof(EXPECTED_LOWERED_DUMP) - 1;

        if (actual_length != expected_length
            || memcmp(
                actual,
                EXPECTED_LOWERED_DUMP,
                expected_length
            ) != 0) {
            fprintf(
                stderr,
                "FAIL lowering snapshot\n"
                "expected:\n%s"
                "actual:\n%s",
                EXPECTED_LOWERED_DUMP,
                actual
            );
            passed = false;
        }
    }

    free(actual);
    expr_destroy(lowered);
    expr_destroy(syntax);

    if (passed) {
        printf("PASS pipeline lowering\n");
    }

    return passed;
}

static bool test_program_lowering(void)
{
    const char *source =
        "let p = uv.center();\n"
        "p.vec4(0.0, 1.0)";

    Program *syntax = NULL;
    ParseError parse_error;

    if (!parse_program(
            source,
            strlen(source),
            &syntax,
            &parse_error
        )) {
        fprintf(
            stderr,
            "FAIL program lowering: parse error at %zu:%zu: %s\n",
            parse_error.span.offset,
            parse_error.span.length,
            parse_error.message
        );
        return false;
    }

    Program *lowered = NULL;
    LowerError lower_error;

    if (!lower_program(
            syntax,
            &lowered,
            &lower_error
        )) {
        fprintf(
            stderr,
            "FAIL program lowering at %zu:%zu: %s\n",
            lower_error.span.offset,
            lower_error.span.length,
            lower_error.message
        );
        program_destroy(syntax);
        return false;
    }

    bool passed = lowered->binding_count == 1
        && !contains_pipeline(
            lowered->bindings[0].initializer
        )
        && !contains_pipeline(lowered->result);

    if (!passed) {
        fprintf(
            stderr,
            "FAIL program lowering: output contains pipeline syntax\n"
        );
    } else {
        printf("PASS program lowering\n");
    }

    program_destroy(lowered);
    program_destroy(syntax);
    return passed;
}

int main(void)
{
    size_t passed = 0;

    if (test_pipeline_lowering()) {
        passed += 1;
    }

    if (test_program_lowering()) {
        passed += 1;
    }

    printf(
        "%zu/2 lowering tests passed\n",
        passed
    );

    return passed == 2 ? EXIT_SUCCESS : EXIT_FAILURE;
}
