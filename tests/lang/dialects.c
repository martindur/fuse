#include "dialects.h"

static const BuiltinSignature POINT_BUILTINS[] = {
    {"shift", VALUE_TYPE_VEC2, 2, {VALUE_TYPE_VEC2, VALUE_TYPE_FLOAT}},
    {"magnitude", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_VEC2}},
    {"scale", VALUE_TYPE_FLOAT, 2, {VALUE_TYPE_FLOAT, VALUE_TYPE_FLOAT}},
    {"scale", VALUE_TYPE_VEC2, 2, {VALUE_TYPE_VEC2, VALUE_TYPE_FLOAT}},
    {"finish", VALUE_TYPE_VEC4, 1, {VALUE_TYPE_FLOAT}},
};

static const GlobalSignature POINT_GLOBALS[] = {
    {"point", VALUE_TYPE_VEC2},
};

static const SymbolDocumentation POINT_DOCUMENTATION[] = {
    {"shift", "Offsets a test point by the supplied amount."},
    {"point", "The input point for this test dialect."},
};

static const char *const POINT_RESERVED[] = {"internal"};

static const LanguageEnvironment POINT_ENVIRONMENT = {
    .builtins = POINT_BUILTINS,
    .builtin_count = sizeof(POINT_BUILTINS) / sizeof(POINT_BUILTINS[0]),
    .globals = POINT_GLOBALS,
    .global_count = sizeof(POINT_GLOBALS) / sizeof(POINT_GLOBALS[0]),
    .documentation = POINT_DOCUMENTATION,
    .documentation_count = sizeof(POINT_DOCUMENTATION) / sizeof(POINT_DOCUMENTATION[0]),
    .reserved_names = POINT_RESERVED,
    .reserved_count = sizeof(POINT_RESERVED) / sizeof(POINT_RESERVED[0]),
};

const FuseDialect TEST_POINT_DIALECT = {
    .language_id = "test-point",
    .file_extension = ".point",
    .environment = &POINT_ENVIRONMENT,
    .required_result_type = VALUE_TYPE_VEC4,
};

static const BuiltinSignature OBJECT_BUILTINS[] = {
    {"object", VALUE_TYPE_MESH, 0, {0}},
    {"resize", VALUE_TYPE_MESH, 2, {VALUE_TYPE_MESH, VALUE_TYPE_FLOAT}},
    {"measure", VALUE_TYPE_FLOAT, 1, {VALUE_TYPE_MESH}},
};

static const SymbolDocumentation OBJECT_DOCUMENTATION[] = {
    {"resize", "Resizes the test object."},
};

static const LanguageEnvironment OBJECT_ENVIRONMENT = {
    .builtins = OBJECT_BUILTINS,
    .builtin_count = sizeof(OBJECT_BUILTINS) / sizeof(OBJECT_BUILTINS[0]),
    .documentation = OBJECT_DOCUMENTATION,
    .documentation_count = sizeof(OBJECT_DOCUMENTATION) / sizeof(OBJECT_DOCUMENTATION[0]),
};

const FuseDialect TEST_OBJECT_DIALECT = {
    .language_id = "test-object",
    .file_extension = ".object",
    .environment = &OBJECT_ENVIRONMENT,
    .required_result_type = VALUE_TYPE_MESH,
};
