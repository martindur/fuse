#ifndef GRAFIC_DIALECT_H
#define GRAFIC_DIALECT_H

#include "grafic/builtins.h"
#include <stdbool.h>

/* Capability-owned metadata. All referenced storage is immutable and borrowed. */
typedef struct {
    const char *language_id;
    const char *file_extension; /* Source suffix, including the leading dot. */
    const LanguageEnvironment *environment;
    ValueType required_result_type;
    bool preview_requires_result_type; /* Disable general numeric previews. */
    ValueType preview_result_type; /* Optional alternate result, e.g. a vertex position. */
} GraficDialect;

#endif
