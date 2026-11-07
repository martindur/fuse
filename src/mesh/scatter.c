#include "mesh.h"

#include <math.h>
#include <stdlib.h>

/* SplitMix64, with open-interval samples: selection cannot land beyond the CDF. */
static double random_unit(uint64_t *state)
{
    *state += UINT64_C(0x9e3779b97f4a7c15);
    uint64_t bits = *state;
    bits = (bits ^ (bits >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    bits = (bits ^ (bits >> 27)) * UINT64_C(0x94d049bb133111eb);
    bits ^= bits >> 31;
    return ((double)(bits >> 32) + 0.5) / 4294967296.0;
}

bool graficmesh_mesh_scatter(const GraficMesh *source, const GraficMesh *surface,
    const GraficMeshScatterSpec *spec, GraficMesh *out_mesh, const char **out_error)
{
    const char *error = "invalid scatter specification";
    GraficMesh mesh = {0};
    double *areas = NULL;
    if (out_error != NULL) *out_error = NULL;
    if (source == NULL || surface == NULL || spec == NULL || out_mesh == NULL
        || source->vertex_count == 0 || source->index_count == 0 || source->index_count % 3 != 0
        || surface->vertex_count == 0 || surface->index_count == 0 || surface->index_count % 3 != 0
        || source->positions == NULL || source->uvs == NULL || source->indices == NULL
        || surface->positions == NULL || surface->indices == NULL
        || spec->count < 1 || spec->count > 1000000
        || !isfinite(spec->minimum_scale) || !isfinite(spec->maximum_scale)
        || spec->minimum_scale <= 0 || spec->maximum_scale < spec->minimum_scale) goto fail;
    error = "scattered mesh is too large";
    if (source->vertex_count > UINT32_MAX / spec->count
        || source->vertex_count > SIZE_MAX / spec->count / (3 * sizeof(float))
        || source->index_count > SIZE_MAX / spec->count / sizeof(uint32_t)) goto fail;
    for (size_t index = 0; index < source->index_count; index += 1) {
        if (source->indices[index] >= source->vertex_count) {
            error = "invalid scatter source indices";
            goto fail;
        }
    }
    size_t triangles = surface->index_count / 3;
    if (triangles > SIZE_MAX / sizeof(double)) goto fail;
    areas = malloc(triangles * sizeof(double));
    if (areas == NULL) { error = "out of memory"; goto fail; }
    double total_area = 0;
    for (size_t triangle = 0; triangle < triangles; triangle += 1) {
        const float *points[3];
        for (size_t corner = 0; corner < 3; corner += 1) {
            uint32_t index = surface->indices[triangle * 3 + corner];
            if (index >= surface->vertex_count) { error = "invalid scatter surface indices"; goto fail; }
            points[corner] = &surface->positions[(size_t)index * 3];
        }
        double a[3], b[3];
        for (size_t axis = 0; axis < 3; axis += 1) {
            a[axis] = (double)points[1][axis] - points[0][axis];
            b[axis] = (double)points[2][axis] - points[0][axis];
        }
        double x = a[1]*b[2] - a[2]*b[1], y = a[2]*b[0] - a[0]*b[2], z = a[0]*b[1] - a[1]*b[0];
        double area = 0.5 * hypot(hypot(x, y), z);
        if (!isfinite(area)) { error = "nonfinite scatter surface"; goto fail; }
        total_area += area;
        areas[triangle] = total_area;
    }
    if (!(total_area > 0) || !isfinite(total_area)) { error = "scatter surface has no finite area"; goto fail; }
    mesh.vertex_count = source->vertex_count * spec->count;
    mesh.index_count = source->index_count * spec->count;
    mesh.positions = malloc(mesh.vertex_count * 3 * sizeof(float));
    mesh.uvs = malloc(mesh.vertex_count * 2 * sizeof(float));
    mesh.indices = malloc(mesh.index_count * sizeof(uint32_t));
    if (mesh.positions == NULL || mesh.uvs == NULL || mesh.indices == NULL) { error = "out of memory"; goto fail; }
    uint64_t random = spec->seed;
    for (size_t copy = 0; copy < spec->count; copy += 1) {
        double selected = random_unit(&random) * total_area;
        size_t low = 0, high = triangles - 1;
        while (low < high) {
            size_t middle = low + (high - low) / 2;
            if (areas[middle] <= selected) low = middle + 1;
            else high = middle;
        }
        double root = sqrt(random_unit(&random)), other = random_unit(&random);
        double weights[] = {1 - root, root * (1 - other), root * other};
        double origin[3] = {0};
        for (size_t corner = 0; corner < 3; corner += 1) {
            const float *point = &surface->positions[(size_t)surface->indices[low * 3 + corner] * 3];
            for (size_t axis = 0; axis < 3; axis += 1) origin[axis] += point[axis] * weights[corner];
        }
        double yaw = random_unit(&random) * 6.283185307179586;
        double scale = spec->minimum_scale + random_unit(&random) * ((double)spec->maximum_scale - spec->minimum_scale);
        double sine = sin(yaw), cosine = cos(yaw);
        size_t start = copy * source->vertex_count;
        for (size_t vertex = 0; vertex < source->vertex_count; vertex += 1) {
            const float *p = &source->positions[vertex * 3];
            float *out = &mesh.positions[(start + vertex) * 3];
            out[0] = (float)(origin[0] + scale * (p[0] * cosine + p[2] * sine));
            out[1] = (float)(origin[1] + scale * p[1]);
            out[2] = (float)(origin[2] + scale * (-p[0] * sine + p[2] * cosine));
            if (!isfinite(out[0]) || !isfinite(out[1]) || !isfinite(out[2])) {
                error = "scatter produced nonfinite positions";
                goto fail;
            }
            mesh.uvs[(start + vertex) * 2] = source->uvs[vertex * 2];
            mesh.uvs[(start + vertex) * 2 + 1] = source->uvs[vertex * 2 + 1];
            if (!isfinite(source->uvs[vertex * 2]) || !isfinite(source->uvs[vertex * 2 + 1])) {
                error = "nonfinite scatter source UVs";
                goto fail;
            }
        }
        for (size_t index = 0; index < source->index_count; index += 1) {
            mesh.indices[copy * source->index_count + index] = (uint32_t)start + source->indices[index];
        }
    }
    free(areas);
    *out_mesh = mesh;
    return true;
fail:
    free(areas);
    graficmesh_mesh_destroy(&mesh);
    if (out_error != NULL) *out_error = error;
    return false;
}
