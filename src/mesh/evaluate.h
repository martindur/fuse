#ifndef GRAFIC_MESH_EVALUATE_H
#define GRAFIC_MESH_EVALUATE_H

#include "grafic/ast.h"
#include "grafic/value_type.h"
#include "mesh.h"

#include <stdbool.h>

typedef struct {
    ValueType type;
    float number;
    GraficMeshDiskSpec disk;
} GraficMeshRuntimeValue;

typedef struct {
    SourceSpan span;
    const char *message;
} GraficMeshEvalError;

bool graficmesh_evaluate_program(
    const Program *program,
    const char *source,
    GraficMeshDiskSpec *out_spec,
    GraficMeshEvalError *out_error
);

#endif
