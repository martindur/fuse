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

    MeshPlan plan;
    GraficMeshEvalError eval_error;

    if (!graficmesh_evaluate_program(
            program,
            source,
            &plan,
            &eval_error
        )) {
        out_error->span = eval_error.span;
        out_error->message = eval_error.message;
        return false;
    }

    const char *mesh_error = NULL;
    SourceSpan error_span = program->result->span;
    bool generated = mesh_plan_render(&plan, out_mesh, &error_span, &mesh_error);
    mesh_plan_destroy(&plan);
    if (!generated) {
        out_error->span = error_span;
        out_error->message = mesh_error;
    }
    return generated;
}
