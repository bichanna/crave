#include "app.h"

#include "config.h"
#include "db.h"
#include "model.h"
#include "raylib.h"
#include "tinyfiledialogs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// summaries

void app_reload(App *app) {
  if (app->summaries == NULL) {
    app->summary_cap = 16;
    app->summaries =
        malloc((size_t)(app->summary_cap * sizeof *app->summaries));
    if (app->summaries == NULL) {
      app->summary_cap = 0;
      app->summary_count = 0;
      return;
    }
  }

  for (;;) {
    int n = db_load_summaries(app->db, app->summaries, app->summary_cap);
    if (n < app->summary_cap) {
      app->summary_count = (n < 0) ? 0 : n;
      return;
    }

    int cap = app->summary_cap * 2;
    RecipeSummary *grown =
        realloc(app->summaries, (size_t)(cap * sizeof *grown));
    if (grown == NULL) {
      app->summary_count = app->summary_cap;
      return;
    }

    app->summaries = grown;
    app->summary_cap = cap;
  }
}

// editor <-> recipe

static void editor_new(Editor *e) { memset(e, 0, sizeof *e); }

static void editor_load(Editor *e, const Recipe *r) {
  memset(e, 0, sizeof *e);
  e->recipe = *r;
  snprintf(e->servings, sizeof e->servings, "%d", r->servings);
  snprintf(e->prep, sizeof e->prep, "%d", r->prep_mins);
  snprintf(e->cook, sizeof e->cook, "%d", r->cook_mins);
}

static void editor_commit(Editor *e) {
  e->recipe.servings = atoi(e->servings);
  e->recipe.prep_mins = atoi(e->prep);
  e->recipe.cook_mins = atoi(e->cook);
}

static bool open_recipe(App *app, long id) {
  Recipe r;
  if (!db_load_recipe(app->db, id, &r))
    return false;
  editor_load(&app->editor, &r);
  return true;
}

// field list

int collect_fields(Editor *e, FieldRef *o) {
  Recipe *r = &e->recipe;
  int n = 0;
  o[n++] = (FieldRef){r->title, CRAVE_TITLE_CAP, false, FK_SCALAR, 0, 1};
  o[n++] = (FieldRef){r->description, CRAVE_DESC_CAP, false, FK_SCALAR, 1, 2};
  o[n++] = (FieldRef){e->servings, EDITOR_NUM_CAP, true, FK_SCALAR, 2, 3};
  o[n++] = (FieldRef){e->prep, EDITOR_NUM_CAP, true, FK_SCALAR, 3, 4};
  o[n++] = (FieldRef){e->cook, EDITOR_NUM_CAP, true, FK_SCALAR, 4, 5};
  for (int i = 0; i < r->ingredient_count; i++) {
    o[n++] = (FieldRef){
        r->ingredients[i].amount, CRAVE_AMOUNT_CAP, false, FK_ING_AMOUNT, i,
        (uint32_t)(100 + 2 * i)};
    o[n++] = (FieldRef){
        r->ingredients[i].name, CRAVE_NAME_CAP, false, FK_ING_NAME, i,
        (uint32_t)(101 + 2 * i)};
  }
  for (int i = 0; i < r->equipment_count; i++) {
    o[n++] =
        (FieldRef){r->equipment[i].name, CRAVE_NAME_CAP, false, FK_EQUIP, i,
                   (uint32_t)(2000 + i)};
  }
  for (int i = 0; i < r->tag_count; i++) {
    o[n++] = (FieldRef){r->tags[i], CRAVE_TAG_CAP,       false, FK_TAG,
                        i,          (uint32_t)(3000 + i)};
  }
  return n;
}

// list add/remove

static int list_cap(ListId l) {
  switch (l) {
  case LIST_ING:
    return CRAVE_MAX_INGREDIENTS;
  case LIST_EQUIP:
    return CRAVE_MAX_EQUIPMENT;
  case LIST_STEP:
    return CRAVE_MAX_STEPS;
  case LIST_TAG:
    return CRAVE_MAX_TAGS;
  default:
    return 0;
  }
}

