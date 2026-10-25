#ifndef FUSEGL_GLSL_EMIT_H
#define FUSEGL_GLSL_EMIT_H

#include "fuse/ast.h"
#include "fuse/semantic.h"

#include <stdbool.h>
#include <stdio.h>

/*
 * Emits a complete GLSL 330 fragment shader. Float and vector results are
 * converted to RGBA so compiler preview programs can be displayed.
 *
 * The program must already be lowered and semantically analyzed.
 */
bool glsl_emit(
    FILE *output,
    const Program *program,
    const SemanticInfo *semantic_info,
    const char *source
);

#endif
