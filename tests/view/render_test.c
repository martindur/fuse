#define GL_GLEXT_PROTOTYPES
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include "view/assets.h"
#include "view/render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool set_source(ViewRenderer *renderer, const char *text, ViewRenderError *error)
{
    ViewSource source = {.text = (char *)text, .length = strlen(text), .available = true};
    char *glsl = NULL;
    CompileError compile_error;
    if (!view_shader_compile(&source, 0, &glsl, &compile_error)) {
        fprintf(stderr, "source failed: %s\n", compile_error.message);
        return false;
    }
    bool loaded = view_renderer_set_shader(renderer, glsl, error);
    free(glsl);
    return loaded;
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
    bool passed = true;
    CHECK(view_renderer_init(&renderer, 64, 64, "Fuse render test", false, &error));
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    CHECK(set_source(&renderer, "vec4(uv, time, 1.0)", &error));
    view_renderer_draw(&renderer, 64, 64, 0.25f);
    CHECK(pixel_equals(16, 48, 66, 193, 64));
    GLuint previous = renderer.program;
    CHECK(!view_renderer_set_shader(&renderer, "#version 330 core\ninvalid source", &error));
    CHECK(renderer.program == previous);
    view_renderer_draw(&renderer, 64, 64, 0.5f);
    CHECK(pixel_equals(16, 48, 66, 193, 128));

    CHECK(!view_renderer_set_shader(&renderer,
        "#version 330 core\nin vec3 fuseUv; out vec4 fragColor;\n"
        "void main() { fragColor = vec4(fuseUv, 1.0); }", &error));
    CHECK(renderer.program == previous);
    CHECK(!view_renderer_set_shader(&renderer,
        "#version 330 core\nin vec2 fuseUv; uniform sampler2D tex0; out vec4 fragColor;\n"
        "void main() { fragColor = texture(tex0, fuseUv); }", &error));
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

    ViewSource ring = {0};
    char read_error[256];
    CHECK(view_source_poll(&ring, "examples/gl/ring.pgl", read_error, sizeof(read_error)) == VIEW_SOURCE_CHANGED);
    bool ring_loaded = set_source(&renderer, ring.text, &error);
    view_source_destroy(&ring);
    CHECK(ring_loaded);
    view_renderer_draw(&renderer, 512, 512, 1.0f);
    CHECK(glGetError() == GL_NO_ERROR);
    if (argc > 1) {
        CHECK(snapshot(argv[1]));
    }

cleanup:
    if (renderer.window != NULL) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &color);
    }
    view_renderer_destroy(&renderer);
    printf("%s rendered UVs/time/resolution, resize, failure preservation, and recovery\n", passed ? "PASS" : "FAIL");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
