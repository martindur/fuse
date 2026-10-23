#include "fuse/semantic.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const Program *program;
    const char *source;
    const LanguageEnvironment *environment;
    SemanticInfo *info;
    SemanticError *error;
} Analyzer;

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

static void analyzer_fail(
    Analyzer *analyzer,
    SourceSpan span,
    const char *message
)
{
    if (analyzer->error->message != NULL) {
        return;
    }

    analyzer->error->span = span;
    analyzer->error->message = message;
}

static bool infer_expression(
    Analyzer *analyzer,
    const Expr *expression,
    size_t visible_bindings,
    ValueType *out_type
);

static bool resolve_identifier(
    Analyzer *analyzer,
    SourceSpan name,
    size_t visible_bindings,
    ValueType *out_type
)
{
    for (size_t index = 0;
         index < analyzer->environment->global_count;
         index += 1) {
        const GlobalSignature *global =
            &analyzer->environment->globals[index];

        if (span_equals_text(
                analyzer->source,
                name,
                global->name
            )) {
            *out_type = global->type;
            return true;
        }
    }

    for (size_t index = visible_bindings;
         index > 0;
         index -= 1) {
        size_t binding_index = index - 1;

        if (spans_equal(
                analyzer->source,
                name,
                analyzer->program->bindings[binding_index].name
            )) {
            *out_type =
                analyzer->info->binding_types[binding_index];
            return true;
        }
    }

    for (size_t index = visible_bindings;
         index < analyzer->program->binding_count;
         index += 1) {
        if (spans_equal(
                analyzer->source,
                name,
                analyzer->program->bindings[index].name
            )) {
            analyzer_fail(
                analyzer,
                name,
                "forward reference is not allowed"
            );
            return false;
        }
    }

    analyzer_fail(analyzer, name, "unknown identifier");
    return false;
}

static bool signature_name_matches(
    const Analyzer *analyzer,
    SourceSpan name,
    const BuiltinSignature *signature
)
{
    return span_equals_text(
        analyzer->source,
        name,
        signature->name
    );
}

static bool resolve_call(
    Analyzer *analyzer,
    const Expr *expression,
    const ValueType *argument_types,
    ValueType *out_type
)
{
    bool found_name = false;
    bool found_arity = false;

    for (size_t index = 0;
         index < analyzer->environment->builtin_count;
         index += 1) {
        const BuiltinSignature *signature =
            &analyzer->environment->builtins[index];

        if (!signature_name_matches(
                analyzer,
                expression->as.call.name,
                signature
            )) {
            continue;
        }

        found_name = true;

        if (signature->parameter_count
            != expression->as.call.arguments.count) {
            continue;
        }

        found_arity = true;
        bool matches = true;

        for (size_t argument_index = 0;
             argument_index < signature->parameter_count;
             argument_index += 1) {
            if (argument_types[argument_index]
                != signature->parameters[argument_index]) {
                matches = false;
                break;
            }
        }

        if (matches) {
            *out_type = signature->result;
            return true;
        }
    }

    if (!found_name) {
        analyzer_fail(
            analyzer,
            expression->as.call.name,
            "unknown function"
        );
    } else if (!found_arity) {
        analyzer_fail(
            analyzer,
            expression->span,
            "incorrect argument count"
        );
    } else {
        analyzer_fail(
            analyzer,
            expression->span,
            "argument type mismatch"
        );
    }

    return false;
}

static bool infer_call(
    Analyzer *analyzer,
    const Expr *expression,
    size_t visible_bindings,
    ValueType *out_type
)
{
    size_t argument_count =
        expression->as.call.arguments.count;
    ValueType *argument_types = NULL;

    if (argument_count > 0) {
        if (argument_count
            > SIZE_MAX / sizeof(*argument_types)) {
            analyzer_fail(
                analyzer,
                expression->span,
                "out of memory"
            );
            return false;
        }

        argument_types = malloc(
            argument_count * sizeof(*argument_types)
        );

        if (argument_types == NULL) {
            analyzer_fail(
                analyzer,
                expression->span,
                "out of memory"
            );
            return false;
        }
    }

    for (size_t index = 0;
         index < argument_count;
         index += 1) {
        if (!infer_expression(
                analyzer,
                expression->as.call.arguments.items[index],
                visible_bindings,
                &argument_types[index]
            )) {
            free(argument_types);
            return false;
        }
    }

    bool resolved = resolve_call(
        analyzer,
        expression,
        argument_types,
        out_type
    );
    free(argument_types);
    return resolved;
}

