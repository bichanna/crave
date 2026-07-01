#include "app.h"

#include "config.h"
#include "image.h"
#include "raylib.h"
#include "theme.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clay.h"

#define ARENA_CHUNK 16384
#define ARENA_MIN 1024 // always leave room for one string before advancing
typedef struct ArenaChunk {
  char buf[ARENA_CHUNK];
  int off;
  struct ArenaChunk *next;
} ArenaChunk;
static ArenaChunk *g_arena_head, *g_arena_curr;

static const char *fmtf(const char *fmt, ...) {
  if (g_arena_head == NULL) {
    g_arena_head = calloc(1, sizeof *g_arena_head);
    g_arena_curr = g_arena_head;
  }
  if (g_arena_curr == NULL)
    return "";

  if (ARENA_CHUNK - g_arena_curr->off < ARENA_MIN) {
    if (g_arena_curr->next == NULL) {
      g_arena_curr->next = calloc(1, sizeof *g_arena_curr->next);
      if (g_arena_curr->next == NULL)
        return "";
    }
    g_arena_curr = g_arena_curr->next;
    g_arena_curr->off = 0;
  }

  char *dst = g_arena_curr->buf + g_arena_curr->off;
  int avail = ARENA_CHUNK - g_arena_curr->off;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(dst, (size_t)avail, fmt, ap);
  va_end(ap);

  if (n < 0)
    return "";
  if (n >= avail)
    n = avail - 1;

  g_arena_curr->off += n + 1;
  return dst;
}

static void arena_reset(void) {
  g_arena_curr = g_arena_head;
  if (g_arena_head)
    g_arena_head->off = 0;
}

static void arena_free(void) {
  for (ArenaChunk *c = g_arena_head; c;) {
    ArenaChunk *next = c->next;
    free(c);
    c = next;
  }
  g_arena_head = g_arena_curr = NULL;
}

// Click pool

typedef struct {
  App *app;
  Msg msg;
} ClickCtx;

#define CLICK_CHUNK 128
typedef struct ClickChunk {
  ClickCtx items[CLICK_CHUNK];
  struct ClickChunk *next;
} ClickChunk;

static ClickChunk *g_head, *g_curr;
static int g_idx;

static ClickCtx *click_alloc(void) {
  if (g_head == NULL) {
    g_head = calloc(1, sizeof *g_head);
    g_curr = g_head;
    g_idx = 0;
  }

  if (g_curr == NULL)
    return NULL;

  if (g_idx == CLICK_CHUNK) {
    if (g_curr->next == NULL)
      g_curr->next = calloc(1, sizeof *g_curr->next);
    g_curr = g_curr->next;
    g_idx = 0;

    if (g_curr == NULL)
      return NULL;
  }

  return &g_curr->items[g_idx++];
}

static void click_reset(void) {
  g_curr = g_head;
  g_idx = 0;
}

void view_free(void) {
  arena_free();

  for (ClickChunk *c = g_head; c;) {
    ClickChunk *next = c->next;
    free(c);
    c = next;
  }

  g_head = g_curr = NULL;
  g_idx = 0;
}

static void on_press(Clay_ElementId id, Clay_PointerData p, void *user) {
  (void)id;
  ClickCtx *c = user;
  if (p.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME)
    c->app->pending = c->msg;
}

static void dispatch(App *app, Msg msg) {
  ClickCtx *c = click_alloc();
  if (!c)
    return;

  *c = (ClickCtx){app, msg};
  Clay_OnHover(on_press, c);
}

// texture cache

#define TEX_CACHE 256
typedef struct {
  char path[CRAVE_PATH_CAP];
  Texture2D tex;
  bool ok, used;
} TexEntry;

static TexEntry g_tex[TEX_CACHE];

static Texture2D *cache_get(const char *rel) {
  if (!rel || rel[0] == '\0')
    return NULL;

  for (int i = 0; i < TEX_CACHE; i++)
    if (g_tex[i].used && strcmp(g_tex[i].path, rel) == 0)
      return g_tex[i].ok ? &g_tex[i].tex : NULL;

  for (int i = 0; i < TEX_CACHE; i++)
    if (!g_tex[i].used) {
      g_tex[i].used = true;
      snprintf(g_tex[i].path, sizeof g_tex[i].path, "%s", rel);
      Image img = crave_load_image(rel);
      if (img.data) {
        Texture2D t = LoadTextureFromImage(img);
        crave_image_free(img.data);
        g_tex[i].tex = t;
        g_tex[i].ok = true;
        return &g_tex[i].tex;
      }

      g_tex[i].ok = false;
      return NULL;
    }

  return NULL;
}

