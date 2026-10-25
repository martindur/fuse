#define _GNU_SOURCE

#include "mesh/builtins.h"
#include "mesh/mesh.h"
#include "mesh/render.h"

#include "fuse/lower.h"
#include "fuse/parser.h"
#include "fuse/semantic.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool compile_mesh(
    const char *source,
    FuseMesh *out_mesh
)
{
    Program *syntax = NULL;
    Program *lowered = NULL;
    ParseError parse_error;
    LowerError lower_error;
    SemanticInfo semantic_info = {0};
    SemanticError semantic_error;
    FuseMeshRenderError render_error;
    bool compiled = parse_program(
            source,
            strlen(source),
            &syntax,
            &parse_error
        )
        && lower_program(syntax, &lowered, &lower_error)
        && semantic_analyze(
            lowered,
            source,
            fusemesh_dialect()->environment,
            &semantic_info,
            &semantic_error
        )
        && semantic_info.result_type == VALUE_TYPE_MESH
        && fusemesh_render(
            lowered,
            source,
            out_mesh,
            &render_error
        );

    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(syntax);
    return compiled;
}

static bool test_disk_mesh(void)
{
    FuseMesh mesh = {0};
    bool rendered = compile_mesh(
        "disk()\n"
        ".radius(2.0)\n"
        ".segments(8)\n"
        ".rings(2)\n"
        ".uv_radial()",
        &mesh
    );
    bool valid = rendered
        && mesh.vertex_count == 17
        && mesh.index_count == 72
        && mesh.positions[0] == 0.0f
        && mesh.positions[1] == 0.0f
        && mesh.uvs[0] == 0.0f
        && mesh.uvs[1] == 0.0f
        && mesh.uvs[2] == 0.5f
        && mesh.uvs[3] == 0.0f
        && mesh.uvs[4] == 0.5f
        && mesh.uvs[5] == 0.125f;

    char *obj = NULL;
    size_t obj_length = 0;
    FILE *output = open_memstream(&obj, &obj_length);
    bool wrote = output != NULL
        && fusemesh_mesh_write_obj(output, &mesh);

    if (output != NULL) {
        fclose(output);
    }

    valid = valid
        && wrote
        && obj != NULL
        && strstr(obj, "v 0 0 0\n") != NULL
        && strstr(obj, "vt 0 0\n") != NULL
        && strstr(obj, "f 1/1 2/2 3/3\n") != NULL;

    free(obj);
    fusemesh_mesh_destroy(&mesh);
    return valid;
}

static bool test_result_type(void)
{
    Program *syntax = NULL;
    ParseError parse_error;
    bool parsed = parse_program(
        "1.0",
        3,
        &syntax,
        &parse_error
    );
    Program *lowered = NULL;
    LowerError lower_error;
    bool lowered_ok = parsed
        && lower_program(syntax, &lowered, &lower_error);
    SemanticInfo semantic_info = {0};
    SemanticError semantic_error;
    bool analyzed = lowered_ok
        && semantic_analyze(
            lowered,
            "1.0",
            fusemesh_dialect()->environment,
            &semantic_info,
            &semantic_error
        );
    bool valid = analyzed
        && semantic_info.result_type == VALUE_TYPE_FLOAT;

    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(syntax);
    return valid;
}

int main(void)
{
    bool mesh = test_disk_mesh();
    bool type = test_result_type();

    if (!mesh) {
        fprintf(stderr, "FAIL fusemesh disk mesh\n");
    }

    if (!type) {
        fprintf(stderr, "FAIL fusemesh semantic type\n");
    }

    if (mesh && type) {
        printf("2/2 fusemesh tests passed\n");
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
