#include "evaluate.h"
#include "grafic/builtins.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const Program *program;
    const char *source;
    GraficMeshEvalError *error;
    MeshPlan *plan;
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

static bool value_is_number(const GraficMeshRuntimeValue *value)
{
    return value->type == VALUE_TYPE_FLOAT;
}

static bool value_is_mesh(const GraficMeshRuntimeValue *value)
{
    return value->type == VALUE_TYPE_MESH;
}

static bool evaluate_expression(
    Evaluator *evaluator,
    const Expr *expression,
    const GraficMeshRuntimeValue *bindings,
    size_t visible_bindings,
    GraficMeshRuntimeValue *out_value
);

static bool resolve_identifier(
    Evaluator *evaluator,
    SourceSpan name,
    const GraficMeshRuntimeValue *bindings,
    size_t visible_bindings,
    GraficMeshRuntimeValue *out_value
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

static bool parse_count(
    Evaluator *evaluator,
    const Expr *expression,
    const GraficMeshRuntimeValue *value,
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
            message
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

static MeshRecipe *new_recipe(Evaluator *evaluator, const Expr *expression,
    MeshRecipeKind kind, const MeshRecipe *copy)
{
    MeshRecipe *recipe = calloc(1, sizeof(*recipe));
    if (recipe == NULL) {
        evaluator_fail(evaluator, expression->span, "out of memory");
        return NULL;
    }
    if (copy != NULL) *recipe = *copy;
    recipe->kind = kind;
    recipe->id = evaluator->plan->count++;
    recipe->span = expression->span;
    recipe->next_owned = evaluator->plan->owned;
    evaluator->plan->owned = recipe;
    return recipe;
}

static bool evaluate_call(Evaluator *evaluator, const Expr *expression,
    const GraficMeshRuntimeValue *bindings, size_t visible_bindings, GraficMeshRuntimeValue *out_value)
{
    size_t count = expression->as.call.arguments.count;
    if (count > BUILTIN_MAX_PARAMETERS) {
        evaluator_fail(evaluator, expression->span, "too many mesh arguments");
        return false;
    }
    GraficMeshRuntimeValue arguments[BUILTIN_MAX_PARAMETERS] = {0};
    for (size_t index = 0; index < count; index += 1) {
        if (!evaluate_expression(evaluator, expression->as.call.arguments.items[index],
                bindings, visible_bindings, &arguments[index])) return false;
    }
    if (call_name_is(evaluator, expression, "show") && count == 1 && value_is_mesh(&arguments[0])) {
        *out_value = arguments[0];
        return true;
    }
    if (count == 0 && (call_name_is(evaluator, expression, "disk")
            || call_name_is(evaluator, expression, "grid") || call_name_is(evaluator, expression, "strip"))) {
        MeshRecipe *recipe = new_recipe(evaluator, expression, MESH_RECIPE_PRIMITIVE, NULL);
        if (recipe == NULL) return false;
        if (call_name_is(evaluator, expression, "disk")) {
            recipe->primitive = (GraficMeshSpec){.kind = GRAFIC_MESH_DISK, .disk = {1.0f, 64, 16, GRAFIC_MESH_UV_PLANAR}};
        } else if (call_name_is(evaluator, expression, "grid")) {
            recipe->primitive = (GraficMeshSpec){.kind = GRAFIC_MESH_GRID, .grid = {2.0f, 2.0f, 64, 64}};
        } else {
            recipe->primitive = (GraficMeshSpec){.kind = GRAFIC_MESH_STRIP, .strip = {1.0f, 1.0f, 4, 0.0f}};
        }
        *out_value = (GraficMeshRuntimeValue){.type = VALUE_TYPE_MESH, .mesh = recipe};
        return true;
    }
    if (count < 1 || !value_is_mesh(&arguments[0])) goto unsupported;
    const MeshRecipe *input = arguments[0].mesh;
    MeshRecipe *recipe = NULL;
    if (call_name_is(evaluator, expression, "rotate") && count == 4) {
        for (size_t index = 1; index < 4; index += 1) {
            if (!value_is_number(&arguments[index]) || !isfinite(arguments[index].number)) {
                evaluator_fail(evaluator, expression->span, "rotation angles must be finite radians");
                return false;
            }
        }
        recipe = new_recipe(evaluator, expression, MESH_RECIPE_ROTATE, NULL);
        if (recipe == NULL) return false;
        recipe->rotate.source = input;
        for (size_t index = 0; index < 3; index += 1) recipe->rotate.angles[index] = arguments[index + 1].number;
    } else if (call_name_is(evaluator, expression, "scatter") && count == 3 && value_is_mesh(&arguments[1])) {
        size_t copies;
        if (!parse_count(evaluator, expression, &arguments[2], 1, 1000000,
                "scatter count must be between 1 and 1000000", &copies)) return false;
        recipe = new_recipe(evaluator, expression, MESH_RECIPE_SCATTER, NULL);
        if (recipe == NULL) return false;
        recipe->scatter.source = input;
        recipe->scatter.surface = arguments[1].mesh;
        recipe->scatter.settings = (GraficMeshScatterSpec){copies, 0, 1.0f, 1.0f};
    } else if (input->kind == MESH_RECIPE_SCATTER && count == 2 && call_name_is(evaluator, expression, "seed")) {
        float seed = arguments[1].number;
        if (!value_is_number(&arguments[1]) || !isfinite(seed) || seed < 0 || seed > 16777215 || floorf(seed) != seed) {
            evaluator_fail(evaluator, expression->span, "scatter seed must be an integer between 0 and 16777215");
            return false;
        }
        recipe = new_recipe(evaluator, expression, MESH_RECIPE_SCATTER, input);
        if (recipe == NULL) return false;
        recipe->scatter.settings.seed = (uint32_t)seed;
    } else if (input->kind == MESH_RECIPE_SCATTER && count == 3 && call_name_is(evaluator, expression, "scale_variation")) {
        float low = arguments[1].number, high = arguments[2].number;
        if (!value_is_number(&arguments[1]) || !value_is_number(&arguments[2])
            || !isfinite(low) || !isfinite(high) || low <= 0 || high < low) {
            evaluator_fail(evaluator, expression->span, "scale variation requires positive finite minimum <= maximum");
            return false;
        }
        recipe = new_recipe(evaluator, expression, MESH_RECIPE_SCATTER, input);
        if (recipe == NULL) return false;
        recipe->scatter.settings.minimum_scale = low;
        recipe->scatter.settings.maximum_scale = high;
    } else if (input->kind == MESH_RECIPE_PRIMITIVE) {
        recipe = new_recipe(evaluator, expression, MESH_RECIPE_PRIMITIVE, input);
        if (recipe == NULL) return false;
        GraficMeshSpec *spec = &recipe->primitive;
        if (call_name_is(evaluator, expression, "size") && count == 3
            && (spec->kind == GRAFIC_MESH_GRID || spec->kind == GRAFIC_MESH_STRIP)) {
            if (!value_is_number(&arguments[1]) || !value_is_number(&arguments[2])
                || !isfinite(arguments[1].number) || !isfinite(arguments[2].number)
                || arguments[1].number <= 0 || arguments[2].number <= 0) {
                evaluator_fail(evaluator, expression->span, "mesh dimensions must be positive and finite");
                return false;
            }
            if (spec->kind == GRAFIC_MESH_GRID) {
                spec->grid.width = arguments[1].number; spec->grid.depth = arguments[2].number;
            } else {
                spec->strip.width = arguments[1].number; spec->strip.height = arguments[2].number;
            }
        } else if (call_name_is(evaluator, expression, "subdivisions") && count == 3 && spec->kind == GRAFIC_MESH_GRID) {
            if (!parse_count(evaluator, expression, &arguments[1], 1, 4096,
                    "grid subdivisions must be between 1 and 4096", &spec->grid.subdivisions_x)
                || !parse_count(evaluator, expression, &arguments[2], 1, 4096,
                    "grid subdivisions must be between 1 and 4096", &spec->grid.subdivisions_z)) return false;
        } else if (call_name_is(evaluator, expression, "segments") && count == 2
            && (spec->kind == GRAFIC_MESH_DISK || spec->kind == GRAFIC_MESH_STRIP)) {
            bool strip = spec->kind == GRAFIC_MESH_STRIP;
            size_t segments;
            if (!parse_count(evaluator, expression, &arguments[1], strip ? 1 : GRAFIC_MESH_MIN_SEGMENTS,
                    GRAFIC_MESH_MAX_SEGMENTS, "invalid segment count", &segments)) return false;
            if (strip) spec->strip.segments = segments;
            else spec->disk.segments = segments;
        } else if (call_name_is(evaluator, expression, "rings") && count == 2 && spec->kind == GRAFIC_MESH_DISK) {
            if (!parse_count(evaluator, expression, &arguments[1], GRAFIC_MESH_MIN_RINGS,
                    GRAFIC_MESH_MAX_RINGS, "invalid ring count", &spec->disk.rings)) return false;
        } else if (call_name_is(evaluator, expression, "uv_radial") && count == 1 && spec->kind == GRAFIC_MESH_DISK) {
            spec->disk.uv_mapping = GRAFIC_MESH_UV_RADIAL;
        } else if (call_name_is(evaluator, expression, "radius") && count == 2 && spec->kind == GRAFIC_MESH_DISK) {
            if (!value_is_number(&arguments[1]) || !isfinite(arguments[1].number) || arguments[1].number <= 0) {
                evaluator_fail(evaluator, expression->span, "radius must be positive");
                return false;
            }
            spec->disk.radius = arguments[1].number;
        } else if (call_name_is(evaluator, expression, "taper") && count == 2 && spec->kind == GRAFIC_MESH_STRIP) {
            if (!value_is_number(&arguments[1]) || !isfinite(arguments[1].number)
                || arguments[1].number < 0 || arguments[1].number > 1) {
                evaluator_fail(evaluator, expression->span, "strip taper must be between 0 and 1");
                return false;
            }
            spec->strip.taper = arguments[1].number;
        } else goto unsupported;
    } else goto unsupported;
    *out_value = (GraficMeshRuntimeValue){.type = VALUE_TYPE_MESH, .mesh = recipe};
    return true;
unsupported:
    evaluator_fail(evaluator, expression->span, "unsupported mesh operation");
    return false;
}

static bool evaluate_expression(
    Evaluator *evaluator,
    const Expr *expression,
    const GraficMeshRuntimeValue *bindings,
    size_t visible_bindings,
    GraficMeshRuntimeValue *out_value
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

    case EXPR_FIELD:
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

bool graficmesh_evaluate_program(
    const Program *program,
    const char *source,
    MeshPlan *out_plan,
    GraficMeshEvalError *out_error
)
{
    if (out_plan == NULL || out_error == NULL) {
        return false;
    }

    *out_plan = (MeshPlan){0};
    *out_error = (GraficMeshEvalError){0};

    if (program == NULL || program->result == NULL || source == NULL) {
        out_error->message = "invalid evaluation input";
        return false;
    }

    GraficMeshRuntimeValue *bindings = NULL;

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
        .plan = out_plan,
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

    GraficMeshRuntimeValue result = {0};

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
        out_plan->root = result.mesh;
    }

    free(bindings);
    if (!evaluated) mesh_plan_destroy(out_plan);
    return evaluated;
}
