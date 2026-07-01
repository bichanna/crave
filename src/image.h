#ifndef CRAVE_IMAGE_H
#define CRAVE_IMAGE_H

#include "raylib.h"

Image crave_load_image(const char *path);
void crave_image_free(void *pixels);

#endif
