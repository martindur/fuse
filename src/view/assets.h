#ifndef FUSE_VIEW_ASSETS_H
#define FUSE_VIEW_ASSETS_H

#include "fuse/compile.h"

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

#endif
