#ifndef GRAFIC_MESH_RECIPE_H
#define GRAFIC_MESH_RECIPE_H

#include "mesh.h"
#include "grafic/ast.h"

typedef enum { MESH_RECIPE_PRIMITIVE, MESH_RECIPE_ROTATE, MESH_RECIPE_SCATTER } MeshRecipeKind;
typedef struct MeshRecipe MeshRecipe;
struct MeshRecipe {
    MeshRecipeKind kind;
    size_t id;
    SourceSpan span;
    MeshRecipe *next_owned;
    union {
        GraficMeshSpec primitive;
        struct { const MeshRecipe *source; float angles[3]; } rotate;
        struct {
            const MeshRecipe *source;
            const MeshRecipe *surface;
            GraficMeshScatterSpec settings;
        } scatter;
    };
};

typedef struct {
    const MeshRecipe *root;
    MeshRecipe *owned;
    size_t count;
} MeshPlan;

/* The plan owns every node once, including nodes shared by aliases. */
void mesh_plan_destroy(MeshPlan *plan);
bool mesh_plan_render(const MeshPlan *plan, GraficMesh *out_mesh,
    SourceSpan *out_span, const char **out_error);

#endif
