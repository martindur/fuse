#ifndef GRAFICGL_GLSL_EMIT_H
#define GRAFICGL_GLSL_EMIT_H

#include "grafic/ast.h"
#include "shader.h"
#include "grafic/semantic.h"

#include <stdbool.h>
#include <stdio.h>

/*
 * Emits a complete GLSL 330 shader. Vertex results must be local-space vec3
 * positions; fragment preview results are converted from numeric values to RGBA.
 *
 * The program must already be lowered and semantically analyzed.
 */
bool glsl_emit(
    FILE *output,
    ShaderStage stage,
    const Program *program,
    const SemanticInfo *semantic_info,
    const char *source
);

#endif
