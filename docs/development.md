# Developing Fuse

## Responsibilities

| Location | Responsibility |
| --- | --- |
| `include/fuse/` | Shared language and tooling contracts |
| `src/lang/` | Lexer, AST, parser, lowering, type checking, source selection, help |
| `src/lsp/` | Shared language server and private JSON support |
| `src/gl/` | Shader catalog, GLSL implementation, and command |
| `src/tex/` | Texture catalog, evaluator, image storage, PNG output, and command |
| `src/mesh/` | Geometry catalog, evaluator, mesh storage, OBJ output, and command |
| `tests/lang/` | Shared tests using small fixture catalogs |
| `tests/{gl,tex,mesh}/` | Capability behavior and command integration tests |
| `tree-sitter-fuse/` | Grammar, generic queries, generated parser, corpus tests |
| `editors/nvim/` | File associations and compiler/LSP command selection |

Capabilities depend on the frontend and shared tooling. The shared layer does
not import capability implementations or enumerate their commands. There is
one source copy and one shared static library, with no vendored self-dependency.
The root Makefile builds all commands or a selected capability.

## Catalog and dialect contract

`LanguageEnvironment` describes builtin signatures, global types, symbol
documentation, and reserved names. Each capability owns these tables together
with the implementations of its functions. Its `FuseDialect` supplies the
canonical language ID, environment, and required complete-program result type.

The same descriptor drives compilation, builtin help, and the integrated LSP.
Shader and texture programs require `vec4`; geometry programs require `mesh`.
The general type checker infers the actual result type independently. A
compiler's intermediate-preview path applies its own output rules.

Keep `ValueType` as the current small shared type vocabulary. Shared code
compares and names texture and mesh types; their representations and execution
belong to the capabilities. Add extensibility only when requirements justify it.

## Shared C API

`make language` builds `build/libfuse-lang.a`. Public headers are included as
`fuse/parser.h`, for example. A consumer links against the library:

```sh
cc -std=c17 -Iinclude consumer.c build/libfuse-lang.a -o consumer
```

The compilation pipeline is `parse_program`, optional
`preview_program_at_line`, `lower_program`, then `semantic_analyze` with a
supplied environment. Evaluation or emission belongs to the capability.

The source buffer remains alive while its ASTs are used: names and numbers
refer to source spans. Selection and lowering each return a separately owned
AST and do not consume their input. Release ASTs and semantic information
with their corresponding destroy functions.

`fuse_lsp_run` borrows a descriptor and the caller's streams. One server
process serves one capability, selected by the command. Filename extensions
and incoming document language IDs do not switch catalogs. The server leaves
streams open, accepts an optional log stream, and writes only protocol data to
its output stream. It uses UTF-16 positions and full document updates. Return
status is nonzero for invalid configuration, premature exit, transport failure,
or an unrecoverable server error; clean EOF and shutdown/exit return zero.

## Verification

`make test` runs the shared native tests, capability native tests, and each
command's integration tests. Focused targets run only their own dependencies;
`make test-lang` needs neither Python nor libpng.

```sh
make test-lang
make test-fusegl
make test-fusetex
make test-fusemesh
```

```sh
make BUILD_DIR=build/sanitize \
  SANITIZERS='-fsanitize=address,undefined -fno-omit-frame-pointer' test
```

Use a separate build directory when changing compiler or instrumentation
flags. Header dependencies are tracked automatically.

Tree-sitter CLI is only needed for grammar generation and corpus tests:

```sh
make tree-sitter-generate
make tree-sitter-test
make tree-sitter-parser
```

Native grammar builds use the checked-in generated C. Reusable queries express
syntax and bindings; capability symbol knowledge lives in the catalogs.

Install individual commands with `make install-fusegl`, `make install-fusetex`,
or `make install-fusemesh`. `make install-library` installs the shared C library
and headers.
`make install-grammar` installs only the grammar. `PREFIX`, `BINDIR`, `LIBDIR`,
`INCLUDEDIR`, `MANDIR`, `DATADIR`, and `DESTDIR` control installation paths.
