#ifndef FUSE_LSP_H
#define FUSE_LSP_H

#include "fuse/dialect.h"

#include <stdio.h>

/*
 * Runs a single-dialect language server over JSON-RPC. The descriptor and its
 * referenced storage must remain alive until return. Streams remain open.
 * input/output are required; log may be NULL. Only protocol data goes to output.
 *
 * Returns zero after shutdown/exit or clean EOF, and nonzero for invalid
 * configuration, premature exit, I/O errors, or unrecoverable server errors.
 */
int fuse_lsp_run(
    const FuseDialect *dialect,
    FILE *input,
    FILE *output,
    FILE *log
);

#endif
