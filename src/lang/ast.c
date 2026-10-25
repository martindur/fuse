#include "fuse/ast.h"

#include <stdint.h>
#include <stdlib.h>

static Expr *expr_allocate(ExprKind kind, SourceSpan span) {
  Expr *expression = malloc(sizeof(*expression));

  if (expression == NULL) {
    return NULL;
  }

  *expression = (Expr){
    .kind = kind,
    .span = span,
  };

  return expression;
}

Expr *expr_create_identifier(SourceSpan span) {
  return expr_allocate(EXPR_IDENTIFIER, span);
}

Expr *expr_create_number(SourceSpan span) {
  return expr_allocate(EXPR_NUMBER, span);
}

Expr *expr_create_call(SourceSpan span, SourceSpan name) {
  Expr *expression = expr_allocate(EXPR_CALL, span);

  if (expression == NULL) {
    return NULL;
  }

  expression->as.call.name = name;
  return expression;
}

Expr *expr_create_pipeline(SourceSpan span, Expr *input, Expr *stage) {
  if (input == NULL
      || stage == NULL
      || stage->kind != EXPR_CALL) {
    return NULL;
  }

  Expr *expression = expr_allocate(EXPR_PIPELINE, span);

  if (expression == NULL) {
    return NULL;
  }

  expression->as.pipeline.input = input;
  expression->as.pipeline.stage = stage;

  return expression;
}

bool expr_call_add_argument(Expr *call, Expr *argument) {
  if (call == NULL
      || call->kind != EXPR_CALL
      || argument == NULL) {
    return false;
  }

  ExprList *arguments = &call->as.call.arguments;

  if (arguments->count == arguments->capacity) {
    size_t new_capacity;

    if (arguments->capacity == 0) {
      new_capacity = 4;
    } else {
      if (arguments->capacity > SIZE_MAX / 2) {
        return false;
      }

      new_capacity = arguments->capacity * 2;
    }

    if (new_capacity > SIZE_MAX / sizeof(*arguments->items)) {
      return false;
    }

    Expr **new_items = realloc(
        arguments->items,
        new_capacity * sizeof(*arguments->items)
    );

    if (new_items == NULL) {
      return false;
    }

    arguments->items = new_items;
    arguments->capacity = new_capacity;
  }

  arguments->items[arguments->count] = argument;
  arguments->count += 1;

  return true;
}

void expr_destroy(Expr *expression) {
  if (expression == NULL) {
    return;
  }

  switch (expression->kind) {
    case EXPR_IDENTIFIER:
    case EXPR_NUMBER:
      break;

    case EXPR_CALL:
      for (size_t index = 0;
          index < expression->as.call.arguments.count;
          index += 1) {
        expr_destroy(expression->as.call.arguments.items[index]);
      }

      free(expression->as.call.arguments.items);
      break;

    case EXPR_PIPELINE:
      expr_destroy(expression->as.pipeline.input);
      expr_destroy(expression->as.pipeline.stage);
      break;
  }

  free(expression);
}

Program *program_create(void) {
  return calloc(1, sizeof(Program));
}

bool program_add_binding(
    Program *program,
    SourceSpan name,
    Expr *initializer
) {
  if (program == NULL || initializer == NULL) {
    return false;
  }

  if (program->binding_count == program->binding_capacity) {
    size_t new_capacity;

    if (program->binding_capacity == 0) {
      new_capacity = 4;
    } else {
      if (program->binding_capacity > SIZE_MAX / 2) {
        return false;
      }

      new_capacity = program->binding_capacity * 2;
    }

    if (new_capacity > SIZE_MAX / sizeof(*program->bindings)) {
      return false;
    }

    Binding *new_bindings = realloc(
        program->bindings,
        new_capacity * sizeof(*program->bindings)
    );

    if (new_bindings == NULL) {
      return false;
    }

    program->bindings = new_bindings;
    program->binding_capacity = new_capacity;
  }

  program->bindings[program->binding_count] = (Binding){
    .name = name,
    .initializer = initializer,
  };
  program->binding_count += 1;

  return true;
}

bool program_set_result(
    Program *program,
    Expr *result
) {
  if (program == NULL
      || result == NULL
      || program->result != NULL) {
    return false;
  }

  program->result = result;
  return true;
}

void program_destroy(Program *program) {
  if (program == NULL) {
    return;
  }

  for (size_t index = 0;
      index < program->binding_count;
      index += 1) {
    expr_destroy(program->bindings[index].initializer);
  }

  free(program->bindings);
  expr_destroy(program->result);
  free(program);
}
