# Examples

Complete assets authored with Grafic. From the repository root, build with `make`.

## Fire

`fire/` contains a stylized flame on an XY plane: an anchored hot base, broad
sweeping tongues, and eroding tips. The shader returns straight RGB color and separate
alpha coverage. Preview it with alpha blending:

```sh
build/grafic view examples/fire/fire.gfrag \
  --tex0 examples/fire/flame.gtex --blend alpha
```

`flame.gtex` packs tileable noise into red (tongues), green (sharp erosion grooves), and
blue (lateral bending); alpha is opaque. The shader scrolls the fields upward at
different speeds. The faster outer fields shape an orange-red edge and yellow
body, with sharp erosion toward the tips. The pale central core follows the same bent flame profile with weaker noise.
A density inset keeps a yellow margin around it as the outer body peels away. Narrow transitions separate the color regions;
the outer silhouette retains a little softness. Fine erosion is confined toward
the tips, leaving the larger sweeps intact. Press `2` to inspect the texture or add `.show()` to `coverage`,
`density`, or `heat` in the fragment shader to inspect the effect.

Tune the two scroll speeds, the taper width, and the height-dependent erosion in
`fire.gfrag`. The `heat` threshold sets the outer color boundary; the weaker
noise and height terms in `core` shape the calmer central glow. `core_inset`
keeps it inside the denser body. Bending is anchored at the base. All motion comes from shader time;
no particles or feedback buffers are required.

Export the same shader and texture for a rendering host:

```sh
build/grafic gl < examples/fire/fire.gfrag > build/fire.frag
build/grafic tex --width 512 --height 512 --seed 42 \
  < examples/fire/flame.gtex > build/flame.png
```

Import the texture as linear data, bind it to `tex0`, and render an XY plane with
0–1 UVs using the documented [shader interface](../docs/shader-interface.md).
Use source-alpha / one-minus-source-alpha color blending. The RGB output is not
premultiplied by coverage. The preview is unlit; bloom and lighting the surrounding
scene belong to the rendering host.

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
each crest as well as vertically. Two nearby swells reinforce and cancel to
form stronger sets and quieter stretches. A phase-shifted second harmonic makes
the dominant crest asymmetric; a weak crossing wave adds variation. Their
vertical amplitudes total 0.15, matching the material's height palette. Adjust
both ranges together when changing overall wave height.

Each component's steepness controls horizontal motion: zero gives vertical sine
motion; larger values compress crests and broaden troughs. Keep
`sum(steepness * amplitude * frequency)` below 1 to avoid folding the horizontal
surface; the example uses about 0.80292. Frequency is in radians per local unit,
and the time coefficient is angular speed in radians per second. The harmonic
stays at twice the main swell's frequency and speed; keep those linked when tuning.

Normals come from both analytic tangents of the full displacement, including
horizontal motion. `tangent_z.cross(tangent_x)` gives an upward-facing normal.
The fragment shader continues to use
`surface.position`, `surface.normal`, and `view.direction` for shading. The
reflection direction samples a sky gradient calculated in the shader. Fresnel
increases reflection at grazing angles, while alignment with the sun creates
moving glints. The light direction and view direction share mesh-local space.

Tune the water palette, sky colors, light direction, Fresnel curve, and glint
power directly in `waves.gfrag`. `water_roughness` blends the directional sky
toward an average sky color and lowers `sun_power` to broaden the highlight;
the glint multiplier controls brightness. This is an artistic approximation of
rough reflections using the analytic sky. `screen.uv.y()` retains a subtle
artistic fade toward a chosen color. The viewer background is unchanged.

The current 64×64 grid preserves the wave silhouettes at the default preview
scale. A 128×128 comparison showed little visible benefit, so the example keeps
64×64; raise subdivisions for closer views if needed.

Each chained operation has its own line, including bindings, so `.show()` can
be inserted between steps while exploring the example.

`ripples.gtex` takes centered differences of periodic noise to generate X/Z
slopes. Red and green store signed data around 0.5; blue stores broader patches
of ripple activity and alpha is opaque. The fragment shader samples the same map at two scales, scrolling
in different directions. Their blue channels vary ripple strength and reflection
roughness, giving quiet and active patches. It decodes both samples, adds their
weighted slopes to the large-wave slope, and normalizes one shading normal for diffuse light, Fresnel,
and reflections. This is a Y-up height-field construction, not a general
purpose tangent-space normal map.

Tune the two slope multipliers and `ripple_strength` in `waves.gfrag` to change
ripple strength.
A neutral map with red and green at 0.5 leaves the large-wave normal intact
apart from texture quantization. Texture noise defaults to seed 42 in the
viewer; pass `--seed` to choose another pattern. Save the `.gtex` source to
regenerate and reload it. `.show()` can inspect intermediate noise or slope
values, and `.show()` on the fragment's shading normal displays that normal
using the usual numeric preview colors.

The foam texture layers cellular distances at several sizes: red holds larger
bubble holes inside irregular patches, and green holds finer bubble breakup.
Both are tileable linear pattern fields; blue is unused and alpha is opaque.
The material samples them with different scrolling UVs and combines them
before thresholding, making patches join and break apart as their overlap changes.
A small UV offset from the moving ripple map bends both fields. Stronger crests
lower the threshold to fill in denser froth; an upward-normal mask restricts it
to wave tops. As the crest falls, the threshold rises: holes widen and strands
separate while surviving fragments stay opaque. Only their edges soften.

`foam_start`, `foam_scale`, and `foam_strength` in `waves.gfrag` control placement,
pattern size, and opacity. Raise `foam_threshold` to leave fewer foam patches.
`foam_connection` lowers the erosion threshold most strongly at compressed peaks,
joining fragments into broken crest lines while weaker crests remain scattered;
`foam_distortion` controls how much the ripple map bends them. `foam_relief`
controls subtle bubble-scale normals sampled from the same moving detail field.
The start threshold uses horizontal compression supplied by the vertex shader's
`.mask(crest_compression)`, exposed as `surface.mask`. Compression is one minus
the XZ area ratio from the full wave tangents: positive means squeezed, negative
means expanded. A tall, gentle swell can stay foam-free. This is a formation
proxy, not a breaking-wave simulation. Add `.show()` at
the end of the `foam_coverage` chain to inspect the animated coverage, or on
`patches` and `strands` in `foam.gtex` to inspect each field separately.

`foam_trail_strength` adds a short tail on the face behind a crest. The normal's
alignment with `foam_direction` identifies that face; compression controls how
far the trail extends into spreading water. `foam_trail_end` is the cutoff in
compression units (negative values permit a little expansion). Keep
`foam_direction` aligned with the dominant swell in `waves.gvert`. The crest
connection boost applies only to crest foam, so the tail is sparser and erodes
into separate opaque fragments. This geometry-driven approximation follows the
wave shape; it does not store foam age or let foam drift independently.

Foam has its own pale diffuse color. Mixing it over the reflected water reduces
water reflections and sun glints according to coverage. It does not inherit the
water's sharp specular lighting. Foam has its own bubble-scale diffuse normals.

Export the foam fields as an ordinary packed PNG texture for another rendering host:

```sh
build/grafic tex --width 512 --height 512 --seed 42 \
  < examples/waves/foam.gtex > build/foam.png
```

Import it as linear mask data and bind it to `tex1` alongside the exported shaders.
This version is stateless: its short trailing region follows the wave rather
than persisting independently after the wave passes.
