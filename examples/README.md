# Examples

From the repository root, build the compilers with `make`, then try:

```sh
build/fusegl < examples/gl/ring.pgl > build/ring.frag
build/fusetex --width 256 --height 256 --seed 42 \
  < examples/tex/clouds.ptex > build/clouds.png
build/fusemesh < examples/mesh/radial.pmesh > build/radial.obj
```

`gl/` contains standalone shader examples; `tex/` contains gradients, rings,
stripes, and noise textures; `mesh/` contains planar and radial-UV disks.
Images can be opened in an image viewer and OBJ files imported into a mesh
application. Shaders need a host implementing the [shader interface](../docs/shader-interface.md).

## Source compositions

`compositions/` preserves the language sources from the previous combined
demos. Compile each texture source with FuseTex and each shader with FuseGL,
then bind the resulting images to the shader's texture slots. These are
source-only examples; the old application configs remain in the legacy repo.

| Directory | Shader | Texture slots | Texture generation |
| --- | --- | --- | --- |
| `broken-scanlines/` | `broken-scanlines.pgl` | `tex0` = `clouds.ptex`, `tex1` = `stripes.ptex` | 512 × 512, seed 42 |
| `energy-shield/` | `energy-shield.pgl` | `tex0` = `noise.ptex`, `tex1` = `rings.ptex` | 512 × 512, seed 42 |
| `fluid-marble/` | `fluid-marble.pgl` | `tex0` = `cloud-a.ptex`, `tex1` = `cloud-b.ptex` | 512 × 512, seed 42 |
| `liquid-movement/` | `liquid-movement.pgl` | `tex0` = `distortion.ptex`, `tex1` = `surface.ptex` | 512 × 512, seed 42 |
| `masked-reveal/` | `masked-reveal.pgl` | `tex0` = `mask.ptex`, `tex1` = `rings.ptex` | 512 × 512, seed 42 |
| `rippling-ink/` | `rippling-ink.pgl` | `tex0` = `clouds.ptex`, `tex1` = `rings.ptex` | 512 × 512, seed 42 |
