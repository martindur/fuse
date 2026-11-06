#include "glsl_stdlib.h"

static const BuiltinSignature BUILTINS[] = {
    {"multiply", VALUE_TYPE_VEC2, 2, {VALUE_TYPE_VEC2, VALUE_TYPE_FLOAT}},
    {"reflect", VALUE_TYPE_VEC3, 2, {VALUE_TYPE_VEC3, VALUE_TYPE_VEC3}},
    {"pow", VALUE_TYPE_FLOAT, 2, {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"add", VALUE_TYPE_VEC3, 2, {VALUE_TYPE_VEC3, VALUE_TYPE_VEC3}},
    {"normalize", VALUE_TYPE_VEC3, 1, {VALUE_TYPE_VEC3}},
    {"dot", VALUE_TYPE_FLOAT, 2, {VALUE_TYPE_VEC3, VALUE_TYPE_VEC3}},
    {"multiply", VALUE_TYPE_VEC3, 2, {VALUE_TYPE_VEC3, VALUE_TYPE_FLOAT}},
    {"x", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC3}},
    {"y", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC3}},
    {"z", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC3}},
    {"x", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC2}},
    {"y", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC2}},
    {"mix", VALUE_TYPE_VEC3, 3, {VALUE_TYPE_VEC3, VALUE_TYPE_VEC3, VALUE_TYPE_FLOAT}},
    {"show", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_FLOAT}},
    {"show", VALUE_TYPE_VEC2, 1, {VALUE_TYPE_VEC2}},
    {"show", VALUE_TYPE_VEC3, 1, {VALUE_TYPE_VEC3}},
    {"show", VALUE_TYPE_VEC4, 1, {VALUE_TYPE_VEC4}},
    {
        "sample",
        VALUE_TYPE_VEC4,
        2,
        {VALUE_TYPE_VEC2, VALUE_TYPE_TEXTURE},
    },
    {
        "scroll",
        VALUE_TYPE_VEC2,
        3,
        {VALUE_TYPE_VEC2, VALUE_TYPE_VEC2, VALUE_TYPE_FLOAT},
    },
    {
        "repeat",
        VALUE_TYPE_VEC2,
        1,
        {VALUE_TYPE_VEC2},
    },
    {
        "mirror",
        VALUE_TYPE_VEC2,
        1,
        {VALUE_TYPE_VEC2},
    },
    {
        "mirror",
        VALUE_TYPE_VEC2,
        2,
        {VALUE_TYPE_VEC2, VALUE_TYPE_TEXTURE},
    },
    {
        "zoom",
        VALUE_TYPE_VEC2,
        2,
        {VALUE_TYPE_VEC2, VALUE_TYPE_FLOAT},
    },
    {
        "distort",
        VALUE_TYPE_VEC2,
        3,
        {VALUE_TYPE_VEC2, VALUE_TYPE_VEC4, VALUE_TYPE_FLOAT},
    },
    {
        "r",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_VEC4},
    },
    {
        "g",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_VEC4},
    },
    {
        "b",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_VEC4},
    },
    {
        "clamp",
        VALUE_TYPE_FLOAT,
        3,
        {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT},
    },
    {
        "a",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_VEC4},
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
        "rotate",
        VALUE_TYPE_VEC2,
        2,
        {VALUE_TYPE_VEC2, VALUE_TYPE_FLOAT},
    },
    {
        "oscillate",
        VALUE_TYPE_FLOAT,
        3,
        {
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
        },
    },
    {
        "cycle",
        VALUE_TYPE_FLOAT,
        4,
        {
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
        },
    },
    {
        "radial_wave",
        VALUE_TYPE_FLOAT,
        4,
        {
            VALUE_TYPE_VEC2,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
            VALUE_TYPE_FLOAT,
        },
    },
    {
        "circle",
        VALUE_TYPE_FLOAT,
        2,
        {VALUE_TYPE_VEC2, VALUE_TYPE_FLOAT},
    },
    {
        "box",
        VALUE_TYPE_FLOAT,
        2,
        {VALUE_TYPE_VEC2, VALUE_TYPE_VEC2},
    },
    {
        "sdf_union",
        VALUE_TYPE_FLOAT,
        2,
        {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT},
    },
    {
        "fill",
        VALUE_TYPE_FLOAT,
        1,
        {VALUE_TYPE_FLOAT},
    },
    {
        "stroke",
        VALUE_TYPE_FLOAT,
        2,
        {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT},
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
    {
        "add",
        VALUE_TYPE_FLOAT,
        2,
        {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT},
    },
    {
        "blend_add",
        VALUE_TYPE_VEC4,
        2,
        {VALUE_TYPE_VEC4, VALUE_TYPE_VEC4},
    },
    {
        "multiply",
        VALUE_TYPE_FLOAT,
        2,
        {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT},
    },
    {
        "multiply",
        VALUE_TYPE_VEC4,
        2,
        {VALUE_TYPE_VEC4, VALUE_TYPE_VEC4},
    },
};

