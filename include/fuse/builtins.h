#ifndef FUSE_BUILTINS_H
#define FUSE_BUILTINS_H

#include "fuse/value_type.h"

#include <stddef.h>

#define BUILTIN_MAX_PARAMETERS 4

typedef struct {
    const char *name;
    ValueType result;
    size_t parameter_count;
    ValueType parameters[BUILTIN_MAX_PARAMETERS];
} BuiltinSignature;

typedef struct {
    const char *name;
    ValueType type;
} GlobalSignature;

typedef struct {
    const char *name;
    const char *documentation;
} SymbolDocumentation;

typedef struct {
    const BuiltinSignature *builtins;
    size_t builtin_count;
    const GlobalSignature *globals;
    size_t global_count;
    const SymbolDocumentation *documentation;
    size_t documentation_count;
    const char *const *reserved_names;
    size_t reserved_count;
} LanguageEnvironment;

#endif
