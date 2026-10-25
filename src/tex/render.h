#ifndef FUSE_TEX_RENDER_H
#define FUSE_TEX_RENDER_H

#include "evaluate.h"
#include "image.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t width;
    size_t height;
    uint64_t seed;
} RenderConfig;

typedef struct {
    SourceSpan span;
    const char *message;
} RenderError;

bool fusetex_render(
    const Program *program,
    const char *source,
    const RenderConfig *config,
    Image *out_image,
    RenderError *out_error
);

#endif