static const FieldSignature FIELDS[] = {
    {VALUE_TYPE_VIEW, "direction", VALUE_TYPE_VEC3, "Normalized direction from this surface point toward the camera, in mesh-local space. Constant for orthographic views; varies with position for perspective views."},
    {VALUE_TYPE_SURFACE, "position", VALUE_TYPE_VEC3, "Displaced mesh-local position, interpolated across the triangle before camera transforms."},
    {VALUE_TYPE_SURFACE, "uv", VALUE_TYPE_VEC2, "Interpolated UV supplied by the vertex program."},
    {VALUE_TYPE_SURFACE, "normal", VALUE_TYPE_VEC3, "Normalized interpolated authored local normal, or winding-oriented triangle normal when none was supplied."},
    {VALUE_TYPE_SCREEN, "position", VALUE_TYPE_VEC2, "Framebuffer pixel-center coordinates, with bottom-left origin."},
    {VALUE_TYPE_SCREEN, "uv", VALUE_TYPE_VEC2, "Framebuffer pixel-center coordinates divided by resolution, independent of mesh UVs."},
};
static const GlobalSignature GLOBALS[] = {
    {"view", VALUE_TYPE_VIEW},
    {"surface", VALUE_TYPE_SURFACE}, {"screen", VALUE_TYPE_SCREEN},
    {"resolution", VALUE_TYPE_VEC2},
    {"time", VALUE_TYPE_FLOAT},
    {"tex0", VALUE_TYPE_TEXTURE},
    {"tex1", VALUE_TYPE_TEXTURE},
    {"tex2", VALUE_TYPE_TEXTURE},
    {"tex3", VALUE_TYPE_TEXTURE},
};

static const SymbolDocumentation DOCUMENTATION[] = {
    {"view", "Camera-provided fragment inputs. view.direction points from the surface toward the viewer, in surface-local coordinates."},
    {"reflect", "Reflects an incoming vec3 direction around a unit normal. For a reflection toward the environment, use view.direction.multiply(-1.0).reflect(surface.normal)."},
    {"pow", "Raises a nonnegative scalar base to an exponent; useful for Fresnel curves and highlights. Zero requires a positive exponent."},
    {"surface", "Interpolated surface.position, surface.uv, and surface.normal supplied by the preceding vertex stage."},
    {"screen", "Framebuffer screen.position in pixels and screen.uv normalized by resolution, with bottom-left origin."},
    {"normalize", "Returns a unit vector with the same direction as a nonzero vec3."},
    {"dot", "Returns the dot product of two vec3 vectors. For unit vectors, this is the cosine of their angle."},
    {"x", "Returns the X component of a vector."},
    {"y", "Returns the Y component of a vector."},
    {"z", "Returns the Z component of a vector."},
    {"mix", "Linearly interpolates between two RGB colors: amount 0 selects the first, and 1 the second. Clamp the amount for a bounded gradient."},
    {"show", "Selects this intermediate value in grafic view. The last show() in each file wins; normal compilation returns the value unchanged."},
    {
        "sample",
        "Samples a texture at the input UV coordinate.",
    },
    {
        "scroll",
        "Offsets a coordinate by velocity scaled by time.",
    },
    {
        "repeat",
        "Repeats coordinates across the unit interval.",
    },
    {
        "mirror",
        "Mirrors coordinates into the unit interval. The texture overload also clamps to texel centers.",
    },
    {
        "zoom",
        "Scales coordinates around the center point (0.5, 0.5).",
    },
    {
        "distort",
        "Offsets coordinates using the map's red and green channels remapped from [0, 1] to [-1, 1].",
    },
    {
        "r",
        "Extracts the red channel from a sampled color.",
    },
    {
        "g",
        "Extracts the green channel from a sampled color.",
    },
    {
        "b",
        "Extracts the blue channel from a sampled color.",
    },
    {
        "clamp",
        "Clamps a scalar value to an inclusive range.",
    },
    {
        "a",
        "Extracts the alpha channel from a sampled color.",
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
        "center",
        "Moves UV coordinates so the center of the image is the origin.",
    },
    {
        "aspect",
        "Corrects horizontal coordinates for the output resolution's aspect ratio.",
    },
    {
        "rotate",
        "Rotates a point by an angle in radians.",
    },
    {
        "oscillate",
        "Maps a sine wave into the inclusive range from low to high.",
    },
    {
        "cycle",
        "Maps a repeating phase into the range from low to high.",
    },
    {
        "radial_wave",
        "Generates a radial sine wave controlled by time, frequency, and speed.",
    },
    {
        "circle",
        "Returns the signed distance from a point to a circle centered at the origin.",
    },
    {
        "box",
        "Returns the signed distance from a point to an axis-aligned box centered at the origin.",
    },
    {
        "sdf_union",
        "Combines two signed-distance fields by taking their union.",
    },
    {
        "fill",
        "Converts a signed-distance field into an anti-aliased filled mask.",
    },
    {
        "stroke",
        "Converts a signed-distance field into an anti-aliased stroke mask.",
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
        "add",
        "Adds two scalars or two RGB vectors.",
    },
    {
        "blend_add",
        "Adds two colors and keeps the larger alpha value.",
    },
    {
        "multiply",
        "Multiplies two scalar values or two colors component by component, or scales a vec2 or vec3 by a scalar.",
    },
    {
        "resolution",
        "Width and height of the output in pixels.",
    },
    {
        "time",
        "Elapsed animation time.",
    },
    {
        "tex0",
        "First configured texture input.",
    },
    {
        "tex1",
        "Second configured texture input.",
    },
    {
        "tex2",
        "Third configured texture input.",
    },
    {
        "tex3",
        "Fourth configured texture input.",
    },
};

