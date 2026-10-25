#include "png_write.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <png.h>

static uint8_t to_byte(float value)
{
    if (!isfinite(value) || value <= 0.0f) {
        return 0;
    }

    if (value >= 1.0f) {
        return 255;
    }

    return (uint8_t)(value * 255.0f + 0.5f);
}

bool png_write(FILE *output, const Image *image)
{
    if (output == NULL
        || image == NULL
        || image->rgba == NULL
        || image->width == 0
        || image->height == 0
        || image->width > UINT32_MAX
        || image->height > UINT32_MAX
        || image->width > SIZE_MAX / 4) {
        return false;
    }

    png_structp png = png_create_write_struct(
        PNG_LIBPNG_VER_STRING,
        NULL,
        NULL,
        NULL
    );

    if (png == NULL) {
        return false;
    }

    png_infop info = png_create_info_struct(png);

    if (info == NULL) {
        png_destroy_write_struct(&png, NULL);
        return false;
    }

    uint8_t *row = malloc(image->width * 4);

    if (row == NULL) {
        png_destroy_write_struct(&png, &info);
        return false;
    }

    if (setjmp(png_jmpbuf(png)) != 0) {
        free(row);
        png_destroy_write_struct(&png, &info);
        return false;
    }

    png_init_io(png, output);
    png_set_IHDR(
        png,
        info,
        (png_uint_32)image->width,
        (png_uint_32)image->height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    for (size_t output_row = 0;
         output_row < image->height;
         output_row += 1) {
        size_t source_y = image->height - output_row - 1;
        const float *source = image_pixel(image, 0, source_y);

        for (size_t x = 0; x < image->width; x += 1) {
            row[x * 4 + 0] = to_byte(source[x * 4 + 0]);
            row[x * 4 + 1] = to_byte(source[x * 4 + 1]);
            row[x * 4 + 2] = to_byte(source[x * 4 + 2]);
            row[x * 4 + 3] = to_byte(source[x * 4 + 3]);
        }

        png_write_row(png, row);
    }

    png_write_end(png, info);
    free(row);
    png_destroy_write_struct(&png, &info);
    return true;
}
