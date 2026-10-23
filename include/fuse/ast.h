#ifndef FUSE_AST_H
#define FUSE_AST_H

#include "fuse/source.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  EXPR_IDENTIFIER,
  EXPR_NUMBER,
  EXPR_CALL,
  EXPR_PIPELINE
} ExprKind;

typedef struct Expr Expr;

typedef struct {
  Expr **items;
  size_t count;
  size_t capacity;
} ExprList;

typedef struct {
  SourceSpan name;
  Expr *initializer;
} Binding;

typedef struct {
  Binding *bindings;
  size_t binding_count;
  size_t binding_capacity;
  Expr *result;
} Program;

struct Expr {
  ExprKind kind;
  SourceSpan span;

  union {
    struct {
      SourceSpan name;
      ExprList arguments;
    } call;

    struct {
      Expr *input;
      Expr *stage;
    } pipeline;
  } as;
};

Expr *expr_create_identifier(SourceSpan span);
Expr *expr_create_number(SourceSpan span);

Expr *expr_create_call(
    SourceSpan span,
    SourceSpan name
);

Expr *expr_create_pipeline(
    SourceSpan span,
    Expr *input,
    Expr *stage
);

bool expr_call_add_argument(
    Expr *call,
    Expr *argument
);

void expr_destroy(Expr *expression);

Program *program_create(void);

bool program_add_binding(
    Program *program,
    SourceSpan name,
    Expr *initializer
);

bool program_set_result(
    Program *program,
    Expr *result
);

void program_destroy(Program *program);

#endif
