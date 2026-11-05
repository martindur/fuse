# Grafic

Grafic is a language and toolchain for composing shaders, textures, and geometry
from small chained transformations.

This project is in early development. The language, tools, and APIs may change
substantially between releases.

| Command | Input | Output |
| --- | --- | --- |
| `grafic gl` | Shader source (`.gfrag`, `.gvert`) | GLSL 330 fragment or vertex shader |
| `grafic tex` | Texture source (`.gtex`) | PNG image |
| `grafic mesh` | Geometry source (`.gmesh`) | Wavefront OBJ mesh |
| `grafic view` | Shader (`.gfrag`, `.gvert`), texture (`.gtex`), or mesh (`.gmesh`) file | Live preview window |

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

The installer also requires curl and tar. It installs `grafic` and its man page
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
build/grafic gl < examples/gl/ring.gfrag > build/ring.frag
build/grafic gl --stage vertex < examples/waves/waves.gvert > build/waves.vert
build/grafic tex --width 256 --height 256 --seed 42 \
  < examples/tex/clouds.gtex > build/clouds.png
build/grafic mesh < examples/mesh/radial.gmesh > build/radial.obj
```

Preview Grafic source files with live reload:

```sh
build/grafic view examples/gl/ring.gfrag
build/grafic view examples/waves/waves.gmesh \
  --vert examples/waves/waves.gvert --frag examples/waves/waves.gfrag
build/grafic view examples/tex/clouds.gtex
build/grafic view examples/mesh/radial.gmesh
```

Save edits to update the image. Space pauses animation; Escape closes the
window. Errors appear in the terminal while the last working shader keeps
running. Bind procedural textures with `--tex0 FILE.gtex` through `--tex3 FILE.gtex`;
each input reloads independently. Press `1` for the shader or `2`–`5` for texture
inputs. In a 3D preview, `W` toggles wireframe and `U` toggles UV colors.
Vertex sources preview on a subdivided grid; use `--mesh`, `--vert`, and `--frag`
to combine geometry and shaders.
Add `.show()` to preview an intermediate value in any source.
See the [viewer guide](docs/view.md) for examples.

Programs chain transformations. For example, this shader draws an orange circle:

```text
surface.uv
  .center()
  .aspect()
  .circle(0.25)
  .fill()
  .colorize(rgb(1.0, 0.4, 0.1))
```

See the [language reference](docs/language.md),
[capability guide](docs/capabilities.md), and [runnable examples](examples/README.md).

## Editors

Configure your editor to run `grafic lsp` for `.gfrag`, `.gvert`, `.gtex`, and `.gmesh` files.
One server provides diagnostics, completion, and hover documentation for all source types.

For Neovim, see the [setup guide](editors/nvim/README.md) to install and enable
the optional LSP integration and syntax highlighting.

## Development

```sh
make test
```

Tests also require Python 3. See [development](docs/development.md) for focused
test targets, architecture, the C API, and installation options.
