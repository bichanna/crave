#ifndef CRAVE_DB_H
#define CRAVE_DB_H

#include "model.h"
#include <sqlite3.h>
#include <stdbool.h>

bool db_open(const char *path, sqlite3 **out_db);
void db_close(sqlite3 *db);

// Create all tables/indexes if they do not yet exist. Safe to call repeatedly
bool db_init_schema(sqlite3 *db);

// Fill out[] with lightweight rows (id/title/image/tags) ordered by title
// Returns the number written (capped at max_out), or -1 on error
int db_load_summaries(sqlite3 *db, RecipeSummary *out, int max_out);

// Load one full recipe by id. Returns false if not found or on error.
bool db_load_recipe(sqlite3 *db, long id, Recipe *out);

// Insert (id<=0) or update (id>0) a recipe and all its children atomically.
// Returns the recipe id (>0) on success, or -1 on error
long db_save_recipe(sqlite3 *db, const Recipe *r);

// Delete a recipe and all its children
bool db_delete_recipe(sqlite3 *db, long id);

// True if any recipe currently references this vault relative image path
bool db_image_path_in_use(sqlite3 *db, const char *rel);

// Distinct tag names in use, sorted. Returns count (<= max_out) or -1.
int db_all_tags(sqlite3 *db, char out[][CRAVE_TAG_CAP], int max_out);

#endif
