#ifndef FUSE_SEMANTIC_H
#define FUSE_SEMANTIC_H

#include "fuse/ast.h"
#include "fuse/builtins.h"
#include "fuse/value_type.h"

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
