#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "image.h"

Image crave_load_image(const char *path) {
  Image img = {0};
  int w = 0, h = 0, comp = 0;
  unsigned char *data = stbi_load(path, &w, &h, &comp, 4); /* force RGBA */
  if (!data)
    return img;
  img.data = data;
  img.width = w;
  img.height = h;
  img.mipmaps = 1;
  img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  return img;
}

void crave_image_free(void *pixels) { stbi_image_free(pixels); }
