#include "db.h"
#include "model.h"
#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

static void db_log(sqlite3 *db, const char *what) {
  fprintf(stderr, "[db] %s: %s\n", what, db ? sqlite3_errmsg(db) : "(no db)");
}

static bool db_exec(sqlite3 *db, const char *sql) {
  char *err = NULL;
  if (sqlite3_exec(db, sql, NULL, NULL, &err) == SQLITE_OK)
    return true;
  fprintf(stderr, "[db] exec failed: %s\n", err ? err : "(unknown error)");
  sqlite3_free(err);
  return false;
}

static int bind_text(sqlite3_stmt *st, int idx, const char *s) {
  return sqlite3_bind_text(st, idx, s ? s : "", -1, SQLITE_TRANSIENT);
}

// Copy a possibly-NULL sqlite text column into a fixed buffer
static void copy_col(char *dst, int cap, const unsigned char *src) {
  if (cap <= 0)
    return;

  if (!src) {
    dst[0] = '\0';
   return;
  }
  snprintf(dst, (size_t)cap, "%s", (const char *)src);
}

bool db_open(const char *path, sqlite3 **out_db) {
  if (out_db)
    *out_db = NULL;

  sqlite3 *db = NULL;
  if (sqlite3_open(path, &db) != SQLITE_OK) {
    db_log(db, "open");
    sqlite3_close(db);
    return false;
  }

  if (!db_exec(db, "PRAGMA foreign_keys = ON;")) {
    sqlite3_close(db);
    return false;
  }

  sqlite3_busy_timeout(db, 2000);
  if (out_db)
    *out_db = db;

  return true;
}

void db_close(sqlite3 *db) {
  if (db)
    sqlite3_close(db);
}

bool db_init_schema(sqlite3 *db) {
  static const char *schema =
    "CREATE TABLE IF NOT EXISTS recipes ("
    "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  title       TEXT NOT NULL DEFAULT '',"
    "  description TEXT NOT NULL DEFAULT '',"
    "  image_path  TEXT NOT NULL DEFAULT '',"
    "  servings    INTEGER NOT NULL DEFAULT 0,"
    "  prep_mins   INTEGER NOT NULL DEFAULT 0,"
    "  cook_mins   INTEGER NOT NULL DEFAULT 0,"
    "  created_at  INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
    "  updated_at  INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
    ");"
    "CREATE TABLE IF NOT EXISTS ingredients ("
    "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  recipe_id INTEGER NOT NULL REFERENCES recipes(id) ON DELETE CASCADE,"
    "  position  INTEGER NOT NULL,"
    "  amount    TEXT NOT NULL DEFAULT '',"
    "  name      TEXT NOT NULL DEFAULT ''"
    ");"
    "CREATE TABLE IF NOT EXISTS equipment ("
    "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  recipe_id INTEGER NOT NULL REFERENCES recipes(id) ON DELETE CASCADE,"
    "  position  INTEGER NOT NULL,"
    "  name      TEXT NOT NULL DEFAULT ''"
    ");"
    "CREATE TABLE IF NOT EXISTS steps ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  recipe_id  INTEGER NOT NULL REFERENCES recipes(id) ON DELETE CASCADE,"
    "  position   INTEGER NOT NULL,"
    "  text       TEXT NOT NULL DEFAULT ''"
    ");"
    "CREATE TABLE IF NOT EXISTS tags ("
    "  id   INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL UNIQUE"
    ");"
    "CREATE TABLE IF NOT EXISTS recipe_tags ("
    "  recipe_id INTEGER NOT NULL REFERENCES recipes(id) ON DELETE CASCADE,"
    "  tag_id    INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,"
    "  PRIMARY KEY (recipe_id, tag_id)"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_ing_recipe   ON ingredients(recipe_id);"
    "CREATE INDEX IF NOT EXISTS idx_equip_recipe ON equipment(recipe_id);"
    "CREATE INDEX IF NOT EXISTS idx_steps_recipe ON steps(recipe_id);"
    "CREATE INDEX IF NOT EXISTS idx_rt_recipe    ON recipe_tags(recipe_id);"
    "CREATE TRIGGER IF NOT EXISTS update_recipe_time "
    "AFTER UPDATE ON recipes "
    "FOR EACH ROW "
    "BEGIN "
    "  UPDATE recipes SET updated_at = strftime('%s','now') WHERE id = OLD.id;"
    "END;";

  return db_exec(db, schema);
}

