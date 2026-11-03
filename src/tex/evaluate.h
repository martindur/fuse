#ifndef GRAFIC_TEX_EVALUATE_H
#define GRAFIC_TEX_EVALUATE_H

#include "grafic/ast.h"
#include "grafic/value_type.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    ValueType type;
    float components[4];
} RuntimeValue;

typedef struct {
    float uv[2];
    float resolution[2];
    uint64_t seed;
} EvalContext;

typedef struct {
    SourceSpan span;
    const char *message;
} EvalError;

bool grafictex_evaluate_program(
    const Program *program,
    const char *source,
    const EvalContext *context,
    RuntimeValue *out_value,
    EvalError *out_error
);

#endif