void ui_textures_unload(void) {
  for (int i = 0; i < TEX_CACHE; i++)
    if (g_tex[i].used && g_tex[i].ok)
      UnloadTexture(g_tex[i].tex);
}

// helpers

static Color rl(Clay_Color c) {
  return (Color){(unsigned char)c.r, (unsigned char)c.g, (unsigned char)c.b,
                 (unsigned char)c.a};
}

static Clay_String str(const char *s) {
  return (Clay_String){
      .isStaticallyAllocated = false, .length = (int32_t)strlen(s), .chars = s};
}

static void txt(const char *s, uint16_t size, uint16_t font, Clay_Color c) {
  CLAY_TEXT(str(s), CLAY_TEXT_CONFIG({.fontId = font,
                                      .fontSize = size,
                                      .textColor = c,
                                      .wrapMode = CLAY_TEXT_WRAP_NONE}));
}

static void txt_it(const char *s, uint16_t size, Clay_Color c) {
  CLAY_TEXT(str(s), CLAY_TEXT_CONFIG({.fontId = CRAVE_FONT_ITALIC,
                                      .fontSize = size,
                                      .textColor = c,
                                      .wrapMode = CLAY_TEXT_WRAP_NONE}));
}

static void para(const char *s, Clay_Color c) {
  CLAY_TEXT(str(s), CLAY_TEXT_CONFIG({.fontId = CRAVE_FONT_REGULAR,
                                      .fontSize = THEME_FONT_BODY,
                                      .textColor = c,
                                      .wrapMode = CLAY_TEXT_WRAP_WORDS}));
}

static void label(const char *s) {
  txt(s, THEME_FONT_LABEL, CRAVE_FONT_BOLD, THEME_SOFT);
}

typedef enum { BTN_PRIMARY, BTN_SECONDARY, BTN_DANGER } ButtonKind;

