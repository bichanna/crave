#ifndef CRAVE_APP_H
#define CRAVE_APP_H

#include "model.h"
#include "raylib.h"
#include "sqlite3.h"
#include <stdbool.h>
#include <stdint.h>

#define EDITOR_NUM_CAP 10

enum {
  CRAVE_FONT_REGULAR = 0,
  CRAVE_FONT_BOLD,
  CRAVE_FONT_ITALIC,
  CRAVE_FONT_COUNT,
};

typedef enum {
  LIST_ING = 0,
  LIST_EQUIP,
  LIST_STEP,
  LIST_TAG,
} ListId;

typedef enum {
  SCREEN_GRID = 0,
  SCREEN_DETAIL, // read-only recipe
  SCREEN_EDIT,   // same layout + editable + save
} Screen;

typedef struct {
  Recipe recipe;
  char servings[EDITOR_NUM_CAP];
  char prep[EDITOR_NUM_CAP];
  char cook[EDITOR_NUM_CAP];
} Editor;

// editable field
typedef enum {
  FK_SCALAR = 0,
  FK_ING_AMOUNT,
  FK_ING_NAME,
  FK_EQUIP,
  FK_STEP,
  FK_TAG,
} FieldKind;

typedef struct {
  char *buf;      // buffer this field edits
  int cap;        // its capacity
  bool numeric;   // digits only?
  FieldKind kind; // which section it belongs to
  int item;       // index within its list
  uint32_t key;   // stable Clay id key
} FieldRef;

#define MAX_FIELDS                                                             \
  (5 + 2 * CRAVE_MAX_INGREDIENTS + CRAVE_MAX_EQUIPMENT + CRAVE_MAX_STEPS +     \
   CRAVE_MAX_TAGS)

typedef enum {
  MSG_NONE = 0,
  MSG_SHOW,       // grid: open read-only detail for .id
  MSG_EDIT,       // detail: switch to edit mode
  MSG_NEW,        // grid: edit a blank recipe
  MSG_SAVE,       // edit: commit, back to detail
  MSG_CANCEL,     // edit: discard, back to detail/grid
  MSG_BACK,       // detail: back to grid
  MSG_FOCUS,      // edit: focus field index .i
  MSG_LIST_ADD,   // edit: append a row to list .list
  MSG_LIST_DEL,   // edit: remove item .i from list .list
  MSG_PICK_IMAGE, // edit: choose + copy an image
  MSG_DELETE,     // detail: open the delete confirmation
  MSG_DELETE_YES, // modal: confirm delete
  MSG_DELETE_NO,  // modal: dismiss
} MsgTag;

typedef struct {
  MsgTag tag;
  long id;     // MSG_SHOW
  int i;       // MSG_FOCUS field index and MSG_LIST_DEL item index
  ListId list; // MSG_LIST_ADD and MSG_LIST_DEL
} Msg;

typedef struct App {
  // grid
  RecipeSummary *summaries; // heap array, grown to fit
  int summary_count;
  int summary_cap;

  // open recipe (detail + editor share this)
  Screen screen;
  Editor editor;

  // editor, text edit state, for focused field
  int focus;
  int cursor;
  int sel_anchor;

  bool confirm_delete;
  Msg pending;

  sqlite3 *db;
  Font fonts[CRAVE_FONT_COUNT];
} App;

bool app_init(App *app);
void app_shutdown(App *app);
void app_reload(App *ap);

// build the ordered editable field list from the editor and returns the count
int collect_fields(Editor *e, FieldRef *out);

// keyboard (editing, Tab/Enter/Esc/Save), not a Msg
void app_handle_input(App *app);

void update(App *app, Msg msg);
void view(App *app);

// ui.c runtime hooks
void view_free(void);           // free the click pool, shutdown
void ui_handle_mouse(App *app); // mouse -> field focus/cursor/selection
void ui_overlay(App *app);      // draw editable field text/caret/selection
void ui_textures_unload(void);  // free cached image textures, shutdown

#endif
