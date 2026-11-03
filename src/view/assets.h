#ifndef GRAFIC_VIEW_ASSETS_H
#define GRAFIC_VIEW_ASSETS_H

#include "grafic/compile.h"
#include "tex/render.h"
#include "mesh/mesh.h"

typedef struct {
    char *text;
    size_t length;
    bool available;
} ViewSource;

typedef enum {
    VIEW_SOURCE_UNCHANGED,
    VIEW_SOURCE_CHANGED,
    VIEW_SOURCE_ERROR
} ViewSourceStatus;

/* Zero-initialize before use. Polling compares bytes and detects atomic saves. */
ViewSourceStatus view_source_poll(ViewSource *source, const char *path, char *error, size_t capacity);
void view_source_destroy(ViewSource *source);
bool view_shader_compile(const ViewSource *source, size_t preview_line, char **out_glsl, CompileError *error);
/* Replaces a zero-initialized image on success; preserves it on failure. */
bool view_texture_render(const ViewSource *source, size_t preview_line, const RenderConfig *config, Image *out, CompileError *error);
/* Replaces a zero-initialized mesh on success; preserves it on failure. */
bool view_mesh_render(const ViewSource *source, size_t preview_line, GraficMesh *out, CompileError *error);

#endif
