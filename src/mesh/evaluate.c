#include "evaluate.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const Program *program;
    const char *source;
    FuseMeshEvalError *error;
} Evaluator;

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

static bool spans_equal(
    const char *source,
    SourceSpan left,
    SourceSpan right
)
{
    return left.length == right.length
        && memcmp(
            source + left.offset,
            source + right.offset,
            left.length
        ) == 0;
}

static void evaluator_fail(
    Evaluator *evaluator,
    SourceSpan span,
    const char *message
)
{
    if (evaluator->error->message == NULL) {
        evaluator->error->span = span;
        evaluator->error->message = message;
    }
}

static bool parse_number(
    const char *source,
    SourceSpan span,
    float *out_value
)
{
    if (span.length == SIZE_MAX) {
        return false;
    }

    char *text = malloc(span.length + 1);

    if (text == NULL) {
        return false;
    }

    memcpy(text, source + span.offset, span.length);
    text[span.length] = '\0';

    char *end = NULL;
    *out_value = strtof(text, &end);
    bool valid = end == text + span.length;
    free(text);
    return valid;
}

static bool value_is_number(const FuseMeshRuntimeValue *value)
{
    return value->type == VALUE_TYPE_FLOAT;
}

static bool value_is_mesh(const FuseMeshRuntimeValue *value)
{
    return value->type == VALUE_TYPE_MESH;
}

static bool evaluate_expression(
    Evaluator *evaluator,
    const Expr *expression,
    const FuseMeshRuntimeValue *bindings,
    size_t visible_bindings,
    FuseMeshRuntimeValue *out_value
);

static bool resolve_identifier(
    Evaluator *evaluator,
    SourceSpan name,
    const FuseMeshRuntimeValue *bindings,
    size_t visible_bindings,
    FuseMeshRuntimeValue *out_value
)
{
    for (size_t index = visible_bindings;
         index > 0;
         index -= 1) {
        size_t binding_index = index - 1;

        if (spans_equal(
                evaluator->source,
                name,
                evaluator->program->bindings[binding_index].name
            )) {
            *out_value = bindings[binding_index];
            return true;
        }
    }

    evaluator_fail(evaluator, name, "unknown identifier");
    return false;
}

static bool call_name_is(
    const Evaluator *evaluator,
    const Expr *expression,
    const char *name
)
{
    return span_equals_text(
        evaluator->source,
        expression->as.call.name,
        name
    );
}

static bool parse_subdivision(
    Evaluator *evaluator,
    const Expr *expression,
    const FuseMeshRuntimeValue *value,
    size_t minimum,
    size_t maximum,
    const char *message,
    size_t *out_count
)
{
    if (!value_is_number(value)
        || !isfinite(value->number)
        || value->number < 1.0f
        || floorf(value->number) != value->number
        || value->number > (float)maximum) {
        evaluator_fail(
            evaluator,
            expression->span,
            "subdivision count must be a positive integer"
        );
        return false;
    }

    size_t count = (size_t)value->number;

    if (count < minimum || count > maximum) {
        evaluator_fail(
            evaluator,
            expression->span,
            message
        );
        return false;
    }

    *out_count = count;
    return true;
}

