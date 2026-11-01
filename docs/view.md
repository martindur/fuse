# Shader preview

```sh
fuse view examples/gl/ring.pgl
```

The window shows the shader across its surface, with UVs from zero to one.
`time` advances in seconds and `resolution` follows the framebuffer size,
including display scaling. Resize the window to change the aspect ratio.

Save the file to reload it. Source contents are checked every 200 ms, including
atomic saves and files removed and recreated by an editor. Unchanged contents
are not recompiled. A failed edit leaves the last working shader running;
the title shows an error state and the terminal gives the diagnostic. Fixing
and saving the source restores the preview. A source that initially contains
errors opens an empty window and waits for a valid edit.

Space pauses or resumes animation. Reloading preserves playback time and pause
state. Escape or the window close button exits. Minimized windows keep watching
the source while rendering is suspended.

## Options

```sh
fuse view --width 1280 --height 720 examples/gl/ring.pgl
fuse view --preview-line 4 examples/gl/ring.pgl
```

`--width` and `--height` set the initial window size (default 960 × 640).
`--preview-line N` displays the last expression point on a one-based source
line, using the same conversions as `fuse gl --preview-line N`. A selected
line without an expression reports a preview error.

This slice supports shaders that do not require texture inputs. Shaders using
`tex0` through `tex3` report a diagnostic; texture-backed compositions are the
next viewer slice.

## Building

The default package includes the viewer. It requires GLFW 3 and OpenGL headers
and libraries in addition to the compiler dependencies. The renderer uses an
OpenGL 3.3 core context through GLFW. No display is needed to build Fuse or run
the compiler and asset tests.

```sh
make CAPABILITIES="gl view"  # Shader compiler and viewer
make CAPABILITIES=gl         # Compiler only, without windowing dependencies
```

The `view` capability requires `gl`. Linux builds discover GLFW and OpenGL
through `pkg-config`; macOS builds use GLFW and the system OpenGL framework.
