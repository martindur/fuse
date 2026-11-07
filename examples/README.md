# Examples

From the repository root, build the compilers with `make`, then try:

```sh
build/grafic gl < examples/gl/ring.gfrag > build/ring.frag
build/grafic tex --width 256 --height 256 --seed 42 \
  < examples/tex/clouds.gtex > build/clouds.png
build/grafic mesh < examples/mesh/radial.gmesh > build/radial.obj
```

`gl/` contains standalone shader examples; `tex/` contains gradients, rings,
stripes, and noise textures; `mesh/` contains planar and radial-UV disks.
`waves/` groups the current animated surface, geometry, and material together.
Preview sources directly with live reload:

```sh
build/grafic view examples/gl/ring.gfrag
build/grafic view examples/tex/clouds.gtex
build/grafic view examples/mesh/radial.gmesh
```

Mesh previews fit the geometry in an angled orthographic view; `W` toggles wireframe and `U`
shows UV colors. See the [viewer guide](../docs/view.md) for controls and options.

## Source compositions

`compositions/` contains shaders with procedural texture inputs. Preview them
directly, with live reload for every source:

```sh
build/grafic view examples/compositions/masked-reveal/masked-reveal.gfrag \
  --tex0 examples/compositions/masked-reveal/mask.gtex \
  --tex1 examples/compositions/masked-reveal/rings.gtex
```

Press `1` for the shader or `2`–`5` to inspect texture slots. Place `.show()` in
any source to use an intermediate result as that file's viewer output.
The viewer defaults match the texture generation settings below. For an external
rendering host, compile textures to PNG with `grafic tex` and shaders with `grafic gl`.

| Directory | Shader | Texture slots | Texture generation |
| --- | --- | --- | --- |
| `broken-scanlines/` | `broken-scanlines.gfrag` | `tex0` = `clouds.gtex`, `tex1` = `stripes.gtex` | 512 × 512, seed 42 |
| `energy-shield/` | `energy-shield.gfrag` | `tex0` = `noise.gtex`, `tex1` = `rings.gtex` | 512 × 512, seed 42 |
| `fluid-marble/` | `fluid-marble.gfrag` | `tex0` = `cloud-a.gtex`, `tex1` = `cloud-b.gtex` | 512 × 512, seed 42 |
| `liquid-movement/` | `liquid-movement.gfrag` | `tex0` = `distortion.gtex`, `tex1` = `surface.gtex` | 512 × 512, seed 42 |
| `masked-reveal/` | `masked-reveal.gfrag` | `tex0` = `mask.gtex`, `tex1` = `rings.gtex` | 512 × 512, seed 42 |
| `rippling-ink/` | `rippling-ink.gfrag` | `tex0` = `clouds.gtex`, `tex1` = `rings.gtex` | 512 × 512, seed 42 |

## Waves

`waves/` contains one complete animated surface:

- `waves.gmesh` sets the grid size and subdivisions.
- `waves.gvert` combines a dominant Gerstner swell, a smaller supporting swell, and a weak crossing wave.
- `waves.gfrag` combines water color, reflected sky, Fresnel reflection, and sun glints.
- `ripples.gtex` supplies a tileable ripple-slope map, bound to `tex0`.
- `foam.gtex` supplies tileable foam patch and breakup fields, bound to `tex1`.

```sh
build/grafic view examples/waves/waves.gmesh \
  --vert examples/waves/waves.gvert \
  --frag examples/waves/waves.gfrag \
  --tex0 examples/waves/ripples.gtex \
  --tex1 examples/waves/foam.gtex
```

Edit any source to reload it independently. Press `W` to inspect triangles or
`U` to compare UV colors. Press `2` to inspect ripples, `3` to inspect foam, and `1` to return to the surface. Increase mesh subdivisions for finer silhouettes; ripple
shading does not add vertices.

The vertex shader uses Gerstner displacement: vertices move horizontally toward
each crest as well as vertically. Most amplitude belongs to the main swell;
a smaller wave travels in a similar direction, and a weak crossing wave adds
variation. Their vertical amplitudes still total 0.15, matching the material's
height palette. Adjust both ranges together when changing overall wave height.

Each component's steepness controls horizontal motion: zero gives the original
vertical sine motion; larger values compress crests and broaden troughs. Keep
`sum(steepness * amplitude * frequency)` below 1 to avoid folding the horizontal
surface; the example uses about 0.6724. Frequency is in radians per local unit,
and the time coefficient is angular speed in radians per second.

Normals come from both analytic tangents of the full displacement, including
horizontal motion. `tangent_z.cross(tangent_x)` gives an upward-facing normal.
The fragment shader continues to use
`surface.position`, `surface.normal`, and `view.direction` for shading. The
reflection direction samples a sky gradient calculated in the shader. Fresnel
increases reflection at grazing angles, while alignment with the sun creates
moving glints. The light direction and view direction share mesh-local space.

Tune the water palette, sky colors, light direction, Fresnel curve, and glint
power directly in `waves.gfrag`. Higher glint power narrows the highlight;
its following multiplier controls brightness. `screen.uv.y()` retains a subtle
artistic fade toward a chosen color. The viewer background is unchanged.

Each chained operation has its own line, including bindings, so `.show()` can
be inserted between steps while exploring the example.

`ripples.gtex` takes centered differences of periodic noise to generate X/Z
slopes. Red and green store signed data around 0.5; blue is unused and alpha
is opaque. The fragment shader samples the same map at two scales, scrolling
in different directions. It decodes both samples, adds their slopes to the
large-wave slope, and normalizes one shading normal for diffuse light, Fresnel,
and reflections. This is a Y-up height-field construction, not a general
purpose tangent-space normal map.

Tune the two slope multipliers in `waves.gfrag` to change ripple strength.
A neutral map with red and green at 0.5 leaves the large-wave normal intact
apart from texture quantization. Texture noise defaults to seed 42 in the
viewer; pass `--seed` to choose another pattern. Save the `.gtex` source to
regenerate and reload it. `.show()` can inspect intermediate noise or slope
values, and `.show()` on the fragment's shading normal displays that normal
using the usual numeric preview colors.

The foam texture layers cellular distances at several sizes: red holds larger
bubble holes inside irregular patches, and green holds finer bubble breakup. Both are tileable linear pattern fields; blue is unused and alpha is
opaque. The material samples them with different scrolling UVs and combines them
before thresholding, making patches join and break apart as their overlap changes.
A small UV offset from the moving ripple map bends both fields. Stronger crests
lower the threshold to fill in denser froth; an upward-normal mask restricts it
to wave tops, leaving more reflective water exposed.

`foam_start`, `foam_scale`, and `foam_strength` in `waves.gfrag` control placement,
pattern size, and opacity. Raise `foam_threshold` to leave fewer foam patches;
`foam_distortion` controls how much the ripple map bends them. The start threshold uses normalized wave height:
it is an artistic crest proxy, not a breaking-wave simulation. Add `.show()` at
the end of the `foam_coverage` chain to inspect the animated coverage, or on
`patches` and `strands` in `foam.gtex` to inspect each field separately.

Foam has its own pale diffuse color. Mixing it over the reflected water reduces
water reflections and sun glints according to coverage. It does not inherit the
water's sharp specular lighting. The former pale height bands are removed.

Export the foam fields as an ordinary packed PNG texture for another rendering host:

```sh
build/grafic tex --width 512 --height 512 --seed 42 \
  < examples/waves/foam.gtex > build/foam.png
```

Import it as linear mask data and bind it to `tex1` alongside the exported shaders.
This version is stateless: it has no persistent trails after a crest passes.