static void button(App *app, const char *id, const char *label, Msg msg,
                   ButtonKind kind) {
  Clay_Color base, hov, fg;
  uint16_t b;
  switch (kind) {
  case BTN_PRIMARY:
    base = THEME_INK;
    hov = THEME_INK_HOVER;
    fg = THEME_INVERT;
    b = 0;
    break;
  case BTN_DANGER:
    base = THEME_DANGER;
    hov = THEME_DANGER_HOVER;
    fg = THEME_INVERT;
    b = 0;
    break;
  default:
    base = THEME_CARD;
    hov = THEME_CARD_HOVER;
    fg = THEME_INK;
    b = THEME_BORDER;
    break;
  }

  CLAY(
      CLAY_SID(str(id)),
      {.layout = {.padding = {THEME_PAD_BTN_X, THEME_PAD_BTN_X, THEME_PAD_BTN_Y,
                              THEME_PAD_BTN_Y},
                  .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
       .backgroundColor = Clay_Hovered() ? hov : base,
       .border = {.color = THEME_LINE, .width = {b, b, b, b, 0}}}) {
    dispatch(app, msg);
    txt(label, THEME_FONT_BTN, CRAVE_FONT_REGULAR, fg);
  }
}

// compact square remove button
static void x_button(App *app, const char *id, Msg msg) {
  CLAY(
      CLAY_SID(str(id)),
      {.layout = {.sizing = {CLAY_SIZING_FIXED(THEME_FIELD_H),
                             CLAY_SIZING_FIXED(THEME_FIELD_H)},
                  .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
       .backgroundColor = Clay_Hovered() ? THEME_CARD_HOVER : THEME_CARD,
       .border = {.color = THEME_LINE,
                  .width = {THEME_BORDER, THEME_BORDER, THEME_BORDER,
                            THEME_BORDER, 0}}}) {
    dispatch(app, msg);
    txt("x", THEME_FONT_BODY, CRAVE_FONT_REGULAR, THEME_SOFT);
  }
}

static void image_block(App *app, const char *rel, float w, float h,
                        const char *id) {
  (void)app;
  Texture2D *t = cache_get(rel);
  if (t && t->width > 0 && t->height > 0) {
    float scale = fminf(w / (float)t->width, h / (float)t->height);
    float iw = (float)t->width * scale;
    float ih = (float)t->height * scale;
    CLAY(CLAY_SID(str(id)),
         {.layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER,
                                        CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = THEME_CARD,
          .border = {.color = THEME_LINE,
                     .width = {THEME_BORDER, THEME_BORDER, THEME_BORDER,
                               THEME_BORDER, 0}}}) {
      CLAY(
          CLAY_SID(str(fmtf("%s_i", id))),
          {.layout = {.sizing = {CLAY_SIZING_FIXED(iw), CLAY_SIZING_FIXED(ih)}},
           .image = {.imageData = t}}) {}
    }
  } else {
    CLAY(CLAY_SID(str(id)),
         {.layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER,
                                        CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = THEME_CARD_HOVER,
          .border = {.color = THEME_LINE,
                     .width = {THEME_BORDER, THEME_BORDER, THEME_BORDER,
                               THEME_BORDER, 0}}}) {
      txt_it("no image", THEME_FONT_LABEL, THEME_SOFT);
    }
  }
}

static float field_scroll(App *app, int idx, float cw, float innerW) {
  if (app->focus != idx)
    return 0;
  float caretX = (float)app->cursor * cw;
  if (caretX > innerW)
    return caretX - innerW + cw;
  return 0;
}

static void edit_field(App *app, FieldRef *fields, int idx,
                       const char *placeholder) {
  FieldRef *f = &fields[idx];
  bool focused = (app->focus == idx);
  bool empty = (f->buf[0] == '\0');

  CLAY(CLAY_IDI("F", f->key),
       {.layout = {.sizing = {CLAY_SIZING_GROW(0),
                              CLAY_SIZING_FIXED(THEME_FIELD_H)},
                   .padding = {THEME_PAD_FIELD, THEME_PAD_FIELD, 0, 0},
                   .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = THEME_FIELD,
        .border = {.color = focused ? THEME_LINE_FOCUS : THEME_LINE,
                   .width = {THEME_BORDER, THEME_BORDER, THEME_BORDER,
                             THEME_BORDER, 0}}}) {
    if (empty && !focused)
      txt_it(placeholder, THEME_FONT_BODY, THEME_SOFT);
  }
}

// fixed width wrapper
static void edit_field_w(App *app, FieldRef *fields, int idx, float w,
                         const char *placeholder, const char *wrapid) {
  CLAY(CLAY_SID(str(wrapid)),
       {.layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIT(0)}}}) {
    edit_field(app, fields, idx, placeholder);
  }
}

static void section_header(const char *s) {
  CLAY(CLAY_SID(str(fmtf("sh_%s", s))),
       {.layout = {.padding = {0, 0, THEME_GAP_SM, 0}}}) {
    txt(s, THEME_FONT_H2, CRAVE_FONT_BOLD, THEME_INK);
  }
}

static const char *truncate_desc(const char *s) {
  int len = (int)strlen(s);
  if (len <= CONFIG_CARD_DESC_MAX)
    return s;
  int cut = CONFIG_CARD_DESC_MAX;
  while (cut > 0 && s[cut] != ' ')
    cut--;
  if (cut == 0)
    cut = CONFIG_CARD_DESC_MAX;
  return fmtf("%.*s...", cut, s);
}

static void card(App *app, RecipeSummary *s, float w) {
  CLAY(CLAY_SID(str(fmtf("card%ld", s->id))),
       {.layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIT(0)},
                   .padding = {THEME_GAP_SM, THEME_GAP_SM, THEME_GAP_SM,
                               THEME_GAP_SM},
                   .childGap = THEME_GAP_SM,
                   .layoutDirection = CLAY_TOP_TO_BOTTOM},
        .backgroundColor = Clay_Hovered() ? THEME_CARD_HOVER : THEME_CARD,
        .border = {.color = THEME_LINE,
                   .width = {THEME_BORDER, THEME_BORDER, THEME_BORDER,
                             THEME_BORDER, 0}}}) {
    dispatch(app, (Msg){.tag = MSG_SHOW, .id = s->id});
    image_block(app, s->image_path, w - 2 * THEME_GAP_SM, CONFIG_CARD_IMG_H,
                fmtf("ci%ld", s->id));
    txt(s->title[0] ? s->title : "Untitled", THEME_FONT_H2, CRAVE_FONT_BOLD,
        THEME_INK);
    if (s->description[0])
      para(truncate_desc(s->description), THEME_SOFT);
    if (s->tag_count > 0)
      CLAY(CLAY_SID(str(fmtf("ct%ld", s->id))),
           {.layout = {.childGap = THEME_GAP_XS}}) {
        for (int t = 0; t < s->tag_count && t < 4; t++)
          CLAY(CLAY_SID(str(fmtf("ctg%ld_%d", s->id, t))),
               {.layout = {.padding = {6, 6, 2, 2}},
                .backgroundColor = THEME_PAPER,
                .border = {.color = THEME_LINE,
                           .width = {THEME_BORDER, THEME_BORDER, THEME_BORDER,
                                     THEME_BORDER, 0}}}) {
            txt(s->tags[t], THEME_FONT_LABEL, CRAVE_FONT_REGULAR, THEME_SOFT);
          }
      }
  }
}

