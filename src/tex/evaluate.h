#ifndef FUSE_TEX_EVALUATE_H
#define FUSE_TEX_EVALUATE_H

#include "fuse/ast.h"
#include "fuse/value_type.h"

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

bool fusetex_evaluate_program(
    const Program *program,
    const char *source,
    const EvalContext *context,
    RuntimeValue *out_value,
    EvalError *out_error
);

#endif
