#ifndef GRAFIC_MESH_MESH_H
#define GRAFIC_MESH_MESH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
    GRAFIC_MESH_MIN_SEGMENTS = 3,
    GRAFIC_MESH_MAX_SEGMENTS = 4096,
    GRAFIC_MESH_MIN_RINGS = 1,
    GRAFIC_MESH_MAX_RINGS = 4096,
};

typedef enum {
    GRAFIC_MESH_UV_PLANAR,
    GRAFIC_MESH_UV_RADIAL,
} GraficMeshUvMapping;

typedef struct {
    float radius;
    size_t segments;
    size_t rings;
    GraficMeshUvMapping uv_mapping;
} GraficMeshDiskSpec;

typedef struct {
    float width;
    float height;
    size_t segments;
    float taper;
} GraficMeshStripSpec;

typedef struct {
    float width;
    float depth;
    size_t subdivisions_x;
    size_t subdivisions_z;
} GraficMeshGridSpec;

typedef enum { GRAFIC_MESH_DISK, GRAFIC_MESH_GRID, GRAFIC_MESH_STRIP } GraficMeshKind;

typedef struct {
    GraficMeshKind kind;
    union {
        GraficMeshDiskSpec disk;
        GraficMeshGridSpec grid;
        GraficMeshStripSpec strip;
    };
} GraficMeshSpec;

typedef struct {
    float *positions;
    float *uvs;
    uint32_t *indices;
    size_t vertex_count;
    size_t index_count;
} GraficMesh;

typedef struct {
    size_t count;
    uint32_t seed;
    float minimum_scale;
    float maximum_scale;
} GraficMeshScatterSpec;

/* Area-weighted surface samples place upright copies, preserving local UVs. */
bool graficmesh_mesh_scatter(const GraficMesh *source, const GraficMesh *surface,
    const GraficMeshScatterSpec *spec, GraficMesh *out_mesh, const char **out_error);

void graficmesh_mesh_destroy(GraficMesh *mesh);

bool graficmesh_mesh_from_disk(
    const GraficMeshDiskSpec *spec,
    GraficMesh *out_mesh,
    const char **out_error
);

bool graficmesh_mesh_from_grid(
    const GraficMeshGridSpec *spec,
    GraficMesh *out_mesh,
    const char **out_error
);

bool graficmesh_mesh_from_strip(
    const GraficMeshStripSpec *spec,
    GraficMesh *out_mesh,
    const char **out_error
);

bool graficmesh_mesh_write_obj(
    FILE *output,
    const GraficMesh *mesh
);

#endif
