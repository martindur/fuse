#define _POSIX_C_SOURCE 200809L

#include "gl/glsl_emit.h"
#include "gl/glsl_stdlib.h"
#include "fuse/lower.h"
#include "fuse/parser.h"
#include "fuse/semantic.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char EXPECTED_SHADER[] =
    "#version 330 core\n"
    "\n"
    "in vec2 fuseUv;\n"
    "out vec4 fragColor;\n"
    "\n"
    "uniform float u_time;\n"
    "uniform vec2 u_resolution;\n"
    "uniform sampler2D tex0;\n"
    "uniform sampler2D tex1;\n"
    "uniform sampler2D tex2;\n"
    "uniform sampler2D tex3;\n"
    "\n"
    "#define time u_time\n"
    "#define resolution u_resolution\n"
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
    "float oscillate(float value, float low, float high) {\n"
    "    float phase = 0.5 + 0.5 * sin(value);\n"
    "    return mix(low, high, phase);\n"
    "}\n"
    "\n"
    "float cycle(float value, float speed, float low, float high) {\n"
    "    return mix(low, high, fract(value * speed));\n"
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
    "float add(float first, float second) {\n"
    "    return first + second;\n"
    "}\n"
    "\n"
    "vec4 blend_add(vec4 base, vec4 layer) {\n"
    "    return vec4(base.rgb + layer.rgb, max(base.a, layer.a));\n"
    "}\n"
    "\n"
    "float multiply(float first, float second) {\n"
    "    return first * second;\n"
    "}\n"
    "\n"
    "vec4 multiply(vec4 base, vec4 layer) {\n"
    "    return base * layer;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 uv = fuseUv;\n"
    "    vec2 p = aspect(center(uv));\n"
    "    fragColor = colorize(fill(sdf_union("
    "circle(p, 0.22), "
    "box(rotate(p, time), "
    "vec2(0.32, 0.06)))), rgb(0.2, 0.65, 1.0));\n"
    "}\n";

static bool emit_to_string(
    const Program *program,
    const SemanticInfo *semantic_info,
    const char *source,
    char **out_text,
    size_t *out_length
)
{
    FILE *stream = open_memstream(out_text, out_length);

    if (stream == NULL) {
        return false;
    }

    bool emitted = glsl_emit(
        stream,
        program,
        semantic_info,
        source
    );
    bool closed = fclose(stream) != EOF;

    return emitted && closed;
}

static bool test_complete_shader(void)
{
    const char *source =
        "let p = uv.center().aspect();\n"
        "\n"
        "p\n"
        ".circle(0.22)\n"
        ".sdf_union(\n"
        "    p\n"
        "    .rotate(time)\n"
        "    .box(vec2(0.32, 0.06))\n"
        ")\n"
        ".fill()\n"
        ".colorize(rgb(0.2, 0.65, 1.0))";

    Program *syntax = NULL;
    ParseError parse_error;

    if (!parse_program(
            source,
            strlen(source),
            &syntax,
            &parse_error
        )) {
        fprintf(
            stderr,
            "FAIL GLSL emission: parse error at %zu:%zu: %s\n",
            parse_error.span.offset,
            parse_error.span.length,
            parse_error.message
        );
        return false;
    }

    Program *lowered = NULL;
    LowerError lower_error;

    if (!lower_program(
            syntax,
            &lowered,
            &lower_error
        )) {
        fprintf(
            stderr,
            "FAIL GLSL emission: lowering error at %zu:%zu: %s\n",
            lower_error.span.offset,
            lower_error.span.length,
            lower_error.message
        );
        program_destroy(syntax);
        return false;
    }

    SemanticInfo semantic_info;
    SemanticError semantic_error;

    if (!semantic_analyze(
            lowered,
            source,
            fusegl_dialect()->environment,
            &semantic_info,
            &semantic_error
        )) {
        fprintf(
            stderr,
            "FAIL GLSL emission: semantic error at %zu:%zu: %s\n",
            semantic_error.span.offset,
            semantic_error.span.length,
            semantic_error.message
        );
        program_destroy(lowered);
        program_destroy(syntax);
        return false;
    }

    char *actual = NULL;
    size_t actual_length = 0;

    bool emitted = emit_to_string(
        lowered,
        &semantic_info,
        source,
        &actual,
        &actual_length
    );

    size_t expected_length = sizeof(EXPECTED_SHADER) - 1;
    bool matches = emitted
        && actual_length == expected_length
        && memcmp(
            actual,
            EXPECTED_SHADER,
            expected_length
        ) == 0;

    if (!matches) {
        fprintf(
            stderr,
            "FAIL complete GLSL snapshot\n"
            "expected:\n%s"
            "actual:\n%s",
            EXPECTED_SHADER,
            actual == NULL ? "(null)\n" : actual
        );
    }

    free(actual);
    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(syntax);

    if (matches) {
        printf("PASS complete GLSL snapshot\n");
    }

    return matches;
}

