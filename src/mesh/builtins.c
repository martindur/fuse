#include "builtins.h"

static const BuiltinSignature BUILTINS[] = {
    {"rotate", VALUE_TYPE_MESH, 4, {VALUE_TYPE_MESH, VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"scatter", VALUE_TYPE_MESH, 3, {VALUE_TYPE_MESH, VALUE_TYPE_MESH, VALUE_TYPE_FLOAT}},
    {"seed", VALUE_TYPE_MESH, 2, {VALUE_TYPE_MESH, VALUE_TYPE_FLOAT}},
    {"scale_variation", VALUE_TYPE_MESH, 3, {VALUE_TYPE_MESH, VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"strip", VALUE_TYPE_MESH, 0, {0}},
    {"taper", VALUE_TYPE_MESH, 2, {VALUE_TYPE_MESH, VALUE_TYPE_FLOAT}},
    {"grid", VALUE_TYPE_MESH, 0, {0}},
    {"size", VALUE_TYPE_MESH, 3, {VALUE_TYPE_MESH, VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"subdivisions", VALUE_TYPE_MESH, 3, {VALUE_TYPE_MESH, VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"show", VALUE_TYPE_MESH, 1, {VALUE_TYPE_MESH}},
    {
        "disk",
        VALUE_TYPE_MESH,
        0,
        {0},
    },
    {
        "radius",
        VALUE_TYPE_MESH,
        2,
        {VALUE_TYPE_MESH, VALUE_TYPE_FLOAT},
    },
    {
        "segments",
        VALUE_TYPE_MESH,
        2,
        {VALUE_TYPE_MESH, VALUE_TYPE_FLOAT},
    },
    {
        "rings",
        VALUE_TYPE_MESH,
        2,
        {VALUE_TYPE_MESH, VALUE_TYPE_FLOAT},
    },
    {
        "uv_radial",
        VALUE_TYPE_MESH,
        1,
        {VALUE_TYPE_MESH},
    },
};

static const SymbolDocumentation DOCUMENTATION[] = {
    {"rotate", "Rotates a mesh about its origin, applying X, then Y, then Z angles in radians. Configure primitive dimensions before rotating."},
    {"scatter", "Bakes upright copies with uniformly random Y rotation onto a target mesh, selecting triangles by surface area. The source origin lands on the surface; local UVs are preserved. Count ranges from 1 to 1000000."},
    {"seed", "Sets a scatter recipe's deterministic seed (integer 0-16777215; default 0). Does not modify aliased meshes."},
    {"scale_variation", "Sets positive minimum and maximum uniform scale for scatter copies (default 1, 1). Configure before rotating the complete patch."},
    {"strip", "Creates an upright XY strip centered across X and rooted at Y=0, facing +Z. Defaults to width 1, height 1, four lengthwise segments, and no taper. UV.y runs from root to tip."},
    {"taper", "Narrows a strip linearly toward its tip: 0 keeps constant width, 1 makes a point. Values must be between 0 and 1."},
    {"grid", "Creates a centered XZ grid with Y up, unit-square UVs, size 2 by 2, and 64 subdivisions per axis."},
    {"size", "Sets grid width/depth or strip width/height; dimensions must be positive and finite."},
    {"subdivisions", "Sets grid cell counts along X and Z, each from 1 through 4096."},
    {"show", "Selects this intermediate mesh in grafic view. The last show() in each file wins; normal compilation returns the mesh unchanged."},
    {
        "disk",
        "Creates a filled disk mesh with centered planar UVs.",
    },
    {
        "radius",
        "Sets the disk radius.",
    },
    {
        "segments",
        "Sets disk angular subdivisions (3-4096) or strip lengthwise segments (1-4096).",
    },
    {
        "rings",
        "Sets the number of concentric subdivisions in a disk.",
    },
    {
        "uv_radial",
        "Maps disk UVs in polar form: U runs from the center to the rim and V wraps around the circumference.",
    },
};

static const LanguageEnvironment ENVIRONMENT = {
    .builtins = BUILTINS,
    .builtin_count = sizeof(BUILTINS) / sizeof(BUILTINS[0]),
    .globals = NULL,
    .global_count = 0,
    .documentation = DOCUMENTATION,
    .documentation_count = sizeof(DOCUMENTATION)
        / sizeof(DOCUMENTATION[0]),
    .reserved_names = NULL,
    .reserved_count = 0,
};

static const GraficDialect DIALECT = {
    .language_id = "graficmesh",
    .file_extension = ".gmesh",
    .environment = &ENVIRONMENT,
    .required_result_type = VALUE_TYPE_MESH,
};

const GraficDialect *graficmesh_dialect(void)
{
    return &DIALECT;
}
