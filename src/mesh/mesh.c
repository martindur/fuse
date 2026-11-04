#define _XOPEN_SOURCE 700

#include "mesh.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static const char *const ERROR_INVALID_SPEC =
    "invalid disk specification";
static const char *const ERROR_TOO_LARGE =
    "disk specification is too large";
static const char *const ERROR_OUT_OF_MEMORY =
    "out of memory";

void graficmesh_mesh_destroy(GraficMesh *mesh)
{
    if (mesh == NULL) {
        return;
    }

    free(mesh->positions);
    free(mesh->uvs);
    free(mesh->indices);
    *mesh = (GraficMesh){0};
}

bool graficmesh_mesh_from_disk(
    const GraficMeshDiskSpec *spec,
    GraficMesh *out_mesh,
    const char **out_error
)
{
    if (out_error != NULL) {
        *out_error = NULL;
    }

    if (out_mesh == NULL || spec == NULL) {
        if (out_error != NULL) {
            *out_error = ERROR_INVALID_SPEC;
        }
        return false;
    }

    *out_mesh = (GraficMesh){0};

    if (!isfinite(spec->radius)
        || spec->radius <= 0.0f
        || spec->segments < GRAFIC_MESH_MIN_SEGMENTS
        || spec->segments > GRAFIC_MESH_MAX_SEGMENTS
        || spec->rings < GRAFIC_MESH_MIN_RINGS
        || spec->rings > GRAFIC_MESH_MAX_RINGS) {
        if (out_error != NULL) {
            *out_error = ERROR_INVALID_SPEC;
        }
        return false;
    }

    if (spec->segments > SIZE_MAX / spec->rings
        || spec->segments * spec->rings > SIZE_MAX - 1) {
        if (out_error != NULL) {
            *out_error = ERROR_TOO_LARGE;
        }
        return false;
    }

    size_t ring_vertex_count = spec->segments * spec->rings;
    size_t vertex_count = ring_vertex_count + 1;
    size_t index_count = spec->segments * 3;

    if (spec->rings > 1) {
        size_t additional_rings = spec->rings - 1;

        if (additional_rings > SIZE_MAX / spec->segments
            || additional_rings * spec->segments
                > (SIZE_MAX - index_count) / 6) {
            if (out_error != NULL) {
                *out_error = ERROR_TOO_LARGE;
            }
            return false;
        }

        index_count += additional_rings * spec->segments * 6;
    }

    if (vertex_count > UINT32_MAX
        || vertex_count > SIZE_MAX / (3 * sizeof(float))
        || vertex_count > SIZE_MAX / (2 * sizeof(float))
        || index_count > SIZE_MAX / sizeof(uint32_t)) {
        if (out_error != NULL) {
            *out_error = ERROR_TOO_LARGE;
        }
        return false;
    }

    float *positions = malloc(
        vertex_count * 3 * sizeof(*positions)
    );
    float *uvs = malloc(vertex_count * 2 * sizeof(*uvs));
    uint32_t *indices = malloc(index_count * sizeof(*indices));

    if (positions == NULL || uvs == NULL || indices == NULL) {
        free(positions);
        free(uvs);
        free(indices);

        if (out_error != NULL) {
            *out_error = ERROR_OUT_OF_MEMORY;
        }
        return false;
    }

    positions[0] = 0.0f;
    positions[1] = 0.0f;
    positions[2] = 0.0f;
    uvs[0] = spec->uv_mapping == GRAFIC_MESH_UV_RADIAL ? 0.0f : 0.5f;
    uvs[1] = spec->uv_mapping == GRAFIC_MESH_UV_RADIAL ? 0.0f : 0.5f;

    const float two_pi = 6.28318530717958647692f;

    for (size_t ring = 1; ring <= spec->rings; ring += 1) {
        float fraction = (float)ring / (float)spec->rings;
        float ring_radius = spec->radius * fraction;

        for (size_t segment = 0;
             segment < spec->segments;
             segment += 1) {
            size_t vertex = 1
                + (ring - 1) * spec->segments
                + segment;
            float angle = two_pi
                * (float)segment
                / (float)spec->segments;
            float x = cosf(angle) * ring_radius;
            float y = sinf(angle) * ring_radius;

            positions[vertex * 3] = x;
            positions[vertex * 3 + 1] = y;
            positions[vertex * 3 + 2] = 0.0f;
            if (spec->uv_mapping == GRAFIC_MESH_UV_RADIAL) {
                /* U is radial and V wraps around the circumference. */
                uvs[vertex * 2] = fraction;
                uvs[vertex * 2 + 1] = (float)segment
                    / (float)spec->segments;
            } else {
                uvs[vertex * 2] = 0.5f
                    + 0.5f * cosf(angle) * fraction;
                uvs[vertex * 2 + 1] = 0.5f
                    + 0.5f * sinf(angle) * fraction;
            }
        }
    }

    size_t index = 0;

    for (size_t segment = 0;
         segment < spec->segments;
         segment += 1) {
        size_t next = (segment + 1) % spec->segments;
        indices[index++] = 0;
        indices[index++] = (uint32_t)(1 + segment);
        indices[index++] = (uint32_t)(1 + next);
    }

    for (size_t ring = 1; ring < spec->rings; ring += 1) {
        size_t previous_start = 1 + (ring - 1) * spec->segments;
        size_t current_start = 1 + ring * spec->segments;

        for (size_t segment = 0;
             segment < spec->segments;
             segment += 1) {
            size_t next = (segment + 1) % spec->segments;
            uint32_t previous = (uint32_t)(previous_start + segment);
            uint32_t previous_next =
                (uint32_t)(previous_start + next);
            uint32_t current = (uint32_t)(current_start + segment);
            uint32_t current_next =
                (uint32_t)(current_start + next);

            indices[index++] = previous;
            indices[index++] = previous_next;
            indices[index++] = current_next;
            indices[index++] = previous;
            indices[index++] = current_next;
            indices[index++] = current;
        }
    }

    *out_mesh = (GraficMesh){
        .positions = positions,
        .uvs = uvs,
        .indices = indices,
        .vertex_count = vertex_count,
        .index_count = index_count,
    };
    return true;
}

