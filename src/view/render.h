#ifndef FUSE_VIEW_RENDER_H
#define FUSE_VIEW_RENDER_H

#include <stdbool.h>

typedef struct GLFWwindow GLFWwindow;

typedef struct {
    GLFWwindow *window;
    unsigned int vertex_array;
    unsigned int vertex_shader;
    unsigned int program;
    int time_uniform;
    int resolution_uniform;
    bool initialized;
} ViewRenderer;

typedef struct {
    char message[4096];
} ViewRenderError;

/* Owns one GLFW runtime and context. Zero-initialize; call on the main thread. */
bool view_renderer_init(ViewRenderer *renderer, int width, int height, const char *title,
    bool visible, ViewRenderError *error);
/* A failed replacement leaves the active shader and uniforms intact. */
bool view_renderer_set_shader(ViewRenderer *renderer, const char *glsl, ViewRenderError *error);
/* Draw into the current framebuffer. Zero sizes (minimized windows) are skipped. */
void view_renderer_draw(const ViewRenderer *renderer, int width, int height, float time);
void view_renderer_destroy(ViewRenderer *renderer);

#endif
