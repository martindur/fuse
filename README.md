# Fuse

Fuse is a language and toolchain for composing shaders, textures, and geometry
from small chained transformations.

| Command | Input | Output |
| --- | --- | --- |
| `fusegl` | Shader source (`.pgl`) | GLSL 330 fragment shader |
| `fusetex` | Texture source (`.ptex`) | PNG image |
| `fusemesh` | Geometry source (`.pmesh`) | Wavefront OBJ mesh |

Each command reads source from stdin, writes output to stdout, and reports
errors on stderr. Use `--builtins` to list its functions and globals.

## Build and try it

Requires Make, a C17 compiler, an archiver, and a POSIX environment.
FuseTex also requires libpng and `pkg-config`.

```sh
make
build/fusegl < examples/gl/ring.pgl > build/ring.frag
build/fusetex --width 256 --height 256 --seed 42 \
  < examples/tex/clouds.ptex > build/clouds.png
build/fusemesh < examples/mesh/radial.pmesh > build/radial.obj
```

To build a single command, use `make fusegl`, `make fusetex`, or `make fusemesh`.
Shaders need a [rendering host](docs/shader-interface.md) to display them.

Programs chain transformations. For example, this shader draws an orange circle:

```text
uv
  .center()
  .aspect()
  .circle(0.25)
  .fill()
  .colorize(rgb(1.0, 0.4, 0.1))
```

See the [language reference](docs/language.md),
[capability guide](docs/capabilities.md), and [runnable examples](examples/README.md).

## Install

```sh
make PREFIX="$HOME/.local" install
```

Installs the three commands to `~/.local/bin`, plus man pages and the C library
and headers. Add `~/.local/bin` to your `PATH`.

## Editors

Configure your editor to run `fusegl lsp` for `.pgl`, `fusetex lsp` for `.ptex`,
or `fusemesh lsp` for `.pmesh`. Each provides diagnostics, completion, and hover
documentation.

For Neovim, see the [setup guide](editors/nvim/README.md) to install and enable
the optional LSP integration and syntax highlighting.

## Development

```sh
make test
```

Tests also require Python 3. See [development](docs/development.md) for focused
test targets, architecture, the C API, and installation options.
