#include "glsl_emit.h"

#include "glsl_stdlib.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char SHADER_HEADER[] =
    "#version 330 core\n"
    "\n"
    "in vec2 matterboxUv;\n"
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
    "\n";

static const char SHADER_MAIN_PREFIX[] =
    "\n"
    "void main() {\n"
    "    vec2 uv = matterboxUv;\n";

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
        return false;
    }

    return fputs(prefix, output) != EOF
        && emit_expression(output, result, source)
        && fputs(suffix, output) != EOF;
}

bool glsl_emit(
    FILE *output,
    const Program *program,
    const SemanticInfo *semantic_info,
    const char *source
)
{
    if (output == NULL
        || program == NULL
        || program->result == NULL
        || semantic_info == NULL
        || semantic_info->binding_count != program->binding_count
        || (program->binding_count > 0
            && semantic_info->binding_types == NULL)
        || !value_type_is_previewable(semantic_info->result_type)
        || source == NULL) {
        return false;
    }

    return fputs(SHADER_HEADER, output) != EOF
        && glsl_stdlib_emit(output)
        && fputs(SHADER_MAIN_PREFIX, output) != EOF
        && emit_bindings(
            output,
            program,
            semantic_info,
            source
        )
        && fputs("    fragColor = ", output) != EOF
        && emit_result(
            output,
            program->result,
            semantic_info->result_type,
            source
        )
        && fputs(SHADER_SUFFIX, output) != EOF;
}
