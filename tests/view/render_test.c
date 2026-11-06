#define GL_GLEXT_PROTOTYPES
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include "view/assets.h"
#include "view/render.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "gl/shader.h"

static bool set_source(ViewRenderer *renderer, const char *text, ViewRenderError *error)
{
    ViewSource source = {.text = (char *)text, .length = strlen(text), .available = true};
    char *glsl = NULL;
    CompileError compile_error;
    if (!view_shader_compile(&source, SHADER_STAGE_FRAGMENT, 0, &glsl, &compile_error)) {
        fprintf(stderr, "source failed: %s\n", compile_error.message);
        return false;
    }
    bool loaded = view_renderer_set_shader(renderer, glsl, error);
    free(glsl);
    return loaded;
}

static bool set_mesh_sources(ViewRenderer *renderer, const char *vertex, const char *fragment,
    ViewRenderError *error)
{
    const char *texts[] = {fragment, vertex};
    char *glsl[2] = {NULL, NULL};
    bool loaded = false;
    for (size_t stage = 0; stage < 2; stage += 1) {
        if (texts[stage] == NULL) continue;
        ViewSource source = {.available = true, .text = (char *)texts[stage], .length = strlen(texts[stage])};
        CompileError compile_error;
        if (!view_shader_compile(&source, (ShaderStage)stage, 0, &glsl[stage], &compile_error)) {
            snprintf(error->message, sizeof(error->message), "%s", compile_error.message);
            goto cleanup;
        }
    }
    loaded = view_renderer_set_mesh_shaders(renderer, glsl[SHADER_STAGE_VERTEX], glsl[SHADER_STAGE_FRAGMENT], error);
cleanup:
    free(glsl[0]);
    free(glsl[1]);
    return loaded;
}

/* Compare the exported shader's authored normals to finite differences of its positions. */
static bool check_wave_normals(const ViewSource *source, ViewRenderError *error)
{
    enum { SIDE = 7, POINTS = SIDE * SIDE * 5 };
    const float epsilon = 0.001f;
    float inputs[POINTS][3], outputs[POINTS][6];
    for (int z = 0; z < SIDE; z += 1) {
        for (int x = 0; x < SIDE; x += 1) {
            for (int neighbor = 0; neighbor < 5; neighbor += 1) {
                int index = (z * SIDE + x) * 5 + neighbor;
                inputs[index][0] = (float)x / 3 - 1 + (neighbor == 1 ? epsilon : neighbor == 2 ? -epsilon : 0);
                inputs[index][1] = 0;
                inputs[index][2] = (float)z / 3 - 1 + (neighbor == 3 ? epsilon : neighbor == 4 ? -epsilon : 0);
            }
        }
    }
    GLint old_program, old_array, old_buffer, old_feedback;
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_array);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_buffer);
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &old_feedback);
    GLboolean old_discard = glIsEnabled(GL_RASTERIZER_DISCARD);
    GLuint shader = 0, program = 0, array = 0, buffers[2] = {0};
    char *glsl = NULL;
    CompileError compile_error;
    bool valid = false;
    if (!view_shader_compile(source, SHADER_STAGE_VERTEX, 0, &glsl, &compile_error)) {
        snprintf(error->message, sizeof(error->message), "%s", compile_error.message);
        goto cleanup;
    }
    shader = glCreateShader(GL_VERTEX_SHADER);
    const char *shader_source = glsl;
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, sizeof(error->message), NULL, error->message);
        goto cleanup;
    }
    program = glCreateProgram();
    glAttachShader(program, shader);
    const char *varyings[] = {"graficPosition", "graficAuthoredNormal"};
    glTransformFeedbackVaryings(program, 2, varyings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(error->message), NULL, error->message);
        goto cleanup;
    }
    glUseProgram(program);
    glGenVertexArrays(1, &array);
    glBindVertexArray(array);
    glGenBuffers(2, buffers);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(inputs), inputs, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffers[1]);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(outputs), NULL, GL_STREAM_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffers[1]);
    const float identity[] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const char *matrices[] = {"u_model", "u_view", "u_projection"};
    for (size_t index = 0; index < 3; index += 1) {
        glUniformMatrix4fv(glGetUniformLocation(program, matrices[index]), 1, GL_FALSE, identity);
    }
    const float times[] = {0, 0.9f, 2.3f};
    float maximum_horizontal = 0;
    valid = true;
    for (size_t time = 0; valid && time < 3; time += 1) {
        glUniform1f(glGetUniformLocation(program, "u_time"), times[time]);
        glEnable(GL_RASTERIZER_DISCARD);
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, POINTS);
        glEndTransformFeedback();
        if (!old_discard) glDisable(GL_RASTERIZER_DISCARD);
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(outputs), outputs);
        for (int point = 0; valid && point < POINTS; point += 5) {
            float dx[3], dz[3];
            for (int axis = 0; axis < 3; axis += 1) {
                dx[axis] = outputs[point + 1][axis] - outputs[point + 2][axis];
                dz[axis] = outputs[point + 3][axis] - outputs[point + 4][axis];
            }
            float numerical[] = {dz[1]*dx[2] - dz[2]*dx[1], dz[2]*dx[0] - dz[0]*dx[2], dz[0]*dx[1] - dz[1]*dx[0]};
            float magnitude = sqrtf(numerical[0]*numerical[0] + numerical[1]*numerical[1] + numerical[2]*numerical[2]);
            float dot = 0, squared = 0;
            for (int axis = 0; axis < 3; axis += 1) {
                float normal = outputs[point][axis + 3];
                dot += normal * numerical[axis] / magnitude;
                squared += normal * normal;
            }
            valid = isfinite(dot) && dot > 0.9999f && fabsf(squared - 1) < 0.001f && outputs[point][4] > 0;
            float moved_x = outputs[point][0] - inputs[point][0];
            float moved_z = outputs[point][2] - inputs[point][2];
            maximum_horizontal = fmaxf(maximum_horizontal, hypotf(moved_x, moved_z));
        }
    }
    valid = valid && maximum_horizontal > 0.01f && glGetError() == GL_NO_ERROR;
    if (!valid) snprintf(error->message, sizeof(error->message), "wave normals disagree with numerical tangents, fold, or omit horizontal displacement");
