#include "evaluate.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const Program *program;
    const char *source;
    const EvalContext *context;
    EvalError *error;
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
    if (evaluator->error->message != NULL) {
        return;
    }

    evaluator->error->span = span;
    evaluator->error->message = message;
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

static bool value_is_float(const RuntimeValue *value)
{
    return value->type == VALUE_TYPE_FLOAT;
}

static bool value_is_vec2(const RuntimeValue *value)
{
    return value->type == VALUE_TYPE_VEC2;
}

static bool value_is_vec3(const RuntimeValue *value)
{
    return value->type == VALUE_TYPE_VEC3;
}

static bool value_set_float(
    RuntimeValue *value,
    float number
)
{
    *value = (RuntimeValue){
        .type = VALUE_TYPE_FLOAT,
        .components = {number, 0.0f, 0.0f, 0.0f},
    };
    return true;
}

static bool value_set_vec2(
    RuntimeValue *value,
    float x,
    float y
)
{
    *value = (RuntimeValue){
        .type = VALUE_TYPE_VEC2,
        .components = {x, y, 0.0f, 0.0f},
    };
    return true;
}

/*
 * Improved 2D Perlin noise. The seed selects a deterministic gradient field;
 * the coordinate controls frequency, and the public result is normalized to
 * the [0, 1] range expected by texture pipelines.
 */
static uint64_t noise_hash(
    uint64_t seed,
    int64_t x,
    int64_t y
)
{
    uint64_t value = seed;

    value ^= (uint64_t)x * UINT64_C(0x9e3779b97f4a7c15);
    value ^= (uint64_t)y * UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;

    return value;
}

static float noise_lerp(float first, float second, float amount)
{
    return first + (second - first) * amount;
}

static float noise_fade(float amount)
{
    return amount * amount * amount
        * (amount * (amount * 6.0f - 15.0f) + 10.0f);
}

static float noise_gradient(
    uint64_t seed,
    int64_t x,
    int64_t y,
    float offset_x,
    float offset_y
)
{
    static const float GRADIENTS[][2] = {
        {1.0f, 0.0f},
        {-1.0f, 0.0f},
        {0.0f, 1.0f},
        {0.0f, -1.0f},
        {0.7071067812f, 0.7071067812f},
        {-0.7071067812f, 0.7071067812f},
        {0.7071067812f, -0.7071067812f},
        {-0.7071067812f, -0.7071067812f},
    };
    size_t gradient_index = noise_hash(seed, x, y)
        & (sizeof(GRADIENTS) / sizeof(GRADIENTS[0]) - 1);

    return GRADIENTS[gradient_index][0] * offset_x
        + GRADIENTS[gradient_index][1] * offset_y;
}

static int64_t noise_wrap_lattice(
    int64_t coordinate,
    int64_t period
)
{
    int64_t wrapped = coordinate % period;

    return wrapped < 0 ? wrapped + period : wrapped;
}

static bool noise_period_component(
    float value,
    int64_t *out_period
)
{
    if (!isfinite(value)
        || value < 1.0f
        || floorf(value) != value
        || value >= (float)INT64_MAX) {
        return false;
    }

    *out_period = (int64_t)value;
    return *out_period > 0;
}

static float noise_value(
    const EvalContext *context,
    float x,
    float y,
    int64_t period_x,
    int64_t period_y
)
{
    if (!isfinite(x) || !isfinite(y)) {
        return 0.5f;
    }

    float cell_x = floorf(x);
    float cell_y = floorf(y);
    int64_t x0 = (int64_t)cell_x;
    int64_t y0 = (int64_t)cell_y;
    float local_x = x - cell_x;
    float local_y = y - cell_y;
    float fade_x = noise_fade(local_x);
    float fade_y = noise_fade(local_y);
    int64_t x1 = x0 + 1;
    int64_t y1 = y0 + 1;

    if (period_x > 0) {
        x0 = noise_wrap_lattice(x0, period_x);
        x1 = noise_wrap_lattice(x1, period_x);
    }

    if (period_y > 0) {
        y0 = noise_wrap_lattice(y0, period_y);
        y1 = noise_wrap_lattice(y1, period_y);
    }

    float bottom = noise_lerp(
        noise_gradient(context->seed, x0, y0, local_x, local_y),
        noise_gradient(context->seed, x1, y0, local_x - 1.0f, local_y),
        fade_x
    );
    float top = noise_lerp(
        noise_gradient(context->seed, x0, y1, local_x, local_y - 1.0f),
        noise_gradient(
            context->seed,
            x1,
            y1,
            local_x - 1.0f,
            local_y - 1.0f
        ),
        fade_x
    );

    float value = noise_lerp(bottom, top, fade_y);
    float normalized = 0.5f + 0.5f * value;

    if (normalized < 0.0f) {
        return 0.0f;
    }

    if (normalized > 1.0f) {
        return 1.0f;
    }

    return normalized;
}