static bool test_texture_shader(void)
{
    const char *source =
        "let first = uv"
        " .scroll(vec2(0.04, 0.01), time)"
        " .repeat()"
        " .sample(tex0);\n"
        "let second = uv"
        " .scroll(vec2(-0.02, 0.03), time)"
        " .repeat()"
        " .sample(tex1);\n"
        "let warped = uv"
        " .zoom(time.cycle(0.15, 1.0, 0.1))"
        " .distort(first, 0.08)"
        " .mirror(tex2)"
        " .sample(tex2);\n"
        "let pulse = uv"
        " .radial_wave(time, 30.0, 4.0)"
        " .colorize(rgb(1.0, 1.0, 1.0));\n"
        "warped.multiply(second).multiply(pulse)";
    static const char *const EXPECTED_FRAGMENTS[] = {
        "uniform sampler2D tex0;\n",
        "uniform sampler2D tex1;\n",
        "vec4 first = texture(tex0, repeat(scroll(uv, vec2(0.04, 0.01), time)));\n",
        "vec4 second = texture(tex1, repeat(scroll(uv, vec2(-0.02, 0.03), time)));\n",
        "vec4 warped = texture(tex2, mirror(distort(zoom(uv, cycle(time, 0.15, 1.0, 0.1)), first, 0.08), tex2));\n",
        "vec4 pulse = colorize(radial_wave(uv, time, 30.0, 4.0), rgb(1.0, 1.0, 1.0));\n",
        "fragColor = multiply(multiply(warped, second), pulse);\n",
    };

    Program *syntax = NULL;
    ParseError parse_error;

    if (!parse_program(
            source,
            strlen(source),
            &syntax,
            &parse_error
        )) {
        fprintf(
            stderr,
            "FAIL texture GLSL emission: parse error at %zu:%zu: %s\n",
            parse_error.span.offset,
            parse_error.span.length,
            parse_error.message
        );
        return false;
    }

    Program *lowered = NULL;
    LowerError lower_error;

    if (!lower_program(
            syntax,
            &lowered,
            &lower_error
        )) {
        fprintf(
            stderr,
            "FAIL texture GLSL emission: lowering error at %zu:%zu: %s\n",
            lower_error.span.offset,
            lower_error.span.length,
            lower_error.message
        );
        program_destroy(syntax);
        return false;
    }

    SemanticInfo semantic_info;
    SemanticError semantic_error;

    if (!semantic_analyze(
            lowered,
            source,
            fusegl_dialect()->environment,
            &semantic_info,
            &semantic_error
        )) {
        fprintf(
            stderr,
            "FAIL texture GLSL emission: semantic error at %zu:%zu: %s\n",
            semantic_error.span.offset,
            semantic_error.span.length,
            semantic_error.message
        );
        program_destroy(lowered);
        program_destroy(syntax);
        return false;
    }

    char *actual = NULL;
    size_t actual_length = 0;
    bool emitted = emit_to_string(
        lowered,
        &semantic_info,
        source,
        &actual,
        &actual_length
    );
    bool matches = emitted && actual != NULL;

    for (size_t index = 0;
         matches
             && index < sizeof(EXPECTED_FRAGMENTS)
                    / sizeof(EXPECTED_FRAGMENTS[0]);
         index += 1) {
        matches = strstr(actual, EXPECTED_FRAGMENTS[index]) != NULL;
    }

    if (!matches) {
        fprintf(
            stderr,
            "FAIL texture GLSL snapshot\n"
            "actual:\n%s",
            actual == NULL ? "(null)\n" : actual
        );
    }

    free(actual);
    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(syntax);

    if (matches) {
        printf("PASS texture GLSL emission\n");
    }

    (void)actual_length;
    return matches;
}

