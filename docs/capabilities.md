# Language capabilities

All three commands share [Fuse syntax](language.md): bindings, regular calls,
and chains that pass the previous value as the next call's first argument.
Each capability provides its own functions, globals, execution, and output.
Run a command with `--builtins` for the authoritative signatures and docs.

## Shaders: fusegl

```sh
fusegl < examples/gl/ring.pgl > ring.frag
fusegl --preview-line 4 < examples/gl/ring.pgl > preview.frag
```

A complete program returns `vec4`. The shader capability provides coordinates,
texture sampling, animation, shape masks, and color composition. Globals are
`uv`, `resolution`, `time`, and texture inputs `tex0` through `tex3`.

The output is a complete GLSL 330 core fragment shader. Rendering requires a
host that supplies the documented [shader interface](shader-interface.md).
The historical `matterboxUv` input name is retained for compatibility.

## Textures: fusetex

```sh
fusetex --width 256 --height 256 --seed 42 \
  < examples/tex/clouds.ptex > clouds.png
```

A complete program returns `vec4`. Texture functions include seeded noise,
coordinate transformations, scalar math, and colorization. Globals are `uv`
and `resolution`. The default dimensions are 256 by 256, and the default seed
is zero. The same source, dimensions, and seed produce the same image.

The output is an RGBA PNG image that ordinary image tools can open. A period
argument to `noise`, such as `uv.noise(vec2(3.0, 3.0))`, makes noise tileable
in noise-coordinate units.

## Geometry: fusemesh

```sh
fusemesh < examples/mesh/radial.pmesh > radial.obj
```

A complete program returns `mesh`. The current primitive is a filled disk.
`radius`, `segments`, and `rings` configure its size and subdivisions.
`uv_radial` changes its planar UVs into polar UVs: U runs from center to rim,
while V wraps around the circumference. Geometry has no predefined globals.

The output is Wavefront OBJ text with positions, UVs, and triangle faces.
It can be imported into an OBJ-capable application.

## Intermediate results and editor tools

Each compiler supports `--preview-line N` to select the last expression point
on a one-based source line. Shader and texture previews accept numeric/vector
results and convert them to RGBA; geometry previews require a mesh.

Each command also supports `lsp`. Editors choose the command appropriate to
the file type. The shared engine supplies diagnostics, completion, and hover
using that capability's catalog. For example, `uv.` offers texture noise in
FuseTex and texture sampling in FuseGL; `disk().` offers mesh operations in
FuseMesh. This vocabulary does not require separate language implementations.