static FieldKind list_first_kind(ListId l) {
  switch (l) {
  case LIST_ING:
    return FK_ING_AMOUNT;
  case LIST_EQUIP:
    return FK_EQUIP;
  case LIST_STEP:
    return FK_STEP;
  case LIST_TAG:
    return FK_TAG;
  default:
    return FK_SCALAR;
  }
}

static void list_add(Recipe *r, ListId l) {
  switch (l) {
  case LIST_ING:
    if (r->ingredient_count < CRAVE_MAX_INGREDIENTS)
      memset(&r->ingredients[r->ingredient_count++], 0,
             sizeof r->ingredients[0]);
    break;
  case LIST_EQUIP:
    if (r->equipment_count < CRAVE_MAX_EQUIPMENT)
      memset(&r->equipment[r->equipment_count++], 0, sizeof r->equipment[0]);
    break;
  case LIST_STEP:
    if (r->step_count < CRAVE_MAX_STEPS)
      memset(&r->steps[r->equipment_count++], 0, sizeof r->steps[0]);
    break;
  case LIST_TAG:
    if (r->tag_count < CRAVE_MAX_TAGS)
      r->tags[r->tag_count++][0] = '\0';
    break;
  }
}

static void list_del(Recipe *r, ListId l, int idx) {
  switch (l) {
  case LIST_ING:
    if (idx >= 0 && idx < r->ingredient_count) {
      memmove(&r->ingredients[idx], &r->ingredients[idx + 1],
              (size_t)(r->ingredient_count - idx - 1) *
                  sizeof r->ingredients[0]);
      r->ingredient_count--;
    }
    break;
  case LIST_EQUIP:
    if (idx >= 0 && idx < r->equipment_count) {
      memmove(&r->equipment[idx], &r->equipment[idx + 1],
              (size_t)(r->equipment_count - idx - 1) * sizeof r->equipment[0]);
      r->equipment_count--;
    }
    break;
  case LIST_STEP:
    if (idx >= 0 && idx < r->step_count) {
      memmove(&r->steps[idx], &r->steps[idx + 1],
              (size_t)(r->step_count - idx - 1) * sizeof r->steps[0]);
      r->step_count--;
    }
    break;
  case LIST_TAG:
    if (idx >= 0 && idx < r->tag_count) {
      memmove(&r->tags[idx], &r->tags[idx + 1],
              (size_t)(r->tag_count - idx - 1) * sizeof r->tags[0]);
      r->tag_count--;
    }
    break;
  }
}

static int find_field(FieldRef *fs, int n, FieldKind k, int item) {
  for (int i = 0; i < n; i++)
    if (fs[i].kind == k && fs[i].item == item)
      return i;
  return -1;
}

static bool row_has_content(Recipe *r, FieldKind k, int item) {
  switch (k) {
  case FK_ING_NAME:
    return r->ingredients[item].amount[0] || r->ingredients[item].name[0];
  case FK_EQUIP:
    return r->equipment[item].name[0] != '\0';
  case FK_STEP:
    return r->steps[item].text[0] != '\0';
  case FK_TAG:
    return r->tags[item][0] != '\0';
  default:
    return false;
  }
}

// image picking

static bool copy_file(const char *src, const char *dst) {
  FILE *in = fopen(src, "rb");
  if (!in)
    return false;

  FILE *out = fopen(dst, "wb");
  if (!out) {
    fclose(in);
    return false;
  }

  char buf[8192];
  size_t got;
  bool ok = true;
  while ((got = fread(buf, 1, sizeof buf, in)) > 0) {
    if (fwrite(buf, 1, got, out) != got) {
      ok = false;
      break;
    }
  }

  fclose(in);
  fclose(out);
  return ok;
}

static const char *base_name(const char *p) {
  const char *b = p;
  for (const char *s = p; *s; s++)
    if (*s == '/' || *s == '\\')
      b = s + 1;
  return b;
}