static bool test_channel_helpers(void)
{
    const char *source =
        "let sampled = uv.sample(tex0);\n"
        "let amount = sampled.b().multiply(-1.0).add(1.0).multiply(0.5).clamp(0.0, 1.0);\n"
        "vec4(sampled.r(), sampled.g(), sampled.b(), amount)";
    Program *syntax = NULL;
    Program *lowered = NULL;
    ParseError parse_error;
    LowerError lower_error;
    SemanticError semantic_error;
    SemanticInfo semantic_info = {0};
    char *actual = NULL;
    size_t actual_length = 0;
    bool passed = parse_program(
        source,
        strlen(source),
        &syntax,
        &parse_error
    ) && lower_program(syntax, &lowered, &lower_error)
        && semantic_analyze(
            lowered,
            source,
            fusegl_dialect()->environment,
            &semantic_info,
            &semantic_error
        )
        && emit_to_string(
            lowered,
            &semantic_info,
            source,
            &actual,
            &actual_length
        )
        && strstr(
            actual,
            "float amount = clamp(multiply(add(multiply((sampled).b, -1.0), 1.0), 0.5), 0.0, 1.0);\n"
            "    fragColor = vec4((sampled).r, (sampled).g, (sampled).b, amount);"
        ) != NULL;

    if (!passed) {
        fprintf(
            stderr,
            "FAIL GLSL channel helpers\n"
            "actual:\n%s",
            actual == NULL ? "(null)\n" : actual
        );
    } else {
        printf("PASS GLSL channel helpers\n");
    }

    (void)actual_length;
    free(actual);
    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(syntax);
    return passed;
}

static bool test_preview_result_conversion(void)
{
    const char *source = "uv.center()";
    Program *syntax = NULL;
    Program *lowered = NULL;
    ParseError parse_error;
    LowerError lower_error;
    SemanticError semantic_error;
    SemanticInfo semantic_info = {0};
    char *actual = NULL;
    size_t actual_length = 0;
    bool passed = parse_program(
        source,
        strlen(source),
        &syntax,
        &parse_error
    ) && lower_program(syntax, &lowered, &lower_error)
        && semantic_analyze(
            lowered,
            source,
            fusegl_dialect()->environment,
            &semantic_info,
            &semantic_error
        )
        && emit_to_string(
            lowered,
            &semantic_info,
            source,
            &actual,
            &actual_length
        )
        && strstr(
            actual,
            "fragColor = vec4(center(uv), 0.0, 1.0);"
        ) != NULL;

    if (!passed) {
        fprintf(stderr, "FAIL preview GLSL result conversion\n");
    } else {
        printf("PASS preview GLSL result conversion\n");
    }

    (void)actual_length;
    free(actual);
    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(syntax);
    return passed;
}

int main(void)
{
    size_t passed = 0;

    if (test_complete_shader()) {
        passed += 1;
    }

    if (test_texture_shader()) {
        passed += 1;
    }

    if (test_channel_helpers()) {
        passed += 1;
    }

    if (test_preview_result_conversion()) {
        passed += 1;
    }

    printf(
        "%zu/4 GLSL emission tests passed\n",
        passed
    );

    return passed == 4 ? EXIT_SUCCESS : EXIT_FAILURE;
}
