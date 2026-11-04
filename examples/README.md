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
Preview sources directly with live reload:

```sh
build/grafic view examples/gl/ring.gfrag
build/grafic view examples/gl/waves.gvert
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

`gl/waves.gvert` displaces a subdivided grid on the GPU. Open it directly for
the default surface, or use `--mesh examples/mesh/grid.gmesh` to edit its size
and subdivisions separately. `W` toggles wireframe to inspect displacement.
Compile it for another host with:

```sh
build/grafic gl --stage vertex < examples/gl/waves.gvert > build/waves.vert
```

The initial example uses UV colors; lighting and surface normals come later.
