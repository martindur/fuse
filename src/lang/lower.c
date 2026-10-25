#include "fuse/lower.h"

#include <stddef.h>

typedef struct {
    LowerError *error;
} Lowerer;

static Expr *lower_node(
    Lowerer *lowerer,
    const Expr *syntax
);

static void lowerer_fail(
    Lowerer *lowerer,
    SourceSpan span,
    const char *message
)
{
    if (lowerer->error->message != NULL) {
        return;
    }

    lowerer->error->span = span;
    lowerer->error->message = message;
}

static bool lower_arguments(
    Lowerer *lowerer,
    Expr *target_call,
    const ExprList *syntax_arguments
)
{
    for (size_t index = 0;
         index < syntax_arguments->count;
         index += 1) {
        Expr *argument = lower_node(
            lowerer,
            syntax_arguments->items[index]
        );

        if (argument == NULL) {
            return false;
        }

        if (!expr_call_add_argument(target_call, argument)) {
            /*
             * Ownership did not transfer because adding failed.
             * The local variable still owns `argument`.
             */
            expr_destroy(argument);
            lowerer_fail(
                lowerer,
                target_call->span,
                "out of memory"
            );
            return false;
        }
    }

    return true;
}

static Expr *lower_call(
    Lowerer *lowerer,
    const Expr *syntax
)
{
    Expr *call = expr_create_call(
        syntax->span,
        syntax->as.call.name
    );

    if (call == NULL) {
        lowerer_fail(
            lowerer,
            syntax->span,
            "out of memory"
        );
        return NULL;
    }

    if (!lower_arguments(
            lowerer,
            call,
            &syntax->as.call.arguments
        )) {
        expr_destroy(call);
        return NULL;
    }

    return call;
}

static Expr *lower_pipeline(
    Lowerer *lowerer,
    const Expr *syntax
)
{
    const Expr *stage = syntax->as.pipeline.stage;

    if (stage == NULL || stage->kind != EXPR_CALL) {
        lowerer_fail(
            lowerer,
            syntax->span,
            "pipeline stage is not a call"
        );
        return NULL;
    }

    Expr *input = lower_node(
        lowerer,
        syntax->as.pipeline.input
    );

    if (input == NULL) {
        return NULL;
    }

    Expr *call = expr_create_call(
        syntax->span,
        stage->as.call.name
    );

    if (call == NULL) {
        expr_destroy(input);
        lowerer_fail(
            lowerer,
            syntax->span,
            "out of memory"
        );
        return NULL;
    }

    if (!expr_call_add_argument(call, input)) {
        /*
         * Adding failed, so ownership of input did not transfer.
         * Both allocations still have to be destroyed separately.
         */
        expr_destroy(input);
        expr_destroy(call);
        lowerer_fail(
            lowerer,
            syntax->span,
            "out of memory"
        );
        return NULL;
    }

    /*
     * The call now owns input. If lowering a later argument fails,
     * destroying call recursively destroys input as well.
     */
    if (!lower_arguments(
            lowerer,
            call,
            &stage->as.call.arguments
        )) {
        expr_destroy(call);
        return NULL;
    }

    return call;
}

static Expr *lower_node(
    Lowerer *lowerer,
    const Expr *syntax
)
{
    if (syntax == NULL) {
        lowerer_fail(
            lowerer,
            (SourceSpan){0},
            "syntax expression is NULL"
        );
        return NULL;
    }

    Expr *lowered = NULL;

    switch (syntax->kind) {
    case EXPR_IDENTIFIER:
        lowered = expr_create_identifier(syntax->span);
        break;

    case EXPR_NUMBER:
        lowered = expr_create_number(syntax->span);
        break;

    case EXPR_CALL:
        return lower_call(lowerer, syntax);

    case EXPR_PIPELINE:
        return lower_pipeline(lowerer, syntax);
    }

    if (lowered == NULL) {
        lowerer_fail(
            lowerer,
            syntax->span,
            "out of memory"
        );
    }

    return lowered;
}

bool lower_expression(
    const Expr *syntax,
    Expr **out_lowered,
    LowerError *out_error
)
{
    if (out_lowered == NULL || out_error == NULL) {
        return false;
    }

    *out_lowered = NULL;
    *out_error = (LowerError){0};

    if (syntax == NULL) {
        out_error->message = "syntax expression is NULL";
        return false;
    }

    Lowerer lowerer = {
        .error = out_error,
    };

    Expr *lowered = lower_node(&lowerer, syntax);

    if (lowered == NULL) {
        return false;
    }

  *out_lowered = lowered;
  return true;
}

bool lower_program(
    const Program *syntax,
    Program **out_lowered,
    LowerError *out_error
) {
  if (out_lowered == NULL || out_error == NULL) {
    return false;
  }

  *out_lowered = NULL;
  *out_error = (LowerError){0};

  if (syntax == NULL || syntax->result == NULL) {
    out_error->message = "syntax program is incomplete";
    return false;
  }

  Lowerer lowerer = {
    .error = out_error,
  };

  Program *lowered = program_create();

  if (lowered == NULL) {
    out_error->message = "out of memory";
    return false;
  }

  for (size_t index = 0;
      index < syntax->binding_count;
      index += 1) {
    const Binding *binding = &syntax->bindings[index];
    Expr *initializer = lower_node(
        &lowerer,
        binding->initializer
    );

    if (initializer == NULL) {
      program_destroy(lowered);
      return false;
    }

    if (!program_add_binding(
          lowered,
          binding->name,
          initializer
        )) {
      expr_destroy(initializer);
      program_destroy(lowered);
      lowerer_fail(
          &lowerer,
          binding->name,
          "out of memory"
      );
      return false;
    }
  }

  Expr *result = lower_node(&lowerer, syntax->result);

  if (result == NULL) {
    program_destroy(lowered);
    return false;
  }

  if (!program_set_result(lowered, result)) {
    expr_destroy(result);
    program_destroy(lowered);
    lowerer_fail(
        &lowerer,
        syntax->result->span,
        "out of memory"
    );
    return false;
  }

  *out_lowered = lowered;
  return true;
}
