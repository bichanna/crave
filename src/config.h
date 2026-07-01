#ifndef CRAVE_CONFIG_H
#define CRAVE_CONFIG_H

// Build-time configs

#define CONFIG_WINDOW_W 980
#define CONFIG_WINDOW_H 720
#define CONFIG_WINDOW_TITLE "Crave"

#define CONFIG_DB_PATH "crave.db"
#define CONFIG_IMAGE_DIR "images"

#define CONFIG_FONT_ATLAS 94      // px the embedded TTFs are rasterised at
#define CONFIG_MAX_ELEMENTS 16384 // Clay element budget

#define CONFIG_THUMB 56      // grid thumbnail size (px)
#define CONFIG_PREVIEW_W 240 // recipe image preview (px)
#define CONFIG_PREVIEW_H 160

#endif
