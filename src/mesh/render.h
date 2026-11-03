#ifndef GRAFIC_MESH_RENDER_H
#define GRAFIC_MESH_RENDER_H

#include "mesh.h"
#include "grafic/ast.h"

#include <stdbool.h>

typedef struct {
    SourceSpan span;
    const char *message;
} GraficMeshRenderError;

bool graficmesh_render(
    const Program *program,
    const char *source,
    GraficMesh *out_mesh,
    GraficMeshRenderError *out_error
);

#endif