static bool evaluate_expression(
    Evaluator *evaluator,
    const Expr *expression,
    const RuntimeValue *bindings,
    size_t visible_bindings,
    RuntimeValue *out_value
);

static bool resolve_identifier(
    Evaluator *evaluator,
    SourceSpan name,
    const RuntimeValue *bindings,
    size_t visible_bindings,
    RuntimeValue *out_value
)
{
    if (span_equals_text(evaluator->source, name, "uv")) {
        *out_value = (RuntimeValue){
            .type = VALUE_TYPE_VEC2,
            .components = {
                evaluator->context->uv[0],
                evaluator->context->uv[1],
                0.0f,
                0.0f,
            },
        };
        return true;
    }

    if (span_equals_text(evaluator->source, name, "resolution")) {
        *out_value = (RuntimeValue){
            .type = VALUE_TYPE_VEC2,
            .components = {
                evaluator->context->resolution[0],
                evaluator->context->resolution[1],
                0.0f,
                0.0f,
            },
        };
        return true;
    }

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

static bool evaluate_call(
    Evaluator *evaluator,
    const Expr *expression,
    const RuntimeValue *bindings,
    size_t visible_bindings,
    RuntimeValue *out_value
)
{
    size_t argument_count = expression->as.call.arguments.count;
    RuntimeValue *arguments = NULL;

    if (argument_count > 0) {
        if (argument_count > SIZE_MAX / sizeof(*arguments)) {
            evaluator_fail(evaluator, expression->span, "out of memory");
            return false;
        }

        arguments = calloc(argument_count, sizeof(*arguments));

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

    bool resolved = false;

    if (call_name_is(evaluator, expression, "center")
        && argument_count == 1
        && value_is_vec2(&arguments[0])) {
        resolved = value_set_vec2(
            out_value,
            arguments[0].components[0] - 0.5f,
            arguments[0].components[1] - 0.5f
        );
    } else if (call_name_is(evaluator, expression, "aspect")
        && argument_count == 1
        && value_is_vec2(&arguments[0])) {
        float ratio = evaluator->context->resolution[0]
            / evaluator->context->resolution[1];
        resolved = value_set_vec2(
            out_value,
            arguments[0].components[0] * ratio,
            arguments[0].components[1]
        );
    } else if (call_name_is(evaluator, expression, "add")
        && argument_count == 2
        && value_is_float(&arguments[0])
        && value_is_float(&arguments[1])) {
        resolved = value_set_float(
            out_value,
            arguments[0].components[0] + arguments[1].components[0]
        );
    } else if (call_name_is(evaluator, expression, "add")
        && argument_count == 2
        && value_is_vec2(&arguments[0])
        && value_is_vec2(&arguments[1])) {
        resolved = value_set_vec2(
            out_value,
            arguments[0].components[0] + arguments[1].components[0],
            arguments[0].components[1] + arguments[1].components[1]
        );
    } else if (call_name_is(evaluator, expression, "multiply")
        && argument_count == 2
        && value_is_float(&arguments[0])
        && value_is_float(&arguments[1])) {
        resolved = value_set_float(
            out_value,
            arguments[0].components[0] * arguments[1].components[0]
        );
    } else if (call_name_is(evaluator, expression, "multiply")
        && argument_count == 2
        && value_is_vec2(&arguments[0])
        && value_is_float(&arguments[1])) {
        resolved = value_set_vec2(
            out_value,
            arguments[0].components[0] * arguments[1].components[0],
            arguments[0].components[1] * arguments[1].components[0]
        );
    } else if (call_name_is(evaluator, expression, "sin")
        && argument_count == 1
        && value_is_float(&arguments[0])) {
        resolved = value_set_float(
            out_value,
            sinf(arguments[0].components[0])
        );
    } else if (call_name_is(evaluator, expression, "cos")
        && argument_count == 1
        && value_is_float(&arguments[0])) {
        resolved = value_set_float(
            out_value,
            cosf(arguments[0].components[0])
        );
    } else if (call_name_is(evaluator, expression, "abs")
        && argument_count == 1
        && value_is_float(&arguments[0])) {
        resolved = value_set_float(
            out_value,
            fabsf(arguments[0].components[0])
        );
    } else if (call_name_is(evaluator, expression, "floor")
        && argument_count == 1
        && value_is_float(&arguments[0])) {
        resolved = value_set_float(
            out_value,
            floorf(arguments[0].components[0])
        );
    } else if (call_name_is(evaluator, expression, "fract")
        && argument_count == 1
        && value_is_float(&arguments[0])) {
        float value = arguments[0].components[0];
        resolved = value_set_float(out_value, value - floorf(value));
    } else if (call_name_is(evaluator, expression, "length")
        && argument_count == 1
        && value_is_vec2(&arguments[0])) {
        float x = arguments[0].components[0];
        float y = arguments[0].components[1];
        resolved = value_set_float(out_value, sqrtf(x * x + y * y));
    } else if (call_name_is(evaluator, expression, "dot")
        && argument_count == 2
        && value_is_vec2(&arguments[0])
        && value_is_vec2(&arguments[1])) {
        resolved = value_set_float(
            out_value,
            arguments[0].components[0] * arguments[1].components[0]
                + arguments[0].components[1] * arguments[1].components[1]
        );
    } else if (call_name_is(evaluator, expression, "smoothstep")
        && argument_count == 3
        && value_is_float(&arguments[0])
        && value_is_float(&arguments[1])
        && value_is_float(&arguments[2])) {
        float value = arguments[0].components[0];
        float lower = arguments[1].components[0];
        float upper = arguments[2].components[0];
        float amount = upper == lower
            ? (value < lower ? 0.0f : 1.0f)
            : (value - lower) / (upper - lower);

        if (amount < 0.0f) {
            amount = 0.0f;
        } else if (amount > 1.0f) {
            amount = 1.0f;
        }

        resolved = value_set_float(
            out_value,
            amount * amount * (3.0f - 2.0f * amount)
        );
    } else if (call_name_is(evaluator, expression, "noise")
        && argument_count == 1
        && value_is_vec2(&arguments[0])) {
        resolved = value_set_float(
            out_value,
            noise_value(
                evaluator->context,
                arguments[0].components[0],
                arguments[0].components[1],
                0,
                0
            )
        );
    } else if (call_name_is(evaluator, expression, "noise")
        && argument_count == 2
        && value_is_vec2(&arguments[0])
        && value_is_vec2(&arguments[1])) {
        int64_t period_x = 0;
        int64_t period_y = 0;

        if (!noise_period_component(
                arguments[1].components[0],
                &period_x
            )
            || !noise_period_component(
                arguments[1].components[1],
                &period_y
            )) {
            evaluator_fail(
                evaluator,
                expression->span,
                "noise period must contain positive integers"
            );
        } else {
            resolved = value_set_float(
                out_value,
                noise_value(
                    evaluator->context,
                    arguments[0].components[0],
                    arguments[0].components[1],
                    period_x,
                    period_y
                )
            );
        }
    } else if (call_name_is(evaluator, expression, "vec2")
        && argument_count == 2
        && value_is_float(&arguments[0])
        && value_is_float(&arguments[1])) {
        *out_value = (RuntimeValue){
            .type = VALUE_TYPE_VEC2,
            .components = {
                arguments[0].components[0],
                arguments[1].components[0],
                0.0f,
                0.0f,
            },
        };
        resolved = true;
    } else if (span_equals_text(
            evaluator->source,
            expression->as.call.name,
            "vec3"
        )
        && argument_count == 3
        && value_is_float(&arguments[0])
        && value_is_float(&arguments[1])
        && value_is_float(&arguments[2])) {
        *out_value = (RuntimeValue){
            .type = VALUE_TYPE_VEC3,
            .components = {
                arguments[0].components[0],
                arguments[1].components[0],
                arguments[2].components[0],
                0.0f,
            },
        };
        resolved = true;
    } else if (span_equals_text(
            evaluator->source,
            expression->as.call.name,
            "vec4"
        )) {
        if (argument_count == 4
            && value_is_float(&arguments[0])
            && value_is_float(&arguments[1])
            && value_is_float(&arguments[2])
            && value_is_float(&arguments[3])) {
            *out_value = (RuntimeValue){
                .type = VALUE_TYPE_VEC4,
                .components = {
                    arguments[0].components[0],
                    arguments[1].components[0],
                    arguments[2].components[0],
                    arguments[3].components[0],
                },
            };
            resolved = true;
        } else if (argument_count == 3
            && value_is_vec2(&arguments[0])
            && value_is_float(&arguments[1])
            && value_is_float(&arguments[2])) {
            *out_value = (RuntimeValue){
                .type = VALUE_TYPE_VEC4,
                .components = {
                    arguments[0].components[0],
                    arguments[0].components[1],
                    arguments[1].components[0],
                    arguments[2].components[0],
                },
            };
            resolved = true;
        } else if (argument_count == 2
            && value_is_vec3(&arguments[0])
            && value_is_float(&arguments[1])) {
            *out_value = (RuntimeValue){
                .type = VALUE_TYPE_VEC4,
                .components = {
                    arguments[0].components[0],
                    arguments[0].components[1],
                    arguments[0].components[2],
                    arguments[1].components[0],
                },
            };
            resolved = true;
        }
    } else if (span_equals_text(
            evaluator->source,
            expression->as.call.name,
            "rgb"
        )
        && argument_count == 3
        && value_is_float(&arguments[0])
        && value_is_float(&arguments[1])
        && value_is_float(&arguments[2])) {
        *out_value = (RuntimeValue){
            .type = VALUE_TYPE_VEC3,
            .components = {
                arguments[0].components[0],
                arguments[1].components[0],
                arguments[2].components[0],
                0.0f,
            },
        };
        resolved = true;
    } else if (span_equals_text(
            evaluator->source,
            expression->as.call.name,
            "colorize"
        )
        && argument_count == 2
        && value_is_float(&arguments[0])
        && value_is_vec3(&arguments[1])) {
        float mask = arguments[0].components[0];
        *out_value = (RuntimeValue){
            .type = VALUE_TYPE_VEC4,
            .components = {
                arguments[1].components[0] * mask,
                arguments[1].components[1] * mask,
                arguments[1].components[2] * mask,
                mask,
            },
        };
        resolved = true;
    }

    free(arguments);

    if (!resolved) {
        evaluator_fail(
            evaluator,
            expression->span,
            "unsupported texture function"
        );
    }

    return resolved;
}

static bool evaluate_expression(
    Evaluator *evaluator,
    const Expr *expression,
    const RuntimeValue *bindings,
    size_t visible_bindings,
    RuntimeValue *out_value
)
{
    if (expression == NULL) {
        evaluator_fail(evaluator, (SourceSpan){0}, "expression is NULL");
        return false;
    }

    switch (expression->kind) {
    case EXPR_IDENTIFIER:
        return resolve_identifier(
            evaluator,
            expression->span,
            bindings,
            visible_bindings,
            out_value
        );

    case EXPR_NUMBER: {
        float number = 0.0f;

        if (!parse_number(evaluator->source, expression->span, &number)) {
            evaluator_fail(evaluator, expression->span, "invalid number");
            return false;
        }

        return value_set_float(out_value, number);
    }

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

bool fusetex_evaluate_program(
    const Program *program,
    const char *source,
    const EvalContext *context,
    RuntimeValue *out_value,
    EvalError *out_error
)
{
    if (out_value == NULL || out_error == NULL) {
        return false;
    }

    *out_value = (RuntimeValue){0};
    *out_error = (EvalError){0};

    if (program == NULL
        || program->result == NULL
        || source == NULL
        || context == NULL) {
        out_error->message = "invalid evaluation input";
        return false;
    }

    RuntimeValue *bindings = NULL;

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
        .context = context,
        .error = out_error,
    };

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
            free(bindings);
            return false;
        }
    }

    bool evaluated = evaluate_expression(
        &evaluator,
        program->result,
        bindings,
        program->binding_count,
        out_value
    );
    free(bindings);
    return evaluated;
}
