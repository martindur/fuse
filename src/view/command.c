#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "cli/commands.h"
#include "assets.h"
#include "render.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *label;
    const char *path;
    ViewSource source;
    char error[4608];
} ViewInput;

typedef struct {
    ViewRenderer renderer;
    ViewInput primary;
    ViewInput textures[VIEW_TEXTURE_COUNT];
    RenderConfig texture_config;
    size_t preview_line;
    bool paused;
    bool shader_pending;
    bool texture_only;
    int selected_texture;
} ViewApp;

static void usage(FILE *output)
{
    fputs("Usage: fuse view [OPTION]... FILE.pgl|FILE.ptex\n\n"
        "Preview Fuse shader (.pgl) and texture (.ptex) source files with live reload.\n\n"
        "Options:\n"
        "  --width N           initial window width (default: 960)\n"
        "  --height N          initial window height (default: 640)\n"
        "  --preview-line N    display the intermediate result on a line\n"
        "  --tex0 FILE.ptex    bind a procedural texture to tex0\n"
        "  --tex1 FILE.ptex    bind a procedural texture to tex1\n"
        "  --tex2 FILE.ptex    bind a procedural texture to tex2\n"
        "  --tex3 FILE.ptex    bind a procedural texture to tex3\n"
        "  --texture-width N   generated texture width (default: 512)\n"
        "  --texture-height N  generated texture height (default: 512)\n"
        "  --seed N            texture noise seed (default: 42)\n"
        "  -h, --help          show this help\n\n"
        "1: shader; 2-5: supplied tex0-tex3. Space: pause; Escape: close.\n"
        "Errors go to stderr; the last working shader and textures keep running.\n", output);
}

static bool parse_unsigned(const char *text, uintmax_t *value)
{
    if (text[0] == '\0') {
        return false;
    }
    for (const char *cursor = text; *cursor != '\0'; cursor += 1) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
    }
    errno = 0;
    uintmax_t parsed = strtoumax(text, NULL, 10);
    if (errno != 0) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool parse_positive(const char *text, size_t *value)
{
    uintmax_t parsed;
    if (!parse_unsigned(text, &parsed) || parsed == 0 || parsed > SIZE_MAX) {
        return false;
    }
    *value = (size_t)parsed;
    return true;
}

static bool has_errors(const ViewApp *app)
{
    if (app->primary.error[0] != '\0') {
        return true;
    }
    for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
        if (app->textures[slot].error[0] != '\0') {
            return true;
        }
    }
    return false;
}

static void update_title(const ViewApp *app)
{
    const char *name = strrchr(app->primary.path, '/');
    name = name == NULL ? app->primary.path : name + 1;
    char title[1024];
    const char *output = app->texture_only ? "texture"
        : app->selected_texture < 0 ? "shader" : view_texture_name((size_t)app->selected_texture);
    const char *controls = app->texture_only ? "" : "1: shader  2-5: textures | ";
    snprintf(title, sizeof(title), "Fuse%s%s: %s [%s] | %sSpace: %s | Esc: close",
        has_errors(app) ? " [error]" : "", app->paused ? " [paused]" : "",
        name, output, controls, app->paused ? "resume" : "pause");
    glfwSetWindowTitle(app->renderer.window, title);
}

static void on_key(GLFWwindow *window, int key, int scancode, int action, int modifiers)
{
    (void)scancode;
    (void)modifiers;
    if (action != GLFW_PRESS) {
        return;
    }
    ViewApp *app = glfwGetWindowUserPointer(window);
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_SPACE) {
        app->paused = !app->paused;
        update_title(app);
    } else if (!app->texture_only && key >= GLFW_KEY_1 && key <= GLFW_KEY_5) {
        int slot = key - GLFW_KEY_2;
        if (slot < 0 || app->textures[slot].path != NULL) {
            app->selected_texture = slot;
            update_title(app);
        }
    }
}

static void report_error(ViewApp *app, ViewInput *input, const char *message)
{
    if (strcmp(input->error, message) != 0) {
        fprintf(stderr, "fuse view: %s: %s\n", input->label, message);
    }
    snprintf(input->error, sizeof(input->error), "%s", message);
    if (app->renderer.window != NULL) {
        update_title(app);
    }
}

static void report_compile_error(ViewApp *app, ViewInput *input, const CompileError *error)
{
    SourcePosition position = source_position_at(input->source.text, input->source.length, error->span.offset);
    char message[4608];
    snprintf(message, sizeof(message), "%s:%zu:%zu: %s error: %s", input->path,
        position.line, position.column, error->kind, error->message);
    report_error(app, input, message);
}

