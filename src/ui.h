/* A small immediate-mode widget layer: buttons, text fields, keyboard focus.
 * Widgets are drawn and answered in the same call, every frame. */
#ifndef ADDERALL_UI_H
#define ADDERALL_UI_H

#include <SDL3/SDL.h>

#include "gfx.h"

#define UI_MAX_KEYS 32
#define UI_MAX_IDS 128

typedef struct {
  SDL_Keycode key;
  SDL_Keymod mod;
  bool repeat;
  bool used;
} ui_key;

typedef struct {
  gfx *g;

  /* input gathered for this frame; cleared by ui_end_frame */
  float mx, my;
  bool mouse_in;
  bool lpress, rpress, lrelease, ldown;
  float wheel_x, wheel_y;
  ui_key keys[UI_MAX_KEYS];
  int nkeys;
  char text[256];
  bool mouse_moved;

  /* persistent state */
  int focus;          /* id of the focused widget, 0 = none */
  bool focus_visible; /* show the focus ring (keyboard use) */
  int prev_ids[UI_MAX_IDS], n_prev;
  int cur_ids[UI_MAX_IDS], n_cur;
  bool want_text;     /* a text field is focused this frame */
  bool text_active;   /* ...as of the last finished frame: hotkeys stay quiet */
  float line;         /* stroke width in pixels */
  /* text field editing */
  int cursor;         /* byte offset in the focused field */
  bool consumed_click; /* a widget already took this frame's click */
} ui_state;

typedef enum { UI_BTN_NORMAL = 0, UI_BTN_PRIMARY = 1, UI_BTN_DISABLED = 2, UI_BTN_SELECTED = 4 } ui_btn_flags;

void ui_begin_frame(ui_state *u);
void ui_end_frame(ui_state *u);

/* Keys: find and consume a pressed key. */
bool ui_take_key(ui_state *u, SDL_Keycode key, SDL_Keymod required_mods);
bool ui_any_key(const ui_state *u);

/* Register a widget as keyboard-focusable; returns true if it has focus. */
bool ui_focusable(ui_state *u, int id);

bool ui_hover(const ui_state *u, ad_rect r);

/* Returns true when clicked (or activated with Enter/Space while focused). */
bool ui_button(ui_state *u, int id, ad_rect r, const char *label, int flags);

/* A button holding a symbol mask instead of text. */
bool ui_icon_button(ui_state *u, int id, ad_rect r, gfx_mask_kind kind, int param, int flags);

/* Single-line text field. Returns 1 when Enter was pressed, 2 for Escape. */
int ui_text_field(ui_state *u, int id, ad_rect r, char *buf, size_t cap, int max_chars, const char *placeholder);

float ui_button_width(ui_state *u, const char *label);

/* A boxed panel with a border. */
void ui_panel(ui_state *u, ad_rect r);

/* Draw a tooltip near a point, kept inside the window. */
void ui_tooltip(ui_state *u, const char *line1, const char *line2, float x, float y);

/* Wrap text into a box; returns the height used. Draws only if draw is true. */
float ui_wrapped_text(ui_state *u, const char *s, float x, float y, float w, gfx_color c, bool draw);

#endif
