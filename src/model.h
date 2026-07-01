#ifndef CRAVE_MODEL_H
#define CRAVE_MODEL_H

#define CRAVE_MAX_INGREDIENTS 64
#define CRAVE_MAX_STEPS 64
#define CRAVE_MAX_EQUIPMENT 32
#define CRAVE_MAX_TAGS 16

#define CRAVE_TITLE_CAP 256
#define CRAVE_DESC_CAP 1024
#define CRAVE_PATH_CAP 512
#define CRAVE_AMOUNT_CAP 48
#define CRAVE_NAME_CAP 160
#define CRAVE_STEP_CAP 600
#define CRAVE_TAG_CAP 64

typedef struct {
  char amount[CRAVE_AMOUNT_CAP];
  char name[CRAVE_NAME_CAP];
} Ingredient;

typedef struct {
  char text[CRAVE_STEP_CAP];
} Step;

typedef struct {
  char name[CRAVE_NAME_CAP];
} Equipment;

typedef struct {
  long id; // 0 == not yet persisted
  char title[CRAVE_TITLE_CAP];
  char description[CRAVE_DESC_CAP];
  char image_path[CRAVE_PATH_CAP];
  int servings;
  int prep_mins;
  int cook_mins;

  Ingredient ingredients[CRAVE_MAX_INGREDIENTS];
  int ingredient_count;

  Equipment equipment[CRAVE_MAX_EQUIPMENT];
  int equipment_count;

  Step steps[CRAVE_MAX_STEPS];
  int step_count;

  char tags[CRAVE_MAX_TAGS][CRAVE_TAG_CAP];
  int tag_count;
} Recipe;

// Lightweight row used to render the home grid without loading full detail
typedef struct {
  long id;
  char title[CRAVE_TITLE_CAP];
  char description[CRAVE_DESC_CAP];
  char image_path[CRAVE_PATH_CAP];
  char tags[CRAVE_MAX_TAGS][CRAVE_TAG_CAP];
  int tag_count;
} RecipeSummary;

#endif