cleanup:
    if (!old_discard) glDisable(GL_RASTERIZER_DISCARD);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, (GLuint)old_feedback);
    glBindVertexArray((GLuint)old_array);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)old_buffer);
    glUseProgram((GLuint)old_program);
    glDeleteBuffers(2, buffers);
    glDeleteVertexArrays(1, &array);
    glDeleteProgram(program);
    glDeleteShader(shader);
    free(glsl);
    return valid;
}

static bool pixel_equals(int x, int y, int r, int g, int b)
{
    unsigned char pixel[4];
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    if (abs((int)pixel[0] - r) > 3 || abs((int)pixel[1] - g) > 3
        || abs((int)pixel[2] - b) > 3 || pixel[3] != 255) {
        fprintf(stderr, "pixel %d,%d: expected %d,%d,%d,255, got %u,%u,%u,%u\n",
            x, y, r, g, b, pixel[0], pixel[1], pixel[2], pixel[3]);
        return false;
    }
    return true;
}

static bool set_texture(ViewRenderer *renderer, size_t slot, const char *text,
    size_t width, size_t height, ViewRenderError *error)
{
    ViewSource source = {.text = (char *)text, .length = strlen(text), .available = true};
    RenderConfig config = {.width = width, .height = height, .seed = 42};
    CompileError compile_error;
    Image image = {0};
    if (!view_texture_render(&source, 0, &config, &image, &compile_error)) {
        fprintf(stderr, "texture failed: %s\n", compile_error.message);
        return false;
    }
    bool loaded = view_renderer_set_texture(renderer, slot, &image, error);
    image_destroy(&image);
    return loaded;
}

static bool load_texture(ViewRenderer *renderer, size_t slot, const char *path, ViewRenderError *error)
{
    ViewSource source = {0};
    char read_error[256];
    bool loaded = view_source_poll(&source, path, read_error, sizeof(read_error)) == VIEW_SOURCE_CHANGED
        && set_texture(renderer, slot, source.text, 512, 512, error);
    view_source_destroy(&source);
    return loaded;
}