static const char *const RESERVED_NAMES[] = {
    "fragColor",
    "graficUv",
    "graficPosition",
    "grafic_surface_normal", "grafic_normalize", "GraficSurface", "GraficScreen",
    "graficAuthoredNormal", "graficHasNormal", "graficFaceNormal",
    "GraficView", "u_view_direction", "u_view_position", "u_view_is_orthographic",
};

static const LanguageEnvironment ENVIRONMENT = {
    .fields = FIELDS, .field_count = sizeof(FIELDS) / sizeof(FIELDS[0]),
    .builtins = BUILTINS,
    .builtin_count = sizeof(BUILTINS) / sizeof(BUILTINS[0]),
    .globals = GLOBALS,
    .global_count = sizeof(GLOBALS) / sizeof(GLOBALS[0]),
    .documentation = DOCUMENTATION,
    .documentation_count = sizeof(DOCUMENTATION) / sizeof(DOCUMENTATION[0]),
    .reserved_names = RESERVED_NAMES,
    .reserved_count = sizeof(RESERVED_NAMES) / sizeof(RESERVED_NAMES[0]),
};

static const GraficDialect DIALECT = {
    .language_id = "graficgl",
    .file_extension = ".gfrag",
    .environment = &ENVIRONMENT,
    .required_result_type = VALUE_TYPE_VEC4,
};

