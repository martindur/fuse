#include "image.h"

#include <stdint.h>
#include <stdlib.h>

static float *pixel_at(
    Image *image,
    size_t x,
    size_t y
)
{
    return &image->rgba[(y * image->width + x) * 4];
}

bool image_init(Image *image, size_t width, size_t height)
{
    if (image == NULL || width == 0 || height == 0) {
        return false;
    }

    if (width > SIZE_MAX / height
        || width * height > SIZE_MAX / (4 * sizeof(float))) {
        return false;
    }

    float *rgba = calloc(
        width * height * 4,
        sizeof(*rgba)
    );

    if (rgba == NULL) {
        return false;
    }

    free(image->rgba);
    image->rgba = rgba;
    image->width = width;
    image->height = height;
    return true;
}

void image_destroy(Image *image)
{
    if (image == NULL) {
        return;
    }

    free(image->rgba);
    *image = (Image){0};
}

bool image_store_value(
    Image *image,
    size_t x,
    size_t y,
    const RuntimeValue *value
)
{
    if (image == NULL
        || image->rgba == NULL
        || value == NULL
        || x >= image->width
        || y >= image->height) {
        return false;
    }

    float *pixel = pixel_at(image, x, y);

    switch (value->type) {
    case VALUE_TYPE_FLOAT:
        pixel[0] = value->components[0];
        pixel[1] = value->components[0];
        pixel[2] = value->components[0];
        pixel[3] = 1.0f;
        return true;

    case VALUE_TYPE_VEC2:
        pixel[0] = value->components[0];
        pixel[1] = value->components[1];
        pixel[2] = 0.0f;
        pixel[3] = 1.0f;
        return true;

    case VALUE_TYPE_VEC3:
        pixel[0] = value->components[0];
        pixel[1] = value->components[1];
        pixel[2] = value->components[2];
        pixel[3] = 1.0f;
        return true;

    case VALUE_TYPE_VEC4:
        pixel[0] = value->components[0];
        pixel[1] = value->components[1];
        pixel[2] = value->components[2];
        pixel[3] = value->components[3];
        return true;

    case VALUE_TYPE_INVALID:
    case VALUE_TYPE_TEXTURE:
    case VALUE_TYPE_MESH:
        return false;
    }

    return false;
}

const float *image_pixel(
    const Image *image,
    size_t x,
    size_t y
)
{
    if (image == NULL
        || image->rgba == NULL
        || x >= image->width
        || y >= image->height) {
        return NULL;
    }

    return &image->rgba[(y * image->width + x) * 4];
}
