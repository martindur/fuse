# Shader host interface

By default, `grafic gl` emits a complete GLSL 330 core fragment shader. A host provides the
vertex stage, textures, uniform values, and drawing context.

| Name | GLSL type | Host responsibility |
| --- | --- | --- |
| `graficUv` | Fragment input `vec2` | Supply interpolated UV coordinates from the vertex stage |
| `fragColor` | Fragment output `vec4` | Use as the shader's RGBA color output |
| `u_time` | Uniform `float` | Set elapsed animation time |
| `u_resolution` | Uniform `vec2` | Set output width and height in pixels |
| `tex0` through `tex3` | Uniform `sampler2D` | Bind the texture inputs used by the program |

The language's `uv` value comes from `graficUv`. Its `time` and `resolution`
globals refer to the corresponding uniforms. Builtins that correct aspect
ratio use `resolution.x / resolution.y`, so the host supplies a nonzero height.

A compatible vertex shader declares `out vec2 graficUv` and assigns UVs
appropriate for its geometry. For a full-screen surface, these typically span
the unit square. Mesh rendering can use the mesh's own UV coordinates.

Complete source programs must return `vec4`. Intermediate previews can return
`float`, `vec2`, `vec3`, or `vec4`; emission converts them as follows:

| Result | Emitted RGBA |
| --- | --- |
| `float x` | `vec4(vec3(x), 1.0)` |
| `vec2 xy` | `vec4(xy, 0.0, 1.0)` |
| `vec3 rgb` | `vec4(rgb, 1.0)` |
| `vec4 rgba` | Used directly |

The shader text includes its builtin GLSL definitions and `main`. Hosts can
compile it directly as a fragment stage; no additional shader prelude is
required. Standard GLSL optimization may remove unused uniforms, so hosts can
skip uniform locations that are inactive.

## Vertex shaders

Compile `.gvert` sources with `grafic gl --stage vertex`. A complete program
returns a `vec3` position in mesh-local coordinates. `position` is the original
vertex position, `uv` its original UV, and `time` elapsed seconds. UVs pass
unchanged to the fragment shader through `graficUv`.

```text
let phase = position.x().multiply(5.0).add(time.multiply(2.0));
position.add(vec3(0.0, phase.sin().multiply(0.15), 0.0))
```

The host binds these vertex inputs and uniforms:

| Name | GLSL type | Host responsibility |
| --- | --- | --- |
| `a_position` | `vec3`, attribute location 0 | Original mesh-local XYZ position |
| `a_uv` | `vec2`, attribute location 1 | Mesh UV coordinates |
| `u_model` | `mat4` | Local-to-world transform |
| `u_view` | `mat4` | World-to-camera transform |
| `u_projection` | `mat4` | Camera-to-clip transform |
| `u_time` | `float` | Elapsed animation time in seconds |

Generated code computes `u_projection * u_view * u_model * vec4(result, 1.0)`.
Vertices use the same coordinate units as the generated mesh. Grids lie in the
XZ plane, centered at the origin, with Y up. The host can use any camera.

Vertex functions include `vec2`, `vec3`, component accessors `x`, `y`, and `z`,
scalar `sin`, `cos`, `oscillate`, and `cycle`, scalar/vector `add`, and scalar
`multiply` or vector scaling. Run `grafic gl --stage vertex --builtins` for
signatures. Fragment-only functions, screen resolution, and texture inputs are
not exposed in this initial vertex environment.

Vertex `.show()` markers and `--preview-line` results must also be `vec3`.
Inspection renders the mesh at those intermediate positions. Normal compilation
treats `.show()` as an identity and exports the complete transformation.
