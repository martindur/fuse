#ifndef GRAFIC_HELP_H
#define GRAFIC_HELP_H

#include "grafic/builtins.h"

#include <stdio.h>

/*
 * Prints the language's public functions and globals in a form suitable for
 * terminal help output. The language environment is the same catalog used by
 * semantic analysis and LSP hover responses.
 */
void language_print_builtins(
    FILE *output,
    const LanguageEnvironment *environment
);

#endif
