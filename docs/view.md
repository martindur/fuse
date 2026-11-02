# Shader and texture preview

```sh
fuse view examples/gl/ring.pgl
fuse view examples/tex/clouds.ptex
```

Open a `.pgl` shader or `.ptex` texture directly. The window fills with the
output, with UVs from zero to one. For shaders, `time` advances in seconds and
`resolution` follows the framebuffer size,
including display scaling. Resize the window to change the aspect ratio.

Save the file to reload it. Source contents are checked every 200 ms, including
atomic saves and files removed and recreated by an editor. Unchanged contents
are not recompiled. A failed edit leaves the last working shader running;
the title shows an error state and the terminal gives the diagnostic. Fixing
and saving the source restores the preview. An initially invalid shader or
required texture leaves an empty window until corrected.

Space pauses or resumes animation. Reloading preserves playback time and pause
state. Escape or the window close button exits. Minimized windows keep watching
the sources while rendering is suspended.

## Procedural textures

Bind `.ptex` sources directly to the shader's texture slots:

```sh
fuse view examples/compositions/masked-reveal/masked-reveal.pgl \
  --tex0 examples/compositions/masked-reveal/mask.ptex \
  --tex1 examples/compositions/masked-reveal/rings.ptex
```

`--tex0` through `--tex3` generate and bind textures without intermediate PNG
files. Paths are relative to the working directory. Each source is watched
independently: editing a texture regenerates only that slot, and editing the
shader leaves textures intact. Failed edits keep the last working shader and
textures. Errors remain visible until the affected input is fixed.

Press `1` to see the shader, or `2`–`5` to inspect supplied `tex0`–`tex3` inputs.
The window title names the selected output. Unbound slots are skipped. Switching
preserves the shader, playback time, and pause state; inputs continue reloading
while you inspect a texture, even if the shader currently has an error.

Textures default to 512 × 512 pixels and noise seed 42. Set
`--texture-width N`, `--texture-height N`, and `--seed N` to override these for
all supplied textures. Window resizing affects shader `resolution`, without
regenerating textures. Texture generation runs when a source changes; textures
do not animate with shader time.

The viewer uses the same RGBA byte conversion as PNG export, with UV origin at
the bottom left. Sampling uses linear filtering and clamps at the edges; use
`repeat()` or `mirror(texN)` for wrapping. A shader using an unbound texture slot
reports which `--texN` option is needed.

## Intermediate outputs with `.show()`

Place `.show()` at the point you want to preview:

```text
uv
.multiply(4.5)
.noise()
.show()
.smoothstep(0.4, 0.68)
.colorize(rgb(1.0, 1.0, 1.0))
```

In the viewer, the last `.show()` in each file, in source order, selects that
file's output. Later stages are excluded from evaluation. The entire file must
still parse. A marker in a texture changes the image supplied to the shader;
the viewer stays on your selected output. Switch to the texture to inspect that
same intermediate image directly.

Markers accept scalar and vector values. Scalars display as opaque grayscale;
`vec2` fills red/green with blue zero and alpha one; `vec3` gets alpha one;
`vec4` preserves all channels. Without a marker, the full program must return
`vec4`. Names for markers are not supported yet.

Normal `fuse gl` and `fuse tex` compilation treats `.show()` as an identity
operation and compiles the complete program. Explicit `--preview-line N`
overrides markers for the file opened directly; bound textures still use their
own markers.

## Window and preview options

```sh
fuse view --width 1280 --height 720 examples/gl/ring.pgl
fuse view --preview-line 4 examples/gl/ring.pgl
fuse view --preview-line 3 examples/tex/clouds.ptex
```

`--width` and `--height` set the initial window size (default 960 × 640).
`--preview-line N` displays the last expression point on a one-based source
line, using the same conversions as the compiler's `--preview-line N`. A selected
line without an expression reports a preview error.

## Building

The default package includes the viewer. It requires GLFW 3 and OpenGL headers
and libraries in addition to the compiler dependencies. The renderer uses an
OpenGL 3.3 core context through GLFW. No display is needed to build Fuse or run
the compiler and asset tests.

```sh
make CAPABILITIES="gl tex view"  # Shader/texture compilers and viewer
make CAPABILITIES=gl             # Compiler only, without windowing dependencies
```

The `view` capability requires `gl` and `tex`. Linux builds discover GLFW and
OpenGL through `pkg-config`; macOS builds use GLFW and the system OpenGL framework.