static void pick_image(App *app) {
  const char *filters[] = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif"};
  const char *src = tinyfd_openFileDialog("Choose an image for the food", "", 5,
                                          filters, "Images", 0);
  if (!src)
    return;

  (void)mkdir(CONFIG_IMAGE_DIR, 0755); // ignore "already exists"

  char dst[CRAVE_PATH_CAP];
  snprintf(dst, sizeof dst, "%s/%ld_%s", CONFIG_IMAGE_DIR, (long)time(NULL),
           base_name(src));

  if (copy_file(src, dst))
    snprintf(app->editor.recipe.image_path, CRAVE_PATH_CAP, "%s", dst);
}

// update

void update(App *app, Msg msg) {
  switch (msg.tag) {
  case MSG_NEW:
    editor_new(&app->editor);
    app->screen = SCREEN_EDIT;
    app->focus = 0;
    app->cursor = 0;
    app->sel_anchor = 0;
    break;

  case MSG_SHOW:
    if (open_recipe(app, msg.id))
      app->screen = SCREEN_DETAIL;
    break;

  case MSG_EDIT:
    app->screen = SCREEN_EDIT;
    app->focus = 0;
    app->cursor = 0;
    app->sel_anchor = 0;
    break;

  case MSG_SAVE: {
    editor_commit(&app->editor);
    long id = db_save_recipe(app->db, &app->editor.recipe);
    if (id > 0) {
      app_reload(app);
      open_recipe(app, id);
      app->screen = SCREEN_DETAIL;
    }
    break;
  }

  case MSG_CANCEL:
    if (app->editor.recipe.id != 0) {
      open_recipe(app, app->editor.recipe.id);
      app->screen = SCREEN_DETAIL;
    } else {
      app->screen = SCREEN_GRID;
    }
    break;

  case MSG_BACK:
    app->screen = SCREEN_GRID;
    break;

  case MSG_FOCUS: {
    FieldRef fs[MAX_FIELDS];
    int n = collect_fields(&app->editor, fs);
    if (msg.i >= 0 && msg.i < n) {
      app->focus = msg.i;
      int len = (int)strlen(fs[msg.i].buf);
      app->cursor = len;
      app->sel_anchor = len;
    }
    break;
  }

  case MSG_LIST_ADD:
    list_add(&app->editor.recipe, msg.list);
    break;

  case MSG_LIST_DEL:
    list_del(&app->editor.recipe, msg.list, msg.i);
    app->focus = -1; // indices shifted, so refocus on next interaction
    break;

  case MSG_PICK_IMAGE:
    pick_image(app);
    break;

  case MSG_DELETE:
    app->confirm_delete = true;
    break;

  case MSG_DELETE_YES:
    if (app->editor.recipe.id != 0)
      db_delete_recipe(app->db, app->editor.recipe.id);
    app->confirm_delete = false;
    app_reload(app);
    app->screen = SCREEN_GRID;
    break;

  case MSG_DELETE_NO:
    app->confirm_delete = false;
    break;

  case MSG_NONE:
    break;
  }
}

// text editing

static bool ed_has_sel(App *app) { return app->sel_anchor != app->cursor; }

static void ed_sel_bounds(App *app, int *lo, int *hi) {
  if (app->sel_anchor < app->cursor) {
    *lo = app->sel_anchor;
    *hi = app->cursor;
  } else {
    *lo = app->cursor;
    *hi = app->sel_anchor;
  }
}

static void ed_del_span(char *buf, int lo, int hi) {
  int len = (int)strlen(buf);
  if (lo < 0)
    lo = 0;
  if (hi > len)
    hi = len;
  if (lo >= hi)
    return;
  memmove(buf + lo, buf + hi, (size_t)(len - hi) + 1);
}

static void ed_del_sel(App *app, char *buf) {
  int lo, hi;
  ed_sel_bounds(app, &lo, &hi);
  ed_del_span(buf, lo, hi);
  app->cursor = lo;
  app->sel_anchor = lo;
}

