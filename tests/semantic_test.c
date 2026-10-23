#include "dialects.h"
#include "fuse/lower.h"
#include "fuse/parser.h"
#include "fuse/semantic.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *source;
    const FuseDialect *dialect;
    ValueType result;
    const char *error;
} SemanticCase;

static bool check_case(const SemanticCase *test_case)
{
    Program *syntax = NULL;
    Program *lowered = NULL;
    ParseError parse_error;
    LowerError lower_error;
    SemanticInfo info = {0};
    SemanticError error = {0};
    bool prepared = parse_program(
        test_case->source, strlen(test_case->source), &syntax, &parse_error
    ) && lower_program(syntax, &lowered, &lower_error);
    bool analyzed = prepared && semantic_analyze(
        lowered, test_case->source, test_case->dialect->environment, &info, &error
    );
    bool passed = prepared && (test_case->error != NULL
        ? !analyzed && error.message != NULL
            && strcmp(error.message, test_case->error) == 0
        : analyzed && info.result_type == test_case->result);

    if (!passed) {
        fprintf(stderr, "FAIL semantic analysis: %s (%s)\n", test_case->source,
            error.message == NULL ? "unexpected result" : error.message);
    }

    semantic_info_destroy(&info);
    program_destroy(lowered);
    program_destroy(syntax);
    return passed;
}

int main(void)
{
    static const SemanticCase CASES[] = {
        {"let p = point.shift(1.0); p.magnitude().finish()", &TEST_POINT_DIALECT, VALUE_TYPE_VEC4, NULL},
        {"point.scale(2.0)", &TEST_POINT_DIALECT, VALUE_TYPE_VEC2, NULL},
        {"2.0.scale(3.0)", &TEST_POINT_DIALECT, VALUE_TYPE_FLOAT, NULL},
        {"let m = object().resize(2.0); m", &TEST_OBJECT_DIALECT, VALUE_TYPE_MESH, NULL},
        {"object().measure()", &TEST_OBJECT_DIALECT, VALUE_TYPE_FLOAT, NULL},
        {"missing", &TEST_POINT_DIALECT, 0, "unknown identifier"},
        {"let p = q; let q = point; p", &TEST_POINT_DIALECT, 0, "forward reference is not allowed"},
        {"let p = point; let p = point; p", &TEST_POINT_DIALECT, 0, "duplicate binding"},
        {"point.shift(point)", &TEST_POINT_DIALECT, 0, "argument type mismatch"},
        {"point.shift()", &TEST_POINT_DIALECT, 0, "incorrect argument count"},
        {"point.missing()", &TEST_POINT_DIALECT, 0, "unknown function"},
        {"let internal = point; internal", &TEST_POINT_DIALECT, 0, "binding name is reserved"},
        {"let point = 1.0; point", &TEST_POINT_DIALECT, 0, "binding name is reserved"},
        {"let shift = 1.0; shift", &TEST_POINT_DIALECT, 0, "binding name is reserved"},
        {"object()", &TEST_POINT_DIALECT, 0, "unknown function"},
        {"point", &TEST_OBJECT_DIALECT, 0, "unknown identifier"},
        {"uv", &TEST_OBJECT_DIALECT, 0, "unknown identifier"},
    };
    size_t passed = 0;
    size_t count = sizeof(CASES) / sizeof(CASES[0]);

    for (size_t i = 0; i < count; i += 1) {
        passed += check_case(&CASES[i]);
    }

    printf("%zu/%zu semantic cases passed\n", passed, count);
    return passed == count ? EXIT_SUCCESS : EXIT_FAILURE;
}
