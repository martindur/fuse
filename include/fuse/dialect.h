#ifndef FUSE_DIALECT_H
#define FUSE_DIALECT_H

#include "fuse/builtins.h"

/* Capability-owned metadata. All referenced storage is immutable and borrowed. */
typedef struct {
    const char *language_id;
    const char *file_extension; /* Source suffix, including the leading dot. */
    const LanguageEnvironment *environment;
    ValueType required_result_type;
} FuseDialect;

#endif
