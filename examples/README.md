# Examples

From the repository root, build the compilers with `make`, then try:

```sh
build/fuse gl < examples/gl/ring.pgl > build/ring.frag
build/fuse tex --width 256 --height 256 --seed 42 \
  < examples/tex/clouds.ptex > build/clouds.png
build/fuse mesh < examples/mesh/radial.pmesh > build/radial.obj
```

`gl/` contains standalone shader examples; `tex/` contains gradients, rings,
stripes, and noise textures; `mesh/` contains planar and radial-UV disks.
Preview standalone shaders and textures with live reload:

```sh
build/fuse view examples/gl/ring.pgl
build/fuse view examples/tex/clouds.ptex
```

Images can be opened in an image viewer and OBJ files imported into a mesh
application. See the [viewer guide](../docs/view.md) for controls and options.

## Source compositions

`compositions/` contains shaders with procedural texture inputs. Preview them
directly, with live reload for every source:

```sh
build/fuse view examples/compositions/masked-reveal/masked-reveal.pgl \
  --tex0 examples/compositions/masked-reveal/mask.ptex \
  --tex1 examples/compositions/masked-reveal/rings.ptex
```

Press `1` for the shader or `2`–`5` to inspect texture slots. Place `.show()` in
any source to use an intermediate result as that file's viewer output.
The viewer defaults match the texture generation settings below. For an external
rendering host, compile textures to PNG with `fuse tex` and shaders with `fuse gl`.

| Directory | Shader | Texture slots | Texture generation |
| --- | --- | --- | --- |
| `broken-scanlines/` | `broken-scanlines.pgl` | `tex0` = `clouds.ptex`, `tex1` = `stripes.ptex` | 512 × 512, seed 42 |
| `energy-shield/` | `energy-shield.pgl` | `tex0` = `noise.ptex`, `tex1` = `rings.ptex` | 512 × 512, seed 42 |
| `fluid-marble/` | `fluid-marble.pgl` | `tex0` = `cloud-a.ptex`, `tex1` = `cloud-b.ptex` | 512 × 512, seed 42 |
| `liquid-movement/` | `liquid-movement.pgl` | `tex0` = `distortion.ptex`, `tex1` = `surface.ptex` | 512 × 512, seed 42 |
| `masked-reveal/` | `masked-reveal.pgl` | `tex0` = `mask.ptex`, `tex1` = `rings.ptex` | 512 × 512, seed 42 |
| `rippling-ink/` | `rippling-ink.pgl` | `tex0` = `clouds.ptex`, `tex1` = `rings.ptex` | 512 × 512, seed 42 |
