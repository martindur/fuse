#include "builtins.h"

static const BuiltinSignature BUILTINS[] = {
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
    {"grid", "Creates a centered XZ grid with Y up, unit-square UVs, size 2 by 2, and 64 subdivisions per axis."},
    {"size", "Sets grid width along X and depth along Z; both must be positive."},
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
        "Sets the number of angular subdivisions in a disk.",
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
