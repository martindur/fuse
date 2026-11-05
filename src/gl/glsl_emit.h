#ifndef GRAFICGL_GLSL_EMIT_H
#define GRAFICGL_GLSL_EMIT_H

#include "grafic/ast.h"
#include "shader.h"
#include "grafic/compile.h"

#include <stdbool.h>
#include <stdio.h>

/*
 * Emits a complete GLSL 330 shader. Vertex results carry local position, UV,
 * and an optional normal; fragment preview results are converted from numeric values to RGBA.
 *
 * The program must already be lowered and semantically analyzed.
 */
bool glsl_emit(
    FILE *output,
    ShaderStage stage,
    const PreparedProgram *prepared,
    const char *source
);

#endif
