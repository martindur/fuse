#include "glsl_stdlib.h"

static const BuiltinSignature BUILTINS[] = {
    {"cross", VALUE_TYPE_VEC3, 2, {VALUE_TYPE_VEC3, VALUE_TYPE_VEC3}},
    {"vertex", VALUE_TYPE_VERTEX, 2, {VALUE_TYPE_VEC3, VALUE_TYPE_VEC2}},
    {"vertex", VALUE_TYPE_VERTEX, 3, {VALUE_TYPE_VEC3, VALUE_TYPE_VEC2, VALUE_TYPE_VEC3}},
    {"show", VALUE_TYPE_VERTEX, 1, {VALUE_TYPE_VERTEX}},
    {"normalize", VALUE_TYPE_VEC3, 1, {VALUE_TYPE_VEC3}},
    {"dot", VALUE_TYPE_FLOAT, 2, {VALUE_TYPE_VEC3, VALUE_TYPE_VEC3}},
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
static const FieldSignature FIELDS[] = {
    {VALUE_TYPE_VERTEX, "position", VALUE_TYPE_VEC3, "Mesh-local output position."},
    {VALUE_TYPE_VERTEX, "uv", VALUE_TYPE_VEC2, "Output UV coordinates."},
    {VALUE_TYPE_VERTEX, "normal", VALUE_TYPE_VEC3, "Authored local-space normal, or zero when omitted."},
};
static const GlobalSignature GLOBALS[] = {
    {"position", VALUE_TYPE_VEC3}, {"uv", VALUE_TYPE_VEC2}, {"time", VALUE_TYPE_FLOAT},
};
static const SymbolDocumentation DOCUMENTATION[] = {
    {"cross", "Returns the right-handed cross product of two vec3 vectors; cross(Z, X) points toward +Y for a flat XZ grid."},
    {"vertex", "Creates a vertex result from local position, UV, and an optional authored local normal. Omit the normal for triangle-normal shading."},
    {"normalize", "Returns a unit vector with the direction of a nonzero vec3."},
    {"dot", "Returns the dot product of two vec3 vectors."},
    {"position", "Original mesh position in local coordinates; Y is up for grids."},
    {"uv", "Original mesh UV coordinates. Pass them to vertex() or supply transformed UVs."},
    {"time", "Elapsed animation time in seconds."},
    {"show", "Previews this vertex value or intermediate vec3 position; the last marker wins. Export returns the value unchanged."},
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
    "GraficVertex", "graficResult", "graficContext", "graficAuthoredNormal", "graficHasNormal", "a_position", "a_uv", "graficUv", "graficPosition", "u_model", "u_view", "u_projection", "u_time", "gl_Position",
};
static const LanguageEnvironment ENVIRONMENT = {
    .fields = FIELDS, .field_count = sizeof(FIELDS) / sizeof(FIELDS[0]),
    .builtins = BUILTINS, .builtin_count = sizeof(BUILTINS) / sizeof(BUILTINS[0]),
    .globals = GLOBALS, .global_count = sizeof(GLOBALS) / sizeof(GLOBALS[0]),
    .documentation = DOCUMENTATION, .documentation_count = sizeof(DOCUMENTATION) / sizeof(DOCUMENTATION[0]),
    .reserved_names = RESERVED, .reserved_count = sizeof(RESERVED) / sizeof(RESERVED[0]),
};
const GraficDialect *gl_vertex_dialect(void)
{
    static const GraficDialect DIALECT = {
        .language_id = "graficvert", .file_extension = ".gvert", .environment = &ENVIRONMENT,
        .required_result_type = VALUE_TYPE_VERTEX, .preview_requires_result_type = true,
        .preview_result_type = VALUE_TYPE_VEC3,
    };
    return &DIALECT;
}
bool glsl_vertex_stdlib_emit(FILE *output)
{
    return fputs("GraficVertex vertex(vec3 p, vec2 uv) { return GraficVertex(p, uv, vec3(0.0), false); }\n"
        "GraficVertex vertex(vec3 p, vec2 uv, vec3 n) { return GraficVertex(p, uv, n, true); }\n"
        "vec3 add(vec3 a, vec3 b) { return a + b; }\n"
        "vec3 multiply(vec3 a, float b) { return a * b; }\n", output) != EOF;
}
