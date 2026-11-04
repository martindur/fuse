#include "render.h"

#include "evaluate.h"

bool graficmesh_render(
    const Program *program,
    const char *source,
    GraficMesh *out_mesh,
    GraficMeshRenderError *out_error
)
{
    if (out_mesh == NULL || out_error == NULL) {
        return false;
    }

    *out_mesh = (GraficMesh){0};
    *out_error = (GraficMeshRenderError){0};

    GraficMeshSpec spec;
    GraficMeshEvalError eval_error;

    if (!graficmesh_evaluate_program(
            program,
            source,
            &spec,
            &eval_error
        )) {
        out_error->span = eval_error.span;
        out_error->message = eval_error.message;
        return false;
    }

    const char *mesh_error = NULL;

    bool generated = spec.kind == GRAFIC_MESH_GRID
        ? graficmesh_mesh_from_grid(&spec.grid, out_mesh, &mesh_error)
        : graficmesh_mesh_from_disk(&spec.disk, out_mesh, &mesh_error);
    if (!generated) {
        out_error->span = program->result->span;
        out_error->message = mesh_error;
        return false;
    }

    return true;
}
