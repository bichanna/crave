#ifndef CRAVE_CLAY_BACKEND_H
#define CRAVE_CLAY_BACKEND_H

// public surface of the Clay -> raylib backend. The renderer
// (thirdparty/clay/clay_renderer_raylib.c) is compiled exactly once, from
// clay_raylib.c together with the Clay implementation

#include "clay.h"
#include "raylib.h"

void Clay_Raylib_Initialize(int width, int height, const char *title,
                            unsigned int flags);
void Clay_Raylib_Close(void);
void Clay_Raylib_Render(Clay_RenderCommandArray commands, Font *fonts);
void Clay_Raylib_SetMeasureText(Font *fonts);

#endif
