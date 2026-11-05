# Developing Grafic

## Responsibilities

| Location | Responsibility |
| --- | --- |
| `include/grafic/` | Shared language and tooling contracts |
| `src/lang/` | Lexer, AST, parser, lowering, type checking, source selection, help |
| `src/lsp/` | Shared language server and private JSON support |
| `src/cli/` | Command dispatch and enabled capability registration |
| `src/gl/` | Shader catalog, GLSL implementation, and command |
| `src/tex/` | Texture catalog, evaluator, image storage, PNG output, and command |
| `src/mesh/` | Geometry catalog, evaluator, mesh storage, OBJ output, and command |
| `src/view/` | Source polling, shader preview, GLFW window, and GPU resources |
| `tests/lang/` | Shared tests using small fixture catalogs |
| `tests/{gl,tex,mesh}/` | Capability behavior and command integration tests |
| `tests/view/` | Source lifecycle, viewer arguments, rendered pixels, and live reload |
| `tree-sitter-grafic/` | Grammar, generic queries, generated parser, corpus tests |
| `editors/nvim/` | File associations and compiler/LSP command selection |

Capabilities depend on the frontend and shared tooling. The shared layer does
not import capability implementations or enumerate their commands. There is
one source copy and one shared static library, with no vendored self-dependency.
The root Makefile builds one executable with all or selected capabilities.

## Catalog and dialect contract

`LanguageEnvironment` describes builtin signatures, global types, symbol
documentation, and reserved names. Each capability owns these tables together
with the implementations of its functions. Its `GraficDialect` supplies the
canonical language ID, environment, and required complete-program result type.

The same descriptor drives compilation, builtin help, and the integrated LSP.
Shader and texture programs require `vec4`; geometry programs require `mesh`.
The general type checker infers the actual result type independently. A
compiler's intermediate-preview path applies its own output rules.

Keep `ValueType` as the current small shared type vocabulary. Shared code
compares and names texture and mesh types; their representations and execution
belong to the capabilities. Add extensibility only when requirements justify it.

## Shared C API

`make language` builds `build/libgrafic-lang.a`. Public headers are included as
`grafic/parser.h`, for example. A consumer links against the library:

```sh
cc -std=c17 -Iinclude consumer.c build/libgrafic-lang.a -o consumer
```

`prepare_program` in `grafic/compile.h` performs parsing, optional one-based
preview selection, lowering, type checking, and result validation against a
dialect. It returns an owned `PreparedProgram` containing the lowered AST and
semantic information. Initialize it to zero and release it with
`prepared_program_destroy`. Success replaces a previous result; failure
preserves it. The caller keeps the source buffer alive. Compiler commands and
the viewer use this pipeline; `prepare_view_program` adds the policy of selecting
the last `show()` when no explicit line is given. Evaluation and emission remain
with each capability.

The source buffer remains alive while its ASTs are used: names and numbers
refer to source spans. Selection and lowering each return a separately owned
AST and do not consume their input. Release ASTs and semantic information
with their corresponding destroy functions.

`grafic_lsp_run` borrows a list of dialect descriptors and the caller's streams.
Each dialect supplies a unique language ID and filename extension. On document
open, a recognized language ID takes precedence over the URI extension; the
selected capability stays with that document across changes. Unknown types
receive a diagnostic and no completion or hover results. One process serves
mixed shader, texture, and geometry documents.

The server leaves streams open, accepts an optional log stream, and writes
only protocol data to its output stream. It uses UTF-16 positions and full
updates. Return status is nonzero for invalid configuration, premature exit,
transport failure, or an unrecoverable server error; clean EOF and shutdown/exit
return zero.

## Verification

`make test` runs the shared native tests, capability native tests, and each
command's integration tests, plus source installer tests using a local archive.
Focused targets run only their own dependencies;
`make test-lang` needs neither Python nor libpng.

