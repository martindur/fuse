#ifndef GRAFICGL_GLSL_STDLIB_H
#define GRAFICGL_GLSL_STDLIB_H

#include "grafic/dialect.h"

#include <stdbool.h>
#include <stdio.h>

bool glsl_stdlib_emit(FILE *output);

const GraficDialect *graficgl_dialect(void);

#endif
