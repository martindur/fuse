#ifndef GRAFICGL_GLSL_STDLIB_H
#define GRAFICGL_GLSL_STDLIB_H

#include "grafic/dialect.h"
#include "shader.h"

#include <stdbool.h>
#include <stdio.h>

bool glsl_stdlib_emit(FILE *output, ShaderStage stage);
bool glsl_math_emit(FILE *output);
bool glsl_vertex_stdlib_emit(FILE *output);
const GraficDialect *gl_vertex_dialect(void);

const GraficDialect *gl_dialect(ShaderStage stage);

#endif
