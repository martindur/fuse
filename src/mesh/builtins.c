#include "builtins.h"

static const BuiltinSignature BUILTINS[] = {
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

static const FuseDialect DIALECT = {
    .language_id = "fusemesh",
    .file_extension = ".pmesh",
    .environment = &ENVIRONMENT,
    .required_result_type = VALUE_TYPE_MESH,
};

const FuseDialect *fusemesh_dialect(void)
{
    return &DIALECT;
}
