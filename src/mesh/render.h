#ifndef FUSE_MESH_RENDER_H
#define FUSE_MESH_RENDER_H

#include "mesh.h"
#include "fuse/ast.h"

#include <stdbool.h>

typedef struct {
    SourceSpan span;
    const char *message;
} FuseMeshRenderError;

bool fusemesh_render(
    const Program *program,
    const char *source,
    FuseMesh *out_mesh,
    FuseMeshRenderError *out_error
);

#endif
