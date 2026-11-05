# Shader host interface

`grafic gl` emits complete GLSL 330 core shaders. Fragment is the default stage;
use `--stage vertex` for `.gvert` programs. The host supplies geometry, transforms,
textures, and uniform values. Inactive uniforms and inputs may be optimized out.

## Vertex results

Vertex inputs remain `position: vec3`, `uv: vec2`, and `time: float`. Positions
are original mesh-local XYZ coordinates. A complete program returns a vertex:

```text
vertex(position, uv)
```

The optional third argument supplies an authored mesh-local normal:

```text
let direction = vec3(0.0, 1.0, 0.0);
vertex(position, uv, direction)
```

Omitting the normal requests triangle-normal shading. Supplying it requests
interpolated normal shading; the fragment stage normalizes the interpolated
vector. Authors are responsible for choosing normals that describe their surface.
A zero normal produces zero rather than an undefined normalized direction.
UVs may be transformed before being passed to `vertex(...)`.

Vertex values have `.position`, `.uv`, and `.normal` fields. An omitted authored
normal reads as zero on a vertex value. The vertex result is a dedicated typed
value; arbitrary record literals are not supported.

| Host input | GLSL type | Meaning |
| --- | --- | --- |
| `a_position` | `vec3`, attribute 0 | Original mesh-local position |
| `a_uv` | `vec2`, attribute 1 | Mesh UV coordinates |
| `u_model` | `mat4` | Local-to-world transform |
| `u_view` | `mat4` | World-to-camera transform |
| `u_projection` | `mat4` | Camera-to-clip transform |
| `u_time` | `float` | Elapsed seconds |

The compiler evaluates the vertex result once, passes its attributes onward,
and computes `u_projection * u_view * u_model * vec4(result.position, 1.0)`.
The model and camera do not change the coordinate space of surface inputs.

## Fragment inputs

A complete fragment program returns RGBA as `vec4`. Surface and screen inputs
are distinct typed values; the old bare `position`, `uv`, and `normal` fragment
globals have been replaced.

| Grafic input | Type | Meaning |
| --- | --- | --- |
| `surface.position` | `vec3` | Displaced mesh-local position, interpolated across the triangle |
| `surface.uv` | `vec2` | Interpolated UV supplied by the vertex result |
| `surface.normal` | `vec3` | Normalized interpolated authored normal, or geometric triangle normal |
| `screen.position` | `vec2` | Framebuffer pixel-center coordinates, bottom-left origin |
| `screen.uv` | `vec2` | `screen.position` divided by framebuffer resolution |
| `resolution` | `vec2` | Framebuffer width and height in pixels |
| `time` | `float` | Elapsed seconds |
| `tex0` through `tex3` | `texture` | Configured texture inputs |

Pixel centers start at `(0.5, 0.5)`. Resizing changes screen coordinates without
changing mesh UVs. Field access can be chained with calls or used through aliases:

```text
let data = surface;
let height = data.position
  .y();
let coordinate = screen.uv
  .y();
vec4(data.uv, height, 1.0)
```

The fragment GLSL interface is:

| Name | GLSL type | Host responsibility |
| --- | --- | --- |
| `graficPosition` | `in vec3` | Displaced local position |
| `graficUv` | `in vec2` | Surface UV |
| `graficAuthoredNormal` | `in vec3` | Authored local normal, or zero |
| `graficHasNormal` | `flat in int` | 1 for an authored normal; 0 for triangle normals |
| `fragColor` | `out vec4` | RGBA output |
| `u_time` | `uniform float` | Elapsed seconds |
| `u_resolution` | `uniform vec2` | Framebuffer size |
| `tex0` through `tex3` | `uniform sampler2D` | Texture inputs |

A compatible vertex shader declares matching outputs, including `flat out int`
for `graficHasNormal`. Grafic-generated vertex shaders supply them automatically.
Default mesh previews pass original positions and UVs without authored normals.
Fullscreen previews supply an XY plane with visible X/Y from -1 to 1 and Z zero,
unit-square UVs, and no authored normals.

## Lighting and wave normals

Without an authored normal, `surface.normal` comes from derivatives of the
displaced position. Its direction follows counterclockwise triangle winding;
hosts should use counterclockwise front faces. Standard grids point toward +Y,
and fullscreen previews toward +Z. Degenerate derivatives produce zero.
These normals describe individual triangles and can appear faceted.

With an authored normal, the GPU interpolates the supplied vectors and the
fragment stage normalizes the result. These normals are not automatically
flipped toward the camera. Light directions must use the same mesh-local space.
`dot`, `normalize`, and RGB scalar `multiply` support explicit diffuse lighting.

The wave example calculates height and its gradient from the same sine phases.
Its authored normal is proportional to `(-gradient.x, 1, -gradient.z)`. This
smooths lighting while geometry and silhouettes still depend on subdivisions.

The fragment shader uses `surface.position.y()` for height colors and
`screen.uv.y()` to fade toward a chosen background color. This is an artistic
screen-space fade, not physical distance fog; it does not change the viewer's
background color.

## Inspection

Fragment `.show()` and `--preview-line` results display as follows:

| Result | RGBA |
| --- | --- |
| `float x` | `vec4(vec3(x), 1.0)` |
| `vec2 xy` | `vec4(xy, 0.0, 1.0)` |
| `vec3 rgb` | `vec4(rgb, 1.0)` |
| `vec4 rgba` | Unchanged |

Vertex inspection accepts a complete vertex value or a `vec3` position. A
vertex value previews all its attributes. A position preview retains the final
program's UV and normal attributes when that complete program is valid. If a
later semantic error prevents preparing that context, inspection still works
using original mesh UVs and triangle normals. The whole source must parse.
Normal compilation treats `.show()` as an identity and requires a vertex result.
