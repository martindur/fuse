#ifndef FUSE_TEX_IMAGE_H
#define FUSE_TEX_IMAGE_H

#include "evaluate.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t width;
    size_t height;
    float *rgba;
} Image;

/*
 * The image must be zero-initialized before its first initialization.
 * Subsequent calls replace the existing buffer and preserve it on failure.
 */
bool image_init(Image *image, size_t width, size_t height);
void image_destroy(Image *image);

bool image_store_value(
    Image *image,
    size_t x,
    size_t y,
    const RuntimeValue *value
);

const float *image_pixel(
    const Image *image,
    size_t x,
    size_t y
);

#endif
