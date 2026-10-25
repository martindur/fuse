#ifndef FUSE_MESH_EVALUATE_H
#define FUSE_MESH_EVALUATE_H

#include "fuse/ast.h"
#include "fuse/value_type.h"
#include "mesh.h"

#include <stdbool.h>

typedef struct {
    ValueType type;
    float number;
    FuseMeshDiskSpec disk;
} FuseMeshRuntimeValue;

typedef struct {
    SourceSpan span;
    const char *message;
} FuseMeshEvalError;

bool fusemesh_evaluate_program(
    const Program *program,
    const char *source,
    FuseMeshDiskSpec *out_spec,
    FuseMeshEvalError *out_error
);

#endif
