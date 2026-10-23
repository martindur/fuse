#ifndef FUSE_AST_DUMP_H
#define FUSE_AST_DUMP_H

#include "fuse/ast.h"

#include <stdbool.h>
#include <stdio.h>

bool ast_dump(
    FILE *output,
    const Expr *expression,
    const char *source
);

#endif
