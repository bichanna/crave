#include <stdlib.h>

#include "clay.h"
#include "raylib.h"

#include "app.h"
#include "clay_backend.h"
#include "config.h"
#include "fonts.h"

static void on_clay_error(Clay_ErrorData e) {
  TraceLog(LOG_ERROR, "Clay: %.*s", (int)e.errorText.length, e.errorText.chars);
}

static Font load_mem(const unsigned char *data, int len) {
  Font f = LoadFontFromMemory(".ttf", data, len, CONFIG_FONT_ATLAS, NULL, 0);
  if (f.glyphCount == 0) {
    TraceLog(LOG_WARNING, "embedded font failed to load; using the default");
    return GetFontDefault();
  }

  SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
  return f;
}

int main(void) {
  ChangeDirectory(GetApplicationDirectory());

  Clay_Raylib_Initialize(CONFIG_WINDOW_W, CONFIG_WINDOW_H, CONFIG_WINDOW_TITLE,
                         FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT |
                             FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
  SetExitKey(KEY_NULL);

  App app = {0};
  app.focus = -1;
  app.fonts[CRAVE_FONT_REGULAR] = load_mem(FONT_REGULAR, FONT_REGULAR_LEN);
  app.fonts[CRAVE_FONT_BOLD] = load_mem(FONT_BOLD, FONT_BOLD_LEN);
  app.fonts[CRAVE_FONT_ITALIC] = load_mem(FONT_ITALIC, FONT_ITALIC_LEN);

  Clay_SetMaxElementCount(CONFIG_MAX_ELEMENTS);
  uint32_t sz = Clay_MinMemorySize();
  void *mem = malloc(sz);
  Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(sz, mem);
  Clay_Initialize(
      arena,
      (Clay_Dimensions){(float)GetScreenWidth(), (float)GetScreenHeight()},
      (Clay_ErrorHandler){on_clay_error, NULL});
  Clay_Raylib_SetMeasureText(app.fonts);

  if (!app_init(&app)) {
    TraceLog(LOG_ERROR, "could not open the database :(");
    return 1;
  }

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    app.pending = (Msg){.tag = MSG_NONE};

    Clay_SetLayoutDimensions(
        (Clay_Dimensions){(float)GetScreenWidth(), (float)GetScreenHeight()});

    Vector2 mp = GetMousePosition();
    Clay_SetPointerState((Clay_Vector2){mp.x, mp.y},
                         IsMouseButtonDown(MOUSE_BUTTON_LEFT));

    Vector2 sw = GetMouseWheelMoveV();
    Clay_UpdateScrollContainers(true, (Clay_Vector2){sw.x, sw.y}, dt);

    // mouse and then keyboard
    ui_handle_mouse(&app);
    app_handle_input(&app);

    // TEA loop
    update(&app, app.pending);
    Clay_BeginLayout();
    view(&app);
    Clay_RenderCommandArray cmds = Clay_EndLayout(dt);

    BeginDrawing();
    {
      ClearBackground((Color){250, 250, 250, 255}); // same as THEME_PAPER
      Clay_Raylib_Render(cmds, app.fonts);
      ui_overlay(&app); // editable field text and stuff on top
    }
    EndDrawing();
  }

  app_shutdown(&app);
  view_free();
  ui_textures_unload();
  for (int i = 0; i < CRAVE_FONT_COUNT; i++)
    UnloadFont(app.fonts[i]);
  free(mem);
  Clay_Raylib_Close();
  return 0;
}
