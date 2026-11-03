#ifndef GRAFIC_AST_DUMP_H
#define GRAFIC_AST_DUMP_H

#include "grafic/ast.h"

#include <stdbool.h>
#include <stdio.h>

bool ast_dump(
    FILE *output,
    const Expr *expression,
    const char *source
);

#endif