static bool infer_expression(
    Analyzer *analyzer,
    const Expr *expression,
    size_t visible_bindings,
    ValueType *out_type
)
{
    if (expression == NULL) {
        analyzer_fail(
            analyzer,
            (SourceSpan){0},
            "expression is NULL"
        );
        return false;
    }

    switch (expression->kind) {
    case EXPR_IDENTIFIER:
        return resolve_identifier(
            analyzer,
            expression->span,
            visible_bindings,
            out_type
        );

    case EXPR_NUMBER:
        *out_type = VALUE_TYPE_FLOAT;
        return true;

    case EXPR_CALL:
        return infer_call(
            analyzer,
            expression,
            visible_bindings,
            out_type
        );

    case EXPR_PIPELINE:
        analyzer_fail(
            analyzer,
            expression->span,
            "pipeline must be lowered before analysis"
        );
        return false;
    }

    analyzer_fail(
        analyzer,
        expression->span,
        "unknown expression kind"
    );
    return false;
}

static bool binding_name_is_reserved(
    const Analyzer *analyzer,
    SourceSpan name
)
{
    for (size_t index = 0;
         index < analyzer->environment->reserved_count;
         index += 1) {
        if (span_equals_text(
                analyzer->source,
                name,
                analyzer->environment->reserved_names[index]
            )) {
            return true;
        }
    }

    for (size_t index = 0;
         index < analyzer->environment->global_count;
         index += 1) {
        if (span_equals_text(
                analyzer->source,
                name,
                analyzer->environment->globals[index].name
            )) {
            return true;
        }
    }

    for (size_t index = 0;
         index < analyzer->environment->builtin_count;
         index += 1) {
        if (signature_name_matches(
                analyzer,
                name,
                &analyzer->environment->builtins[index]
            )) {
            return true;
        }
    }

    return false;
}

static bool analyze_bindings(Analyzer *analyzer)
{
    for (size_t index = 0;
         index < analyzer->program->binding_count;
         index += 1) {
        const Binding *binding =
            &analyzer->program->bindings[index];

        if (binding_name_is_reserved(
                analyzer,
                binding->name
            )) {
            analyzer_fail(
                analyzer,
                binding->name,
                "binding name is reserved"
            );
            return false;
        }

        for (size_t previous = 0;
             previous < index;
             previous += 1) {
            if (spans_equal(
                    analyzer->source,
                    binding->name,
                    analyzer->program->bindings[previous].name
                )) {
                analyzer_fail(
                    analyzer,
                    binding->name,
                    "duplicate binding"
                );
                return false;
            }
        }

        if (!infer_expression(
                analyzer,
                binding->initializer,
                index,
                &analyzer->info->binding_types[index]
            )) {
            return false;
        }
    }

    return true;
}

bool semantic_analyze(
    const Program *program,
    const char *source,
    const LanguageEnvironment *environment,
    SemanticInfo *out_info,
    SemanticError *out_error
)
{
    if (out_info == NULL || out_error == NULL) {
        return false;
    }

    *out_info = (SemanticInfo){0};
    *out_error = (SemanticError){0};

    if (program == NULL
        || program->result == NULL
        || source == NULL
        || environment == NULL) {
        out_error->message = "invalid semantic input";
        return false;
    }

    if (program->binding_count > 0) {
        if (program->binding_count
            > SIZE_MAX / sizeof(*out_info->binding_types)) {
            out_error->message = "out of memory";
            return false;
        }

        out_info->binding_types = calloc(
            program->binding_count,
            sizeof(*out_info->binding_types)
        );

        if (out_info->binding_types == NULL) {
            out_error->message = "out of memory";
            return false;
        }
    }

    out_info->binding_count = program->binding_count;

    Analyzer analyzer = {
        .program = program,
        .source = source,
        .environment = environment,
        .info = out_info,
        .error = out_error,
    };

    if (!analyze_bindings(&analyzer)
        || !infer_expression(
            &analyzer,
            program->result,
            program->binding_count,
            &out_info->result_type
        )) {
        semantic_info_destroy(out_info);
        return false;
    }

    return true;
}

void semantic_info_destroy(SemanticInfo *info)
{
    if (info == NULL) {
        return;
    }

    free(info->binding_types);
    *info = (SemanticInfo){0};
}
