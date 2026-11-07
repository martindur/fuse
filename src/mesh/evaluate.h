#ifndef GRAFIC_MESH_EVALUATE_H
#define GRAFIC_MESH_EVALUATE_H

#include "grafic/ast.h"
#include "grafic/value_type.h"
#include "recipe.h"

#include <stdbool.h>

typedef struct {
    ValueType type;
    float number;
    MeshRecipe *mesh;
} GraficMeshRuntimeValue;

typedef struct {
    SourceSpan span;
    const char *message;
} GraficMeshEvalError;

bool graficmesh_evaluate_program(
    const Program *program,
    const char *source,
    MeshPlan *out_plan,
    GraficMeshEvalError *out_error
);

#endif
