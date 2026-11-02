# Examples

From the repository root, build the compilers with `make`, then try:

```sh
build/fuse gl < examples/gl/ring.fgl > build/ring.frag
build/fuse tex --width 256 --height 256 --seed 42 \
  < examples/tex/clouds.ftex > build/clouds.png
build/fuse mesh < examples/mesh/radial.fmesh > build/radial.obj
```

`gl/` contains standalone shader examples; `tex/` contains gradients, rings,
stripes, and noise textures; `mesh/` contains planar and radial-UV disks.
Preview sources directly with live reload:

```sh
build/fuse view examples/gl/ring.fgl
build/fuse view examples/tex/clouds.ftex
build/fuse view examples/mesh/radial.fmesh
```

Mesh previews fit the geometry in front view; `W` toggles wireframe and `U`
shows UV colors. See the [viewer guide](../docs/view.md) for controls and options.

## Source compositions

`compositions/` contains shaders with procedural texture inputs. Preview them
directly, with live reload for every source:

```sh
build/fuse view examples/compositions/masked-reveal/masked-reveal.fgl \
  --tex0 examples/compositions/masked-reveal/mask.ftex \
  --tex1 examples/compositions/masked-reveal/rings.ftex
```

Press `1` for the shader or `2`–`5` to inspect texture slots. Place `.show()` in
any source to use an intermediate result as that file's viewer output.
The viewer defaults match the texture generation settings below. For an external
rendering host, compile textures to PNG with `fuse tex` and shaders with `fuse gl`.

| Directory | Shader | Texture slots | Texture generation |
| --- | --- | --- | --- |
| `broken-scanlines/` | `broken-scanlines.fgl` | `tex0` = `clouds.ftex`, `tex1` = `stripes.ftex` | 512 × 512, seed 42 |
| `energy-shield/` | `energy-shield.fgl` | `tex0` = `noise.ftex`, `tex1` = `rings.ftex` | 512 × 512, seed 42 |
| `fluid-marble/` | `fluid-marble.fgl` | `tex0` = `cloud-a.ftex`, `tex1` = `cloud-b.ftex` | 512 × 512, seed 42 |
| `liquid-movement/` | `liquid-movement.fgl` | `tex0` = `distortion.ftex`, `tex1` = `surface.ftex` | 512 × 512, seed 42 |
| `masked-reveal/` | `masked-reveal.fgl` | `tex0` = `mask.ftex`, `tex1` = `rings.ftex` | 512 × 512, seed 42 |
| `rippling-ink/` | `rippling-ink.fgl` | `tex0` = `clouds.ftex`, `tex1` = `rings.ftex` | 512 × 512, seed 42 |
