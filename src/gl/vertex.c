#include "glsl_stdlib.h"

static const BuiltinSignature BUILTINS[] = {
    {"show", VALUE_TYPE_VEC3, 1, {VALUE_TYPE_VEC3}},
    {"vec2", VALUE_TYPE_VEC2, 2, {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"vec3", VALUE_TYPE_VEC3, 3, {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"x", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC3}},
    {"y", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC3}},
    {"z", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC3}},
    {"x", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC2}},
    {"y", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC2}},
    {"sin", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_FLOAT}},
    {"cos", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_FLOAT}},
    {"add", VALUE_TYPE_FLOAT, 2, {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"add", VALUE_TYPE_VEC3, 2, {VALUE_TYPE_VEC3, VALUE_TYPE_VEC3}},
    {"multiply", VALUE_TYPE_FLOAT, 2, {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"multiply", VALUE_TYPE_VEC3, 2, {VALUE_TYPE_VEC3, VALUE_TYPE_FLOAT}},
    {"oscillate", VALUE_TYPE_FLOAT, 3, {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"cycle", VALUE_TYPE_FLOAT, 4, {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
};
static const GlobalSignature GLOBALS[] = {
    {"position", VALUE_TYPE_VEC3}, {"uv", VALUE_TYPE_VEC2}, {"time", VALUE_TYPE_FLOAT},
};
static const SymbolDocumentation DOCUMENTATION[] = {
    {"position", "Original mesh position in local coordinates; Y is up for grids."},
    {"uv", "Original mesh UV coordinates, passed unchanged to the fragment stage."},
    {"time", "Elapsed animation time in seconds."},
    {"show", "Previews this intermediate vec3 position; the last marker wins. Export returns the value unchanged."},
    {"vec2", "Constructs a two-component vector."},
    {"vec3", "Constructs a three-component position or vector."},
    {"x", "Returns the X component of a vector."},
    {"y", "Returns the Y component of a vector."},
    {"z", "Returns the Z component of a vector."},
    {"sin", "Returns the sine of an angle in radians."},
    {"cos", "Returns the cosine of an angle in radians."},
    {"add", "Adds two scalars or two vec3 vectors."},
    {"multiply", "Multiplies two scalars, or scales a vec3 by a scalar."},
    {"oscillate", "Maps the sine of a value between low and high."},
    {"cycle", "Repeats a linear ramp between low and high at the given speed."},
};
static const char *const RESERVED[] = {
    "a_position", "a_uv", "graficUv", "u_model", "u_view", "u_projection", "u_time", "gl_Position",
};
static const LanguageEnvironment ENVIRONMENT = {
    .builtins = BUILTINS, .builtin_count = sizeof(BUILTINS) / sizeof(BUILTINS[0]),
    .globals = GLOBALS, .global_count = sizeof(GLOBALS) / sizeof(GLOBALS[0]),
    .documentation = DOCUMENTATION, .documentation_count = sizeof(DOCUMENTATION) / sizeof(DOCUMENTATION[0]),
    .reserved_names = RESERVED, .reserved_count = sizeof(RESERVED) / sizeof(RESERVED[0]),
};
const GraficDialect *gl_vertex_dialect(void)
{
    static const GraficDialect DIALECT = {
        .language_id = "graficvert", .file_extension = ".gvert", .environment = &ENVIRONMENT,
        .required_result_type = VALUE_TYPE_VEC3, .preview_requires_result_type = true,
    };
    return &DIALECT;
}
bool glsl_vertex_stdlib_emit(FILE *output)
{
    return fputs("vec3 add(vec3 a, vec3 b) { return a + b; }\n"
        "vec3 multiply(vec3 a, float b) { return a * b; }\n", output) != EOF;
}