// case insensitive substring test
static bool ci_contains(const char *hay, const char *needle) {
  if (!needle[0])
    return true;
  size_t nl = strlen(needle);
  for (const char *p = hay; *p; p++) {
    size_t k = 0;
    while (k < nl && p[k] &&
           tolower((unsigned char)p[k]) == tolower((unsigned char)needle[k]))
      k++;
    if (k == nl)
      return true;
  }
  return false;
}

static bool summary_matches(const RecipeSummary *s, const char *q) {
  if (!q[0])
    return true;
  if (ci_contains(s->title, q) || ci_contains(s->description, q))
    return true;
  for (int t = 0; t < s->tag_count; t++)
    if (ci_contains(s->tags[t], q))
      return true;
  return false;
}

// reused scratch buffer of matching summary indics
static int *g_filtered;
static int g_filtered_cap;

// Returns the count of matches into g_filtered or -1 if it can't allocate
static int filter_summaries(App *app) {
  if (g_filtered_cap < app->summary_count) {
    int *p = realloc(g_filtered, (size_t)app->summary_count * sizeof *p);
    if (!p)
      return -1;
    g_filtered = p;
    g_filtered_cap = app->summary_count;
  }
  int m = 0;
  for (int i = 0; i < app->summary_count; i++)
    if (summary_matches(&app->summaries[i], app->search))
      g_filtered[m++] = i;
  return m;
}

void ui_grid_free(void) {
  free(g_filtered);
  g_filtered = NULL;
  g_filtered_cap = 0;
}

