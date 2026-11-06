#include "glsl_emit.h"

#include "glsl_stdlib.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char SHADER_HEADER[] =
    "#version 330 core\n"
    "\n"
    "in vec2 graficUv;\n"
    "in vec3 graficPosition;\n"
    "in vec3 graficAuthoredNormal;\n"
    "flat in int graficHasNormal;\n"
    "struct GraficSurface { vec3 position; vec2 uv; vec3 normal; };\n"
    "struct GraficScreen { vec2 position; vec2 uv; };\n"
    "struct GraficView { vec3 direction; };\n"
    "out vec4 fragColor;\n"
    "\n"
    "uniform float u_time;\n"
    "uniform vec2 u_resolution;\n"
    "uniform vec3 u_view_position;\n"
    "uniform vec3 u_view_direction;\n"
    "uniform bool u_view_is_orthographic;\n"
    "uniform sampler2D tex0;\n"
    "uniform sampler2D tex1;\n"
    "uniform sampler2D tex2;\n"
    "uniform sampler2D tex3;\n"
    "\n"
    "#define time u_time\n"
    "#define resolution u_resolution\n"
    "\n";

static const char VERTEX_HEADER[] =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec2 a_uv;\n"
    "out vec2 graficUv;\n"
    "out vec3 graficPosition;\n"
    "out vec3 graficAuthoredNormal;\n"
    "flat out int graficHasNormal;\n"
    "struct GraficVertex { vec3 position; vec2 uv; vec3 normal; bool has_normal; };\n"
    "uniform mat4 u_model; uniform mat4 u_view; uniform mat4 u_projection;\n"
    "uniform float u_time;\n#define time u_time\n";

static const char SHADER_MAIN_PREFIX[] =
    "\n"
    "void main() {\n"
    "    vec3 graficFaceNormal = grafic_surface_normal(graficPosition);\n"
    "    GraficSurface surface = GraficSurface(graficPosition, graficUv, graficHasNormal != 0 ? grafic_normalize(graficAuthoredNormal) : graficFaceNormal);\n"
    "    GraficScreen screen = GraficScreen(gl_FragCoord.xy, gl_FragCoord.xy / u_resolution);\n"
    "    GraficView view = GraficView(grafic_normalize(u_view_is_orthographic ? u_view_direction : u_view_position - graficPosition));\n";

static const char SHADER_SUFFIX[] =
    ";\n"
    "}\n";

static bool write_span(
    FILE *output,
    const char *source,
    SourceSpan span
)
{
    size_t written = fwrite(
        source + span.offset,
        1,
        span.length,
        output
    );

    return written == span.length;
}

static bool span_equals_text(
    const char *source,
    SourceSpan span,
    const char *text
)
{
    size_t length = strlen(text);

    return span.length == length
        && memcmp(source + span.offset, text, length) == 0;
}

static bool emit_expression(
    FILE *output,
    const Expr *expression,
    const char *source
);

static bool emit_channel_call(
    FILE *output,
    const Expr *expression,
    const char *source,
    const char *channel
)
{
    if (expression->as.call.arguments.count != 1
        || fputc('(', output) == EOF
        || !emit_expression(
            output,
            expression->as.call.arguments.items[0],
            source
        )
        || fputs(").", output) == EOF
        || fputs(channel, output) == EOF) {
        return false;
    }

    return true;
}

static bool emit_expression(
    FILE *output,
    const Expr *expression,
    const char *source
)
{
    switch (expression->kind) {
    case EXPR_IDENTIFIER:
    case EXPR_NUMBER:
        return write_span(
            output,
            source,
            expression->span
        );

    case EXPR_CALL:
        if (span_equals_text(source, expression->as.call.name, "show")) {
            return expression->as.call.arguments.count == 1
                && emit_expression(output, expression->as.call.arguments.items[0], source);
        }
        const char *components[] = {"x", "y", "z"};
        for (size_t index = 0; index < 3; index += 1) {
            if (span_equals_text(source, expression->as.call.name, components[index])) {
                return emit_channel_call(output, expression, source, components[index]);
            }
        }
        if (span_equals_text(source, expression->as.call.name, "r")) {
            return emit_channel_call(output, expression, source, "r");
        }

        if (span_equals_text(source, expression->as.call.name, "g")) {
            return emit_channel_call(output, expression, source, "g");
        }

        if (span_equals_text(source, expression->as.call.name, "b")) {
            return emit_channel_call(output, expression, source, "b");
        }

        if (span_equals_text(source, expression->as.call.name, "a")) {
            return emit_channel_call(output, expression, source, "a");
        }

        if (span_equals_text(
                source,
                expression->as.call.name,
                "sample"
            )) {
            if (expression->as.call.arguments.count != 2
                || fputs("texture(", output) == EOF
                || !emit_expression(
                    output,
                    expression->as.call.arguments.items[1],
                    source
                )
                || fputs(", ", output) == EOF
                || !emit_expression(
                    output,
                    expression->as.call.arguments.items[0],
                    source
                )
                || fputc(')', output) == EOF) {
                return false;
            }

            return true;
        }

        if (!write_span(
                output,
                source,
                expression->as.call.name
            )
            || fputc('(', output) == EOF) {
            return false;
        }

        for (size_t index = 0;
             index < expression->as.call.arguments.count;
             index += 1) {
            if (index > 0 && fputs(", ", output) == EOF) {
                return false;
            }

            if (!emit_expression(
                    output,
                    expression->as.call.arguments.items[index],
                    source
                )) {
                return false;
            }
        }

        return fputc(')', output) != EOF;

    case EXPR_FIELD:
        return emit_expression(output, expression->as.field.input, source)
            && fputc('.', output) != EOF
            && write_span(output, source, expression->as.field.name);
    case EXPR_PIPELINE:
        return false;
    }

    return false;
}

