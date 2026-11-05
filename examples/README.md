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
- `waves.gvert` combines a broad swell, a crossing wave, and smaller ripples.
- `waves.gfrag` blends deep blue, turquoise, and pale crests, with ambient and directional lighting.

```sh
build/grafic view examples/waves/waves.gmesh \
  --vert examples/waves/waves.gvert \
  --frag examples/waves/waves.gfrag
```

Edit any source to reload it independently. Press `W` to inspect triangles or
`U` to compare UV colors. Increase mesh subdivisions for finer faceting.

The vertex shader adds three sine waves with different directions, frequencies,
speeds, and amplitudes. Their amplitudes total 0.15; the fragment shader maps
heights from -0.15 to 0.15 into the water palette. Adjust both ranges together
when changing the overall wave height. The vertex shader derives an authored normal from the same wave phases,
amplitudes, and frequencies as the displacement. The fragment shader uses
`surface.position`, `surface.uv`, and normalized `surface.normal` for shading.
The light direction is mesh-local and points toward the light; ambient light
keeps other slopes visible. `screen.uv.y()` adds an artistic fade toward the
chosen background color, independent of surface UVs. This is not distance fog.

Each chained operation has its own line, including bindings, so `.show()` can
be inserted between steps while exploring the example.
