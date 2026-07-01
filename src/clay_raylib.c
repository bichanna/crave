// The one place Clay and its raylib renderer are compiled. CLAY_IMPLEMENTATION
// pulls in Clay

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "raylib.h"
#include "raymath.h"

#include "clay_renderer_raylib.c"

#include "clay_backend.h"

void Clay_Raylib_SetMeasureText(Font *fonts) {
  Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);
}
