#ifndef FUSE_MESH_MESH_H
#define FUSE_MESH_MESH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
    FUSE_MESH_MIN_SEGMENTS = 3,
    FUSE_MESH_MAX_SEGMENTS = 4096,
    FUSE_MESH_MIN_RINGS = 1,
    FUSE_MESH_MAX_RINGS = 4096,
};

typedef enum {
    FUSE_MESH_UV_PLANAR,
    FUSE_MESH_UV_RADIAL,
} FuseMeshUvMapping;

typedef struct {
    float radius;
    size_t segments;
    size_t rings;
    FuseMeshUvMapping uv_mapping;
} FuseMeshDiskSpec;

typedef struct {
    float *positions;
    float *uvs;
    uint32_t *indices;
    size_t vertex_count;
    size_t index_count;
} FuseMesh;

void fusemesh_mesh_destroy(FuseMesh *mesh);

bool fusemesh_mesh_from_disk(
    const FuseMeshDiskSpec *spec,
    FuseMesh *out_mesh,
    const char **out_error
);

bool fusemesh_mesh_write_obj(
    FILE *output,
    const FuseMesh *mesh
);

#endif
