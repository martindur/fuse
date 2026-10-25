# Fuse

Fuse is a language and toolchain for composing shaders, textures, and geometry
from small chained transformations.

| Command | Input | Output |
| --- | --- | --- |
| `fusegl` | Shader source (`.pgl`) | GLSL 330 fragment shader |
| `fusetex` | Texture source (`.ptex`) | PNG image |
| `fusemesh` | Geometry source (`.pmesh`) | Wavefront OBJ mesh |

Each command is a filter: source on stdin, output on stdout, diagnostics on
stderr. Each also provides `--builtins` and an `lsp` subcommand backed by its
own capability catalog. The shared language implementation lives once in this
repository.

## Build and try it

Requirements are Make, a C17 compiler, an archiver, and POSIX facilities.
FuseTex additionally needs libpng and `pkg-config`. Windowing and graphics
libraries are not required to build the compilers.

```sh
make
build/fusegl < examples/gl/ring.pgl > build/ring.frag
build/fusetex --width 256 --height 256 --seed 42 \
  < examples/tex/clouds.ptex > build/clouds.png
build/fusemesh < examples/mesh/radial.pmesh > build/radial.obj
```

Build just one command with `make fusegl`, `make fusetex`, or `make fusemesh`.
A FuseGL-only build does not need libpng or `pkg-config`.

Programs share call and binding syntax; each capability defines its vocabulary:

```text
uv
  .center()
  .aspect()
  .circle(0.25)
  .fill()
  .colorize(rgb(1.0, 0.4, 0.1))
```

The example above is a shader. See the [language reference](docs/language.md),
[capability guide](docs/capabilities.md), and [runnable examples](examples/README.md).
The existing `.pgl`, `.ptex`, and `.pmesh` extensions remain supported.

## Install and editor support

```sh
make PREFIX="$HOME/.local" install
```

This installs the commands, man pages, and shared C library/headers. Add the
prefix's `bin` directory to `PATH`. Individual commands can be installed with
`install-fusegl`, `install-fusetex`, or `install-fusemesh`; `install-library`
installs only the shared library. `DESTDIR` supports staged packaging.

Any LSP-capable editor can start `fusegl lsp`, `fusetex lsp`, or `fusemesh lsp`
for the corresponding file type. The language servers provide diagnostics,
receiver-aware completion, and hover documentation from the same catalogs
used by compilation and builtin help.

```sh
make PREFIX="$HOME/.local" install-editor
```

This optional target installs the shared grammar and native Neovim runtime.
It builds the checked-in parser without needing Tree-sitter CLI. See
[Neovim setup](editors/nvim/README.md).

## Development

```sh
make test
make test-lang
make test-fusegl
make test-fusetex
make test-fusemesh
```

Command integration tests use Python 3's standard library. Core language tests
need only the native build tools. See [development](docs/development.md) for
module boundaries, the C API, instrumented builds, and grammar tests.

Fuse owns the language, its capabilities, and its authoring tools. The previous
workspace application, project configuration, watching, and GUI remain in the
legacy repository. Formatting is subsequent shared-tooling work. Extraction
provenance is recorded in [ORIGIN](ORIGIN).
