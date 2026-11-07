# Language capabilities

All three commands share [Grafic syntax](language.md): bindings, regular calls,
and chains that pass the previous value as the next call's first argument.
Each capability provides its own functions, globals, execution, and output.
Run a command with `--builtins` for the authoritative signatures and docs.

## Shaders: grafic gl

```sh
grafic gl < examples/waves/waves.gfrag > waves.frag
grafic gl --stage vertex < examples/waves/waves.gvert > waves.vert
```

A complete fragment program returns `vec4`. The shader capability provides coordinates,
texture sampling, animation, shape masks, and color composition. Globals are
`surface` (position, UV, normal, and mask), `screen` (pixel position and normalized UV),
`view` (normalized local-space direction toward the camera),
`resolution`, `time`, and texture inputs `tex0` through `tex3`.

Use `--stage vertex` for a `.gvert` program returning
`vertex(position, uv)` or `vertex(position, uv, normal)`, with original mesh
`position`, `uv`, and `time` inputs. Authored normals are interpolated and
normalized for smooth shading; omission keeps triangle normals. The output is a complete
GLSL 330 core shader for the selected stage. Rendering requires a
host that supplies the documented [shader interface](shader-interface.md).

Chain `.mask(value)` onto a vertex result to pass an authored scalar material
mask to the fragment shader as `surface.mask`. It defaults to zero.

## Textures: grafic tex

```sh
grafic tex --width 256 --height 256 --seed 42 \
  < examples/waves/ripples.gtex > ripples.png
```

A complete program returns `vec4`. Texture functions include seeded noise,
coordinate transformations, scalar math, and colorization. Globals are `uv`
and `resolution`. The default dimensions are 256 by 256, and the default seed
is zero. The same source, dimensions, and seed produce the same image.

The output is an RGBA PNG image that ordinary image tools can open. A period
argument to `noise`, such as `uv.noise(vec2(3.0, 3.0))`, makes noise tileable
in noise-coordinate units.

`point.cellular()` returns Euclidean distance to the nearest seeded feature
point, with one point per unit lattice cell. Distances are in input-coordinate
units, bounded by `sqrt(2)` rather than normalized to [0, 1]. The optional period
uses positive integer components, just like `noise`: for example,
`uv.multiply(8.0).cellular(vec2(8.0, 8.0))` repeats every UV unit.
Use thresholds to turn these distances into bubble holes or cellular patterns.
The texture seed controls point placement.

## Geometry: grafic mesh

```sh
grafic mesh < examples/waves/waves.gmesh > waves.obj
```

A complete program returns `mesh`. `disk()` creates a filled disk.
`radius`, `segments`, and `rings` configure its size and subdivisions.
`uv_radial` changes its planar UVs into polar UVs: U runs from center to rim,
while V wraps around the circumference. `grid()` creates a centered XZ surface
with Y up. Use `.size(width, depth)` and `.subdivisions(x, z)` to configure it;
the defaults are size 2 by 2 and 64 cells per axis. Subdivisions must be integers
from 1 through 4096. Grid operations apply only to grids, and disk operations
only to disks. Geometry has no predefined globals.

The output is Wavefront OBJ text with positions, UVs, and triangle faces.
It can be imported into an OBJ-capable application.

## Intermediate results and editor tools

Each compiler supports `--preview-line N` to select the last expression point
on a one-based source line. Fragment and texture previews accept numeric/vector
results and convert them to RGBA; vertex previews accept a vertex value or `vec3` position,
and geometry previews require a mesh.

`grafic view` opens shaders, textures, and meshes with live reload. In a shader session,
keys `1`–`5` select the shader or a supplied texture input. A `.show()` marker
selects an intermediate output for its own file; it returns its input unchanged
during normal compilation. Mesh previews have a fitted angled orthographic view with
wireframe and UV-color modes. See the [viewer guide](view.md).

Run `grafic lsp` for diagnostics, completion, and hover across all source
types. A recognized document language ID (`graficgl`, `graficvert`, `grafictex`, or `graficmesh`)
selects its capability; otherwise the server uses `.gfrag`, `.gtex`, or `.gmesh`.
For example, `uv.` offers texture noise in texture files and `surface.uv.` offers texture sampling
in shader files; `disk().` offers mesh operations in geometry files.
