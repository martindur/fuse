#include "builtins.h"

static const BuiltinSignature BUILTINS[] = {
    {
        "center",
        VALUE_TYPE_VEC2,
        1,
        {VALUE_TYPE_VEC2},
    },
    {
        "aspect",
        VALUE_TYPE_VEC2,
        1,
        {VALUE_TYPE_VEC2},
    },
    {
        "add",
        VALUE_TYPE_FLOAT,
        2,
        {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT},
    },
    {
        "add",
        VALUE_TYPE_VEC2,
        2,
        {VALUE_TYPE_VEC2, VALUE_TYPE_VEC2},
    },
    {
        "multiply",
        VALUE_TYPE_FLOAT,
        2,
        {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT},
    },
    {
        "multiply",
        VALUE_TYPE_VEC2,
        2,
        {VALUE_TYPE_VEC2, VALUE_TYPE_FLOAT},
    },
    {
        "sin",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_FLOAT},
    },
    {
        "cos",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_FLOAT},
    },
    {
        "abs",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_FLOAT},
    },
    {
        "floor",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_FLOAT},
    },
    {
        "fract",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_FLOAT},
    },
    {
        "length",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_VEC2},
    },
    {
        "dot",
        VALUE_TYPE_FLOAT,
        2,
        {VALUE_TYPE_VEC2, VALUE_TYPE_VEC2},
    },
    {
        "smoothstep",
        VALUE_TYPE_FLOAT,
        3,
        {
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
        },
    },
    {
        "noise",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_VEC2},
    },
    {
        "noise",
        VALUE_TYPE_FLOAT,
        2,
        {VALUE_TYPE_VEC2, VALUE_TYPE_VEC2},
    },
    {
        "vec2",
        VALUE_TYPE_VEC2,
        2,
        {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT},
    },
    {
        "vec3",
        VALUE_TYPE_VEC3,
        3,
        {
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
        },
    },
    {
        "vec4",
        VALUE_TYPE_VEC4,
        3,
        {
            VALUE_TYPE_VEC2,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
        },
    },
    {
        "vec4",
        VALUE_TYPE_VEC4,
        2,
        {VALUE_TYPE_VEC3, VALUE_TYPE_FLOAT},
    },
    {
        "vec4",
        VALUE_TYPE_VEC4,
        4,
        {
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
        },
    },
    {
        "rgb",
        VALUE_TYPE_VEC3,
        3,
        {
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
        },
    },
    {
        "colorize",
        VALUE_TYPE_VEC4,
        2,
        {VALUE_TYPE_FLOAT, VALUE_TYPE_VEC3},
    },
};

static const GlobalSignature GLOBALS[] = {
    {"uv", VALUE_TYPE_VEC2},
    {"resolution", VALUE_TYPE_VEC2},
};

static const SymbolDocumentation DOCUMENTATION[] = {
    {
        "center",
        "Moves UV coordinates so the center of the image is the origin.",
    },
    {
        "aspect",
        "Corrects horizontal coordinates for the output resolution's aspect ratio.",
    },
    {
        "add",
        "Adds two scalar values or two vectors component by component.",
    },
    {
        "multiply",
        "Multiplies a scalar by a scalar or a vector by a scalar.",
    },
    {
        "sin",
        "Returns the sine of a scalar value in radians.",
    },
    {
        "cos",
        "Returns the cosine of a scalar value in radians.",
    },
    {
        "abs",
        "Returns the absolute value of a scalar.",
    },
    {
        "floor",
        "Rounds a scalar down to the nearest integer.",
    },
    {
        "fract",
        "Returns the fractional part of a scalar.",
    },
    {
        "length",
        "Returns the length of a two-component vector.",
    },
    {
        "dot",
        "Returns the dot product of two vectors.",
    },
    {
        "smoothstep",
        "Interpolates smoothly from 0 to 1 between lower and upper bounds.",
    },
    {
        "noise",
        "Generates seeded 2D improved Perlin noise. Input coordinates control scale; the FuseTex seed controls variation. An optional period makes the noise tileable. Returns a value in the [0, 1] range.",
    },
    {
        "vec2",
        "Constructs a two-component vector.",
    },
    {
        "vec3",
        "Constructs a three-component vector.",
    },
    {
        "vec4",
        "Constructs a four-component vector.",
    },
    {
        "rgb",
        "Constructs an RGB color from red, green, and blue components.",
    },
    {
        "colorize",
        "Applies a color to a scalar mask and uses the mask as alpha.",
    },
    {
        "uv",
        "Normalized coordinates for the current texture pixel.",
    },
    {
        "resolution",
        "Width and height of the output in pixels.",
    },
};

static const LanguageEnvironment ENVIRONMENT = {
    .builtins = BUILTINS,
    .builtin_count = sizeof(BUILTINS) / sizeof(BUILTINS[0]),
    .globals = GLOBALS,
    .global_count = sizeof(GLOBALS) / sizeof(GLOBALS[0]),
    .documentation = DOCUMENTATION,
    .documentation_count = sizeof(DOCUMENTATION) / sizeof(DOCUMENTATION[0]),
    .reserved_names = NULL,
    .reserved_count = 0,
};

static const FuseDialect DIALECT = {
    .language_id = "fusetex",
    .environment = &ENVIRONMENT,
    .required_result_type = VALUE_TYPE_VEC4,
};

const FuseDialect *fusetex_dialect(void)
{
    return &DIALECT;
}