bool graficmesh_mesh_write_obj(
    FILE *output,
    const GraficMesh *mesh
)
{
    if (output == NULL
        || mesh == NULL
        || mesh->positions == NULL
        || mesh->uvs == NULL
        || mesh->indices == NULL
        || mesh->vertex_count == 0
        || mesh->index_count == 0
        || mesh->index_count % 3 != 0
        || mesh->vertex_count > UINT32_MAX) {
        return false;
    }

    if (fputs("# generated by grafic mesh\n", output) == EOF) {
        return false;
    }

    for (size_t vertex = 0;
         vertex < mesh->vertex_count;
         vertex += 1) {
        if (fprintf(
                output,
                "v %.9g %.9g %.9g\n",
                mesh->positions[vertex * 3],
                mesh->positions[vertex * 3 + 1],
                mesh->positions[vertex * 3 + 2]
            ) < 0) {
            return false;
        }
    }

    for (size_t vertex = 0;
         vertex < mesh->vertex_count;
         vertex += 1) {
        if (fprintf(
                output,
                "vt %.9g %.9g\n",
                mesh->uvs[vertex * 2],
                mesh->uvs[vertex * 2 + 1]
            ) < 0) {
            return false;
        }
    }

    for (size_t index = 0;
         index < mesh->index_count;
         index += 3) {
        uint32_t first = mesh->indices[index];
        uint32_t second = mesh->indices[index + 1];
        uint32_t third = mesh->indices[index + 2];

        if (first >= mesh->vertex_count
            || second >= mesh->vertex_count
            || third >= mesh->vertex_count
            || fprintf(
                output,
                "f %u/%u %u/%u %u/%u\n",
                first + 1,
                first + 1,
                second + 1,
                second + 1,
                third + 1,
                third + 1
            ) < 0) {
            return false;
        }
    }

    return fflush(output) == 0;
}

bool graficmesh_mesh_from_grid(const GraficMeshGridSpec *spec, GraficMesh *out_mesh,
    const char **out_error)
{
    if (out_error != NULL) *out_error = NULL;
    if (out_mesh == NULL || spec == NULL || !isfinite(spec->width) || !isfinite(spec->depth)
        || spec->width <= 0 || spec->depth <= 0
        || spec->subdivisions_x < 1 || spec->subdivisions_x > 4096
        || spec->subdivisions_z < 1 || spec->subdivisions_z > 4096) {
        if (out_error != NULL) *out_error = "invalid grid specification";
        return false;
    }
    size_t columns = spec->subdivisions_x + 1;
    size_t rows = spec->subdivisions_z + 1;
    size_t vertices = columns * rows;
    size_t indices = spec->subdivisions_x * spec->subdivisions_z * 6;
    if (vertices > UINT32_MAX || vertices > SIZE_MAX / (3 * sizeof(float))
        || indices > SIZE_MAX / sizeof(uint32_t)) {
        if (out_error != NULL) *out_error = "grid specification is too large";
        return false;
    }
    GraficMesh mesh = {.vertex_count = vertices, .index_count = indices};
    mesh.positions = malloc(vertices * 3 * sizeof(float));
    mesh.uvs = malloc(vertices * 2 * sizeof(float));
    mesh.indices = malloc(indices * sizeof(uint32_t));
    if (mesh.positions == NULL || mesh.uvs == NULL || mesh.indices == NULL) {
        graficmesh_mesh_destroy(&mesh);
        if (out_error != NULL) *out_error = ERROR_OUT_OF_MEMORY;
        return false;
    }
    for (size_t z = 0; z < rows; z += 1) {
        for (size_t x = 0; x < columns; x += 1) {
            size_t index = z * columns + x;
            float u = (float)x / (float)spec->subdivisions_x;
            float v = (float)z / (float)spec->subdivisions_z;
            mesh.positions[index * 3] = (u - 0.5f) * spec->width;
            mesh.positions[index * 3 + 1] = 0;
            mesh.positions[index * 3 + 2] = (v - 0.5f) * spec->depth;
            mesh.uvs[index * 2] = u;
            mesh.uvs[index * 2 + 1] = v;
        }
    }
    size_t next = 0;
    for (size_t z = 0; z < spec->subdivisions_z; z += 1) {
        for (size_t x = 0; x < spec->subdivisions_x; x += 1) {
            uint32_t a = (uint32_t)(z * columns + x);
            uint32_t b = a + 1;
            uint32_t c = a + (uint32_t)columns;
            uint32_t d = c + 1;
            /* Counterclockwise when viewed from +Y. */
            mesh.indices[next++] = a; mesh.indices[next++] = c; mesh.indices[next++] = b;
            mesh.indices[next++] = b; mesh.indices[next++] = c; mesh.indices[next++] = d;
        }
    }
    *out_mesh = mesh;
    return true;
}
