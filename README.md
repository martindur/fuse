# Fuse

Fuse is the shared language foundation for products that turn chained
expressions into shaders, images, and geometry. It provides a C17 frontend,
catalog-driven type checking and authoring tools, and a Tree-sitter grammar.
Each product supplies its own functions, globals, documentation, and compiler.

```text
let p = point.shift(0.5);
p.magnitude().finish()
```

The syntax is shared. Names such as `point`, `shift`, and `finish` come from a
product's catalog. See [the language reference](docs/language.md).

## Build and test

The native library needs a C17 compiler, an archiver, Make, and POSIX facilities
including `getline`, `open_memstream`, and `strdup`. It has no graphics, image,
mesh, or TOML dependency. Builds use only files in this checkout.

```sh
make
make test
```

The output is `build/libfuse-lang.a`. Public headers are in `include/fuse`.
For an instrumented test build, use a separate build directory:

```sh
make BUILD_DIR=build/sanitize \
  SANITIZERS='-fsanitize=address,undefined -fno-omit-frame-pointer' test
```

The grammar is independently buildable from its checked-in generated parser.
Tree-sitter CLI is needed only for grammar generation and corpus tests:

```sh
make tree-sitter-parser
make tree-sitter-test
make tree-sitter-generate  # after changing grammar.js
```

## Product integration

Compile and link against the public include directory and static library:

```sh
cc -std=c17 -Ivendor/fuse/include main.c \
  vendor/fuse/build/libfuse-lang.a -o my-product
```

Products own their `LanguageEnvironment`: builtin signatures, global types,
documentation, and reserved names. The same catalog is used by the compiler,
builtin help, and LSP. Fuse has no product registry.

The compilation stages are:

1. `parse_program` builds the syntax AST from source.
2. Optionally, `preview_program_at_line` selects an intermediate expression.
3. `lower_program` converts chains into ordinary function calls.
4. `semantic_analyze` infers types using the product environment.
5. The product checks its result type, evaluates or emits the program, and
   releases the semantic information and owned ASTs.

The source buffer must remain alive while its ASTs are used: identifiers and
numbers refer to spans in the original source. Lowering and intermediate
selection each produce a separately owned AST. Their output does not replace
or consume the input AST.

To implement a product's `lsp` command, construct a borrowed descriptor at
command startup and call the shared server:

```c
#include "fuse/lsp.h"

/* Inside product command dispatch; product_environment() is product-owned. */
const FuseDialect dialect = {
    .language_id = "my-product",
    .environment = product_environment(),
    .required_result_type = VALUE_TYPE_VEC4,
};

return fuse_lsp_run(&dialect, stdin, stdout, stderr);
```

One server process serves the supplied dialect. Filename extensions and
document language IDs do not switch catalogs. Editors select the appropriate
product command for each file type. The shared server provides diagnostics,
receiver-aware completion, and hover documentation, using UTF-16 positions
and full document updates.

The descriptor and all catalog storage remain alive until the server returns.
The caller owns the streams; the server leaves them open. `log` may be NULL.
Return status is zero after clean EOF or shutdown followed by exit, and
nonzero for premature exit, invalid configuration, transport failure, or an
unrecoverable server error.

Keep the existing `ValueType` enum when supplying catalogs. The `texture` and
`mesh` variants are opaque to Fuse; storage, evaluation, and output belong to
the product. The normal result rule is part of `FuseDialect`; intermediate
output rules belong to each compiler.

## Installation and vendoring

```sh
make PREFIX="$HOME/.local" install
make PREFIX="$HOME/.local" install-grammar
```

The first command installs public headers and the static library. The optional
second command installs the grammar parser and queries under
`share/fuse/tree-sitter-fuse`. `DESTDIR` supports staged installation. See
[generic Neovim grammar setup](editors/nvim/README.md) for editor integration.

A product can instead check in a source snapshot under `vendor/fuse` and
record its upstream repository and exact commit in `vendor/fuse/UPSTREAM`.
Build the snapshot with `make -C vendor/fuse`, and include its public headers.
Make shared changes upstream, then update the pinned snapshot in each product.

Fuse currently ships a library and grammar. Formatting is planned subsequent
work. Product commands and the legacy MatterBox workspace live separately.
Extraction provenance is recorded in [ORIGIN](ORIGIN).