static const char GLSL_STDLIB[] =
    "vec3 grafic_normalize(vec3 value) { float magnitude = length(value); return magnitude > 0.0 ? value / magnitude : vec3(0.0); }\n"
    "vec3 grafic_surface_normal(vec3 point) {\n"
    "    vec3 direction = cross(dFdx(point), dFdy(point));\n"
    "    float magnitude = length(direction);\n"
    "    return magnitude > 0.0 ? direction * (gl_FrontFacing ? 1.0 : -1.0) / magnitude : vec3(0.0);\n"
    "}\n"
    "\n"
    "vec3 add(vec3 first, vec3 second) { return first + second; }\n"
    "vec2 multiply(vec2 point, float amount) { return point * amount; }\n"
    "vec3 multiply(vec3 color, float amount) { return color * amount; }\n"
    "\n"
    "vec2 center(vec2 uv) {\n"
    "    return uv - 0.5;\n"
    "}\n"
    "\n"
    "vec2 aspect(vec2 point) {\n"
    "    float ratio = resolution.x / resolution.y;\n"
    "    return vec2(point.x * ratio, point.y);\n"
    "}\n"
    "\n"
    "vec2 scroll(vec2 point, vec2 velocity, float time) {\n"
    "    return point + velocity * time;\n"
    "}\n"
    "\n"
    "vec2 repeat(vec2 point) {\n"
    "    return fract(point);\n"
    "}\n"
    "\n"
    "vec2 mirror(vec2 point) {\n"
    "    vec2 phase = fract(point * 0.5) * 2.0;\n"
    "    return min(1.0 - abs(phase - 1.0), 0.999999);\n"
    "}\n"
    "\n"
    "vec2 mirror(vec2 point, sampler2D source) {\n"
    "    vec2 phase = fract(point * 0.5) * 2.0;\n"
    "    vec2 coordinate = 1.0 - abs(phase - 1.0);\n"
    "    vec2 texel = 0.5 / vec2(textureSize(source, 0));\n"
    "    return clamp(coordinate, texel, 1.0 - texel);\n"
    "}\n"
    "\n"
    "vec2 zoom(vec2 point, float amount) {\n"
    "    return 0.5 + (point - 0.5) * amount;\n"
    "}\n"
    "\n"
    "vec2 distort(vec2 point, vec4 map, float amount) {\n"
    "    return point + (map.rg * 2.0 - 1.0) * amount;\n"
    "}\n"
    "\n"
    "vec2 rotate(vec2 p, float angle) {\n"
    "    float c = cos(angle);\n"
    "    float s = sin(angle);\n"
    "    return mat2(c, -s, s, c) * p;\n"
    "}\n"
    "\n"
    "float radial_wave(vec2 point, float time, float frequency, float speed) {\n"
    "    vec2 centered = point - 0.5;\n"
    "    centered.x *= resolution.x / resolution.y;\n"
    "    float radius = length(centered);\n"
    "    return 0.5 + 0.5 * sin(radius * frequency - time * speed);\n"
    "}\n"
    "\n"
    "float circle(vec2 point, float radius) {\n"
    "    return length(point) - radius;\n"
    "}\n"
    "\n"
    "float box(vec2 point, vec2 half_size) {\n"
    "    vec2 offset = abs(point) - half_size;\n"
    "    return length(max(offset, 0.0))\n"
    "        + min(max(offset.x, offset.y), 0.0);\n"
    "}\n"
    "\n"
    "float sdf_union(float first, float second) {\n"
    "    return min(first, second);\n"
    "}\n"
    "\n"
    "float fill(float signed_distance) {\n"
    "    return 1.0 - smoothstep(\n"
    "        0.0,\n"
    "        fwidth(signed_distance),\n"
    "        signed_distance\n"
    "    );\n"
    "}\n"
    "\n"
    "float stroke(float signed_distance, float thickness) {\n"
    "    float edge = abs(signed_distance) - thickness * 0.5;\n"
    "    return 1.0 - smoothstep(0.0, fwidth(edge), edge);\n"
    "}\n"
    "\n"
    "vec3 rgb(float red, float green, float blue) {\n"
    "    return vec3(red, green, blue);\n"
    "}\n"
    "\n"
    "vec4 colorize(float mask, vec3 color) {\n"
    "    return vec4(color * mask, mask);\n"
    "}\n"
    "\n"
    "vec4 blend_add(vec4 base, vec4 layer) {\n"
    "    return vec4(base.rgb + layer.rgb, max(base.a, layer.a));\n"
    "}\n"
    "\n"
    "vec4 multiply(vec4 base, vec4 layer) {\n"
    "    return base * layer;\n"
    "}\n";

bool glsl_stdlib_emit(FILE *output, ShaderStage stage)
{
    if (output == NULL) {
        return false;
    }

    return glsl_math_emit(output) && (stage == SHADER_STAGE_VERTEX
        ? glsl_vertex_stdlib_emit(output) : fputs(GLSL_STDLIB, output) != EOF);
}

const GraficDialect *gl_dialect(ShaderStage stage)
{
    return stage == SHADER_STAGE_VERTEX ? gl_vertex_dialect() : &DIALECT;
}

bool glsl_math_emit(FILE *output)
{
    static const char MATH[] =
    "float oscillate(float value, float low, float high) {\n"
    "    float phase = 0.5 + 0.5 * sin(value);\n"
    "    return mix(low, high, phase);\n"
    "}\n"
    "\n"
    "float cycle(float value, float speed, float low, float high) {\n"
    "    return mix(low, high, fract(value * speed));\n"
    "}\n"
    "\n"
    "float add(float first, float second) {\n"
    "    return first + second;\n"
    "}\n"
    "\n"
    "float multiply(float first, float second) {\n"
    "    return first * second;\n"
    "}\n"
    "\n";
    return fputs(MATH, output) != EOF;
}