static const char *value_type_glsl_name(ValueType type)
{
    switch (type) {
    case VALUE_TYPE_FLOAT:
        return "float";
    case VALUE_TYPE_VEC2:
        return "vec2";
    case VALUE_TYPE_VEC3:
        return "vec3";
    case VALUE_TYPE_VEC4:
        return "vec4";
    case VALUE_TYPE_TEXTURE:
        return "sampler2D";
    case VALUE_TYPE_VERTEX: return "GraficVertex";
    case VALUE_TYPE_SURFACE: return "GraficSurface";
    case VALUE_TYPE_SCREEN: return "GraficScreen";
    case VALUE_TYPE_VIEW: return "GraficView";
    case VALUE_TYPE_MESH:
        return NULL;
    case VALUE_TYPE_INVALID:
        return NULL;
    }

    return NULL;
}

static bool emit_bindings(
    FILE *output,
    const Program *program,
    const SemanticInfo *semantic_info,
    const char *source
)
{
    for (size_t index = 0;
         index < program->binding_count;
         index += 1) {
        const Binding *binding = &program->bindings[index];
        const char *type_name = value_type_glsl_name(
            semantic_info->binding_types[index]
        );

        if (type_name == NULL
            || fputs("    ", output) == EOF
            || fputs(type_name, output) == EOF
            || fputc(' ', output) == EOF
            || !write_span(output, source, binding->name)
            || fputs(" = ", output) == EOF
            || !emit_expression(
                output,
                binding->initializer,
                source
            )
            || fputs(";\n", output) == EOF) {
            return false;
        }
    }

    return true;
}

static bool emit_result(
    FILE *output,
    const Expr *result,
    ValueType type,
    const char *source
)
{
    const char *prefix = NULL;
    const char *suffix = NULL;

    switch (type) {
    case VALUE_TYPE_FLOAT:
        prefix = "vec4(vec3(";
        suffix = "), 1.0)";
        break;

    case VALUE_TYPE_VEC2:
        prefix = "vec4(";
        suffix = ", 0.0, 1.0)";
        break;

    case VALUE_TYPE_VEC3:
        prefix = "vec4(";
        suffix = ", 1.0)";
        break;

    case VALUE_TYPE_VEC4:
        prefix = "";
        suffix = "";
        break;

    case VALUE_TYPE_INVALID:
    case VALUE_TYPE_TEXTURE:
    case VALUE_TYPE_MESH:
    case VALUE_TYPE_VERTEX:
    case VALUE_TYPE_SURFACE:
    case VALUE_TYPE_VIEW:
    case VALUE_TYPE_SCREEN:
        return false;
    }

    return fputs(prefix, output) != EOF
        && emit_expression(output, result, source)
        && fputs(suffix, output) != EOF;
}

bool glsl_emit(
    FILE *output,
    ShaderStage stage,
    const PreparedProgram *prepared,
    const char *source
)
{
    if (prepared == NULL) return false;
    const Program *program = prepared->program;
    const SemanticInfo *semantic_info = &prepared->semantic_info;
    if (output == NULL
        || (stage != SHADER_STAGE_FRAGMENT && stage != SHADER_STAGE_VERTEX)
        || program == NULL
        || program->result == NULL
        || semantic_info->binding_count != program->binding_count
        || (program->binding_count > 0
            && semantic_info->binding_types == NULL)
        || (stage == SHADER_STAGE_VERTEX ? (semantic_info->result_type != VALUE_TYPE_VERTEX && semantic_info->result_type != VALUE_TYPE_VEC3)
            : !value_type_is_previewable(semantic_info->result_type))
        || source == NULL) {
        return false;
    }

    bool vertex = stage == SHADER_STAGE_VERTEX;
    if (fputs(vertex ? VERTEX_HEADER : SHADER_HEADER, output) == EOF
        || !glsl_stdlib_emit(output, stage)
        || fputs(vertex ? "\nvoid main() {\n    vec3 position = a_position;\n    vec2 uv = a_uv;\n"
            : SHADER_MAIN_PREFIX, output) == EOF
        || !emit_bindings(output, prepared->context_program != NULL ? prepared->context_program : program,
            prepared->context_program != NULL ? &prepared->context_semantic_info : semantic_info, source)) {
        return false;
    }
    if (vertex) {
        bool position_preview = semantic_info->result_type == VALUE_TYPE_VEC3;
        if (position_preview && prepared->context_program != NULL) {
            if (fputs("    GraficVertex graficContext = ", output) == EOF
                || !emit_expression(output, prepared->context_program->result, source)
                || fputs(";\n", output) == EOF) return false;
        }
        if (fputs("    GraficVertex graficResult = ", output) == EOF) return false;
        if (position_preview && fputs(prepared->context_program != NULL ? "GraficVertex(" : "vertex(", output) == EOF) return false;
        if (!emit_expression(output, program->result, source)) return false;
        if (position_preview && fputs(prepared->context_program != NULL
                ? ", graficContext.uv, graficContext.normal, graficContext.has_normal)" : ", uv)", output) == EOF) return false;
        return fputs(";\n    graficUv = graficResult.uv;\n    graficPosition = graficResult.position;\n"
            "    graficAuthoredNormal = graficResult.normal;\n    graficHasNormal = graficResult.has_normal ? 1 : 0;\n"
            "    gl_Position = u_projection * u_view * u_model * vec4(graficPosition, 1.0);\n}\n", output) != EOF;
    }
    return fputs("    fragColor = ", output) != EOF
        && emit_result(output, program->result, semantic_info->result_type, source)
        && fputs(SHADER_SUFFIX, output) != EOF;
}
