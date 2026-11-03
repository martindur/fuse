#ifndef GRAFIC_SEMANTIC_H
#define GRAFIC_SEMANTIC_H

#include "grafic/ast.h"
#include "grafic/builtins.h"
#include "grafic/value_type.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    SourceSpan span;
    const char *message;
} SemanticError;

typedef struct {
    ValueType *binding_types;
    size_t binding_count;
    ValueType result_type;
} SemanticInfo;

bool semantic_analyze(
    const Program *program,
    const char *source,
    const LanguageEnvironment *environment,
    SemanticInfo *out_info,
    SemanticError *out_error
);

void semantic_info_destroy(SemanticInfo *info);

#endif
