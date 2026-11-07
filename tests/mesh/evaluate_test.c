#define _GNU_SOURCE

#include "mesh/builtins.h"
#include "mesh/mesh.h"
#include "mesh/render.h"

#include "grafic/lower.h"
#include "grafic/parser.h"
#include "grafic/semantic.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool compile_mesh(
    const char *source,
    GraficMesh *out_mesh
)
{
    Program *syntax = NULL;
    Program *lowered = NULL;
    ParseError parse_error;
    LowerError lower_error;
    SemanticInfo semantic_info = {0};
    SemanticError semantic_error;
    GraficMeshRenderError render_error;
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
            graficmesh_dialect()->environment,
            &semantic_info,
            &semantic_error
        )
        && semantic_info.result_type == VALUE_TYPE_MESH
        && graficmesh_render(
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
    GraficMesh mesh = {0};
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
        && graficmesh_mesh_write_obj(output, &mesh);

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
    graficmesh_mesh_destroy(&mesh);
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
            graficmesh_dialect()->environment,
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

static bool test_scatter_area_sampling(void)
{
    float source_positions[] = {-0.01f,0,0, 0.01f,0,0, 0,0.1f,0};
    float source_uvs[] = {0,0, 1,0, 0.5f,1};
    uint32_t source_indices[] = {0,1,2};
    GraficMesh source = {.positions = source_positions, .uvs = source_uvs,
        .indices = source_indices, .vertex_count = 3, .index_count = 3};
    float surface_positions[] = {0,0,0, 1,0,0, 0,0,1, 10,0,0, 13,0,0, 10,0,3};
    uint32_t surface_indices[] = {0,1,2, 3,4,5, 0,0,0};
    GraficMesh surface = {.positions = surface_positions, .indices = surface_indices,
        .vertex_count = 6, .index_count = 9};
    GraficMeshScatterSpec settings = {10000, 42, 1, 1};
    GraficMesh scattered = {0};
    const char *error;
    bool valid = graficmesh_mesh_scatter(&source, &surface, &settings, &scattered, &error);
    size_t small = 0;
    double mean_x = 0, mean_z = 0;
    for (size_t copy = 0; valid && copy < settings.count; copy += 1) {
        const float *a = &scattered.positions[copy * 9], *b = a + 3;
        double x = (a[0] + b[0]) * 0.5, z = (a[2] + b[2]) * 0.5;
        valid = a[1] == 0 && b[1] == 0;
        if (x < 2) {
            small += 1; mean_x += x; mean_z += z;
            valid = valid && x >= -0.00001 && z >= -0.00001 && x + z <= 1.00001;
        } else {
            valid = valid && x >= 9.99999 && z >= -0.00001 && x - 10 + z <= 3.00001;
        }
    }
    valid = valid && small > 850 && small < 1150
        && fabs(mean_x / (double)small - 1.0/3) < 0.03
        && fabs(mean_z / (double)small - 1.0/3) < 0.03;
    graficmesh_mesh_destroy(&scattered);
    surface.indices = &surface_indices[6]; surface.index_count = 3;
    valid = valid && !graficmesh_mesh_scatter(&source, &surface, &settings, &scattered, &error)
        && error != NULL && scattered.positions == NULL;
    printf("%s scatter area weighting, barycentric placement, and zero-area rejection\n", valid ? "PASS" : "FAIL");
    return valid;
}

int main(void)
{
    bool scatter = test_scatter_area_sampling();
    bool mesh = test_disk_mesh();
    bool type = test_result_type();

    if (!mesh) {
        fprintf(stderr, "FAIL graficmesh disk mesh\n");
    }

    if (!type) {
        fprintf(stderr, "FAIL graficmesh semantic type\n");
    }

    if (mesh && type && scatter) {
        printf("3/3 graficmesh tests passed\n");
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
