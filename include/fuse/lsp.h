#ifndef FUSE_LSP_H
#define FUSE_LSP_H

#include "fuse/dialect.h"

#include <stddef.h>
#include <stdio.h>

/*
 * Runs a language server over JSON-RPC. The descriptors and their
 * referenced storage must remain alive until return. Streams remain open.
 * input/output are required; log may be NULL. Only protocol data goes to output.
 * At least one dialect is required, with unique language IDs and extensions.
 * A recognized language ID selects the dialect on document open; otherwise
 * the URI extension is used. Selection is preserved across document changes.
 * Unknown types receive a diagnostic and no completion/hover results.
 *
 * Returns zero after shutdown/exit or clean EOF, and nonzero for invalid
 * configuration, premature exit, I/O errors, or unrecoverable server errors.
 */
int fuse_lsp_run(
    const FuseDialect *const *dialects,
    size_t dialect_count,
    FILE *input,
    FILE *output,
    FILE *log
);

#endif