static long upsert_tag(sqlite3 *db, const char *name) {
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO tags(name) VALUES(?1);", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare tag insert");
    return -1;
  }
  bind_text(st, 1, name);
  if (sqlite3_step(st) != SQLITE_DONE) {
    db_log(db, "tag insert");
    sqlite3_finalize(st);
    return -1;
  }
  sqlite3_finalize(st);

  st = NULL;
  if (sqlite3_prepare_v2(db, "SELECT id FROM tags WHERE name = ?1;", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare tag select");
    return -1;
  }
  bind_text(st, 1, name);
  long id = -1;
  if (sqlite3_step(st) == SQLITE_ROW)
    id = (long) sqlite3_column_int64(st, 0);
  sqlite3_finalize(st);
  return id;
}

int db_load_summaries(sqlite3 *db, RecipeSummary *out, int max_out) {
  if (!out || max_out <= 0)
    return 0;

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db, "SELECT id, title, image_path FROM recipes ORDER BY title COLLATE NOCASE, id;", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare summaries");
    return -1;
  }

  sqlite3_stmt *tg = NULL;
  if (sqlite3_prepare_v2(db, "SELECT t.name FROM tags t JOIN recipe_tags rt ON rt.tag_id = t.id WHERE rt.recipe_id = ?1 ORDER BY t.name COLLATE NOCASE;", -1, &tg, NULL) != SQLITE_OK) {
    db_log(db, "prepare summary tags");
    sqlite3_finalize(st);
    return -1;
  }

  int n = 0;
  while (n < max_out && sqlite3_step(st) == SQLITE_ROW) {
    RecipeSummary *s = &out[n];
    memset(s, 0, sizeof(*s));
    s->id = (long)sqlite3_column_int64(st, 0);
    copy_col(s->title, CRAVE_TITLE_CAP, sqlite3_column_text(st, 1));
    copy_col(s->image_path, CRAVE_PATH_CAP, sqlite3_column_text(st, 2));

    sqlite3_reset(tg);
    sqlite3_bind_int64(tg, 1, s->id);
    while (s->tag_count < CRAVE_MAX_TAGS && sqlite3_step(tg) == SQLITE_ROW) {
      copy_col(s->tags[s->tag_count], CRAVE_TAG_CAP, sqlite3_column_text(tg, 0));
      s->tag_count++;
    }
    n++;
  }

  sqlite3_finalize(tg);
  sqlite3_finalize(st);
  return n;
}

