# Fuse

Fuse is a language and toolchain for composing shaders, textures, and geometry
from small chained transformations.

| Command | Input | Output |
| --- | --- | --- |
| `fuse gl` | Shader source (`.pgl`) | GLSL 330 fragment shader |
| `fuse tex` | Texture source (`.ptex`) | PNG image |
| `fuse mesh` | Geometry source (`.pmesh`) | Wavefront OBJ mesh |
| `fuse view` | Shader file (`.pgl`) | Live preview window |

Compiler commands read source from stdin, write output to stdout, and report
errors on stderr. Use `--builtins` to list its functions and globals.

## Install

Requires Make, a C17 compiler, an archiver, and a POSIX environment.
The default build also requires `pkg-config`, libpng, GLFW 3, and OpenGL
development files. Viewing requires an OpenGL 3.3-capable display.

Install the latest release from source:

```sh
curl -fsSL https://raw.githubusercontent.com/martindur/fuse/main/install.sh | sh
```

The installer also requires curl and tar. It installs `fuse` and its man page
under `~/.local`; add `~/.local/bin` to your `PATH`.
To choose a release or destination, pass `--version v0.2.0` or `--prefix /path`
to `sh -s --` at the end of the command.

From a checkout:

```sh
make PREFIX="$HOME/.local" install
```

The C library and headers can be installed separately with `make install-library`.

## Build and try it

```sh
make
build/fuse gl < examples/gl/ring.pgl > build/ring.frag
build/fuse tex --width 256 --height 256 --seed 42 \
  < examples/tex/clouds.ptex > build/clouds.png
build/fuse mesh < examples/mesh/radial.pmesh > build/radial.obj
```

Preview a shader with live reload:

```sh
build/fuse view examples/gl/ring.pgl
```

Save edits to update the image. Space pauses animation; Escape closes the
window. Errors appear in the terminal while the last working shader keeps
running. See the [viewer guide](docs/view.md) for options.

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

## Editors

Configure your editor to run `fuse lsp` for `.pgl`, `.ptex`, and `.pmesh` files.
One server provides diagnostics, completion, and hover documentation for all three.

For Neovim, see the [setup guide](editors/nvim/README.md) to install and enable
the optional LSP integration and syntax highlighting.

## Development

```sh
make test
```

Tests also require Python 3. See [development](docs/development.md) for focused
test targets, architecture, the C API, and installation options.