static void screen_grid(App *app) {
  FieldRef fields[MAX_FIELDS];
  active_fields(app, fields); // [0] = search box

  CLAY(CLAY_ID("GHead"),
       {.layout = {
            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
            .childGap = THEME_GAP,
            .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
    txt("Crave", THEME_FONT_TITLE, CRAVE_FONT_BOLD, THEME_INK);
    edit_field(app, fields, 0, "Search recipes"); // grows to fill the middle
    button(app, "bNew", "+ New", (Msg){.tag = MSG_NEW}, BTN_PRIMARY);
  }

  float avail = (float)GetScreenWidth() - 2.0f * THEME_PAD;
  int cols = (int)(avail / (float)CONFIG_CARD_MIN_W);
  if (cols < 1)
    cols = 1;
  if (cols > 4)
    cols = 4;
  float cardw = (avail - (float)(cols - 1) * THEME_GAP) / (float)cols;

  int m = filter_summaries(app);
  bool filtered = (m >= 0);
  int visible = filtered ? m : app->summary_count;

  CLAY(CLAY_ID("Grid"),
       {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .childGap = THEME_GAP,
                   .layoutDirection = CLAY_TOP_TO_BOTTOM},
        .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
    for (int i = 0; i < visible; i += cols) {
      CLAY(CLAY_SID(str(fmtf("grow%d", i))),
           {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                       .childGap = THEME_GAP}}) {
        for (int j = i; j < i + cols && j < visible; j++) {
          int si = filtered ? g_filtered[j] : j;
          card(app, &app->summaries[si], cardw);
        }
      }
    }

    if (app->summary_count == 0)
      txt_it("No recipes yet - press + New.", THEME_FONT_BODY, THEME_SOFT);
    else if (visible == 0)
      txt_it("No recipes match your search.", THEME_FONT_BODY, THEME_SOFT);
  }
}

static void detail_stat(const char *lbl, const char *value) {
  CLAY(CLAY_SID(str(fmtf("st_%s", lbl))),
       {.layout = {.padding = {THEME_PAD_FIELD, THEME_PAD_FIELD,
                               THEME_PAD_FIELD, THEME_PAD_FIELD},
                   .childGap = THEME_GAP_XS,
                   .layoutDirection = CLAY_TOP_TO_BOTTOM},
        .backgroundColor = THEME_CARD,
        .border = {.color = THEME_LINE,
                   .width = {THEME_BORDER, THEME_BORDER, THEME_BORDER,
                             THEME_BORDER, 0}}}) {
    label(lbl);
    txt(value[0] ? value : "-", THEME_FONT_BODY, CRAVE_FONT_REGULAR, THEME_INK);
  }
}

static void screen_detail(App *app) {
  Recipe *r = &app->editor.recipe;

  CLAY(CLAY_ID("DHead"),
       {.layout = {
            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
            .childGap = THEME_GAP_SM,
            .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
    txt(r->title[0] ? r->title : "Untitled", THEME_FONT_TITLE, CRAVE_FONT_BOLD,
        THEME_INK);
    CLAY(CLAY_ID("DSpacer"),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {}
    button(app, "bBack", "Back", (Msg){.tag = MSG_BACK}, BTN_SECONDARY);
    button(app, "bDel", "Delete", (Msg){.tag = MSG_DELETE}, BTN_DANGER);
    button(app, "bEdit", "Edit", (Msg){.tag = MSG_EDIT}, BTN_PRIMARY);
  }

  CLAY(CLAY_ID("DBody"),
       {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .childGap = THEME_GAP,
                   .layoutDirection = CLAY_TOP_TO_BOTTOM},
        .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
    image_block(app, r->image_path, CONFIG_PREVIEW_W, CONFIG_PREVIEW_H, "dimg");

    CLAY(CLAY_ID("DMeta"), {.layout = {.childGap = THEME_GAP}}) {
      detail_stat("SERVINGS", app->editor.servings);
      detail_stat("PREP (MIN)", app->editor.prep);
      detail_stat("COOK (MIN)", app->editor.cook);
    }

    if (r->description[0]) {
      section_header("DESCRIPTION");
      para(r->description, THEME_INK);
    }

    if (r->ingredient_count > 0) {
      section_header("INGREDIENTS");
      for (int i = 0; i < r->ingredient_count; i++)
        CLAY(CLAY_SID(str(fmtf("di%d", i))),
             {.layout = {.childGap = THEME_GAP_SM,
                         .childAlignment = {CLAY_ALIGN_X_LEFT,
                                            CLAY_ALIGN_Y_CENTER}}}) {
          if (r->ingredients[i].amount[0])
            txt(r->ingredients[i].amount, THEME_FONT_BODY, CRAVE_FONT_BOLD,
                THEME_INK);
          txt(r->ingredients[i].name, THEME_FONT_BODY, CRAVE_FONT_REGULAR,
              THEME_INK);
        }
    }

    if (r->equipment_count > 0) {
      section_header("EQUIPMENT");
      for (int i = 0; i < r->equipment_count; i++)
        txt(r->equipment[i].name, THEME_FONT_BODY, CRAVE_FONT_REGULAR,
            THEME_INK);
    }

    if (r->step_count > 0) {
      section_header("STEPS");
      for (int i = 0; i < r->step_count; i++)
        CLAY(CLAY_SID(str(fmtf("ds%d", i))),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .childGap = THEME_GAP_SM}}) {
          txt(fmtf("%d.", i + 1), THEME_FONT_BODY, CRAVE_FONT_BOLD, THEME_SOFT);
          para(r->steps[i].text, THEME_INK);
        }
    }

    if (r->tag_count > 0) {
      section_header("TAGS");
      CLAY(CLAY_ID("DTags"), {.layout = {.childGap = THEME_GAP_XS}}) {
        for (int i = 0; i < r->tag_count; i++)
          CLAY(CLAY_SID(str(fmtf("dt%d", i))),
               {.layout = {.padding = {8, 8, 3, 3}},
                .backgroundColor = THEME_CARD,
                .border = {.color = THEME_LINE,
                           .width = {THEME_BORDER, THEME_BORDER, THEME_BORDER,
                                     THEME_BORDER, 0}}}) {
            txt(r->tags[i], THEME_FONT_LABEL, CRAVE_FONT_REGULAR, THEME_SOFT);
          }
      }
    }
  }
}

// editor

static void list_row(App *app, const char *rowid, ListId list, int item,
                     bool ingredient, FieldRef *fields, int *fi) {
  CLAY(CLAY_SID(str(rowid)),
       {.layout = {
            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
            .childGap = THEME_GAP_SM,
            .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
    if (ingredient) {
      edit_field_w(app, fields, (*fi)++, 130, "Amount", fmtf("%s_a", rowid));
      edit_field(app, fields, (*fi)++, "Ingredient");
    } else {
      edit_field(app, fields, (*fi)++, "...");
    }
    x_button(app, fmtf("%s_x", rowid),
             (Msg){.tag = MSG_LIST_DEL, .list = list, .i = item});
  }
}

static void screen_edit(App *app) {
  Editor *e = &app->editor;
  Recipe *r = &e->recipe;
  FieldRef fields[MAX_FIELDS];
  int nf = collect_fields(e, fields);
  (void)nf;
  int fi = 0;

  CLAY(CLAY_ID("EHead"),
       {.layout = {
            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
            .childGap = THEME_GAP_SM,
            .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
    txt(r->id == 0 ? "New recipe" : "Edit recipe", THEME_FONT_TITLE,
        CRAVE_FONT_BOLD, THEME_INK);
    CLAY(CLAY_ID("ESpacer"),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {}
    button(app, "bCancel", "Cancel", (Msg){.tag = MSG_CANCEL}, BTN_SECONDARY);
    button(app, "bSave", "Save", (Msg){.tag = MSG_SAVE}, BTN_PRIMARY);
  }

  CLAY(CLAY_ID("EBody"),
       {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .childGap = THEME_GAP,
                   .layoutDirection = CLAY_TOP_TO_BOTTOM},
        .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
    /* image + upload */
    CLAY(CLAY_ID("EImg"),
         {.layout = {
              .childGap = THEME_GAP,
              .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
      image_block(app, r->image_path, CONFIG_PREVIEW_W, CONFIG_PREVIEW_H,
                  "eimg");
      button(app, "bUpload", "Upload image", (Msg){.tag = MSG_PICK_IMAGE},
             BTN_SECONDARY);
    }

    label("TITLE");
    edit_field(app, fields, fi++, "Recipe name");
    label("DESCRIPTION");
    edit_field(app, fields, fi++, "Short description");

    CLAY(CLAY_ID("EMeta"), {.layout = {.childGap = THEME_GAP}}) {
      CLAY(CLAY_ID("EmS"),
           {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                       .childGap = THEME_GAP_XS,
                       .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        label("SERVINGS");
        edit_field(app, fields, fi++, "0");
      }
      CLAY(CLAY_ID("EmP"),
           {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                       .childGap = THEME_GAP_XS,
                       .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        label("PREP (MIN)");
        edit_field(app, fields, fi++, "0");
      }
      CLAY(CLAY_ID("EmC"),
           {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                       .childGap = THEME_GAP_XS,
                       .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        label("COOK (MIN)");
        edit_field(app, fields, fi++, "0");
      }
    }

    section_header("INGREDIENTS");
    for (int i = 0; i < r->ingredient_count; i++)
      list_row(app, fmtf("ei%d", i), LIST_ING, i, true, fields, &fi);
    button(app, "addIng", "+ Add ingredient",
           (Msg){.tag = MSG_LIST_ADD, .list = LIST_ING}, BTN_SECONDARY);

    section_header("EQUIPMENT");
    for (int i = 0; i < r->equipment_count; i++)
      list_row(app, fmtf("ee%d", i), LIST_EQUIP, i, false, fields, &fi);
    button(app, "addEq", "+ Add equipment",
           (Msg){.tag = MSG_LIST_ADD, .list = LIST_EQUIP}, BTN_SECONDARY);

    section_header("STEPS");
    for (int i = 0; i < r->step_count; i++)
      list_row(app, fmtf("es%d", i), LIST_STEP, i, false, fields, &fi);
    button(app, "addStep", "+ Add step",
           (Msg){.tag = MSG_LIST_ADD, .list = LIST_STEP}, BTN_SECONDARY);

    section_header("TAGS");
    for (int i = 0; i < r->tag_count; i++)
      list_row(app, fmtf("et%d", i), LIST_TAG, i, false, fields, &fi);
    button(app, "addTag", "+ Add tag",
           (Msg){.tag = MSG_LIST_ADD, .list = LIST_TAG}, BTN_SECONDARY);
  }
}

// delete modal

static void modal_delete(App *app) {
  CLAY(
      CLAY_ID("Scrim"),
      {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                  .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
       .backgroundColor = THEME_SCRIM,
       .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                    .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE}}) {
    CLAY(CLAY_ID("Modal"),
         {.layout = {.sizing = {CLAY_SIZING_FIXED(380), CLAY_SIZING_FIT(0)},
                     .padding = {THEME_PAD, THEME_PAD, THEME_PAD, THEME_PAD},
                     .childGap = THEME_GAP,
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = THEME_CARD,
          .border = {.color = THEME_LINE,
                     .width = {THEME_BORDER, THEME_BORDER, THEME_BORDER,
                               THEME_BORDER, 0}}}) {
      txt("Delete this recipe?", THEME_FONT_H2, CRAVE_FONT_BOLD, THEME_INK);
      para("This can't be undone.", THEME_SOFT);
      CLAY(CLAY_ID("MBtns"),
           {.layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                .childGap = THEME_GAP_SM,
                .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY(CLAY_ID("MSp"), {.layout = {.sizing = {CLAY_SIZING_GROW(0),
                                                    CLAY_SIZING_FIT(0)}}}) {}
        button(app, "mNo", "Cancel", (Msg){.tag = MSG_DELETE_NO},
               BTN_SECONDARY);
        button(app, "mYes", "Delete", (Msg){.tag = MSG_DELETE_YES}, BTN_DANGER);
      }
    }
  }
}

// view

void view(App *app) {
  arena_reset();

  click_reset();

  CLAY(CLAY_ID("Root"),
       {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .padding = {THEME_PAD, THEME_PAD, THEME_PAD, THEME_PAD},
                   .childGap = THEME_GAP,
                   .layoutDirection = CLAY_TOP_TO_BOTTOM},
        .backgroundColor = THEME_PAPER}) {
    switch (app->screen) {
    case SCREEN_DETAIL:
      screen_detail(app);
      break;
    case SCREEN_EDIT:
      screen_edit(app);
      break;
    default:
      screen_grid(app);
      break;
    }
    if (app->confirm_delete)
      modal_delete(app);
  }
}

// mouse + overlay

static int g_drag = -1;

void ui_handle_mouse(App *app) {
  if (app->screen != SCREEN_EDIT && app->screen != SCREEN_GRID) {
    g_drag = -1;
    return;
  }
  Vector2 mp = GetMousePosition();
  Font font = app->fonts[CRAVE_FONT_REGULAR];
  float fs = THEME_FONT_BODY;
  float cw = MeasureTextEx(font, "W", fs, 0).x;
  if (cw <= 0)
    cw = fs * 0.6f;
  float pad = THEME_PAD_FIELD;
  FieldRef fields[MAX_FIELDS];
  int n = active_fields(app, fields);

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    static double last_click_t = -1e9;
    static int last_click_field = -1;
    g_drag = -1;
    bool hit = false;
    for (int i = 0; i < n; i++) {
      Clay_ElementData ed = Clay_GetElementData(CLAY_IDI("F", fields[i].key));
      if (!ed.found)
        continue;
      Clay_BoundingBox b = ed.boundingBox;
      if (mp.x >= b.x && mp.x <= b.x + b.width && mp.y >= b.y &&
          mp.y <= b.y + b.height) {
        float scroll = field_scroll(app, i, cw, b.width - 2 * pad);
        int idx = (int)((mp.x - (b.x + pad - scroll)) / cw + 0.5f);
        int len = (int)strlen(fields[i].buf);
        if (idx < 0)
          idx = 0;
        if (idx > len)
          idx = len;
        app->focus = i;
        double now = GetTime();
        bool dbl =
            (now - last_click_t < CONFIG_DOUBLE_CLICK) && last_click_field == i;
        if (dbl) {
          ed_select_word(app, fields[i].buf, idx);
          g_drag = -1;
          last_click_t = -1e9;
        } else {
          app->cursor = idx;
          app->sel_anchor = idx;
          g_drag = i;
          last_click_t = now;
          last_click_field = i;
        }
        hit = true;
        break;
      }
    }

    if (!hit) { // clicked empty space or a button: drop focus
      app->focus = -1;
      app->cursor = 0;
      app->sel_anchor = 0;
      last_click_field = -1;
    }
  } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && g_drag >= 0 &&
             g_drag < n) {
    Clay_ElementData ed =
        Clay_GetElementData(CLAY_IDI("F", fields[g_drag].key));
    if (ed.found) {
      Clay_BoundingBox b = ed.boundingBox;
      float scroll = field_scroll(app, g_drag, cw, b.width - 2 * pad);
      int idx = (int)((mp.x - (b.x + pad - scroll)) / cw + 0.5f);
      int len = (int)strlen(fields[g_drag].buf);
      if (idx < 0)
        idx = 0;
      if (idx > len)
        idx = len;
      app->cursor = idx;
    }
  } else if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    g_drag = -1;
  }
}

void ui_overlay(App *app) {
  if (app->screen != SCREEN_EDIT && app->screen != SCREEN_GRID)
    return;
  FieldRef fields[MAX_FIELDS];
  int n = active_fields(app, fields);
  Font font = app->fonts[CRAVE_FONT_REGULAR];
  float fs = THEME_FONT_BODY;
  float cw = MeasureTextEx(font, "W", fs, 0).x;
  if (cw <= 0)
    cw = fs * 0.6f;
  float pad = THEME_PAD_FIELD;

  // restart the blink whenever focus/caret/selection/length changes, so the
  // caret is solid while moving or typing and only blinks once idle :) I'm
  // quite proud of this one
  static int bp_focus = -2, bp_cur = -1, bp_sel = -1, bp_len = -1;
  static double blink_base = 0.0;
  int cur_len = (app->focus >= 0 && app->focus < n)
                    ? (int)strlen(fields[app->focus].buf)
                    : -1;
  if (app->focus != bp_focus || app->cursor != bp_cur ||
      app->sel_anchor != bp_sel || cur_len != bp_len) {
    blink_base = GetTime();
    bp_focus = app->focus;
    bp_cur = app->cursor;
    bp_sel = app->sel_anchor;
    bp_len = cur_len;
  }
  bool caret_on = fmod(GetTime() - blink_base, CONFIG_CARET_PERIOD) <
                  CONFIG_CARET_PERIOD / 2;

  for (int i = 0; i < n; i++) {
    Clay_ElementData ed = Clay_GetElementData(CLAY_IDI("F", fields[i].key));
    if (!ed.found)
      continue;
    Clay_BoundingBox b = ed.boundingBox;
    float innerW = b.width - 2 * pad;
    if (innerW < 1)
      continue;
    const char *text = fields[i].buf;
    bool focused = (app->focus == i);
    float scroll = field_scroll(app, i, cw, innerW);
    float tx = b.x + pad - scroll;
    float ty = b.y + (b.height - fs) / 2.0f;

    BeginScissorMode((int)(b.x + pad), (int)b.y, (int)innerW, (int)b.height);
    if (focused && app->sel_anchor != app->cursor) {
      int lo = app->sel_anchor < app->cursor ? app->sel_anchor : app->cursor;
      int hi = app->sel_anchor < app->cursor ? app->cursor : app->sel_anchor;
      DrawRectangle((int)(tx + lo * cw), (int)ty, (int)((hi - lo) * cw),
                    (int)fs, rl(THEME_SELECT));
    }
    if (text[0])
      DrawTextEx(font, text, (Vector2){tx, ty}, fs, 0, rl(THEME_INK));
    if (focused && caret_on)
      DrawRectangle((int)(tx + (float)app->cursor * cw), (int)ty, 2, (int)fs,
                    rl(THEME_INK));
    EndScissorMode();
  }
}
