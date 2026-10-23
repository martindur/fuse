#ifndef FUSE_VALUE_TYPE_H
#define FUSE_VALUE_TYPE_H

#include <stdbool.h>

typedef enum {
    VALUE_TYPE_INVALID,
    VALUE_TYPE_FLOAT,
    VALUE_TYPE_VEC2,
    VALUE_TYPE_VEC3,
    VALUE_TYPE_VEC4,
    VALUE_TYPE_TEXTURE,
    VALUE_TYPE_MESH
} ValueType;

static inline bool value_type_is_previewable(ValueType type)
{
    return type == VALUE_TYPE_FLOAT
        || type == VALUE_TYPE_VEC2
        || type == VALUE_TYPE_VEC3
        || type == VALUE_TYPE_VEC4;
}

static inline const char *value_type_name(ValueType type)
{
    switch (type) {
    case VALUE_TYPE_INVALID:
        return "invalid";
    case VALUE_TYPE_FLOAT:
        return "float";
    case VALUE_TYPE_VEC2:
        return "vec2";
    case VALUE_TYPE_VEC3:
        return "vec3";
    case VALUE_TYPE_VEC4:
        return "vec4";
    case VALUE_TYPE_TEXTURE:
        return "texture";
    case VALUE_TYPE_MESH:
        return "mesh";
    }

    return "unknown";
}

#endif
