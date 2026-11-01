#include "fuse/compile.h"
#include "dialects.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char *source = "let p = point.shift(1.0);\np.magnitude().finish()";
    PreparedProgram prepared = {0};
    CompileError error;
    bool passed = prepare_program(source, strlen(source), &TEST_POINT_DIALECT, 0, &prepared, &error)
        && prepared.program->binding_count == 1
        && prepared.program->result->kind == EXPR_CALL
        && prepared.semantic_info.result_type == VALUE_TYPE_VEC4
        && prepared.semantic_info.binding_count == 1
        && prepared.semantic_info.binding_types[0] == VALUE_TYPE_VEC2;

    Program *previous = prepared.program;
    struct {
        const char *source;
        size_t line;
        const char *kind;
        const char *message;
    } failures[] = {
        {"point.", 0, "parse", "expected function call"},
        {"point.missing()", 0, "semantic", "unknown function"},
        {"point", 0, "semantic", "must be vec4"},
        {"point\n\n.magnitude().finish()", 2, "preview", "no previewable expression"},
    };
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i += 1) {
        bool rejected = !prepare_program(failures[i].source, strlen(failures[i].source),
            &TEST_POINT_DIALECT, failures[i].line, &prepared, &error);
        passed = rejected && error.kind != NULL && strcmp(error.kind, failures[i].kind) == 0
            && strstr(error.message, failures[i].message) != NULL
            && prepared.program == previous && passed;
    }
    const char *preview = "point\n.shift(1.0)\n.magnitude().finish()";
    passed = prepare_program(preview, strlen(preview), &TEST_POINT_DIALECT, 2, &prepared, &error)
        && prepared.semantic_info.result_type == VALUE_TYPE_VEC2 && passed;

    const char *mesh = "object()\n.resize(2.0)";
    passed = prepare_program(mesh, strlen(mesh), &TEST_OBJECT_DIALECT, 1, &prepared, &error)
        && prepared.semantic_info.result_type == VALUE_TYPE_MESH && passed;
    const char *scalar_mesh = "object().measure()";
    passed = !prepare_program(scalar_mesh, strlen(scalar_mesh), &TEST_OBJECT_DIALECT, 1, &prepared, &error)
        && strstr(error.message, "must be mesh") != NULL && passed;

    const char nul[] = {'p', '\0', 'x'};
    passed = !prepare_program(nul, sizeof(nul), &TEST_POINT_DIALECT, 0, &prepared, &error)
        && error.span.offset == 1 && strstr(error.message, "NUL") != NULL && passed;
    passed = !prepare_program(NULL, 0, &TEST_POINT_DIALECT, 0, &prepared, &error) && passed;
    prepared_program_destroy(&prepared);
    prepared_program_destroy(&prepared);
    passed = prepared.program == NULL && prepared.semantic_info.binding_types == NULL && passed;
    printf("%s shared program preparation, preview policy, and failure preservation\n", passed ? "PASS" : "FAIL");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
