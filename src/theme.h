#ifndef CRAVE_THEME_H
#define CRAVE_THEME_H

/*
 * Monotone design tokens: palette, spacing, type scale. Macros (not
 * const) so unused tokens don't trip -Werror and the colors expand to
 * Clay_Color literals. Included only where Clay is in scope (ui.c)
 */

// palette - black/white/grey   hierarchy by weight and size, not hue
#define THEME_PAPER (Clay_Color){250, 250, 250, 255}
#define THEME_CARD (Clay_Color){255, 255, 255, 255}
#define THEME_CARD_HOVER (Clay_Color){244, 244, 244, 255}
#define THEME_FIELD (Clay_Color){255, 255, 255, 255}
#define THEME_INK (Clay_Color){17, 17, 19, 255}
#define THEME_INK_HOVER (Clay_Color){40, 40, 42, 255}
#define THEME_SOFT (Clay_Color){122, 122, 128, 255}
#define THEME_LINE (Clay_Color){222, 222, 224, 255}
#define THEME_LINE_FOCUS (Clay_Color){17, 17, 19, 255}
#define THEME_SELECT                                                           \
  (Clay_Color){206, 211, 220, 255} // text selection highlight
#define THEME_INVERT (Clay_Color){250, 250, 250, 255}
#define THEME_SCRIM (Clay_Color){17, 17, 19, 140} // modal backdrop
#define THEME_DANGER (Clay_Color){176, 42, 42, 255}
#define THEME_DANGER_HOVER (Clay_Color){150, 32, 32, 255}

// spacing (px)
#define THEME_PAD 22
#define THEME_PAD_FIELD 10
#define THEME_PAD_BTN_X 14
#define THEME_PAD_BTN_Y 9
#define THEME_GAP 14
#define THEME_GAP_SM 8
#define THEME_GAP_XS 4
#define THEME_BORDER 1

// type scale (px)
#define THEME_FONT_TITLE 30
#define THEME_FONT_H2 20
#define THEME_FONT_BODY 17
#define THEME_FONT_LABEL 12
#define THEME_FONT_BTN 14

// a single editable line is this tall (font + padding top/bottom)
#define THEME_FIELD_H (THEME_FONT_BODY + 2 * THEME_PAD_FIELD)

#endif
