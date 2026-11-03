#ifndef GRAFIC_JSON_H
#define GRAFIC_JSON_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    GRAFIC_JSON_INVALID,
    GRAFIC_JSON_NULL,
    GRAFIC_JSON_BOOL,
    GRAFIC_JSON_NUMBER,
    GRAFIC_JSON_STRING,
    GRAFIC_JSON_OBJECT,
    GRAFIC_JSON_ARRAY,
} GraficJsonType;

typedef struct {
    const char *data;
    size_t length;
    size_t offset;
} GraficJsonValue;

bool grafic_json_parse(
    const char *data,
    size_t length,
    GraficJsonValue *out_value
);

GraficJsonType grafic_json_type(GraficJsonValue value);

bool grafic_json_object_get(
    GraficJsonValue object,
    const char *key,
    GraficJsonValue *out_value
);

bool grafic_json_array_first(
    GraficJsonValue array,
    GraficJsonValue *out_value
);

bool grafic_json_string_copy(
    GraficJsonValue value,
    char **out_string
);

#endif
