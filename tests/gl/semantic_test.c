#include "fuse/lower.h"
#include "fuse/parser.h"
#include "fuse/semantic.h"
#include "gl/glsl_stdlib.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *source;
    const char *message;
} FailureCase;

static Program *parse_and_lower(const char *source)
{
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
            "test setup parse error at %zu:%zu: %s\n",
            parse_error.span.offset,
            parse_error.span.length,
            parse_error.message
        );
        return NULL;
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
            "test setup lowering error at %zu:%zu: %s\n",
            lower_error.span.offset,
            lower_error.span.length,
            lower_error.message
        );
    }

    program_destroy(syntax);
    return lowered;
}

static bool test_valid_program(void)
{
    const char *source =
        "let p = uv.center().aspect();\n"
        "let shape = p.circle(0.2);\n"
        "shape\n"
        ".fill()\n"
        ".colorize(rgb(0.2, 0.6, 1.0))";

    Program *program = parse_and_lower(source);

    if (program == NULL) {
        return false;
    }

    SemanticInfo info;
    SemanticError error;
    bool analyzed = semantic_analyze(
        program,
        source,
        fusegl_dialect()->environment,
        &info,
        &error
    );
    bool passed = analyzed
        && info.binding_count == 2
        && info.binding_types[0] == VALUE_TYPE_VEC2
        && info.binding_types[1] == VALUE_TYPE_FLOAT
        && info.result_type == VALUE_TYPE_VEC4;

    if (!passed) {
        fprintf(
            stderr,
            "FAIL valid semantic program: %s\n",
            error.message == NULL ? "wrong types" : error.message
        );
    } else {
        printf("PASS valid semantic program\n");
    }

    semantic_info_destroy(&info);
    program_destroy(program);
    return passed;
}

static bool test_texture_program(void)
{
    const char *source =
        "let first = uv"
        " .scroll(vec2(0.04, 0.01), time)"
        " .repeat()"
        " .sample(tex0);\n"
        "let second = uv"
        " .scroll(vec2(-0.02, 0.03), time)"
        " .repeat()"
        " .sample(tex1);\n"
        "first.multiply(second)";

    Program *program = parse_and_lower(source);

    if (program == NULL) {
        return false;
    }

    SemanticInfo info;
    SemanticError error;
    bool analyzed = semantic_analyze(
        program,
        source,
        fusegl_dialect()->environment,
        &info,
        &error
    );
    bool passed = analyzed
        && info.binding_count == 2
        && info.binding_types[0] == VALUE_TYPE_VEC4
        && info.binding_types[1] == VALUE_TYPE_VEC4
        && info.result_type == VALUE_TYPE_VEC4;

    if (!passed) {
        fprintf(
            stderr,
            "FAIL texture semantic program: %s\n",
            error.message == NULL ? "wrong types" : error.message
        );
    } else {
        printf("PASS texture semantic program\n");
    }

    semantic_info_destroy(&info);
    program_destroy(program);
    return passed;
}

static bool test_semantic_failures(void)
{
    static const FailureCase CASES[] = {
        {
            "missing.vec4(0.0, 1.0)",
            "unknown identifier",
        },
        {
            "let p = q;\n"
            "let q = uv;\n"
            "p.vec4(0.0, 1.0)",
            "forward reference is not allowed",
        },
        {
            "let p = uv;\n"
            "let p = uv;\n"
            "p.vec4(0.0, 1.0)",
            "duplicate binding",
        },
        {
            "uv.fill().colorize(rgb(1.0, 1.0, 1.0))",
            "argument type mismatch",
        },
        {
            "uv.mystery().vec4(0.0, 1.0)",
            "unknown function",
        },
    };

    bool passed = true;
    size_t case_count = sizeof(CASES) / sizeof(CASES[0]);

    for (size_t index = 0; index < case_count; index += 1) {
        const FailureCase *test_case = &CASES[index];
        Program *program = parse_and_lower(test_case->source);

        if (program == NULL) {
            passed = false;
            continue;
        }

        SemanticInfo info;
        SemanticError error;
        bool analyzed = semantic_analyze(
            program,
            test_case->source,
            fusegl_dialect()->environment,
            &info,
            &error
        );

        if (analyzed
            || error.message == NULL
            || strcmp(error.message, test_case->message) != 0) {
            fprintf(
                stderr,
                "FAIL semantic error for \"%s\"\n"
                "  expected: %s\n"
                "  actual:   %s\n",
                test_case->source,
                test_case->message,
                error.message == NULL
                    ? "(no message)"
                    : error.message
            );
            passed = false;
        }

        semantic_info_destroy(&info);
        program_destroy(program);
    }

    if (passed) {
        printf("PASS semantic failures\n");
    }

    return passed;
}

static bool test_non_vec4_result_inference(void)
{
    const char *source = "uv.center()";
    Program *program = parse_and_lower(source);

    if (program == NULL) {
        return false;
    }

    SemanticInfo info;
    SemanticError error;
    bool analyzed = semantic_analyze(
        program,
        source,
        fusegl_dialect()->environment,
        &info,
        &error
    );
    bool passed = analyzed && info.result_type == VALUE_TYPE_VEC2;

    if (!passed) {
        fprintf(
            stderr,
            "FAIL non-vec4 semantic result inference: %s\n",
            error.message == NULL ? "wrong type" : error.message
        );
    } else {
        printf("PASS non-vec4 semantic result inference\n");
    }

    semantic_info_destroy(&info);
    program_destroy(program);
    return passed;
}

int main(void)
{
    size_t passed = 0;

    if (test_valid_program()) {
        passed += 1;
    }

    if (test_texture_program()) {
        passed += 1;
    }

    if (test_semantic_failures()) {
        passed += 1;
    }

    if (test_non_vec4_result_inference()) {
        passed += 1;
    }

    printf("%zu/4 semantic tests passed\n", passed);
    return passed == 4 ? EXIT_SUCCESS : EXIT_FAILURE;
}