```sh
make test-lang
make test-gl
make test-tex
make test-mesh
make test-view
make test-cli
make test-install
make test-release
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

`make test-shaders` compiles every `.gfrag` and `.gvert` example and validates the output with
`glslangValidator` (provided by `glslang-tools` on Debian/Ubuntu). CI runs the
compiler, installer, grammar, and shader checks on pushes and pull requests.

`make test-view` checks source changes, atomic saves, deletion recovery,
arguments, texture/mesh generation, and show() selection without opening a window.
Rendered tests require a graphics display:

```sh
make test-view-render
xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 make test-view-render
```

These check pixel output, UVs, time, framebuffer resolution, resizing, and
preservation of the active program after shader compile/link failures. Texture
checks cover orientation, filtering, sampler slots, replacement, and a complete
composition. Mesh checks cover fitted UV colors, aspect ratio, wireframe, and
preservation after invalid uploads. Live tests exercise independent shader/texture
edits, standalone texture/mesh previews, startup error recovery, and file
deletion/recreation.
CI runs these under Xvfb and Mesa software rendering.

## Installation and releases

`make install` installs `grafic` and its man page. The source installer uses
this target after downloading a release archive. It defaults to the latest
published GitHub release and accepts `--version TAG` and `--prefix PATH`.
Installer tests use a local source archive and a substitute curl command;
they do not access GitHub or install outside a temporary directory.

For user-visible changes, add a [changelog fragment](../changes/README.md) to
the feature PR. `VERSION` records the project version; Tree-sitter metadata
uses the same version. Version numbers change during release preparation.

To prepare a release, authenticate GitHub CLI (`gh auth login`), start from a
clean `main` that matches `origin/main`, and run:

```sh
make release VERSION=0.2.0
```

This creates a local `release/v0.2.0` branch, updates the version, collects
pending fragments into a dated `CHANGELOG.md` entry, and removes those fragments.
It commits with your Git identity, pushes the branch, and opens a PR using your
GitHub login. Your checkout stays on the release branch so you can edit the
changelog and push adjustments. Only one release PR can be open at a time.
Review the version and notes before manually merging. If new
fragments land on `main` during review, update the release PR to include them;
release checks reject unconsumed fragments.

Merging a release PR starts **Publish release**, which runs the shared CI
checks against the exact merged commit. It then creates `v0.2.0`, publishes
the changelog entry as GitHub release notes, and tests installation from the
published source archive. GitHub supplies the archive; no binary artifacts
are needed. Retrying publication preserves an existing tag on the same commit
and an already published release. Tags on other commits are rejected.

Normal PR CI runs when the release PR is opened or updated. Publication uses
the repository token with write permission only in the publishing job. If
branch protection is enabled, require the CI `test` check before merging.

The installer's default requires a published release; `--version` can select
any existing version tag. Choose a version greater than the current `VERSION`.

The default package contains all capabilities. Source users can build a subset:

```sh
make CAPABILITIES=gl
make CAPABILITIES=gl PREFIX="$HOME/.local" install
```

This still produces `grafic`, with only `gl` and its LSP catalog. It needs neither
libpng nor pkg-config. Space-separated selections such as `CAPABILITIES="gl mesh"`
are supported; unknown or empty selections are rejected. Changing the selection
in the same build directory rebuilds the dispatcher. `make test` checks the enabled
capabilities; installer tests run by default only for a full build. The standard
source installer always builds the full package.

The viewer is included by default and requires `gl`, `tex`, and `mesh`.
Compiler-only selections continue to avoid GLFW and
OpenGL dependencies. See the [viewer guide](view.md).

`make install-library` installs the shared C library and headers separately.
`make install-grammar` installs only the grammar. `PREFIX`, `BINDIR`, `LIBDIR`,
`INCLUDEDIR`, `MANDIR`, `DATADIR`, and `DESTDIR` control installation paths.

The `gl` capability owns `ShaderStage` and stage-specific dialects. Vertex
and fragment shaders share expression emission and scalar math definitions;
each has its own globals, result contract, and entry point. Vertex previews
require a vertex result, with `vec3` allowed for inspection. `PreparedProgram`
retains optional complete-program context for position-only previews.
Mesh evaluation produces a tagged disk or grid specification; generators
produce the same XYZ/UV/index buffers used by OBJ export and the viewer.

## Grafic source layout

Write each chained operation on its own indented line, including chains in
bindings, so `.show()` can be inserted between steps:

```text
let brightness = diffuse
  .multiply(0.8)
  .add(0.2);
```

Keep this layout in future formatter work. Short nested arguments can stay
inline; the outer chain should remain easy to inspect one operation at a time.

Field access is an AST expression distinct from chained calls. Capability-owned
`FieldSignature` entries drive type checking, help, completion, and hover. The
shader capability defines the vertex, surface, and screen interfaces; the shared
language does not construct arbitrary records. Authored normals use a separate
interpolated varying and a flat presence flag. Vertex-preview context is prepared
without selecting markers and is owned by `PreparedProgram`.
