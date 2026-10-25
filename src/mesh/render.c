#include "render.h"

#include "evaluate.h"

bool fusemesh_render(
    const Program *program,
    const char *source,
    FuseMesh *out_mesh,
    FuseMeshRenderError *out_error
)
{
    if (out_mesh == NULL || out_error == NULL) {
        return false;
    }

    *out_mesh = (FuseMesh){0};
    *out_error = (FuseMeshRenderError){0};

    FuseMeshDiskSpec spec;
    FuseMeshEvalError eval_error;

    if (!fusemesh_evaluate_program(
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

    if (!fusemesh_mesh_from_disk(
            &spec,
            out_mesh,
            &mesh_error
        )) {
        out_error->span = program->result->span;
        out_error->message = mesh_error;
        return false;
    }

    return true;
}