static ViewSourceStatus poll_input(ViewApp *app, ViewInput *input)
{
    char error[256];
    ViewSourceStatus status = view_source_poll(&input->source, input->path, error, sizeof(error));
    if (status == VIEW_SOURCE_ERROR) {
        char message[4608];
        snprintf(message, sizeof(message), "%s: %s", input->path, error);
        report_error(app, input, message);
    }
    return status;
}

static void reload_shader(ViewApp *app)
{
    char *glsl = NULL;
    CompileError compile_error;
    char message[4608];
    app->shader_pending = true;
    if (!view_shader_compile(&app->primary.source, app->preview_line, &glsl, &compile_error)) {
        report_compile_error(app, &app->primary, &compile_error);
        return;
    }
    ViewRenderError render_error;
    bool loaded = view_renderer_set_shader(&app->renderer, glsl, &render_error);
    free(glsl);
    if (!loaded) {
        snprintf(message, sizeof(message), "%s: %s", app->primary.path, render_error.message);
        report_error(app, &app->primary, message);
        return;
    }
    app->shader_pending = false;
    app->primary.error[0] = '\0';
    update_title(app);
    fprintf(stderr, "fuse view: %s: loaded\n", app->primary.path);
}

static bool reload_texture(ViewApp *app, size_t slot)
{
    ViewInput *input = app->texture_only ? &app->primary : &app->textures[slot];
    Image image = {0};
    CompileError compile_error;
    if (!view_texture_render(&input->source, app->texture_only ? app->preview_line : 0, &app->texture_config, &image, &compile_error)) {
        report_compile_error(app, input, &compile_error);
        return false;
    }
    ViewRenderError render_error;
    bool uploaded = view_renderer_set_texture(&app->renderer, slot, &image, &render_error);
    image_destroy(&image);
    if (!uploaded) {
        char message[4608];
        snprintf(message, sizeof(message), "%s: %s", input->path, render_error.message);
        report_error(app, input, message);
        return false;
    }
    input->error[0] = '\0';
    update_title(app);
    fprintf(stderr, "fuse view: %s: %s: loaded\n", input->label, input->path);
    return true;
}

