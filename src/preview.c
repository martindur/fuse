#include "fuse/preview.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const Expr *expression;
    size_t visible_bindings;
    size_t anchor;
    bool found;
} PreviewPoint;

static void set_error(
    PreviewError *error,
    SourceSpan span,
    const char *message
)
{
    if (error->message == NULL) {
        error->span = span;
        error->message = message;
    }
}

static bool line_bounds(
    const char *source,
    size_t line,
    size_t *out_start,
    size_t *out_end
)
{
    if (line == 0) {
        return false;
    }

    size_t current_line = 1;
    size_t start = 0;

    while (current_line < line) {
        const char *newline = strchr(source + start, '\n');

        if (newline == NULL) {
            return false;
        }

        start = (size_t)(newline - source) + 1;
        current_line += 1;
    }

    const char *newline = strchr(source + start, '\n');
    size_t end = newline == NULL
        ? strlen(source)
        : (size_t)(newline - source);

    *out_start = start;
    *out_end = end;
    return true;
}

static void consider_point(
    PreviewPoint *point,
    const Expr *expression,
    size_t visible_bindings,
    size_t anchor,
    size_t line_start,
    size_t line_end
)
{
    if (anchor < line_start
        || anchor >= line_end
        || (point->found && anchor < point->anchor)) {
        return;
    }

    *point = (PreviewPoint){
        .expression = expression,
        .visible_bindings = visible_bindings,
        .anchor = anchor,
        .found = true,
    };
}

static void find_nested_points(
    const Expr *expression,
    size_t visible_bindings,
    size_t line_start,
    size_t line_end,
    PreviewPoint *point
);

static void find_argument_points(
    const ExprList *arguments,
    size_t visible_bindings,
    size_t line_start,
    size_t line_end,
    PreviewPoint *point
)
{
    for (size_t index = 0; index < arguments->count; index += 1) {
        const Expr *argument = arguments->items[index];

        if (argument->kind == EXPR_PIPELINE) {
            find_nested_points(
                argument,
                visible_bindings,
                line_start,
                line_end,
                point
            );
        } else if (argument->kind == EXPR_CALL) {
            find_argument_points(
                &argument->as.call.arguments,
                visible_bindings,
                line_start,
                line_end,
                point
            );
        }
    }
}

static void find_nested_points(
    const Expr *expression,
    size_t visible_bindings,
    size_t line_start,
    size_t line_end,
    PreviewPoint *point
)
{
    if (expression->kind != EXPR_PIPELINE) {
        consider_point(
            point,
            expression,
            visible_bindings,
            expression->span.offset,
            line_start,
            line_end
        );

        if (expression->kind == EXPR_CALL) {
            find_argument_points(
                &expression->as.call.arguments,
                visible_bindings,
                line_start,
                line_end,
                point
            );
        }

        return;
    }

    find_nested_points(
        expression->as.pipeline.input,
        visible_bindings,
        line_start,
        line_end,
        point
    );

    const Expr *stage = expression->as.pipeline.stage;

    if (stage != NULL && stage->kind == EXPR_CALL) {
        consider_point(
            point,
            expression,
            visible_bindings,
            stage->as.call.name.offset,
            line_start,
            line_end
        );
        find_argument_points(
            &stage->as.call.arguments,
            visible_bindings,
            line_start,
            line_end,
            point
        );
    }
}

static Expr *clone_expression(
    const Expr *source,
    PreviewError *error
)
{
    Expr *clone = NULL;

    switch (source->kind) {
    case EXPR_IDENTIFIER:
        clone = expr_create_identifier(source->span);
        break;

    case EXPR_NUMBER:
        clone = expr_create_number(source->span);
        break;

    case EXPR_CALL:
        clone = expr_create_call(
            source->span,
            source->as.call.name
        );

        if (clone == NULL) {
            break;
        }

        for (size_t index = 0;
             index < source->as.call.arguments.count;
             index += 1) {
            Expr *argument = clone_expression(
                source->as.call.arguments.items[index],
                error
            );

            if (argument == NULL
                || !expr_call_add_argument(clone, argument)) {
                expr_destroy(argument);
                expr_destroy(clone);
                return NULL;
            }
        }

        return clone;

    case EXPR_PIPELINE: {
        Expr *input = clone_expression(
            source->as.pipeline.input,
            error
        );
        Expr *stage = clone_expression(
            source->as.pipeline.stage,
            error
        );

        if (input == NULL || stage == NULL) {
            expr_destroy(input);
            expr_destroy(stage);
            return NULL;
        }

        clone = expr_create_pipeline(source->span, input, stage);

        if (clone == NULL) {
            expr_destroy(input);
            expr_destroy(stage);
        }

        break;
    }
    }

    if (clone == NULL) {
        set_error(error, source->span, "out of memory");
    }

    return clone;
}

bool preview_program_at_line(
    const Program *syntax,
    const char *source,
    size_t line,
    Program **out_preview,
    PreviewError *out_error
)
{
    if (out_preview == NULL || out_error == NULL) {
        return false;
    }

    *out_preview = NULL;
    *out_error = (PreviewError){0};

    if (syntax == NULL || syntax->result == NULL || source == NULL) {
        out_error->message = "invalid preview input";
        return false;
    }

    size_t line_start = 0;
    size_t line_end = 0;

    if (!line_bounds(source, line, &line_start, &line_end)) {
        out_error->message = "preview line is outside the source";
        return false;
    }

    PreviewPoint point = {0};

    for (size_t index = 0; index < syntax->binding_count; index += 1) {
        find_nested_points(
            syntax->bindings[index].initializer,
            index,
            line_start,
            line_end,
            &point
        );
    }

    find_nested_points(
        syntax->result,
        syntax->binding_count,
        line_start,
        line_end,
        &point
    );

    if (!point.found) {
        out_error->span = (SourceSpan){
            .offset = line_start,
            .length = line_end - line_start,
        };
        out_error->message = "line has no previewable expression";
        return false;
    }

    Program *preview = program_create();

    if (preview == NULL) {
        out_error->message = "out of memory";
        return false;
    }

    for (size_t index = 0;
         index < point.visible_bindings;
         index += 1) {
        Expr *initializer = clone_expression(
            syntax->bindings[index].initializer,
            out_error
        );

        if (initializer == NULL
            || !program_add_binding(
                preview,
                syntax->bindings[index].name,
                initializer
            )) {
            expr_destroy(initializer);
            program_destroy(preview);
            set_error(
                out_error,
                syntax->bindings[index].name,
                "out of memory"
            );
            return false;
        }
    }

    Expr *result = clone_expression(point.expression, out_error);

    if (result == NULL || !program_set_result(preview, result)) {
        expr_destroy(result);
        program_destroy(preview);
        set_error(out_error, point.expression->span, "out of memory");
        return false;
    }

    *out_preview = preview;
    return true;
}