static void ed_insert_char(App *app, char *buf, int cap, int ch, bool numeric) {
  if (ch < 32 || ch > 126)
    return;

  if (numeric && (ch < '0' || ch > '9'))
    return;

  if (ed_has_sel(app))
    ed_del_sel(app, buf);

  int len = (int)strlen(buf);
  if (len + 1 >= cap)
    return;

  memmove(buf + app->cursor + 1, buf + app->cursor,
          (size_t)(len - app->cursor) + 1);
  buf[app->cursor] = (char)ch;
  app->cursor++;
  app->sel_anchor = app->cursor;
}

static void ed_insert_str(App *app, char *buf, int cap, const char *s,
                          bool numeric) {
  if (ed_has_sel(app))
    ed_del_sel(app, buf);

  for (const char *p = s; *p; p++)
    ed_insert_char(app, buf, cap, (unsigned char)*p, numeric);
}

static void ed_copy_sel(App *app, char *buf) {
  if (!ed_has_sel(app))
    return;

  int lo, hi;
  ed_sel_bounds(app, &lo, &hi);
  char tmp[CRAVE_DESC_CAP];
  int m = hi - lo;
  if (m > (int)sizeof tmp - 1)
    m = (int)sizeof tmp - 1;

  memcpy(tmp, buf + lo, (size_t)m);
  tmp[m] = '\0';
  SetClipboardText(tmp);
}

