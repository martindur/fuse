#include "recipe.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mesh_plan_destroy(MeshPlan *plan)
{
    if (plan == NULL) return;
    while (plan->owned != NULL) {
        MeshRecipe *next = plan->owned->next_owned;
        free(plan->owned);
        plan->owned = next;
    }
    *plan = (MeshPlan){0};
}

static bool rotate_mesh(GraficMesh *mesh, const float angles[3], const char **error)
{
    double sx = sin(angles[0]), cx = cos(angles[0]);
    double sy = sin(angles[1]), cy = cos(angles[1]);
    double sz = sin(angles[2]), cz = cos(angles[2]);
    for (size_t index = 0; index < mesh->vertex_count; index += 1) {
        float *p = &mesh->positions[index * 3];
        double x = p[0], y = p[1] * cx - p[2] * sx, z = p[1] * sx + p[2] * cx;
        double next_x = x * cy + z * sy;
        z = -x * sy + z * cy;
        x = next_x;
        p[0] = (float)(x * cz - y * sz);
        p[1] = (float)(x * sz + y * cz);
        p[2] = (float)z;
        if (!isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2])) {
            *error = "rotation produced nonfinite positions";
            return false;
        }
    }
    return true;
}

typedef struct {
    GraficMesh mesh;
    bool built;
} MeshCache;

static bool copy_mesh(const GraficMesh *source, GraficMesh *out, const char **error)
{
    GraficMesh copy = {.vertex_count = source->vertex_count, .index_count = source->index_count};
    copy.positions = malloc(copy.vertex_count * 3 * sizeof(float));
    copy.uvs = malloc(copy.vertex_count * 2 * sizeof(float));
    copy.indices = malloc(copy.index_count * sizeof(uint32_t));
    if (copy.positions == NULL || copy.uvs == NULL || copy.indices == NULL) {
        graficmesh_mesh_destroy(&copy);
        *error = "out of memory";
        return false;
    }
    memcpy(copy.positions, source->positions, copy.vertex_count * 3 * sizeof(float));
    memcpy(copy.uvs, source->uvs, copy.vertex_count * 2 * sizeof(float));
    memcpy(copy.indices, source->indices, copy.index_count * sizeof(uint32_t));
    *out = copy;
    return true;
}

static bool render_recipe(const MeshRecipe *recipe, size_t depth, MeshCache *cache,
    SourceSpan *span, const char **error)
{
    if (recipe == NULL || depth > 256) {
        *error = "invalid or excessively nested mesh recipe";
        return false;
    }
    MeshCache *entry = &cache[recipe->id];
    if (entry->built) return true;
    GraficMesh *mesh = &entry->mesh;
    *span = recipe->span;
    bool generated = false;
    switch (recipe->kind) {
    case MESH_RECIPE_PRIMITIVE:
        switch (recipe->primitive.kind) {
        case GRAFIC_MESH_DISK:
            generated = graficmesh_mesh_from_disk(&recipe->primitive.disk, mesh, error);
            break;
        case GRAFIC_MESH_GRID:
            generated = graficmesh_mesh_from_grid(&recipe->primitive.grid, mesh, error);
            break;
        case GRAFIC_MESH_STRIP:
            generated = graficmesh_mesh_from_strip(&recipe->primitive.strip, mesh, error);
            break;
        }
        break;
    case MESH_RECIPE_ROTATE:
        if (!render_recipe(recipe->rotate.source, depth + 1, cache, span, error)) return false;
        *span = recipe->span;
        generated = copy_mesh(&cache[recipe->rotate.source->id].mesh, mesh, error)
            && rotate_mesh(mesh, recipe->rotate.angles, error);
        break;
    case MESH_RECIPE_SCATTER:
        if (!render_recipe(recipe->scatter.source, depth + 1, cache, span, error)
            || !render_recipe(recipe->scatter.surface, depth + 1, cache, span, error)) return false;
        *span = recipe->span;
        generated = graficmesh_mesh_scatter(&cache[recipe->scatter.source->id].mesh,
            &cache[recipe->scatter.surface->id].mesh, &recipe->scatter.settings, mesh, error);
        break;
    }
    entry->built = generated;
    return generated;
}

bool mesh_plan_render(const MeshPlan *plan, GraficMesh *out_mesh,
    SourceSpan *out_span, const char **out_error)
{
    *out_mesh = (GraficMesh){0};
    *out_error = NULL;
    if (plan->root == NULL || plan->count == 0 || plan->count > SIZE_MAX / sizeof(MeshCache)) {
        *out_error = "invalid mesh plan";
        return false;
    }
    MeshCache *cache = calloc(plan->count, sizeof(*cache));
    if (cache == NULL) { *out_error = "out of memory"; return false; }
    bool generated = render_recipe(plan->root, 0, cache, out_span, out_error);
    if (generated) {
        *out_mesh = cache[plan->root->id].mesh;
        cache[plan->root->id].mesh = (GraficMesh){0};
    }
    for (size_t index = 0; index < plan->count; index += 1) graficmesh_mesh_destroy(&cache[index].mesh);
    free(cache);
    return generated;
}