static bool load_children(sqlite3 *db, Recipe *r) {
  sqlite3_stmt *st = NULL;

  // ingredients
  if (sqlite3_prepare_v2(db, "SELECT amount, name FROM ingredients WHERE recipe_id=?1 ORDER BY position, id;", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare ingredients");
    return false;
  }
  sqlite3_bind_int64(st, 1, r->id);
  while (r->ingredient_count < CRAVE_MAX_INGREDIENTS && sqlite3_step(st) == SQLITE_ROW) {
    Ingredient *ing = &r->ingredients[r->ingredient_count++];
    copy_col(ing->amount, CRAVE_AMOUNT_CAP, sqlite3_column_text(st, 0));
    copy_col(ing->name, CRAVE_NAME_CAP, sqlite3_column_text(st, 1));
  }
  sqlite3_finalize(st);
  st = NULL;

  // equipment
  if (sqlite3_prepare_v2(db, "SELECT name FROM equipment WHERE recipe_id=?1 ORDER BY position, id;", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare equipment");
    return false;
  }
  sqlite3_bind_int64(st, 1, r->id);
  while (r->equipment_count < CRAVE_MAX_EQUIPMENT && sqlite3_step(st) == SQLITE_ROW) {
    Equipment *eq = &r->equipment[r->equipment_count++];
    copy_col(eq->name, CRAVE_NAME_CAP, sqlite3_column_text(st, 0));
  }
  sqlite3_finalize(st);
  st = NULL;

  // steps
  if (sqlite3_prepare_v2(db, "SELECT text FROM steps WHERE recipe_id=?1 ORDER BY position, id;", -2, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare steps");
    return false;
  }
  sqlite3_bind_int64(st, 1, r->id);
  while (r->step_count < CRAVE_MAX_STEPS && sqlite3_step(st) == SQLITE_ROW) {
    Step *step = &r->steps[r->step_count++];
    copy_col(step->text, CRAVE_STEP_CAP, sqlite3_column_text(st, 0));
  }
  sqlite3_finalize(st);
  st = NULL;

  // tags
  if (sqlite3_prepare_v2(db, "SELECT t.name FROM tags t JOIN recipe_tags rt ON rt.tag_id=t.id WHERE rt.recipe_id=?1 ORDER BY t.name COLLATE NOCASE;", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare tags");
    return false;
  }
  sqlite3_bind_int64(st, 1, r->id);
  while (r->tag_count < CRAVE_MAX_TAGS && sqlite3_step(st) == SQLITE_ROW) {
    copy_col(r->tags[r->tag_count++], CRAVE_TAG_CAP, sqlite3_column_text(st, 0));
  }
  sqlite3_finalize(st);
  return true;
}

bool db_load_recipe(sqlite3 *db, long id, Recipe *out) {
  if (!out)
    return false;

  memset(out, 0, sizeof(*out));

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db, "SELECT id, title, description, image_path, servings, prep_mins, cook_mins FROM recipes WHERE id=?1;", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare load recipe");
    return false;
  }
  sqlite3_bind_int64(st, 1, id);

  bool found = false;
  if (sqlite3_step(st) == SQLITE_ROW) {
    found = true;
    out->id = (long)sqlite3_column_int64(st, 0);
    copy_col(out->title, CRAVE_TITLE_CAP, sqlite3_column_text(st, 1));
    copy_col(out->description, CRAVE_DESC_CAP, sqlite3_column_text(st, 2));
    copy_col(out->image_path, CRAVE_PATH_CAP, sqlite3_column_text(st, 3));
    out->servings = sqlite3_column_int(st, 4);
    out->prep_mins = sqlite3_column_int(st, 5);
    out->cook_mins = sqlite3_column_int(st, 6);
  }
  sqlite3_finalize(st);

  if (!found)
    return false;

  return load_children(db, out);
}

static bool insert_children(sqlite3 *db, const Recipe *r, long rid) {
  sqlite3_stmt *st = NULL;

  // ingredients
  if (sqlite3_prepare_v2(db, "INSERT INTO ingredients(recipe_id, position, amount, name) VALUES(?1,?2,?3,?4);", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare insert ingredient");
    return false;
  }

  for (int i = 0; i < r->ingredient_count; i++) {
    if (r->ingredients[i].amount[0] == '\0' && r->ingredients[i].name[0] == '\0')
      continue;
    sqlite3_reset(st);
    sqlite3_bind_int64(st, 1, rid);
    sqlite3_bind_int(st, 2, i);
    bind_text(st, 3, r->ingredients[i].amount);
    bind_text(st, 4, r->ingredients[i].name);
    if (sqlite3_step(st) != SQLITE_DONE) {
      db_log(db, "insert ingredient");
      sqlite3_finalize(st);
      return false;
    }
  }

  sqlite3_finalize(st);
  st = NULL;

  // equipment
  if (sqlite3_prepare_v2(db, "INSERT INTO equipment(recipe_id, position, name) VALUES(?1,?2,?3);", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare insert equipment");
    return false;
  }

  for (int i = 0; i < r->equipment_count; i++) {
    if (r->equipment[i].name[0] == '\0')
      continue;
    sqlite3_reset(st);
    sqlite3_bind_int64(st, 1, rid);
    sqlite3_bind_int(st, 2, i);
    bind_text(st, 3, r->equipment[i].name);
    if (sqlite3_step(st) != SQLITE_DONE) {
      db_log(db, "insert equipment");
      sqlite3_finalize(st);
      return false;
    }
  }

  sqlite3_finalize(st);
  st = NULL;

  // steps
  if (sqlite3_prepare_v2(db, "INSERT INTO steps(recipe_id, position, text) VALUES(?1,?2,?3);", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare insert step");
    return false;
  }

  for (int i = 0; i < r->step_count; i++) {
    if (r->steps[i].text[0] == '\0')
      continue;
    sqlite3_reset(st);
    sqlite3_bind_int64(st, 1, rid);
    sqlite3_bind_int(st, 2, i);
    bind_text(st, 3, r->steps[i].text);
    if (sqlite3_step(st) != SQLITE_DONE) {
      db_log(db, "insert step");
      sqlite3_finalize(st);
      return false;
    }
  }

  sqlite3_finalize(st);
  st = NULL;

  // tags, ensure each exists then link
  if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO recipe_tags(recipe_id, tag_id) VALUES(?1,?2);", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare link tag");
    return false;
  }

  for (int i = 0; i < r->tag_count; i++) {
    if (r->tags[i][0] == '\0')
      continue;
    long tid = upsert_tag(db, r->tags[i]);
    if (tid < 0) {
      sqlite3_finalize(st);
      return false;
    }
    sqlite3_reset(st);
    sqlite3_bind_int64(st, 1, rid);
    sqlite3_bind_int64(st, 2, tid);
    if (sqlite3_step(st) != SQLITE_DONE) {
      db_log(db, "link tag");
      sqlite3_finalize(st);
      return false;
    }
  }

  sqlite3_finalize(st);

  return true;
}

static bool delete_children(sqlite3 *db, long rid) {
  sqlite3_stmt *st = NULL;
  static const char *tables[] = {"ingredients", "equipment", "steps", "recipe_tags"};

  for (int t = 0; t < 4; t++) {
    char sql[64];
    snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE recipe_id=?1;", tables[t]);

    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
      db_log(db, "prepare delete child");
      return false;
    }

    sqlite3_bind_int64(st, 1, rid);
    if (sqlite3_step(st) != SQLITE_DONE) {
      db_log(db, "delete child");
      sqlite3_finalize(st);
      return false;
    }

    sqlite3_finalize(st);
    st = NULL;
  }

  return true;
}

long db_save_recipe(sqlite3 *db, const Recipe *r) {
  if (!r) return -1;
  if (!db_exec(db, "BEGIN IMMEDIATE;")) return -1;

  long rid = r->id;
  sqlite3_stmt *st = NULL;

  if (rid <= 0) {
    if (sqlite3_prepare_v2(db, "INSERT INTO recipes(title, description, image_path, servings, prep_mins, cook_mins) VALUES(?1,?2,?3,?4,?5,?6);", -1, &st, NULL) != SQLITE_OK) {
      db_log(db, "prepare insert recipe");
      goto fail;
    }

    bind_text(st, 1, r->title);
    bind_text(st, 2, r->description);
    bind_text(st, 3, r->image_path);
    sqlite3_bind_int(st, 4, r->servings);
    sqlite3_bind_int(st, 5, r->prep_mins);
    sqlite3_bind_int(st, 6, r->cook_mins);
    if (sqlite3_step(st) != SQLITE_DONE) {
      db_log(db, "insert recipe");
      sqlite3_finalize(st);
      goto fail;
    }
    sqlite3_finalize(st);
    st = NULL;
    rid = (long)sqlite3_last_insert_rowid(db);
  } else {
    if (sqlite3_prepare_v2(db, "UPDATE recipes SET title=?1, description=?2, image_path=?3, servings=?4, prep_mins=?5, cook_mins=?6, updated_at=strftime('%s','now') WHERE id=?7;", -1, &st, NULL) != SQLITE_OK) {
      db_log(db, "prepare update recipe");
      goto fail;
    }
    bind_text(st, 1, r->title);
    bind_text(st, 2, r->description);
    bind_text(st, 3, r->image_path);
    sqlite3_bind_int(st, 4, r->servings);
    sqlite3_bind_int(st, 5, r->prep_mins);
    sqlite3_bind_int(st, 6, r->cook_mins);
    sqlite3_bind_int64(st, 7, rid);
    if (sqlite3_step(st) != SQLITE_DONE) {
      db_log(db, "update recipe");
      sqlite3_finalize(st);
      goto fail;
    }

    sqlite3_finalize(st);
    st = NULL;

    if (!delete_children(db, rid))
      goto fail;
  }

  if (!insert_children(db, r, rid)) goto fail;
  if (!db_exec(db, "COMMIT;")) goto fail;

  return rid;

fail:
  db_exec(db, "ROLLBACK;");
  return -1;
}

bool db_delete_recipe(sqlite3 *db, long id) {
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db, "DELETE FROM recipes WHERE id=?1;", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare delete recipe");
    return false;
  }

  sqlite3_bind_int64(st, 1, id);
  bool ok = sqlite3_step(st) == SQLITE_DONE;
  if (!ok)
    db_log(db, "delete recipe");

  sqlite3_finalize(st);
  return ok;
}

int db_all_tags(sqlite3 *db, char out[][CRAVE_TAG_CAP], int max_out) {
  if (!out || max_out <= 0) return 0;
  sqlite3_stmt *st = NULL;

  // only tags actually attahed to a recipe
  if (sqlite3_prepare_v2(db, "SELECT DISTINCT t.name FROM tags t JOIN recipe_tags rt ON rt.tag_id=t.id ORDER BY t.name COLLATE NOCASE;", -1, &st, NULL) != SQLITE_OK) {
    db_log(db, "prepare all tags");
    return -1;
  }

  int n = 0;
  while (n < max_out && sqlite3_step(st) == SQLITE_ROW) {
    copy_col(out[n], CRAVE_TAG_CAP, sqlite3_column_text(st, 0));
    n++;
  }

  sqlite3_finalize(st);
  return n;
}

bool db_image_path_in_use(sqlite3 *db, const char *rel) {
  if (rel == NULL || rel[0] == '\0') return false;

  sqlite3_stmt *st = NULL;

  if (sqlite3_prepare_v2(db, "SELECT 1 FROM recipes WHERE image_path=?1 LIMIT 1;", -1, &st, NULL) != SQLITE_OK) return false;

  bind_text(st, 1, rel);
  bool used = (sqlite3_step(st) == SQLITE_ROW);

  sqlite3_finalize(st);
  return used;
}
