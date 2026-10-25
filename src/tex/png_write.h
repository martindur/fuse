#ifndef FUSE_TEX_PNG_WRITE_H
#define FUSE_TEX_PNG_WRITE_H

#include "image.h"

#include <stdbool.h>
#include <stdio.h>

bool png_write(FILE *output, const Image *image);

#endif
