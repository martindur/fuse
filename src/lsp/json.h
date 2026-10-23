#ifndef FUSE_JSON_H
#define FUSE_JSON_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    FUSE_JSON_INVALID,
    FUSE_JSON_NULL,
    FUSE_JSON_BOOL,
    FUSE_JSON_NUMBER,
    FUSE_JSON_STRING,
    FUSE_JSON_OBJECT,
    FUSE_JSON_ARRAY,
} FuseJsonType;

typedef struct {
    const char *data;
    size_t length;
    size_t offset;
} FuseJsonValue;

bool fuse_json_parse(
    const char *data,
    size_t length,
    FuseJsonValue *out_value
);

FuseJsonType fuse_json_type(FuseJsonValue value);

bool fuse_json_object_get(
    FuseJsonValue object,
    const char *key,
    FuseJsonValue *out_value
);

bool fuse_json_array_first(
    FuseJsonValue array,
    FuseJsonValue *out_value
);

bool fuse_json_string_copy(
    FuseJsonValue value,
    char **out_string
);

#endif
