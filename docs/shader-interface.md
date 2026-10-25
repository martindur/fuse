# Shader host interface

FuseGL emits a complete GLSL 330 core fragment shader. A host provides the
vertex stage, textures, uniform values, and drawing context.

| Name | GLSL type | Host responsibility |
| --- | --- | --- |
| `matterboxUv` | Fragment input `vec2` | Supply interpolated UV coordinates from the vertex stage |
| `fragColor` | Fragment output `vec4` | Use as the shader's RGBA color output |
| `u_time` | Uniform `float` | Set elapsed animation time |
| `u_resolution` | Uniform `vec2` | Set output width and height in pixels |
| `tex0` through `tex3` | Uniform `sampler2D` | Bind the texture inputs used by the program |

The language's `uv` value comes from `matterboxUv`. Its `time` and `resolution`
globals refer to the corresponding uniforms. Builtins that correct aspect
ratio use `resolution.x / resolution.y`, so the host supplies a nonzero height.

A compatible vertex shader declares `out vec2 matterboxUv` and assigns UVs
appropriate for its geometry. For a full-screen surface, these typically span
the unit square. Mesh rendering can use the mesh's own UV coordinates.

`matterboxUv` is the existing shader interface name, retained so generated
output stays compatible with legacy hosts. FuseGL has no dependency on
MatterBox or a particular viewer implementation.

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