static bool snapshot(const char *path)
{
    unsigned char *pixels = malloc(512 * 512 * 3);
    if (pixels == NULL) {
        return false;
    }
    glReadPixels(0, 0, 512, 512, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    FILE *file = fopen(path, "wb");
    bool written = file != NULL;
    if (file != NULL) {
        written = fprintf(file, "P6\n512 512\n255\n") > 0;
        for (int y = 511; y >= 0; y -= 1) {
            written = fwrite(pixels + y * 512 * 3, 1, 512 * 3, file) == 512 * 3 && written;
        }
        written = fclose(file) == 0 && written;
    }
    free(pixels);
    return written;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL viewer renderer at line %d: %s\n", __LINE__, error.message); \
    passed = false; goto cleanup; } } while (0)

int main(int argc, char **argv)
{
    ViewRenderer renderer = {0};
    ViewRenderError error = {0};
    GLuint framebuffer = 0;
    GLuint color = 0;
    GLuint depth = 0;
    GraficMesh mesh = {0};
    bool passed = true;
    CHECK(view_renderer_init(&renderer, 64, 64, "Grafic render test", false, &error));
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    CHECK(set_source(&renderer, "vec4(surface.uv, time, 1.0)", &error));
    view_renderer_draw(&renderer, 64, 64, 0.25f);
    CHECK(pixel_equals(16, 48, 66, 193, 64));
    GLuint previous = renderer.program;
    CHECK(!view_renderer_set_shader(&renderer, "#version 330 core\ninvalid source", &error));
    CHECK(renderer.program == previous);
    view_renderer_draw(&renderer, 64, 64, 0.5f);
    CHECK(pixel_equals(16, 48, 66, 193, 128));

    CHECK(!view_renderer_set_shader(&renderer,
        "#version 330 core\nin vec3 graficUv; out vec4 fragColor;\n"
        "void main() { fragColor = vec4(graficUv, 1.0); }", &error));
    CHECK(renderer.program == previous);
    CHECK(!view_renderer_set_shader(&renderer,
        "#version 330 core\nin vec2 graficUv; uniform sampler2D tex0; out vec4 fragColor;\n"
        "void main() { fragColor = texture(tex0, graficUv); }", &error));
    CHECK(strstr(error.message, "tex0") != NULL && renderer.program == previous);
    CHECK(set_source(&renderer, "rgb(0.0, 1.0, 0.0).vec4(1.0)", &error));
    view_renderer_draw(&renderer, 64, 64, 0.0f);
    CHECK(pixel_equals(20, 20, 0, 255, 0));

    CHECK(view_renderer_set_shader(&renderer,
        "#version 330 core\nuniform vec2 u_resolution; out vec4 fragColor;\n"
        "void main() { fragColor = vec4(u_resolution / 512.0, 0.0, 1.0); }", &error));
    view_renderer_draw(&renderer, 128, 64, 0.0f);
    CHECK(pixel_equals(0, 0, 64, 32, 0));
    view_renderer_draw(&renderer, 64, 128, 0.0f);
    CHECK(pixel_equals(0, 0, 32, 64, 0));
    view_renderer_draw(&renderer, 0, 0, 0.0f);
    CHECK(pixel_equals(0, 0, 32, 64, 0));

    glfwSetWindowSize(renderer.window, 96, 48);
    glfwPollEvents();
    int width;
    int height;
    glfwGetFramebufferSize(renderer.window, &width, &height);
    CHECK(width > 0 && height > 0);
    view_renderer_draw(&renderer, width, height, 0.0f);
    CHECK(pixel_equals(0, 0, (int)(width * 255.0f / 512.0f), (int)(height * 255.0f / 512.0f), 0));

    CHECK(set_source(&renderer,
        "vec4(surface.position.x().multiply(0.5).add(0.5), surface.position.y().multiply(0.5).add(0.5), surface.position.z(), 1.0)", &error));
    view_renderer_draw(&renderer, 64, 64, 0.0f);
    CHECK(pixel_equals(16, 48, 66, 193, 0));

    CHECK(set_texture(&renderer, 0, "vec4(uv, 0.0, 1.0)", 4, 2, &error));
    CHECK(set_source(&renderer, "surface.uv.sample(tex0)", &error));
    view_renderer_draw(&renderer, 4, 2, 0.0f);
    CHECK(pixel_equals(0, 0, 32, 64, 0));
    CHECK(pixel_equals(3, 1, 223, 191, 0));
    CHECK(set_source(&renderer, "vec2(-1.0, 2.0).sample(tex0)", &error));
    view_renderer_draw(&renderer, 1, 1, 0.0f);
    CHECK(pixel_equals(0, 0, 32, 191, 0));
    CHECK(set_source(&renderer, "vec2(0.5, 0.5).sample(tex0)", &error));
    view_renderer_draw(&renderer, 1, 1, 0.0f);
    CHECK(pixel_equals(0, 0, 128, 128, 0));

    CHECK(set_texture(&renderer, 1, "vec4(0.0, 1.0, 0.0, 1.0)", 1, 1, &error));
    CHECK(set_texture(&renderer, 3, "vec4(0.0, 0.0, 1.0, 1.0)", 1, 1, &error));
    CHECK(set_source(&renderer, "surface.uv.sample(tex1).blend_add(surface.uv.sample(tex3)).multiply(vec4(0.5, 0.5, 0.5, 1.0))", &error));
    view_renderer_draw(&renderer, 4, 2, 0.0f);
    CHECK(pixel_equals(0, 0, 0, 128, 128));
    previous = renderer.program;
    CHECK(!set_source(&renderer, "surface.uv.sample(tex2)", &error));
    CHECK(strstr(error.message, "--tex2") != NULL && renderer.program == previous);
    GLuint previous_texture = renderer.textures[1];
    float white[] = {1, 1, 1, 1};
    Image invalid_image = {.width = (size_t)renderer.texture_limit + 1, .height = 1, .rgba = white};
    CHECK(!view_renderer_set_texture(&renderer, 1, &invalid_image, &error));
    CHECK(renderer.textures[1] == previous_texture);
    CHECK(!view_renderer_set_texture(&renderer, VIEW_TEXTURE_COUNT, &invalid_image, &error));
    view_renderer_draw(&renderer, 4, 2, 0.0f);
    CHECK(pixel_equals(0, 0, 0, 128, 128));
    CHECK(set_texture(&renderer, 1, "vec4(2.0, -1.0, 0.0, 1.0)", 1, 1, &error));
    CHECK(renderer.program == previous);
    view_renderer_draw(&renderer, 4, 2, 0.0f);
    CHECK(pixel_equals(0, 0, 128, 0, 128));
    CHECK(set_source(&renderer, "surface.uv.sample(tex1).blend_add(surface.uv.sample(tex3)).multiply(vec4(0.5, 0.5, 0.5, 1.0))", &error));
    view_renderer_draw(&renderer, 4, 2, 0.0f);
    CHECK(pixel_equals(0, 0, 128, 0, 128));

    CHECK(set_texture(&renderer, 0,
        "vec4(1.0, 0.0, 0.0, 1.0).show().multiply(vec4(0.0, 1.0, 0.0, 1.0))", 4, 2, &error));
    CHECK(set_source(&renderer,
        "surface.uv.sample(tex0).multiply(vec4(0.5, 1.0, 1.0, 1.0)).show().missing()", &error));
    previous = renderer.program;
    view_renderer_draw(&renderer, 4, 2, 1.0f);
    CHECK(pixel_equals(0, 0, 128, 0, 0));
    view_renderer_draw_texture(&renderer, 4, 2, 0);
    CHECK(pixel_equals(0, 0, 255, 0, 0));
    view_renderer_draw_texture(&renderer, 4, 2, 3);
    CHECK(pixel_equals(0, 0, 0, 0, 255));
    CHECK(renderer.program == previous);
    view_renderer_draw(&renderer, 4, 2, 2.0f);
    CHECK(pixel_equals(0, 0, 128, 0, 0));
    CHECK(set_source(&renderer, "surface.uv.show().missing()", &error));
    view_renderer_draw(&renderer, 4, 2, 0.0f);
    CHECK(pixel_equals(0, 0, 32, 64, 0));

    ViewSource ring = {0};
    char read_error[256];
    CHECK(view_source_poll(&ring, "examples/gl/ring.gfrag", read_error, sizeof(read_error)) == VIEW_SOURCE_CHANGED);
    bool ring_loaded = set_source(&renderer, ring.text, &error);
    view_source_destroy(&ring);
    CHECK(ring_loaded);
    view_renderer_draw(&renderer, 512, 512, 1.0f);
    CHECK(glGetError() == GL_NO_ERROR);
    if (argc > 1) {
        CHECK(snapshot(argv[1]));
    }

    CHECK(load_texture(&renderer, 0, "examples/compositions/masked-reveal/mask.gtex", &error));
    CHECK(load_texture(&renderer, 1, "examples/compositions/masked-reveal/rings.gtex", &error));
    ViewSource composition = {0};
    CHECK(view_source_poll(&composition, "examples/compositions/masked-reveal/masked-reveal.gfrag",
        read_error, sizeof(read_error)) == VIEW_SOURCE_CHANGED);
    bool composition_loaded = set_source(&renderer, composition.text, &error);
    view_source_destroy(&composition);
    CHECK(composition_loaded);
    view_renderer_draw(&renderer, 512, 512, 1.0f);
    CHECK(glGetError() == GL_NO_ERROR);
    if (argc > 2) {
        CHECK(snapshot(argv[2]));
    }

    GraficMeshDiskSpec spec = {.radius = 2.0f, .segments = 64, .rings = 4};
    const char *mesh_error;
    CHECK(graficmesh_mesh_from_disk(&spec, &mesh, &mesh_error));
    CHECK(view_renderer_set_mesh(&renderer, &mesh, &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    unsigned char center_pixel[4];
    glReadPixels(64, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center_pixel);
    CHECK(center_pixel[0] > 100 && center_pixel[0] < 155 && center_pixel[1] > 100 && center_pixel[1] < 155);
    CHECK(pixel_equals(64, 64, center_pixel[0], center_pixel[1], center_pixel[2]));
    CHECK(pixel_equals(0, 0, 31, 31, 31));
    view_renderer_draw_mesh(&renderer, 256, 128, 0.0f, false, false);
    CHECK(pixel_equals(128, 64, center_pixel[0], center_pixel[1], center_pixel[2]));
    CHECK(pixel_equals(192, 64, 31, 31, 31));
    view_renderer_draw_mesh(&renderer, 128, 256, 0.0f, false, false);
    CHECK(pixel_equals(64, 128, center_pixel[0], center_pixel[1], center_pixel[2]));
    GLuint previous_mesh = renderer.mesh.vertex_array;
    uint32_t first_index = mesh.indices[0];
    mesh.indices[0] = (uint32_t)mesh.vertex_count;
    CHECK(!view_renderer_set_mesh(&renderer, &mesh, &error));
    CHECK(renderer.mesh.vertex_array == previous_mesh);
    mesh.indices[0] = first_index;
    mesh.positions[0] = NAN;
    CHECK(!view_renderer_set_mesh(&renderer, &mesh, &error));
    CHECK(renderer.mesh.vertex_array == previous_mesh);
    mesh.positions[0] = 0.0f;
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, center_pixel[0], center_pixel[1], center_pixel[2]));
    view_renderer_draw_mesh(&renderer, 0, 0, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, center_pixel[0], center_pixel[1], center_pixel[2]));

    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, true, false);
    unsigned char wire[128 * 128 * 4];
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, wire);
    size_t lines = 0;
    size_t background = 0;
    for (size_t index = 0; index < 128 * 128; index += 1) {
        lines += wire[index * 4] > 200 && wire[index * 4 + 1] > 200;
        background += wire[index * 4] == 31 && wire[index * 4 + 1] == 31;
    }
    CHECK(lines > 100 && background > 1000);
    GLint polygon_modes[2];
    glGetIntegerv(GL_POLYGON_MODE, polygon_modes);
    CHECK(polygon_modes[0] == GL_FILL && polygon_modes[1] == GL_FILL);
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, center_pixel[0], center_pixel[1], center_pixel[2]));
    CHECK(set_source(&renderer, "vec4(0.0, 1.0, 0.0, 1.0)", &error));
    view_renderer_draw(&renderer, 128, 128, 0.0f);
    CHECK(pixel_equals(0, 0, 0, 255, 0));

    graficmesh_mesh_destroy(&mesh);
    spec.uv_mapping = GRAFIC_MESH_UV_RADIAL;
    CHECK(graficmesh_mesh_from_disk(&spec, &mesh, &mesh_error));
    CHECK(view_renderer_set_mesh(&renderer, &mesh, &error));
    CHECK(renderer.mesh.vertex_array != previous_mesh);
    view_renderer_draw_mesh(&renderer, 512, 512, 0.0f, false, false);
    CHECK(glGetError() == GL_NO_ERROR);
    if (argc > 3) {
        CHECK(snapshot(argv[3]));
    }
    view_renderer_draw_mesh(&renderer, 512, 512, 0.0f, true, false);
    if (argc > 4) {
        CHECK(snapshot(argv[4]));
    }

    graficmesh_mesh_destroy(&mesh);
    GraficMeshGridSpec grid = {2, 2, 32, 32};
    CHECK(graficmesh_mesh_from_grid(&grid, &mesh, &mesh_error));
    CHECK(view_renderer_set_mesh(&renderer, &mesh, &error));
    float original[3];
    glBindBuffer(GL_ARRAY_BUFFER, renderer.mesh.buffers[0]);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(original), original);
    CHECK(original[0] == -1 && original[1] == 0 && original[2] == -1);
    ViewSource wave = {.available = true,
        .text = "vertex(position.add(vec3(0.0, position.x().multiply(5.0).add(time).sin().multiply(0.3), 0.0)), uv)"};
    wave.length = strlen(wave.text);
    char *vertex_glsl = NULL;
    CompileError compile_error;
    CHECK(view_shader_compile(&wave, SHADER_STAGE_VERTEX, 0, &vertex_glsl, &compile_error));
    bool wave_loaded = view_renderer_set_mesh_shaders(&renderer, vertex_glsl, NULL, &error);
    free(vertex_glsl);
    CHECK(wave_loaded);
    float extent = renderer.mesh.extent;
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    unsigned char before_wave[128 * 128 * 4], after_wave[128 * 128 * 4];
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, before_wave);
    view_renderer_draw_mesh(&renderer, 128, 128, 2.0f, false, false);
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, after_wave);
    CHECK(memcmp(before_wave, after_wave, sizeof(before_wave)) != 0);
    CHECK(renderer.mesh.extent == extent);
    GLuint wave_program = renderer.mesh.program;
    CHECK(!view_renderer_set_mesh_shaders(&renderer, "invalid vertex shader", NULL, &error));
    CHECK(renderer.mesh.program == wave_program);
    view_renderer_draw_mesh(&renderer, 128, 128, 2.0f, false, false);
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, before_wave);
    CHECK(memcmp(before_wave, after_wave, sizeof(before_wave)) == 0);
    if (argc > 5) {
        view_renderer_draw_mesh(&renderer, 512, 512, 2.0f, true, false);
        CHECK(snapshot(argv[5]));
    }

    /* Fragment positions are local coordinates after displacement, before the camera. */
    CHECK(set_mesh_sources(&renderer, NULL, "vec4(surface.position.y().add(0.25), 0.0, 0.0, 1.0)", &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 64, 0, 0));
    CHECK(set_mesh_sources(&renderer, "vertex(position.add(vec3(0.0, time, 0.0)), uv)",
        "vec4(surface.position.y(), 0.0, 0.0, 1.0)", &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.25f, false, false);
    CHECK(pixel_equals(64, 64, 64, 0, 0));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.5f, false, false);
    CHECK(pixel_equals(64, 64, 128, 0, 0));

    /* A height varying from 0 to 1 across the grid produces intermediate colors. */
    CHECK(set_mesh_sources(&renderer, "vertex(vec3(position.x(), uv.x(), position.z()), uv)",
        "vec4(rgb(0.0, 0.0, 1.0).mix(rgb(1.0, 0.0, 0.0), surface.position.y()), 1.0)", &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    unsigned char gradient[128 * 128 * 4];
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, gradient);
    size_t blended = 0;
    for (size_t index = 0; index < 128 * 128; index += 1) {
        const unsigned char *pixel = &gradient[index * 4];
        if (pixel[1] == 0 && pixel[0] > 20 && pixel[2] > 20) {
            CHECK(abs((int)pixel[0] + pixel[2] - 255) <= 1);
            blended += 1;
        }
    }
    CHECK(blended > 100);
    CHECK(set_mesh_sources(&renderer, "vertex(position.add(vec3(0.0, 0.25, 0.0)).show().add(vec3(0.0, 0.25, 0.0)), uv)",
        "vec4(surface.position.y(), 0.0, 0.0, 1.0)", &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 64, 0, 0));

    /* Normal direction is local, includes displacement, and follows triangle winding. */
    const char *normal_color = "vec4(surface.normal.x().multiply(0.5).add(0.5), surface.normal.y().multiply(0.5).add(0.5), surface.normal.z().multiply(0.5).add(0.5), 1.0)";
    CHECK(set_mesh_sources(&renderer, NULL, normal_color, &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 128, 255, 128));
    CHECK(set_mesh_sources(&renderer, "vertex(vec3(position.x(), position.x().multiply(0.5), position.z()), uv)",
        normal_color, &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 70, 242, 128));

    const char *lit_color = "vec4(rgb(0.5, 0.5, 0.5).multiply(surface.normal.dot(vec3(0.0, 3.0, 0.0).normalize()).clamp(0.0, 1.0).multiply(0.8).add(0.2)), 1.0)";
    CHECK(set_mesh_sources(&renderer, NULL, lit_color, &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 128, 128, 128));
    for (size_t index = 0; index < mesh.index_count; index += 3) {
        uint32_t swap = mesh.indices[index + 1];
        mesh.indices[index + 1] = mesh.indices[index + 2];
        mesh.indices[index + 2] = swap;
    }
    CHECK(view_renderer_set_mesh(&renderer, &mesh, &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 26, 26, 26));
    CHECK(set_mesh_sources(&renderer, NULL, normal_color, &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 128, 0, 128));
    CHECK(set_source(&renderer, normal_color, &error));
    view_renderer_draw(&renderer, 64, 64, 0.0f);
    CHECK(pixel_equals(16, 48, 128, 128, 255));

    /* Structured outputs preserve authored UVs and normalize interpolated normals. */
    CHECK(set_mesh_sources(&renderer, "vertex(position, vec2(0.25, 0.75), vec3(position.x(), 1.0, 0.0))",
        "vec4(surface.uv, screen.uv.y(), 1.0)", &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 64, 191, 128));
    view_renderer_draw_mesh(&renderer, 128, 256, 0.0f, false, false);
    CHECK(pixel_equals(64, 128, 64, 191, 128));
    CHECK(set_mesh_sources(&renderer, "vertex(position, uv, vec3(position.x(), 1.0, 0.0))",
        "vec4(surface.normal.x().multiply(0.5).add(0.5), surface.normal.y().multiply(0.5).add(0.5), surface.position.x().multiply(0.5).add(0.5), 1.0)", &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    for (int x = 48; x <= 80; x += 16) {
        unsigned char pixel[4];
        glReadPixels(x, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        float nx = pixel[0] / 127.5f - 1, ny = pixel[1] / 127.5f - 1;
        float px = pixel[2] / 127.5f - 1;
        CHECK(fabsf(nx * nx + ny * ny - 1) < 0.03f);
        CHECK(fabsf(nx / ny - px) < 0.03f);
    }
    CHECK(set_mesh_sources(&renderer,
        "let p = position.show(); vertex(p.add(vec3(0.0, 0.2, 0.0)), vec2(0.2, 0.8), vec3(1.0, 0.0, 0.0))",
        "vec4(surface.uv, surface.normal.x(), 1.0)", &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 51, 204, 255));
    CHECK(set_mesh_sources(&renderer,
        "vertex(position, vec2(0.3, 0.7), vec3(0.0, 0.0, 1.0)).show()",
        "vec4(surface.uv, surface.normal.z(), 1.0)", &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 77, 179, 255));
    CHECK(set_source(&renderer, "vec4(screen.position.x().multiply(0.01), screen.position.y().multiply(0.01), 0.0, 1.0)", &error));
    view_renderer_draw(&renderer, 64, 64, 0.0f);
    CHECK(pixel_equals(0, 0, 1, 1, 0));
    CHECK(pixel_equals(10, 20, 27, 52, 0));

    CHECK(set_source(&renderer,
        "vec4(rgb(1.0, 0.0, 0.0).mix(rgb(0.0, 0.0, 1.0), screen.uv.y()), 1.0)", &error));
    view_renderer_draw(&renderer, 4, 4, 0.0f);
    CHECK(pixel_equals(0, 0, 223, 0, 32));
    CHECK(pixel_equals(0, 3, 32, 0, 223));

    /* Orthographic views are constant; perspective views depend on fragment position. */
    const char *view_color = "vec4(view.direction.multiply(0.5).add(vec3(0.5, 0.5, 0.5)), 1.0)";
    CHECK(set_source(&renderer, view_color, &error));
    view_renderer_draw(&renderer, 64, 64, 0.0f);
    CHECK(pixel_equals(0, 0, 128, 128, 255));
    CHECK(pixel_equals(63, 63, 128, 128, 255));
    glUniform1i(glGetUniformLocation(renderer.program, "u_view_is_orthographic"), GL_FALSE);
    glUniform3f(glGetUniformLocation(renderer.program, "u_view_position"), 0, 0, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK(pixel_equals(0, 0, 201, 201, 202));
    CHECK(pixel_equals(63, 63, 54, 54, 202));
    CHECK(set_mesh_sources(&renderer, NULL, view_color, &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(48, 64, 201, 201, 201));
    CHECK(pixel_equals(80, 64, 201, 201, 201));
    CHECK(set_mesh_sources(&renderer, NULL,
        "vec4(view.direction.multiply(-1.0).reflect(surface.normal).multiply(0.5).add(vec3(0.5, 0.5, 0.5)), 1.0)", &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 54, 201, 54));
    CHECK(set_source(&renderer,
        "vec4(rgb(0.25, 0.5, 0.75).add(rgb(0.25, 0.0, 0.0)).multiply(0.5.pow(2.0)), 1.0)", &error));
    view_renderer_draw(&renderer, 4, 4, 0.0f);
    CHECK(pixel_equals(0, 0, 32, 32, 48));

    CHECK(load_texture(&renderer, 0, "examples/waves/ripples.gtex", &error));
    ViewSource wave_file = {0}, water_file = {0};
    CHECK(view_source_poll(&wave_file, "examples/waves/waves.gvert", read_error, sizeof(read_error)) == VIEW_SOURCE_CHANGED);
    CHECK(view_source_poll(&water_file, "examples/waves/waves.gfrag", read_error, sizeof(read_error)) == VIEW_SOURCE_CHANGED);
    CHECK(check_wave_normals(&wave_file, &error));
    /* With static geometry, only scrolling ripple samples can change these frames. */
    CHECK(set_mesh_sources(&renderer, "vertex(position, uv, vec3(0.0, 1.0, 0.0))", water_file.text, &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, before_wave);
    view_renderer_draw_mesh(&renderer, 128, 128, 1.0f, false, false);
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, after_wave);
    CHECK(memcmp(before_wave, after_wave, sizeof(before_wave)) != 0);
    CHECK(set_texture(&renderer, 0, "vec4(0.5, 0.5, 0.5, 1.0)", 1, 1, &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, before_wave);
    CHECK(memcmp(before_wave, after_wave, sizeof(before_wave)) != 0);
    view_renderer_draw_mesh(&renderer, 128, 128, 1.0f, false, false);
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, after_wave);
    CHECK(memcmp(before_wave, after_wave, sizeof(before_wave)) == 0);
    CHECK(load_texture(&renderer, 0, "examples/waves/ripples.gtex", &error));
    bool water_loaded = set_mesh_sources(&renderer, wave_file.text, water_file.text, &error);
    view_source_destroy(&wave_file);
    view_source_destroy(&water_file);
    CHECK(water_loaded);
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, before_wave);
    view_renderer_draw_mesh(&renderer, 128, 128, 1.0f, false, false);
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, after_wave);
    CHECK(memcmp(before_wave, after_wave, sizeof(before_wave)) != 0);
    if (argc > 6) {
        view_renderer_draw_mesh(&renderer, 512, 512, 1.0f, false, false);
        CHECK(snapshot(argv[6]));
    }

    /* A farther triangle drawn last must not obscure the nearer triangle. */
    float layered_positions[] = {-1,-1,-0.5f, 1,-1,-0.5f, 0,1,-0.5f,
                                -1,-1, 0.5f, 1,-1, 0.5f, 0,1, 0.5f};
    float layered_uvs[] = {1,0, 1,0, 1,0, 0,1, 0,1, 0,1};
    uint32_t layered_indices[] = {0,1,2,3,4,5};
    GraficMesh layered = {.positions = layered_positions, .uvs = layered_uvs, .indices = layered_indices,
        .vertex_count = 6, .index_count = 6};
    CHECK(view_renderer_set_mesh(&renderer, &layered, &error));
    CHECK(view_renderer_set_mesh_shaders(&renderer,
        "#version 330 core\nlayout(location=0) in vec3 a_position; layout(location=1) in vec2 a_uv;"
        "out vec2 graficUv; void main(){graficUv=a_uv;gl_Position=vec4(a_position,1);}", NULL, &error));
    view_renderer_draw_mesh(&renderer, 128, 128, 0.0f, false, false);
    CHECK(pixel_equals(64, 64, 255, 0, 0));
    CHECK(glGetError() == GL_NO_ERROR);

cleanup:
    graficmesh_mesh_destroy(&mesh);
    if (renderer.window != NULL) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &depth);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &color);
    }
    view_renderer_destroy(&renderer);
    printf("%s rendered uniforms, textures, composition, fitted meshes, wireframe, and failure recovery\n",
        passed ? "PASS" : "FAIL");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
