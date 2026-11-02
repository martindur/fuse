#ifndef FUSE_VIEW_RENDER_H
#define FUSE_VIEW_RENDER_H

#include <stdbool.h>
#include <stddef.h>
#include "tex/image.h"

enum { VIEW_TEXTURE_COUNT = 4 };

typedef struct GLFWwindow GLFWwindow;

typedef struct {
    GLFWwindow *window;
    unsigned int vertex_array;
    unsigned int vertex_shader;
    unsigned int program;
    unsigned int texture_program;
    int texture_uniform;
    int time_uniform;
    int resolution_uniform;
    unsigned int textures[VIEW_TEXTURE_COUNT];
    int texture_limit;
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
const char *view_texture_name(size_t slot);
/* Upload a replacement before deleting the slot's previous texture; borrows image. */
bool view_renderer_set_texture(ViewRenderer *renderer, size_t slot, const Image *image, ViewRenderError *error);
/* Draw into the current framebuffer. Zero sizes (minimized windows) are skipped. */
void view_renderer_draw(const ViewRenderer *renderer, int width, int height, float time);
/* Inspect a slot without changing the active composition program. */
void view_renderer_draw_texture(const ViewRenderer *renderer, int width, int height, size_t slot);
void view_renderer_destroy(ViewRenderer *renderer);

#endif
