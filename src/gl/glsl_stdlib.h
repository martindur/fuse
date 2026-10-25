#ifndef FUSEGL_GLSL_STDLIB_H
#define FUSEGL_GLSL_STDLIB_H

#include "fuse/dialect.h"

#include <stdbool.h>
#include <stdio.h>

bool glsl_stdlib_emit(FILE *output);

const FuseDialect *fusegl_dialect(void);

#endif
