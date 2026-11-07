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

Attach an authored scalar material mask with a chain:

```text
vertex(position, uv, direction)
  .mask(0.75)
```

The mask defaults to zero and interpolates with the other surface attributes
using perspective-correct interpolation. Values are not implicitly clamped;
the fragment program decides how to interpret them. Calling `.mask(...)` again
replaces the previous mask and preserves position, UV, and normal.

Vertex values have `.position`, `.uv`, `.normal`, and `.mask` fields. An omitted authored
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

A complete fragment program returns RGBA as `vec4`. Surface, screen, and view
inputs are distinct typed values; the old bare `position`, `uv`, and `normal` fragment
globals have been replaced.

| Grafic input | Type | Meaning |
| --- | --- | --- |
| `surface.position` | `vec3` | Displaced mesh-local position, interpolated across the triangle |
| `surface.uv` | `vec2` | Interpolated UV supplied by the vertex result |
| `surface.normal` | `vec3` | Normalized interpolated authored normal, or geometric triangle normal |
| `surface.mask` | `float` | Interpolated authored material mask; zero when omitted |
| `screen.position` | `vec2` | Framebuffer pixel-center coordinates, bottom-left origin |
| `screen.uv` | `vec2` | `screen.position` divided by framebuffer resolution |
| `view.direction` | `vec3` | Normalized surface-to-camera direction, in mesh-local space |
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
| `graficMask` | `in float` | Authored scalar mask, or zero |
| `fragColor` | `out vec4` | RGBA output |
| `u_time` | `uniform float` | Elapsed seconds |
| `u_resolution` | `uniform vec2` | Framebuffer size |
| `u_view_is_orthographic` | `uniform bool` | Selects constant direction (`true`) or point camera (`false`) |
| `u_view_direction` | `uniform vec3` | Orthographic direction toward the viewer, in mesh-local space |
| `u_view_position` | `uniform vec3` | Perspective camera position, in mesh-local space |
| `tex0` through `tex3` | `uniform sampler2D` | Texture inputs |

A compatible vertex shader declares matching outputs, including `flat out int`
for `graficHasNormal`. Grafic-generated vertex shaders supply them automatically.
Default mesh previews pass original positions and UVs without authored normals.
Fullscreen previews supply an XY plane with visible X/Y from -1 to 1 and Z zero,
unit-square UVs, and no authored normals. Both default previews supply a zero mask.

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

The wave example uses Gerstner displacement, including horizontal motion toward
the crests. It derives both surface tangents from that complete displacement and
normalizes `tangent_z.cross(tangent_x)` for its authored normal. The vertex-stage
`cross(vec3, vec3)` is a right-handed cross product. A height-only gradient would
miss the horizontal derivatives. Smooth lighting still sits on geometry whose
silhouette depends on subdivisions.

The unnormalized cross product's Y component is the horizontal XZ area ratio
(the Jacobian determinant). The wave example exports `1 - area_ratio` through
`.mask(...)`: positive values indicate compression, negative values expansion.
The fragment program uses `surface.mask` to place foam on compressed crests.
This is a formation proxy, not a breaking-wave simulation or persistent history.

The fragment shader uses `surface.position.y()` for height colors and
`screen.uv.y()` to fade toward a chosen background color. This is an artistic
screen-space fade, not physical distance fog; it does not change the viewer's
background color.

## View direction and reflections

`view.direction` points from the shaded surface toward the camera and is
normalized per fragment. For orthographic views, the host sets
`u_view_is_orthographic = true` and supplies a nonzero `u_view_direction`.
For perspective views, it sets the flag to false and supplies `u_view_position`;
the shader normalizes `u_view_position - surface.position`. Host camera inputs
must be in the same mesh-local space as the surface. With model transforms,
convert camera position and direction into that space before supplying them.

The current viewer remains orthographic. Mesh previews use their fixed angled
camera direction; fullscreen previews use `(0, 0, 1)`. Window resizing, animation,
and displacement do not change an orthographic viewing direction. Perspective
support here is an exported-shader contract; the viewer has no new camera mode.

`reflect(incoming, unit_normal)` returns the reflected direction. Since the view
vector points toward the camera, reverse it to look up the reflected environment:

```text
let reflected = view.direction
  .multiply(-1.0)
  .reflect(surface.normal);
```

Scalar `pow(base, exponent)` shapes reflection curves and sun highlights. Use a
nonnegative base; a zero base requires a positive exponent. RGB `add` combines
light contributions. `multiply(vec2, float)` scales texture coordinates, matching
the texture compiler's vector scaling operation. These functions and `view` belong to the fragment stage.

The wave material computes a sky gradient from the reflection's Y component,
uses a Schlick Fresnel curve with 0.02 base reflectance, and adds sun glints from
alignment with the light direction. A ripple-activity field controls an artistic
roughness approximation: blend the directional sky toward an average sky color,
and broaden the weaker sun lobe. All colors and parameters are authored in
`waves.gfrag`. The sky is a shader calculation; it does not require an environment
texture, sky geometry, or a change to the viewer background.

## Ripple data

The water example binds `ripples.gtex` to `tex0` as data. Its red and green
channels encode scaled X/Z height slopes around 0.5; blue stores broader ripple
activity. Two scrolling, repeated
samples perturb the analytic wave normal. For a Y-up height field, multiplying
the added slopes by `-surface.normal.y()` before adding to `surface.normal`
keeps the added local slopes consistent with the Y-up normal construction.
The activity field varies ripple strength and reflection roughness. The fragment
then normalizes and uses the same water shading normal throughout its lighting. Mesh geometry and the stage interface are unchanged.

## Crest foam

The wave material binds `foam.gtex` to `tex1`. It stores a tileable linear
patch field in red and a finer breakup field in green; blue is unused and alpha
is opaque. Layered cellular distances form the bubbles. The fields scroll
independently, bend with a small ripple-driven UV offset, and combine before a
soft threshold. Vertex-authored compression and an upward-facing normal mask control erosion
through the threshold.
Declining crests lose fragments as holes widen; crest strength does not multiply
the final opacity. Finite differences of the moving green field give the foam
its own bubble-scale normal for diffuse lighting. Foam is a separate diffuse
material mixed over reflected water before the screen-space fade, so high coverage suppresses
water reflections instead of adding white to the specular result. Both the
pattern and the crest gate remain inspectable with `.show()`.

The example bakes cellular patterns with `.gtex` and uses ordinary shader math.
It has no foam history; persistent trails are deferred to future stateful effects
and export contracts.

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
program's UV, normal, and mask attributes when that complete program is valid. If a
later semantic error prevents preparing that context, inspection still works
using original mesh UVs, triangle normals, and a zero mask. The whole source must parse.
Normal compilation treats `.show()` as an identity and requires a vertex result.
