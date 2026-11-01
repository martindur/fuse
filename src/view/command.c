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
    ViewRenderer renderer;
    ViewSource source;
    const char *path;
    size_t preview_line;
    bool paused;
    bool has_error;
    char last_error[4608];
} ViewApp;

static void usage(FILE *output)
{
    fputs("Usage: fuse view [OPTION]... FILE.pgl\n\n"
        "Preview a shader and reload it when its source changes.\n\n"
        "Options:\n"
        "  --width N         initial window width (default: 960)\n"
        "  --height N        initial window height (default: 640)\n"
        "  --preview-line N  display the intermediate result on a source line\n"
        "  -h, --help        show this help\n\n"
        "Space pauses or resumes animation. Escape closes the window.\n"
        "Compiler errors go to stderr; the last working shader keeps running.\n", output);
}

static bool parse_positive(const char *text, size_t *value)
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
    if (errno != 0 || parsed == 0 || parsed > SIZE_MAX) {
        return false;
    }
    *value = (size_t)parsed;
    return true;
}

static void update_title(const ViewApp *app)
{
    const char *name = strrchr(app->path, '/');
    name = name == NULL ? app->path : name + 1;
    char title[1024];
    snprintf(title, sizeof(title), "Fuse%s%s: %s | Space: %s | Esc: close",
        app->has_error ? " [shader error]" : "", app->paused ? " [paused]" : "",
        name, app->paused ? "resume" : "pause");
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
    }
}

static void report_error(ViewApp *app, const char *message)
{
    if (!app->has_error || strcmp(app->last_error, message) != 0) {
        fprintf(stderr, "fuse view: %s\n", message);
    }
    snprintf(app->last_error, sizeof(app->last_error), "%s", message);
    app->has_error = true;
    update_title(app);
}

static void reload_shader(ViewApp *app)
{
    char *glsl = NULL;
    CompileError compile_error;
    char message[4608];
    if (!view_shader_compile(&app->source, app->preview_line, &glsl, &compile_error)) {
        SourcePosition position = source_position_at(app->source.text, app->source.length, compile_error.span.offset);
        snprintf(message, sizeof(message), "%s:%zu:%zu: %s error: %s", app->path,
            position.line, position.column, compile_error.kind, compile_error.message);
        report_error(app, message);
        return;
    }
    ViewRenderError render_error;
    bool loaded = view_renderer_set_shader(&app->renderer, glsl, &render_error);
    free(glsl);
    if (!loaded) {
        snprintf(message, sizeof(message), "%s: %s", app->path, render_error.message);
        report_error(app, message);
        return;
    }
    app->has_error = false;
    app->last_error[0] = '\0';
    update_title(app);
    fprintf(stderr, "fuse view: %s: loaded\n", app->path);
}

int fuse_view_main(int argc, char **argv)
{
    ViewApp app = {0};
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
        } else if (!files_only && (strcmp(argument, "--width") == 0 || strcmp(argument, "--height") == 0
                || strcmp(argument, "--preview-line") == 0)) {
            size_t *value = strcmp(argument, "--width") == 0 ? &width
                : strcmp(argument, "--height") == 0 ? &height : &app.preview_line;
            if (index + 1 >= argc || !parse_positive(argv[++index], value)
                || (value != &app.preview_line && *value > INT_MAX)) {
                fprintf(stderr, "fuse view: invalid %s\n", argument);
                usage(stderr);
                return EXIT_FAILURE;
            }
        } else if ((!files_only && argument[0] == '-') || app.path != NULL) {
            usage(stderr);
            return EXIT_FAILURE;
        } else {
            app.path = argument;
        }
    }
    if (app.path == NULL) {
        usage(stderr);
        return EXIT_FAILURE;
    }
    char read_error[256];
    if (view_source_poll(&app.source, app.path, read_error, sizeof(read_error)) == VIEW_SOURCE_ERROR) {
        fprintf(stderr, "fuse view: %s: %s\n", app.path, read_error);
        view_source_destroy(&app.source);
        return EXIT_FAILURE;
    }
    ViewRenderError render_error;
    if (!view_renderer_init(&app.renderer, (int)width, (int)height, "Fuse", true, &render_error)) {
        fprintf(stderr, "fuse view: %s\n", render_error.message);
        view_source_destroy(&app.source);
        return EXIT_FAILURE;
    }
    glfwSetWindowUserPointer(app.renderer.window, &app);
    glfwSetKeyCallback(app.renderer.window, on_key);
    reload_shader(&app);
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
            ViewSourceStatus status = view_source_poll(&app.source, app.path, read_error, sizeof(read_error));
            if (status == VIEW_SOURCE_CHANGED) {
                reload_shader(&app);
            } else if (status == VIEW_SOURCE_ERROR) {
                char message[4608];
                snprintf(message, sizeof(message), "%s: %s", app.path, read_error);
                report_error(&app, message);
            }
            next_poll = now + 0.2;
        }
        int framebuffer_width;
        int framebuffer_height;
        glfwGetFramebufferSize(app.renderer.window, &framebuffer_width, &framebuffer_height);
        if (framebuffer_width > 0 && framebuffer_height > 0
            && !glfwGetWindowAttrib(app.renderer.window, GLFW_ICONIFIED)) {
            view_renderer_draw(&app.renderer, framebuffer_width, framebuffer_height, (float)time);
            glfwSwapBuffers(app.renderer.window);
            glfwPollEvents();
        } else {
            glfwWaitEventsTimeout(0.1);
        }
    }
    view_renderer_destroy(&app.renderer);
    view_source_destroy(&app.source);
    return EXIT_SUCCESS;
}