static bool evaluate_call(
    Evaluator *evaluator,
    const Expr *expression,
    const FuseMeshRuntimeValue *bindings,
    size_t visible_bindings,
    FuseMeshRuntimeValue *out_value
)
{
    size_t argument_count = expression->as.call.arguments.count;
    FuseMeshRuntimeValue *arguments = NULL;

    if (argument_count > 0) {
        if (argument_count > SIZE_MAX / sizeof(*arguments)) {
            evaluator_fail(evaluator, expression->span, "out of memory");
            return false;
        }

        arguments = malloc(argument_count * sizeof(*arguments));

        if (arguments == NULL) {
            evaluator_fail(evaluator, expression->span, "out of memory");
            return false;
        }
    }

    for (size_t index = 0; index < argument_count; index += 1) {
        if (!evaluate_expression(
                evaluator,
                expression->as.call.arguments.items[index],
                bindings,
                visible_bindings,
                &arguments[index]
            )) {
            free(arguments);
            return false;
        }
    }

    bool evaluated = false;

    if (call_name_is(evaluator, expression, "disk")
        && argument_count == 0) {
        *out_value = (FuseMeshRuntimeValue){
            .type = VALUE_TYPE_MESH,
            .disk = {
                .radius = 1.0f,
                .segments = 64,
                .rings = 16,
            },
        };
        evaluated = true;
    } else if (call_name_is(evaluator, expression, "radius")
        && argument_count == 2) {
        if (!value_is_mesh(&arguments[0])
            || !value_is_number(&arguments[1])) {
            evaluator_fail(
                evaluator,
                expression->span,
                "radius expects a mesh and a scalar"
            );
        } else if (!isfinite(arguments[1].number)
            || arguments[1].number <= 0.0f) {
            evaluator_fail(
                evaluator,
                expression->span,
                "radius must be positive"
            );
        } else {
            *out_value = arguments[0];
            out_value->disk.radius = arguments[1].number;
            evaluated = true;
        }
    } else if (call_name_is(evaluator, expression, "segments")
        && argument_count == 2
        && value_is_mesh(&arguments[0])) {
        size_t count;

        if (parse_subdivision(
                evaluator,
                expression,
                &arguments[1],
                FUSE_MESH_MIN_SEGMENTS,
                FUSE_MESH_MAX_SEGMENTS,
                "segments must be between 3 and 4096",
                &count
            )) {
            *out_value = arguments[0];
            out_value->disk.segments = count;
            evaluated = true;
        }
    } else if (call_name_is(evaluator, expression, "rings")
        && argument_count == 2
        && value_is_mesh(&arguments[0])) {
        size_t count;

        if (parse_subdivision(
                evaluator,
                expression,
                &arguments[1],
                FUSE_MESH_MIN_RINGS,
                FUSE_MESH_MAX_RINGS,
                "rings must be between 1 and 4096",
                &count
            )) {
            *out_value = arguments[0];
            out_value->disk.rings = count;
            evaluated = true;
        }
    } else if (call_name_is(evaluator, expression, "uv_radial")
        && argument_count == 1
        && value_is_mesh(&arguments[0])) {
        *out_value = arguments[0];
        out_value->disk.uv_mapping = FUSE_MESH_UV_RADIAL;
        evaluated = true;
    }

    if (!evaluated && evaluator->error->message == NULL) {
        evaluator_fail(
            evaluator,
            expression->span,
            "unsupported mesh operation"
        );
    }

    free(arguments);
    return evaluated;
}

static bool evaluate_expression(
    Evaluator *evaluator,
    const Expr *expression,
    const FuseMeshRuntimeValue *bindings,
    size_t visible_bindings,
    FuseMeshRuntimeValue *out_value
)
{
    switch (expression->kind) {
    case EXPR_IDENTIFIER:
        return resolve_identifier(
            evaluator,
            expression->span,
            bindings,
            visible_bindings,
            out_value
        );

    case EXPR_NUMBER:
        if (!parse_number(
                evaluator->source,
                expression->span,
                &out_value->number
            )) {
            evaluator_fail(evaluator, expression->span, "invalid number");
            return false;
        }

        out_value->type = VALUE_TYPE_FLOAT;
        return true;

    case EXPR_CALL:
        return evaluate_call(
            evaluator,
            expression,
            bindings,
            visible_bindings,
            out_value
        );

    case EXPR_PIPELINE:
        evaluator_fail(
            evaluator,
            expression->span,
            "pipeline must be lowered before evaluation"
        );
        return false;
    }

    evaluator_fail(evaluator, expression->span, "unknown expression kind");
    return false;
}

bool fusemesh_evaluate_program(
    const Program *program,
    const char *source,
    FuseMeshDiskSpec *out_spec,
    FuseMeshEvalError *out_error
)
{
    if (out_spec == NULL || out_error == NULL) {
        return false;
    }

    *out_spec = (FuseMeshDiskSpec){0};
    *out_error = (FuseMeshEvalError){0};

    if (program == NULL || program->result == NULL || source == NULL) {
        out_error->message = "invalid evaluation input";
        return false;
    }

    FuseMeshRuntimeValue *bindings = NULL;

    if (program->binding_count > 0) {
        if (program->binding_count > SIZE_MAX / sizeof(*bindings)) {
            out_error->message = "out of memory";
            return false;
        }

        bindings = calloc(program->binding_count, sizeof(*bindings));

        if (bindings == NULL) {
            out_error->message = "out of memory";
            return false;
        }
    }

    Evaluator evaluator = {
        .program = program,
        .source = source,
        .error = out_error,
    };
    bool evaluated = true;

    for (size_t index = 0;
         index < program->binding_count;
         index += 1) {
        if (!evaluate_expression(
                &evaluator,
                program->bindings[index].initializer,
                bindings,
                index,
                &bindings[index]
            )) {
            evaluated = false;
            break;
        }
    }

    FuseMeshRuntimeValue result = {0};

    if (evaluated
        && !evaluate_expression(
            &evaluator,
            program->result,
            bindings,
            program->binding_count,
            &result
        )) {
        evaluated = false;
    }

    if (evaluated && !value_is_mesh(&result)) {
        evaluator_fail(
            &evaluator,
            program->result->span,
            "program result must be a mesh"
        );
        evaluated = false;
    }

    if (evaluated) {
        *out_spec = result.disk;
    }

    free(bindings);
    return evaluated;
}