int fuse_view_main(int argc, char **argv)
{
    ViewApp app = {.primary = {.label = "shader"}, .shader_pending = true, .selected_texture = -1,
        .texture_config = {.width = 512, .height = 512, .seed = 42}};
    for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
        app.textures[slot].label = view_texture_name(slot);
    }
    size_t width = 960;
    size_t height = 640;
    bool files_only = false;
    for (int index = 1; index < argc; index += 1) {
        const char *argument = argv[index];
        if (!files_only && (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0)) {
            usage(stdout);
            return EXIT_SUCCESS;
        }
        if (!files_only && strcmp(argument, "--") == 0) {
            files_only = true;
        } else if (!files_only && strncmp(argument, "--tex", 5) == 0
            && argument[5] >= '0' && argument[5] < '0' + VIEW_TEXTURE_COUNT && argument[6] == '\0') {
            size_t slot = (size_t)(argument[5] - '0');
            if (app.textures[slot].path != NULL || index + 1 >= argc || argv[index + 1][0] == '-') {
                fprintf(stderr, "fuse view: invalid or repeated %s\n", argument);
                usage(stderr);
                return EXIT_FAILURE;
            }
            app.textures[slot].path = argv[++index];
        } else if (!files_only && strcmp(argument, "--seed") == 0) {
            uintmax_t seed;
            if (index + 1 >= argc || !parse_unsigned(argv[++index], &seed) || seed > UINT64_MAX) {
                fprintf(stderr, "fuse view: invalid --seed\n");
                usage(stderr);
                return EXIT_FAILURE;
            }
            app.texture_config.seed = (uint64_t)seed;
        } else if (!files_only && (strcmp(argument, "--width") == 0 || strcmp(argument, "--height") == 0
                || strcmp(argument, "--preview-line") == 0 || strcmp(argument, "--texture-width") == 0
                || strcmp(argument, "--texture-height") == 0)) {
            size_t *value = strcmp(argument, "--width") == 0 ? &width
                : strcmp(argument, "--height") == 0 ? &height
                : strcmp(argument, "--texture-width") == 0 ? &app.texture_config.width
                : strcmp(argument, "--texture-height") == 0 ? &app.texture_config.height : &app.preview_line;
            if (index + 1 >= argc || !parse_positive(argv[++index], value)
                || (value != &app.preview_line && *value > INT_MAX)) {
                fprintf(stderr, "fuse view: invalid %s\n", argument);
                usage(stderr);
                return EXIT_FAILURE;
            }
        } else if ((!files_only && argument[0] == '-') || app.primary.path != NULL) {
            usage(stderr);
            return EXIT_FAILURE;
        } else {
            app.primary.path = argument;
        }
    }
    if (app.primary.path == NULL) {
        usage(stderr);
        return EXIT_FAILURE;
    }
    const char *extension = strrchr(app.primary.path, '.');
    app.texture_only = extension != NULL && strcmp(extension, ".ptex") == 0;
    if (app.texture_only) {
        app.primary.label = "texture";
        app.selected_texture = 0;
        for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
            if (app.textures[slot].path != NULL) {
                fprintf(stderr, "fuse view: --texN inputs require a shader file\n");
                return EXIT_FAILURE;
            }
        }
    }
    int result = EXIT_FAILURE;
    if (poll_input(&app, &app.primary) == VIEW_SOURCE_ERROR) {
        goto cleanup;
    }
    bool has_textures = app.texture_only;
    for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
        if (app.textures[slot].path != NULL) {
            has_textures = true;
            if (poll_input(&app, &app.textures[slot]) == VIEW_SOURCE_ERROR) {
                goto cleanup;
            }
        }
    }
    ViewRenderError render_error;
    if (!view_renderer_init(&app.renderer, (int)width, (int)height, "Fuse", true, &render_error)) {
        fprintf(stderr, "fuse view: %s\n", render_error.message);
        goto cleanup;
    }
    if (has_textures && (app.texture_config.width > (size_t)app.renderer.texture_limit
            || app.texture_config.height > (size_t)app.renderer.texture_limit)) {
        fprintf(stderr, "fuse view: texture dimensions exceed GPU limit (%d)\n", app.renderer.texture_limit);
        goto cleanup;
    }
    glfwSetWindowUserPointer(app.renderer.window, &app);
    glfwSetKeyCallback(app.renderer.window, on_key);
    for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
        if (app.textures[slot].path != NULL) {
            reload_texture(&app, slot);
        }
    }
    if (app.texture_only) {
        reload_texture(&app, 0);
    } else {
        reload_shader(&app);
    }
    double previous = glfwGetTime();
    double next_poll = previous + 0.2;
    double time = 0.0;
    while (!glfwWindowShouldClose(app.renderer.window)) {
        double now = glfwGetTime();
        if (!app.paused) {
            time += now - previous;
        }
        previous = now;
        if (now >= next_poll) {
            bool primary_changed = poll_input(&app, &app.primary) == VIEW_SOURCE_CHANGED;
            if (primary_changed && app.texture_only) {
                reload_texture(&app, 0);
            }
            bool texture_loaded = false;
            for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
                if (app.textures[slot].path != NULL
                    && poll_input(&app, &app.textures[slot]) == VIEW_SOURCE_CHANGED) {
                    texture_loaded = reload_texture(&app, slot) || texture_loaded;
                }
            }
            if (!app.texture_only && (primary_changed
                    || (texture_loaded && app.shader_pending && app.primary.source.available))) {
                reload_shader(&app);
            }
            next_poll = now + 0.2;
        }
        int framebuffer_width;
        int framebuffer_height;
        glfwGetFramebufferSize(app.renderer.window, &framebuffer_width, &framebuffer_height);
        if (framebuffer_width > 0 && framebuffer_height > 0
            && !glfwGetWindowAttrib(app.renderer.window, GLFW_ICONIFIED)) {
            if (app.selected_texture >= 0) {
                view_renderer_draw_texture(&app.renderer, framebuffer_width, framebuffer_height,
                    (size_t)app.selected_texture);
            } else {
                view_renderer_draw(&app.renderer, framebuffer_width, framebuffer_height, (float)time);
            }
            glfwSwapBuffers(app.renderer.window);
            glfwPollEvents();
        } else {
            glfwWaitEventsTimeout(0.1);
        }
    }
    result = EXIT_SUCCESS;
cleanup:
    view_renderer_destroy(&app.renderer);
    view_source_destroy(&app.primary.source);
    for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
        view_source_destroy(&app.textures[slot].source);
    }
    return result;
}