static bool is_word(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

static int word_left(const char *buf, int i) {
  while (i > 0 && !is_word(buf[i - 1]))
    i--;
  while (i > 0 && is_word(buf[i - 1]))
    i--;
  return i;
}

static int word_right(const char *buf, int i) {
  int len = (int)strlen(buf);
  while (i < len && !is_word(buf[i]))
    i++;
  while (i < len && is_word(buf[i]))
    i++;
  return i;
}

static void move_cursor(App *app, const char *buf, int to, bool shift) {
  int len = (int)strlen(buf);
  if (to < 0)
    to = 0;
  if (to > len)
    to = len;
  app->cursor = to;
  if (!shift)
    app->sel_anchor = to;
}

// focus / Tab navigation

static void focus_to(App *app, FieldRef *fields, int idx) {
  app->focus = idx;
  int len = (int)strlen(fields[idx].buf);
  app->cursor = len;
  app->sel_anchor = len;
}

static void nav_field(App *app, FieldRef *fields, int n, bool back) {
  int curr = app->cursor;
  if (back) {
    focus_to(app, fields, (curr - 1 + n) % n);
    return;
  }

  FieldRef *f = &fields[curr];
  Recipe *r = &app->editor.recipe;
  bool last_field_of_row = f->kind == FK_ING_NAME || f->kind == FK_EQUIP ||
                           f->kind == FK_STEP || f->kind == FK_TAG;

  if (last_field_of_row) {
    ListId list = LIST_ING;
    int count = 0;
    switch (f->kind) {
    case FK_ING_NAME:
      list = LIST_ING;
      count = r->ingredient_count;
      break;
    case FK_EQUIP:
      list = LIST_EQUIP;
      count = r->equipment_count;
      break;
    case FK_STEP:
      list = LIST_STEP;
      count = r->step_count;
      break;
    case FK_TAG:
      list = LIST_TAG;
      count = r->tag_count;
      break;
    default:
      break;
    }

    if (f->item == count - 1 && row_has_content(r, f->kind, f->item) &&
        count < list_cap(list)) {
      list_add(r, list);
      FieldRef nf[MAX_FIELDS];
      int nn = collect_fields(&app->editor, nf);
      int t = find_field(nf, nn, list_first_kind(list), count);
      if (t >= 0)
        focus_to(app, nf, t);
      return;
    }
  }

  focus_to(app, fields, (curr + 1) % n);
}

// keyboard

void app_handle_input(App *app) {
  if (app->confirm_delete) {
    if (IsKeyPressed(KEY_ESCAPE))
      update(app, (Msg){.tag = MSG_DELETE_NO});
    else if (IsKeyPressed(KEY_ENTER))
      update(app, (Msg){.tag = MSG_DELETE_YES});
    return;
  }

  if (app->screen == SCREEN_DETAIL) {
    if (IsKeyPressed(KEY_ESCAPE))
      update(app, (Msg){.tag = MSG_BACK});
    return;
  }

  if (app->screen != SCREEN_EDIT)
    return;

  bool mod = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
             IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
  bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

  if (IsKeyPressed(KEY_ESCAPE)) {
    update(app, (Msg){.tag = MSG_CANCEL});
    return;
  }

  if (mod && IsKeyPressed(KEY_S)) {
    update(app, (Msg){.tag = MSG_SAVE});
    return;
  }

  FieldRef fields[MAX_FIELDS];
  int n = collect_fields(&app->editor, fields);
  if (n == 0)
    return;

  if (app->focus < 0 || app->focus >= n) {
    app->focus = 0;
    app->cursor = 0;
    app->sel_anchor = 0;
  }

  FieldRef *f = &fields[app->focus];
  char *buf = f->buf;
  int len = (int)strlen(buf);
  if (app->cursor > len)
    app->cursor = len;
  if (app->sel_anchor > len)
    app->sel_anchor = len;

  if (mod && IsKeyPressed(KEY_A)) {
    app->sel_anchor = 0;
    app->cursor = len;
    return;
  }

  if (mod && IsKeyPressed(KEY_C)) {
    ed_copy_sel(app, buf);
    return;
  }

  if (mod && IsKeyPressed(KEY_X)) {
    ed_copy_sel(app, buf);
    if (ed_has_sel(app))
      ed_del_sel(app, buf);
    return;
  }

  if (mod && IsKeyPressed(KEY_V)) {
    const char *c = GetClipboardText();
    if (c)
      ed_insert_str(app, buf, f->cap, c, f->numeric);
    return;
  }

  if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ENTER)) {
    nav_field(app, fields, n, shift && IsKeyPressed(KEY_TAB));
    return;
  }

  if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
    if (!shift && ed_has_sel(app)) {
      int lo, hi;
      ed_sel_bounds(app, &lo, &hi);
      app->cursor = lo;
      app->sel_anchor = lo;
    } else {
      move_cursor(app, buf, mod ? word_left(buf, app->cursor) : app->cursor - 1,
                  shift);
    }
    return;
  }

  if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
    if (!shift && ed_has_sel(app)) {
      int lo, hi;
      ed_sel_bounds(app, &lo, &hi);
      app->cursor = hi;
      app->sel_anchor = hi;
    } else {
      move_cursor(app, buf,
                  mod ? word_right(buf, app->cursor) : app->cursor + 1, shift);
    }
    return;
  }

  if (IsKeyPressed(KEY_HOME)) {
    move_cursor(app, buf, 0, shift);
    return;
  }

  if (IsKeyPressed(KEY_END)) {
    move_cursor(app, buf, len, shift);
    return;
  }

  if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
    if (ed_has_sel(app)) {
      ed_del_sel(app, buf);
    } else if (app->cursor > 0) {
      ed_del_span(buf, app->cursor - 1, app->cursor);
      app->cursor--;
      app->sel_anchor = app->cursor;
    }
    return;
  }

  if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
    if (ed_has_sel(app)) {
      ed_del_sel(app, buf);
    } else if (app->cursor < len) {
      ed_del_span(buf, app->cursor, app->cursor + 1);
      app->sel_anchor = app->cursor;
    }
    return;
  }

  for (int ch = GetCharPressed(); ch != 0; ch = GetCharPressed())
    ed_insert_char(app, buf, f->cap, ch, f->numeric);
}

// lifecycle

bool app_init(App *app) {
  if (!db_open(CONFIG_DB_PATH, &app->db))
    return false;

  if (!db_init_schema(app->db)) {
    db_close(app->db);
    app->db = NULL;
    return false;
  }

  app->focus = -1;
  app_reload(app);
  return true;
}

void app_shutdown(App *app) {
  free(app->summaries);
  app->summaries = NULL;
  app->summary_cap = 0;
  app->summary_count = 0;
  db_close(app->db);
  app->db = NULL;
}
