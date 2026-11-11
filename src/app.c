/* Adderall: window, event loop, screens. */
#include "app.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "date.h"
#include "gfx.h"
#include "measure.h"
#include "model.h"
#include "store.h"
#include "tz.h"
#include "ui.h"

typedef enum { SCREEN_BOARD, SCREEN_FATAL, SCREEN_FIRST } Screen;

enum {
  ID_FATAL_FOLDER = 1, ID_FATAL_COPY, ID_FATAL_BACKUP, ID_FATAL_QUIT, ID_NOTICE_OK, ID_ERR_RETRY,
  ID_HABIT_BTN, ID_SETTINGS_BTN,
  ID_POP_CLOSE, ID_POP_NAME, ID_POP_ADD, ID_POP_CANCEL,
  ID_FIRST_NAME, ID_FIRST_HORIZ, ID_FIRST_VERT, ID_FIRST_START, ID_FIRST_SKIP,
  ID_HABIT_ROWS = 1000 /* + habit * 16 + k */
};

typedef enum { EDIT_NONE, EDIT_RENAME, EDIT_SYMBOL, EDIT_DELETE } EditMode;

typedef struct {
  SDL_Window *win;
  SDL_Renderer *ren;
  gfx g;
  ui_state ui;
  ad_model model;
  ad_store store;
  bool store_open;
  ad_view view;
  char bundled_font[1100];
  char zoneinfo_dir[1100];
  ad_zone_info *zones;
  int nzones;
  ad_tz *tz;
  char tz_name[64]; /* the zone in use (the setting, or the computer's own) */
  Screen screen;
  char fatal_title[120];
  char fatal_text[1200];
  bool fatal_can_backup;
  char notice[400];     /* something to tell the person, until dismissed */
  char save_error[400]; /* the last save failed: shown until a save works */
  bool quit;
  ad_rect banner_rect;  /* where the banner was drawn last frame (clicks there are not for the board) */
  bool modal;           /* a pop-up is open: the board does not react */
  bool redraw;          /* draw once more right away (a pop-up changed size) */

  /* the focused cell, for the keyboard */
  bool cell_focus;
  int focus_row;        /* board row */
  ad_day focus_day;
  bool kb_visible;      /* draw the focus ring (the keyboard is in use) */

  /* the measure tool: a press on a cell starts a drag; a drag across cells measures, a plain click fills or skips */
  bool drag_active;
  int drag_button;
  int drag_row;         /* board row of the habit being measured */
  ad_day drag_a, drag_b;
  bool dragged;

  /* the habit pop-up */
  bool popup_open;
  EditMode edit;
  int edit_habit;
  char edit_buf[AD_NAME_MAX_BYTES + 8];
  char new_name[AD_NAME_MAX_BYTES + 8];
  char popup_msg[200];
  bool full_prompt;               /* "the board is full": choose one to archive or delete */
  char pending_name[AD_NAME_MAX_BYTES + 8];
  float popup_scroll, popup_content_h;
  ad_rect popup_rect;

  /* first launch */
  char first_name[AD_NAME_MAX_BYTES + 8];
  ad_layout first_layout;
} App;

/* ---- resources and the system ------------------------------------------ */

static bool file_exists(const char *path) {
  SDL_PathInfo info;
  return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

/* Bundled files live in a "data" folder next to the program; when running
 * straight out of a build tree we fall back to the source tree's copy. */
static bool resource_path(const char *rel, char *out, size_t cap) {
  const char *base = SDL_GetBasePath();
  if (base) {
    SDL_snprintf(out, cap, "%sdata/%s", base, rel);
    if (file_exists(out)) return true;
  }
#ifdef ADDERALL_SOURCE_DATA_DIR
  SDL_snprintf(out, cap, "%s/%s", ADDERALL_SOURCE_DATA_DIR, rel);
  if (file_exists(out)) return true;
#endif
  return false;
}

/* ---- time ---------------------------------------------------------------- */

/* Use the zone named in the settings, or else the computer's own. */
static void apply_timezone(App *a) {
  const char *want = a->model.settings.timezone;
  char sys[96];
  ad_tz *tz = NULL;
  if (want[0]) tz = ad_tz_load(a->zoneinfo_dir, want);
  if (tz) {
    SDL_snprintf(a->tz_name, sizeof a->tz_name, "%s", want);
  } else {
    if (want[0]) SDL_snprintf(a->notice, sizeof a->notice, "The time zone \"%s\" in your data file is not one Adderall knows, so your computer's time zone is used for now.", want);
    ad_tz_detect_system(a->zoneinfo_dir, a->zones, a->nzones, sys, sizeof sys);
    tz = ad_tz_load(a->zoneinfo_dir, sys);
    if (!tz) tz = ad_tz_load(a->zoneinfo_dir, "UTC");
    SDL_snprintf(a->tz_name, sizeof a->tz_name, "%s", sys);
  }
  if (a->tz) ad_tz_free(a->tz);
  a->tz = tz;
}

static ad_day clock_day(const App *a) {
  return ad_tz_day(a->tz, ad_plat_now(), a->model.settings.daystart);
}

/* Called every frame: today changes when the day rolls over. */
static void update_clock(App *a, bool *saved_needed) {
  if (!a->tz) return;
  if (ad_model_set_clock(&a->model, clock_day(a))) *saved_needed = true;
}

/* file:///path with everything unusual percent-encoded, for opening a folder. */
static void folder_url(const char *path, char *out, size_t cap) {
  size_t n = 0;
  const char *prefix = (path[0] == '/') ? "file://" : "file:///";
  for (const char *p = prefix; *p && n + 1 < cap; p++) out[n++] = *p;
  for (const char *p = path; *p && n + 4 < cap; p++) {
    const unsigned char c = (unsigned char)*p;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '-' || c == '_' || c == '.' || c == '~' || c == ':') {
      out[n++] = (char)c;
    } else if (c == '\\') {
      out[n++] = '/';
    } else {
      n += (size_t)SDL_snprintf(out + n, cap - n, "%%%02X", c);
    }
  }
  out[n] = 0;
}

static void open_data_folder(App *a) {
  char url[AD_PATH_MAX * 3];
  folder_url(a->store.dir, url, sizeof url);
  if (!SDL_OpenURL(url)) SDL_snprintf(a->notice, sizeof a->notice, "Could not open the folder. It is at:\n%s", a->store.dir);
}

static void copy_data_path(App *a) {
  if (SDL_SetClipboardText(a->store.path)) SDL_snprintf(a->notice, sizeof a->notice, "Copied the path of your data file.");
}

/* ---- saving ------------------------------------------------------------ */

static void commit(App *a) {
  if (!a->store_open) return;
  char msg[300];
  if (ad_store_save(&a->store, &a->model, msg, sizeof msg)) a->save_error[0] = 0;
  else SDL_snprintf(a->save_error, sizeof a->save_error, "Could not save: %s", msg);
}

/* ---- board drawing ----------------------------------------------------- */

static float line_px(const App *a) {
  const float p = a->view.pitch;
  return fmaxf(1.0f, roundf(p * (a->g.high_contrast ? 0.07f : 0.04f)));
}

static void draw_cell(App *a, ad_rect r, ad_state st, bool is_today, bool hover, bool focus) {
  gfx *g = &a->g;
  const float p = r.w;
  const float s = roundf(p * 0.62f); /* the cell itself */
  const float cx = r.x + p * 0.5f, cy = r.y + p * 0.5f;
  const float lw = line_px(a);
  if (hover) gfx_fill_rect(g, g->theme.faint, cx - s / 2, cy - s / 2, s, s);
  if (is_today) {
    const float big = roundf(p * 0.94f);
    gfx_mask(g, GFX_MASK_SQUIRCLE, 0, (int)lroundf(lw * 1.6f * 4), cx - big / 2, cy - big / 2, big, g->theme.fg);
  }
  gfx_mask(g, GFX_MASK_SQUARE, 0, (int)lroundf(lw * 4), cx - s / 2, cy - s / 2, s, g->theme.fg);
  if (st == AD_FILLED) {
    const float d = roundf(s * 0.66f);
    gfx_mask(g, GFX_MASK_DOT, 100, 0, cx - d / 2, cy - d / 2, d, g->theme.fg);
  } else if (st == AD_SKIPPED) {
    gfx_mask(g, GFX_MASK_SLASH, 0, (int)lroundf(lw * 4), cx - s / 2, cy - s / 2, s, g->theme.fg);
  }
  if (focus) {
    const float f = roundf(s + lw * 4);
    gfx_stroke_rect(g, g->theme.fg, roundf(cx - f / 2), roundf(cy - f / 2), f, f, lw);
  }
}

/* A habit's symbol cell: a solid square with the symbol knocked out of it. */
static void symbol_cell(App *a, float x, float y, float size, ad_symbol sym) {
  gfx *g = &a->g;
  gfx_fill_rect(g, g->theme.fg, roundf(x), roundf(y), roundf(size), roundf(size));
  const float m = roundf(size * 0.72f);
  gfx_mask(g, GFX_MASK_SYMBOL, (int)sym, 0, x + (size - m) * 0.5f, y + (size - m) * 0.5f, m, g->theme.bg);
}

static void draw_label_cell(App *a, ad_rect r, ad_symbol sym) {
  const float s = roundf(r.w * 0.62f);
  symbol_cell(a, r.x + (r.w - s) * 0.5f, r.y + (r.h - s) * 0.5f, s, sym);
}

static void format_long_date(ad_day d, char *out, size_t cap) {
  int y, m, dd;
  ad_day_to_ymd(d, &y, &m, &dd);
  SDL_snprintf(out, cap, "%s %d %s %d", ad_weekday_name(ad_day_weekday(d)), dd, ad_month_name(m), y);
}

/* The habits on the board, in order: board row -> habit index. */
static int board_habits(const App *a, int out[AD_MAX_BOARD]) {
  int n = 0;
  for (int i = 0; i < a->model.n_habits && n < AD_MAX_BOARD; i++)
    if (!a->model.habits[i].archived) out[n++] = i;
  return n;
}

static float corner_bar_h(const App *a) { return 64 * a->g.scale; }

static void layout_view(App *a) {
  gfx *g = &a->g;
  int w, h;
  SDL_GetWindowSizeInPixels(a->win, &w, &h);
  const float margin = 28 * g->scale;
  ad_view *v = &a->view;
  int bh[AD_MAX_BOARD];
  const int nb = board_habits(a, bh);
  v->layout = a->model.settings.layout;
  v->x = margin;
  v->y = margin;
  v->w = w - 2 * margin;
  v->h = h - margin - corner_bar_h(a);
  v->pitch = roundf(46 * g->scale);
  v->head_h = roundf(gfx_text_height(g) + 10 * g->scale);
  /* shrink the cells a little if the rows would not fit in a short window */
  if (v->layout == AD_LAYOUT_HORIZONTAL && nb > 0) {
    const float avail = v->h - v->head_h;
    if (nb * v->pitch > avail) v->pitch = fmaxf(roundf(avail / nb), roundf(26 * g->scale));
  }
  v->rows = nb;
  v->origin = a->model.today;
  for (int i = 0; i < nb; i++)
    if (a->model.habits[bh[i]].start < v->origin) v->origin = a->model.habits[bh[i]].start;
  v->last = a->model.today + AD_HORIZON_DAYS;
  ad_view_update(v);
}

static void click_cell(App *a, int habit, ad_day day, bool left) {
  /* Sealed days simply ignore the click (the data layer refuses it anyway). */
  const ad_result r = left ? ad_model_toggle_fill(&a->model, habit, day) : ad_model_toggle_skip(&a->model, habit, day);
  if (r == AD_OK) commit(a);
}

/* ---- the measure tool's tooltip --------------------------------------------- */

static void draw_measure_tooltip(App *a, const ad_measure m) {
  gfx *g = &a->g;
  ui_state *u = &a->ui;
  char lines[4][96];
  int n = 0;
  SDL_snprintf(lines[n++], sizeof lines[0], "%d %s", m.cells, m.cells == 1 ? "cell" : "cells");
  char counts[96];
  SDL_snprintf(counts, sizeof counts, "%d filled, %d skipped, %d empty", m.filled, m.skipped, m.empty);
  if (m.ahead) SDL_snprintf(counts + strlen(counts), sizeof counts - strlen(counts), ", %d ahead", m.ahead);
  SDL_strlcpy(lines[n++], counts, sizeof lines[0]);
  if (m.longest > 0) {
    if (m.streaks > 1) SDL_snprintf(lines[n++], sizeof lines[0], "Longest streak: %d (%d streaks)", m.longest, m.streaks);
    else SDL_snprintf(lines[n++], sizeof lines[0], "Longest streak: %d", m.longest);
  } else {
    SDL_strlcpy(lines[n++], "No streak in here", sizeof lines[0]);
  }
  if (m.percent >= 0) SDL_snprintf(lines[n++], sizeof lines[0], "%d%% filled", m.percent);
  else SDL_strlcpy(lines[n++], "Nothing here has happened yet", sizeof lines[0]);

  gfx_text_scale(g, 1.3f);
  const float pad = 12 * g->scale, lh = gfx_text_height(g);
  float w = 0;
  for (int i = 0; i < n; i++) {
    const float tw = gfx_text_width(g, lines[i]);
    if (tw > w) w = tw;
  }
  const float bw = w + 2 * pad, bh = lh * n + 2 * pad;
  int W, H;
  SDL_GetWindowSizeInPixels(a->win, &W, &H);
  /* up and to the right of the pointer, so the cells being measured stay visible */
  float x = u->mx + 18 * g->scale, y = u->my - bh - 14 * g->scale;
  if (x + bw > W - 4) x = u->mx - bw - 18 * g->scale;
  if (x < 4) x = 4;
  if (y < 4) y = u->my + 22 * g->scale;
  if (y + bh > H - 4) y = H - bh - 4;
  gfx_fill_rect(g, g->theme.bg, x, y, bw, bh);
  gfx_stroke_rect(g, g->theme.fg, x, y, bw, bh, fmaxf(1.0f, u->line * 1.5f));
  for (int i = 0; i < n; i++) gfx_text(g, lines[i], x + pad, y + pad + i * lh, i == 0 ? g->theme.fg : (i == n - 1 ? g->theme.fg : g->theme.dim));
  gfx_text_scale(g, 1.0f);
}

static void board_frame(App *a) {
  gfx *g = &a->g;
  ui_state *u = &a->ui;
  ad_view *v = &a->view;
  layout_view(a);
  int bh[AD_MAX_BOARD];
  const int nb = board_habits(a, bh);
  const bool live = !a->modal; /* the board reacts to the mouse and wheel */

  if (nb == 0) {
    const char *msg = "No habits yet. Use the Habits button, bottom-left, to add one.";
    gfx_text_center(g, msg, v->x, v->y, v->w, v->h, g->theme.dim);
    return;
  }

  /* scrolling */
  if (live && (u->wheel_x != 0 || u->wheel_y != 0) && !ad_rect_has(a->banner_rect, u->mx, u->my)) {
    const float step = v->pitch * 1.5f;
    if (v->layout == AD_LAYOUT_HORIZONTAL) v->scroll += (u->wheel_x != 0 ? u->wheel_x : -u->wheel_y) * step;
    else v->scroll += -u->wheel_y * step;
    ad_view_update(v);
  }

  ad_day first, last;
  ad_view_visible_days(v, &first, &last);
  const ad_model *m = &a->model;

  int hover_row = -1;
  ad_day hover_day = 0;
  bool hover_cell = live && u->mouse_in && ad_view_hit_cell(v, u->mx, u->my, &hover_row, &hover_day);
  if (hover_cell && hover_day < m->habits[bh[hover_row]].start) hover_cell = false;
  if (ad_rect_has(a->banner_rect, u->mx, u->my)) hover_cell = false;

  /* A press on a cell starts a gesture. Letting go on the same cell is a
   * click (fill or skip). Dragging to other cells is the measure tool, and
   * never changes a cell. */
  if (hover_cell && !u->consumed_click && (u->lpress || u->rpress) && !a->drag_active) {
    a->cell_focus = true;
    a->focus_row = hover_row;
    a->focus_day = hover_day;
    a->kb_visible = false;
    u->focus = 0;
    a->drag_active = true;
    a->drag_button = u->lpress ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
    a->drag_row = hover_row;
    a->drag_a = a->drag_b = hover_day;
    a->dragged = false;
  }
  if (a->drag_active) {
    int hr;
    ad_day hd;
    if (!live || a->drag_row >= nb) {
      a->drag_active = false;
    } else {
      if (u->mouse_in && ad_view_hit_cell(v, u->mx, u->my, &hr, &hd)) {
        const ad_day st = m->habits[bh[a->drag_row]].start;
        a->drag_b = hd < st ? st : hd;
        if (a->drag_b != a->drag_a) a->dragged = true;
      }
      const bool released = a->drag_button == SDL_BUTTON_LEFT ? u->lrelease : u->rrelease;
      if (released) {
        if (!a->dragged) click_cell(a, bh[a->drag_row], a->drag_a, a->drag_button == SDL_BUTTON_LEFT);
        a->drag_active = false;
      }
    }
  }
  const bool measuring = a->drag_active && a->dragged;
  const ad_day sel_lo = a->drag_a < a->drag_b ? a->drag_a : a->drag_b;
  const ad_day sel_hi = a->drag_a < a->drag_b ? a->drag_b : a->drag_a;
  if (a->cell_focus && a->focus_row >= nb) a->cell_focus = false;

  /* Cells and headers are clipped so they slide under the symbol column. */
  const bool horiz = v->layout == AD_LAYOUT_HORIZONTAL;
  SDL_Rect clip = {(int)(v->x + (horiz ? v->pitch : 0)), (int)v->y, (int)(v->w - (horiz ? v->pitch : 0)), (int)v->h};
  gfx_clip(g, &clip);
  for (ad_day d = first; d <= last; d++) {
    int y, mo, dd;
    ad_day_to_ymd(d, &y, &mo, &dd);
    ad_rect hr;
    if (!ad_view_cell_rect(v, 0, d, &hr)) continue;
    int chunk, col;
    ad_view_chunk_of(v, d, &chunk, &col);
    /* header text: months (horizontal) or the first day of each line (vertical) */
    const float ty = ad_view_chunk_top(v, chunk) + (v->head_h - gfx_text_height(g)) * 0.5f;
    char buf[48];
    if (horiz && (dd == 1 || d == v->origin)) {
      if (mo == 1 || d == v->origin) SDL_snprintf(buf, sizeof buf, "%s %d", ad_month_short(mo), y);
      else SDL_snprintf(buf, sizeof buf, "%s", ad_month_short(mo));
      gfx_text(g, buf, hr.x + v->pitch * 0.19f, ty, g->theme.dim);
    } else if (!horiz && col == 0) {
      SDL_snprintf(buf, sizeof buf, "%d %s %d", dd, ad_month_short(mo), y);
      gfx_text(g, buf, hr.x + v->pitch * 0.19f, ty, g->theme.dim);
    }
    for (int r = 0; r < v->rows; r++) {
      const ad_habit *h = &m->habits[bh[r]];
      if (d < h->start) continue;
      ad_rect rc;
      ad_view_cell_rect(v, r, d, &rc);
      if (rc.y + rc.h < v->y || rc.y > v->y + v->h) continue;
      if (measuring && r == a->drag_row && d >= sel_lo && d <= sel_hi)
        gfx_fill_rect(g, gfx_alpha(g->theme.fg, g->high_contrast ? 70 : 38), rc.x + 1, rc.y + 1, rc.w - 2, rc.h - 2);
      draw_cell(a, rc, ad_habit_get(h, d), d == m->today, hover_cell && r == hover_row && d == hover_day,
                a->cell_focus && a->kb_visible && u->focus == 0 && r == a->focus_row && d == a->focus_day);
    }
  }
  gfx_clip(g, NULL);

  /* the symbol column */
  for (int c = 0; c < v->chunks; c++) {
    const float top = ad_view_chunk_top(v, c);
    if (top + v->chunk_h < v->y || top > v->y + v->h) continue;
    SDL_Rect lc = {(int)v->x, (int)v->y, (int)v->pitch, (int)v->h};
    gfx_clip(g, &lc);
    for (int r = 0; r < v->rows; r++) {
      ad_rect lr;
      if (ad_view_label_rect(v, r, c, &lr)) draw_label_cell(a, lr, m->habits[bh[r]].symbol);
    }
    gfx_clip(g, NULL);
  }

  /* tooltips */
  int lrow;
  if (measuring) {
    draw_measure_tooltip(a, ad_measure_range(&m->habits[bh[a->drag_row]], sel_lo, sel_hi, m->today));
  } else if (live && u->mouse_in && ad_view_hit_label(v, u->mx, u->my, &lrow)) {
    ui_tooltip(u, m->habits[bh[lrow]].name, NULL, u->mx, u->my);
  } else if (hover_cell && !a->drag_active) {
    char buf[64];
    format_long_date(hover_day, buf, sizeof buf);
    ui_tooltip(u, buf, hover_day == m->today ? "Today" : NULL, u->mx, u->my);
  }

  /* With the keyboard there is no hover, so say what the focused cell is. */
  if (a->cell_focus && a->kb_visible && u->focus == 0 && !a->modal) {
    static const char *const STATE[3] = {"empty", "filled", "skipped"};
    char date[64], line[200];
    format_long_date(a->focus_day, date, sizeof date);
    const ad_habit *h = &m->habits[bh[a->focus_row]];
    SDL_snprintf(line, sizeof line, "%s, %s: %s%s", h->name, date, STATE[ad_habit_get(h, a->focus_day)], a->focus_day == m->today ? " (today)" : (a->focus_day < m->today ? " (sealed)" : ""));
    int W, H;
    SDL_GetWindowSizeInPixels(a->win, &W, &H);
    gfx_text_center(g, line, 0, H - corner_bar_h(a), (float)W, corner_bar_h(a), g->theme.dim);
  }
}

/* ---- hotkeys ------------------------------------------------------------- */

static bool ad_key_from_event(const ui_key *k, ad_key *out) {
  memset(out, 0, sizeof *out);
  const SDL_Keycode c = k->key;
  if (c >= 'a' && c <= 'z') {
    out->key[0] = (char)(c - 'a' + 'A');
  } else if (c > 32 && c < 127) {
    out->key[0] = (char)c;
  } else if (c == SDLK_RETURN || c == SDLK_KP_ENTER) strcpy(out->key, "Enter");
  else if (c == SDLK_SPACE) strcpy(out->key, "Space");
  else if (c == SDLK_LEFT) strcpy(out->key, "Left");
  else if (c == SDLK_RIGHT) strcpy(out->key, "Right");
  else if (c == SDLK_UP) strcpy(out->key, "Up");
  else if (c == SDLK_DOWN) strcpy(out->key, "Down");
  else if (c == SDLK_TAB) strcpy(out->key, "Tab");
  else if (c == SDLK_ESCAPE) strcpy(out->key, "Escape");
  else if (c == SDLK_BACKSPACE) strcpy(out->key, "Backspace");
  else if (c == SDLK_DELETE) strcpy(out->key, "Delete");
  else if (c == SDLK_HOME) strcpy(out->key, "Home");
  else if (c == SDLK_END) strcpy(out->key, "End");
  else if (c == SDLK_PAGEUP) strcpy(out->key, "PageUp");
  else if (c == SDLK_PAGEDOWN) strcpy(out->key, "PageDown");
  else if (c == SDLK_INSERT) strcpy(out->key, "Insert");
  else if (c >= SDLK_F1 && c <= SDLK_F12) SDL_snprintf(out->key, sizeof out->key, "F%d", (int)(c - SDLK_F1) + 1);
  else return false;
  if (k->mod & SDL_KMOD_CTRL) out->mods |= AD_MOD_CTRL;
  if (k->mod & SDL_KMOD_ALT) out->mods |= AD_MOD_ALT;
  if (k->mod & SDL_KMOD_SHIFT) out->mods |= AD_MOD_SHIFT;
  if (k->mod & SDL_KMOD_GUI) out->mods |= AD_MOD_CMD;
  return true;
}

static ad_binding binding_for(const App *a, ad_action act) {
  if (a->model.settings.hotkey_set[act]) return a->model.settings.hotkey[act];
  return ad_hotkey_default(act);
}

/* Was a key bound to this action pressed? Consumes the key press. */
static bool action_pressed(App *a, ad_action act) {
  ui_state *u = &a->ui;
  const ad_binding b = binding_for(a, act);
  for (int i = 0; i < u->nkeys; i++) {
    ui_key *k = &u->keys[i];
    ad_key ak;
    if (k->used || !ad_key_from_event(k, &ak) || !ad_binding_has(&b, &ak)) continue;
    const bool arrow = act <= AD_ACT_DOWN;
    if (k->repeat && !arrow) continue; /* holding a key down must not toggle cells */
    k->used = true;
    return true;
  }
  return false;
}

/* ---- banners and the fatal screen -------------------------------------- */

static void banners_frame(App *a) {
  ui_state *u = &a->ui;
  gfx *g = &a->g;
  a->banner_rect = (ad_rect){0, 0, 0, 0};
  int w, h;
  SDL_GetWindowSizeInPixels(a->win, &w, &h);
  const float pad = 12 * g->scale;
  const char *text = a->save_error[0] ? a->save_error : (a->notice[0] ? a->notice : NULL);
  if (!text) return;
  const float bw = fminf(w - 2 * pad, 720 * g->scale);
  const float btn_w = 84 * g->scale;
  const float th = ui_wrapped_text(u, text, 0, 0, bw - 3 * pad - btn_w, g->theme.fg, false);
  const float bh = fmaxf(th, 34 * g->scale) + 2 * pad;
  ad_rect r = {(w - bw) * 0.5f, 10 * g->scale, bw, bh};
  a->banner_rect = r;
  ui_panel(u, r);
  ui_wrapped_text(u, text, r.x + pad, r.y + pad, bw - 3 * pad - btn_w, g->theme.fg, true);
  ad_rect b = {r.x + bw - pad - btn_w, r.y + (bh - 34 * g->scale) * 0.5f, btn_w, 34 * g->scale};
  if (a->save_error[0]) {
    if (ui_button(u, ID_ERR_RETRY, b, "Retry", UI_BTN_NORMAL)) commit(a);
  } else if (ui_button(u, ID_NOTICE_OK, b, "OK", UI_BTN_NORMAL)) {
    a->notice[0] = 0;
  }
}

/* ---- the habit pop-up ---------------------------------------------------- */

typedef enum { ACT_NONE, ACT_ADD, ACT_RENAME_SAVE, ACT_ARCHIVE, ACT_RESTORE, ACT_DELETE, ACT_SYMBOL, ACT_PROMPT_ARCHIVE, ACT_PROMPT_DELETE } ActKind;

typedef struct {
  ActKind kind;
  int habit;
  int symbol;
} Action;

static void popup_close(App *a) {
  a->popup_open = false;
  a->modal = false;
  a->edit = EDIT_NONE;
  a->full_prompt = false;
  a->popup_msg[0] = 0;
  a->ui.focus = 0;
}

static void popup_open_now(App *a) {
  a->popup_open = true;
  a->edit = EDIT_NONE;
  a->full_prompt = false;
  a->popup_msg[0] = 0;
  a->popup_scroll = 0;
  a->ui.focus = ID_POP_NAME;
  a->ui.focus_visible = false;
  a->ui.text[0] = 0; /* the key that opened it must not be typed into the field */
}

static float pop_row_h(App *a) { return fmaxf(roundf(42 * a->g.scale), gfx_text_height(&a->g) + 16 * a->g.scale); }

/* One habit in the list. `mode`: 0 board, 1 archived, 2 "the board is full" prompt. */
static void habit_row(App *a, float x, float *y, float w, int hi, int mode, Action *act) {
  ui_state *u = &a->ui;
  gfx *g = &a->g;
  ad_habit *h = &a->model.habits[hi];
  const float rh = pop_row_h(a), gap = 8 * g->scale;
  const float cell = roundf(rh * 0.72f);
  const int id0 = ID_HABIT_ROWS + hi * 16;
  const bool renaming = a->edit == EDIT_RENAME && a->edit_habit == hi;
  const bool deleting = a->edit == EDIT_DELETE && a->edit_habit == hi;
  const float cy = *y;

  symbol_cell(a, x, cy + (rh - cell) * 0.5f, cell, h->symbol);

  /* the buttons, right to left */
  const char *labels[4];
  int ids[4], kinds[4], n = 0;
  if (renaming) {
    labels[n] = "Save"; ids[n] = id0 + 1; kinds[n++] = ACT_RENAME_SAVE;
    labels[n] = "Cancel"; ids[n] = id0 + 2; kinds[n++] = ACT_NONE;
  } else if (deleting) {
    labels[n] = "Delete"; ids[n] = id0 + 1; kinds[n++] = (mode == 2) ? ACT_PROMPT_DELETE : ACT_DELETE;
    labels[n] = "Keep"; ids[n] = id0 + 2; kinds[n++] = ACT_NONE;
  } else if (mode == 0) {
    labels[n] = "Rename"; ids[n] = id0 + 3; kinds[n++] = -1;
    labels[n] = "Symbol"; ids[n] = id0 + 4; kinds[n++] = -2;
    labels[n] = "Archive"; ids[n] = id0 + 5; kinds[n++] = ACT_ARCHIVE;
    labels[n] = "Delete"; ids[n] = id0 + 6; kinds[n++] = -3;
  } else if (mode == 1) {
    labels[n] = "Restore"; ids[n] = id0 + 7; kinds[n++] = ACT_RESTORE;
    labels[n] = "Delete"; ids[n] = id0 + 6; kinds[n++] = -3;
  } else {
    labels[n] = "Archive"; ids[n] = id0 + 5; kinds[n++] = ACT_PROMPT_ARCHIVE;
    labels[n] = "Delete"; ids[n] = id0 + 6; kinds[n++] = -3;
  }
  float bx = x + w;
  const float bh = roundf(rh * 0.78f), by = cy + (rh - bh) * 0.5f;
  for (int i = n - 1; i >= 0; i--) {
    const float bw = ui_button_width(u, labels[i]);
    bx -= bw;
    ad_rect br = {bx, by, bw, bh};
    int flags = UI_BTN_NORMAL;
    if (kinds[i] == ACT_RESTORE && ad_model_board_count(&a->model) >= AD_MAX_BOARD) flags = UI_BTN_DISABLED;
    if (ui_button(u, ids[i], br, labels[i], flags)) {
      if (kinds[i] == -1) {
        a->edit = EDIT_RENAME;
        a->edit_habit = hi;
        SDL_strlcpy(a->edit_buf, h->name, sizeof a->edit_buf);
        a->popup_msg[0] = 0;
        u->focus = id0;
        u->cursor = (int)strlen(a->edit_buf);
      } else if (kinds[i] == -2) {
        a->edit = (a->edit == EDIT_SYMBOL && a->edit_habit == hi) ? EDIT_NONE : EDIT_SYMBOL;
        a->edit_habit = hi;
        a->popup_msg[0] = 0;
      } else if (kinds[i] == -3) {
        a->edit = EDIT_DELETE;
        a->edit_habit = hi;
        a->popup_msg[0] = 0;
      } else if (kinds[i] == ACT_NONE) {
        a->edit = EDIT_NONE;
        a->popup_msg[0] = 0;
      } else {
        act->kind = (ActKind)kinds[i];
        act->habit = hi;
      }
    }
    bx -= gap * 0.6f;
  }

  /* the name, or the field that edits it */
  const float nx = x + cell + gap * 1.5f, nw = bx - nx - gap * 0.5f;
  if (renaming) {
    ad_rect fr = {nx, cy + (rh - bh) * 0.5f, nw, bh};
    const int r = ui_text_field(u, id0, fr, a->edit_buf, sizeof a->edit_buf, AD_NAME_MAX_CHARS, "Habit name");
    if (r == 1) {
      act->kind = ACT_RENAME_SAVE;
      act->habit = hi;
    } else if (r == 2) {
      a->edit = EDIT_NONE;
    }
  } else if (deleting) {
    gfx_text_fit(g, "Delete for good? There is no undo.", nx, cy + (rh - gfx_text_height(g)) * 0.5f, nw, g->theme.fg);
  } else {
    gfx_text_fit(g, h->name, nx, cy + (rh - gfx_text_height(g)) * 0.5f, nw, mode == 1 ? g->theme.dim : g->theme.fg);
  }
  *y += rh;

  /* the symbol chooser opens under its row */
  if (a->edit == EDIT_SYMBOL && a->edit_habit == hi && mode == 0) {
    const float sz = roundf(fminf(rh * 0.9f, (w - cell) / (AD_SYM_COUNT + 1)));
    float sx = x + cell + gap * 1.5f;
    for (int sy = 0; sy < AD_SYM_COUNT; sy++) {
      bool used = false;
      for (int j = 0; j < a->model.n_habits; j++)
        if (j != hi && !a->model.habits[j].archived && a->model.habits[j].symbol == (ad_symbol)sy) used = true;
      ad_rect sr = {sx, *y + gap * 0.5f, sz, sz};
      const int flags = (h->symbol == (ad_symbol)sy ? UI_BTN_SELECTED : 0) | (used ? UI_BTN_DISABLED : 0);
      if (ui_icon_button(u, id0 + 8 + (sy % 8), sr, GFX_MASK_SYMBOL, sy, flags) && !used) {
        act->kind = ACT_SYMBOL;
        act->habit = hi;
        act->symbol = sy;
      }
      sx += sz + gap * 0.6f;
      if (sy == 11) break;
    }
    *y += sz + gap * 1.5f;
  }
}

static void apply_action(App *a, const Action *act) {
  ad_name_result nr = AD_NAME_OK;
  ad_habit_result r;
  switch (act->kind) {
    case ACT_NONE: break;
    case ACT_RENAME_SAVE:
      r = ad_model_rename(&a->model, act->habit, a->edit_buf, &nr);
      if (r == AD_HABIT_OK) {
        a->edit = EDIT_NONE;
        a->popup_msg[0] = 0;
        commit(a);
      } else {
        SDL_snprintf(a->popup_msg, sizeof a->popup_msg, "%s", ad_habit_result_message(r, nr));
      }
      break;
    case ACT_SYMBOL:
      r = ad_model_set_symbol(&a->model, act->habit, (ad_symbol)act->symbol);
      if (r == AD_HABIT_OK) commit(a);
      else SDL_snprintf(a->popup_msg, sizeof a->popup_msg, "%s", ad_habit_result_message(r, nr));
      break;
    case ACT_ARCHIVE:
      if (ad_model_archive(&a->model, act->habit) == AD_HABIT_OK) commit(a);
      a->edit = EDIT_NONE;
      break;
    case ACT_RESTORE: {
      int idx;
      r = ad_model_restore(&a->model, act->habit, &idx);
      if (r == AD_HABIT_OK) commit(a);
      else SDL_snprintf(a->popup_msg, sizeof a->popup_msg, "%s", ad_habit_result_message(r, nr));
      a->edit = EDIT_NONE;
      break;
    }
    case ACT_DELETE:
      if (ad_model_delete(&a->model, act->habit) == AD_HABIT_OK) commit(a);
      a->edit = EDIT_NONE;
      break;
    case ACT_PROMPT_ARCHIVE:
    case ACT_PROMPT_DELETE: {
      /* make room, then add the habit the person was trying to create */
      const bool ok = act->kind == ACT_PROMPT_ARCHIVE ? ad_model_archive(&a->model, act->habit) == AD_HABIT_OK : ad_model_delete(&a->model, act->habit) == AD_HABIT_OK;
      a->edit = EDIT_NONE;
      if (ok) {
        r = ad_model_create(&a->model, a->pending_name, NULL, &nr);
        if (r == AD_HABIT_OK) a->new_name[0] = 0;
        else SDL_snprintf(a->popup_msg, sizeof a->popup_msg, "%s", ad_habit_result_message(r, nr));
        commit(a);
      }
      a->full_prompt = false;
      break;
    }
    case ACT_ADD:
      r = ad_model_create(&a->model, a->new_name, NULL, &nr);
      if (r == AD_HABIT_OK) {
        a->new_name[0] = 0;
        a->popup_msg[0] = 0;
        commit(a);
      } else if (r == AD_HABIT_BOARD_FULL) {
        /* the ninth habit: ask which one to make room by */
        SDL_strlcpy(a->pending_name, a->new_name, sizeof a->pending_name);
        a->full_prompt = true;
        a->edit = EDIT_NONE;
        a->popup_msg[0] = 0;
      } else {
        SDL_snprintf(a->popup_msg, sizeof a->popup_msg, "%s", ad_habit_result_message(r, nr));
      }
      break;
  }
}

static void habit_popup_frame(App *a) {
  ui_state *u = &a->ui;
  gfx *g = &a->g;
  int W, H;
  SDL_GetWindowSizeInPixels(a->win, &W, &H);
  const float s = g->scale, pad = 18 * s;
  const float margin = 28 * s;
  const float pw = fminf(W - 2 * margin, 700 * s);
  const float bottom = H - 52 * s - 10 * s;
  const float title_h = gfx_text_height(g) + 2 * pad * 0.8f;
  const float max_h = bottom - 16 * s;
  float content_h = a->popup_content_h > 0 ? a->popup_content_h : 320 * s;
  float ph = title_h + content_h + pad;
  if (ph > max_h) ph = max_h;
  ad_rect panel = {margin, bottom - ph, pw, ph};
  a->popup_rect = panel;
  ui_panel(u, panel);

  /* title row: fixed, not scrolled */
  char title[64];
  SDL_snprintf(title, sizeof title, "Habits  (%d of %d on the board)", ad_model_board_count(&a->model), AD_MAX_BOARD);
  gfx_text(g, title, panel.x + pad, panel.y + pad * 0.8f, g->theme.fg);
  const float cb = ui_button_width(u, "Close");
  ad_rect close = {panel.x + pw - pad - cb, panel.y + pad * 0.5f, cb, gfx_text_height(g) + pad * 0.6f};
  if (ui_button(u, ID_POP_CLOSE, close, "Close", UI_BTN_NORMAL)) {
    popup_close(a);
    return;
  }
  gfx_fill_rect(g, g->theme.dim, panel.x, panel.y + title_h, pw, fmaxf(1.0f, u->line * 0.5f));

  /* scrolling content */
  ad_rect view = {panel.x, panel.y + title_h + 2, pw, ph - title_h - 2};
  u->use_clip = true;
  u->clip = view;
  SDL_Rect clip = {(int)view.x + 1, (int)view.y, (int)view.w - 2, (int)view.h - 1};
  gfx_clip(g, &clip);
  if (u->mouse_in && ad_rect_has(view, u->mx, u->my) && u->wheel_y != 0) a->popup_scroll -= u->wheel_y * 48 * s;
  const float max_scroll = fmaxf(0, a->popup_content_h + pad - view.h);
  if (a->popup_scroll > max_scroll) a->popup_scroll = max_scroll;
  if (a->popup_scroll < 0) a->popup_scroll = 0;

  const float x = panel.x + pad, w = pw - 2 * pad;
  float y = view.y + pad * 0.6f - a->popup_scroll;
  const float top = y;
  const float lh = gfx_text_height(g);
  Action act = {ACT_NONE, -1, 0};

  if (a->full_prompt) {
    char msg[300];
    SDL_snprintf(msg, sizeof msg, "The board holds %d habits and it is full. To add \"%s\", archive or delete one of these:", AD_MAX_BOARD, a->pending_name);
    y += ui_wrapped_text(u, msg, x, y, w, g->theme.fg, true) + 8 * s;
    for (int n = 0; n < AD_MAX_BOARD; n++) {
      const int hi = ad_model_board_habit(&a->model, n);
      if (hi < 0) break;
      habit_row(a, x, &y, w, hi, 2, &act);
    }
    y += 6 * s;
    ad_rect cr = {x, y, ui_button_width(u, "Never mind"), pop_row_h(a) * 0.78f};
    if (ui_button(u, ID_POP_CANCEL, cr, "Never mind", UI_BTN_NORMAL)) {
      a->full_prompt = false;
      a->edit = EDIT_NONE;
    }
    y += cr.h + 8 * s;
  } else {
    int nboard = 0;
    for (int n = 0; n < AD_MAX_BOARD; n++) {
      const int hi = ad_model_board_habit(&a->model, n);
      if (hi < 0) break;
      habit_row(a, x, &y, w, hi, 0, &act);
      nboard++;
    }
    if (nboard == 0) {
      gfx_text(g, "Nothing on the board yet.", x, y + 6 * s, g->theme.dim);
      y += lh + 14 * s;
    }
    y += 10 * s;

    /* create */
    gfx_text(g, "New habit. It starts today.", x, y, g->theme.dim);
    y += lh + 6 * s;
    const float fh = roundf(pop_row_h(a) * 0.8f), addw = ui_button_width(u, "Add");
    ad_rect fr = {x, y, w - addw - 8 * s, fh};
    const int fres = ui_text_field(u, ID_POP_NAME, fr, a->new_name, sizeof a->new_name, AD_NAME_MAX_CHARS, "Name, up to 40 characters");
    ad_rect ar = {x + w - addw, y, addw, fh};
    if (ui_button(u, ID_POP_ADD, ar, "Add", UI_BTN_PRIMARY) || fres == 1) {
      act.kind = ACT_ADD;
    }
    if (fres == 2) {
      if (a->new_name[0]) a->new_name[0] = 0;
      else {
        popup_close(a);
        return;
      }
    }
    y += fh + 6 * s;
    if (a->popup_msg[0]) y += ui_wrapped_text(u, a->popup_msg, x, y, w, g->theme.fg, true) + 4 * s;

    /* archived */
    int narch = 0;
    for (int i = 0; i < a->model.n_habits; i++)
      if (a->model.habits[i].archived) narch++;
    y += 12 * s;
    char hdr[48];
    SDL_snprintf(hdr, sizeof hdr, "Archived (%d)", narch);
    gfx_text(g, hdr, x, y, g->theme.dim);
    y += lh + 6 * s;
    if (narch == 0) {
      gfx_text(g, "Archived habits keep their history and wait here.", x, y, g->theme.dim);
      y += lh + 8 * s;
    }
    for (int i = 0; i < a->model.n_habits; i++)
      if (a->model.habits[i].archived) habit_row(a, x, &y, w, i, 1, &act);
    if (narch > 0 && ad_model_board_count(&a->model) >= AD_MAX_BOARD) {
      gfx_text(g, "The board is full, so archived habits can't be restored yet.", x, y + 4 * s, g->theme.dim);
      y += lh + 8 * s;
    }
  }
  gfx_clip(g, NULL);
  u->use_clip = false;

  const float used = y - top + pad * 1.2f;
  if (fabsf(used - a->popup_content_h) > 1.0f) {
    a->popup_content_h = used;
    a->redraw = true;
  }

  /* keyboard focus moved out of sight: scroll to it */
  if (u->want_visible) {
    if (u->want_y0 < view.y) a->popup_scroll -= view.y - u->want_y0 + 8 * s;
    else if (u->want_y1 > view.y + view.h) a->popup_scroll += u->want_y1 - (view.y + view.h) + 8 * s;
  }

  /* clicking outside the pop-up closes it (but not on its own button) */
  if (u->lpress && !u->consumed_click && !ad_rect_has(panel, u->mx, u->my)) {
    popup_close(a);
    u->consumed_click = true;
    return;
  }
  apply_action(a, &act);
}

/* ---- the two corner buttons ----------------------------------------------- */

static void corner_buttons_frame(App *a) {
  ui_state *u = &a->ui;
  gfx *g = &a->g;
  int W, H;
  SDL_GetWindowSizeInPixels(a->win, &W, &H);
  const float s = g->scale;
  const float bh = 38 * s;
  ad_rect hb = {28 * s, H - 14 * s - bh, ui_button_width(u, "Habits"), bh};
  if (ui_button(u, ID_HABIT_BTN, hb, "Habits", a->popup_open ? UI_BTN_SELECTED : UI_BTN_NORMAL)) {
    if (a->popup_open) popup_close(a);
    else popup_open_now(a);
  }
}

static void set_fatal(App *a, const char *title, const char *text, bool can_backup) {
  a->screen = SCREEN_FATAL;
  SDL_snprintf(a->fatal_title, sizeof a->fatal_title, "%s", title);
  SDL_snprintf(a->fatal_text, sizeof a->fatal_text, "%s", text);
  a->fatal_can_backup = can_backup;
}

static void fatal_frame(App *a) {
  ui_state *u = &a->ui;
  gfx *g = &a->g;
  int w, h;
  SDL_GetWindowSizeInPixels(a->win, &w, &h);
  const float pw = fminf(w - 40 * g->scale, 680 * g->scale);
  const float pad = 20 * g->scale;
  const float th = ui_wrapped_text(u, a->fatal_text, 0, 0, pw - 2 * pad, g->theme.fg, false);
  const float lh = gfx_text_height(g);
  const float ph = pad * 2 + lh * 2 + th + 40 * g->scale + 36 * g->scale;
  ad_rect panel = {(w - pw) * 0.5f, fmaxf(20 * g->scale, (h - ph) * 0.5f), pw, ph};
  ui_panel(u, panel);
  gfx_text(g, a->fatal_title, panel.x + pad, panel.y + pad, g->theme.fg);
  ui_wrapped_text(u, a->fatal_text, panel.x + pad, panel.y + pad + lh * 2, pw - 2 * pad, g->theme.fg, true);

  float bx = panel.x + pad;
  const float by = panel.y + ph - pad - 34 * g->scale, bh = 34 * g->scale;
  if (a->store.dir[0]) {
    ad_rect b = {bx, by, ui_button_width(u, "Open data folder"), bh};
    if (ui_button(u, ID_FATAL_FOLDER, b, "Open data folder", UI_BTN_NORMAL)) open_data_folder(a);
    bx += b.w + 10 * g->scale;
    ad_rect c = {bx, by, ui_button_width(u, "Copy file path"), bh};
    if (ui_button(u, ID_FATAL_COPY, c, "Copy file path", UI_BTN_NORMAL)) copy_data_path(a);
    bx += c.w + 10 * g->scale;
  }
  if (a->fatal_can_backup) {
    ad_rect b = {bx, by, ui_button_width(u, "Use the backup"), bh};
    if (ui_button(u, ID_FATAL_BACKUP, b, "Use the backup", UI_BTN_NORMAL)) {
      char msg[300];
      ad_open_status st = ad_store_use_backup(&a->store, &a->model, msg, sizeof msg);
      if (st == AD_OPEN_FROM_BACKUP) {
        a->screen = SCREEN_BOARD;
        SDL_snprintf(a->notice, sizeof a->notice, "%s", msg);
        ad_model_set_clock(&a->model, clock_day(a));
        ad_view_center_on(&a->view, a->model.today);
      } else {
        SDL_snprintf(a->fatal_text + strlen(a->fatal_text), sizeof a->fatal_text - strlen(a->fatal_text), "\n\n%s", msg);
      }
    }
    bx += b.w + 10 * g->scale;
  }
  ad_rect q = {panel.x + pw - pad - ui_button_width(u, "Quit"), by, ui_button_width(u, "Quit"), bh};
  if (ui_button(u, ID_FATAL_QUIT, q, "Quit", UI_BTN_PRIMARY) || ui_take_key(u, SDLK_ESCAPE, 0)) a->quit = true;
  if (!u->focus) u->focus = ID_FATAL_QUIT;
}

/* ---- first launch --------------------------------------------------------- */

static void first_frame(App *a) {
  ui_state *u = &a->ui;
  gfx *g = &a->g;
  int W, H;
  SDL_GetWindowSizeInPixels(a->win, &W, &H);
  const float s = g->scale, pad = 24 * s, lh = gfx_text_height(g);
  const float pw = fminf(W - 40 * s, 620 * s);
  const float fh = 42 * s;
  const float ph = pad * 2 + lh * 2 + 16 * s + lh * 2 + fh + 22 * s + lh + 8 * s + fh + 6 * s + lh * 1.2f + 24 * s + fh;
  ad_rect panel = {(W - pw) * 0.5f, fmaxf(16 * s, (H - ph) * 0.5f), pw, ph};
  ui_panel(u, panel);
  const float x = panel.x + pad, w = pw - 2 * pad;
  float y = panel.y + pad;
  gfx_text(g, "Welcome to Adderall", x, y, g->theme.fg);
  y += lh * 1.4f;
  y += ui_wrapped_text(u, "A grid of squares is not going to change your life. But it is a start.", x, y, w, g->theme.dim, true) + 14 * s;

  gfx_text(g, "Your first habit (or skip, and start with an empty board):", x, y, g->theme.fg);
  y += lh + 6 * s;
  ad_rect fr = {x, y, w, fh};
  const int fres = ui_text_field(u, ID_FIRST_NAME, fr, a->first_name, sizeof a->first_name, AD_NAME_MAX_CHARS, "For example: Drink water");
  y += fh + 6 * s;
  if (a->popup_msg[0]) gfx_text(g, a->popup_msg, x, y, g->theme.fg);
  y += lh + 12 * s;

  gfx_text(g, "How should your board be laid out? (You can change it later.)", x, y, g->theme.fg);
  y += lh + 6 * s;
  const float lw = (w - 10 * s) / 2;
  ad_rect l1 = {x, y, lw, fh}, l2 = {x + lw + 10 * s, y, lw, fh};
  if (ui_button(u, ID_FIRST_HORIZ, l1, "Horizontal: a line to the right", a->first_layout == AD_LAYOUT_HORIZONTAL ? UI_BTN_SELECTED : 0)) a->first_layout = AD_LAYOUT_HORIZONTAL;
  if (ui_button(u, ID_FIRST_VERT, l2, "Vertical: wraps downward", a->first_layout == AD_LAYOUT_VERTICAL ? UI_BTN_SELECTED : 0)) a->first_layout = AD_LAYOUT_VERTICAL;
  y += fh + 24 * s;

  const float sw = ui_button_width(u, "Start"), kw = ui_button_width(u, "Skip");
  ad_rect start = {x + w - sw, y, sw, fh}, skip = {x + w - sw - 10 * s - kw, y, kw, fh};
  bool go = ui_button(u, ID_FIRST_START, start, "Start", UI_BTN_PRIMARY) || fres == 1;
  const bool skipped = ui_button(u, ID_FIRST_SKIP, skip, "Skip", UI_BTN_NORMAL);
  if (!u->focus) u->focus = ID_FIRST_NAME;
  if (go && !a->first_name[0]) {
    SDL_strlcpy(a->popup_msg, "Type a name, or choose Skip.", sizeof a->popup_msg);
    go = false;
  }
  if (go || skipped) {
    ad_name_result nr;
    if (go) {
      const ad_habit_result r = ad_model_create(&a->model, a->first_name, NULL, &nr);
      if (r != AD_HABIT_OK) {
        SDL_snprintf(a->popup_msg, sizeof a->popup_msg, "%s", ad_habit_result_message(r, nr));
        return;
      }
    }
    a->model.settings.layout = a->first_layout;
    a->popup_msg[0] = 0;
    a->screen = SCREEN_BOARD;
    a->ui.focus = 0;
    commit(a);
    layout_view(a);
    ad_view_center_on(&a->view, a->model.today);
  }
}

/* ---- the frame ---------------------------------------------------------- */

/* Move the focused cell. dir: 0 left, 1 right, 2 up, 3 down. */
static void move_focus(App *a, int dir) {
  int bh[AD_MAX_BOARD];
  const int nb = board_habits(a, bh);
  if (nb == 0) return;
  layout_view(a);
  ad_view *v = &a->view;
  if (!a->cell_focus) {
    a->cell_focus = true;
    a->focus_row = 0;
    a->focus_day = a->model.today;
    if (a->focus_day < a->model.habits[bh[0]].start) a->focus_day = a->model.habits[bh[0]].start;
    a->kb_visible = true;
    ad_view_ensure_visible(v, a->focus_day);
    return; /* the first press only shows where the focus is */
  }
  ad_day starts[AD_MAX_BOARD];
  for (int i = 0; i < nb; i++) starts[i] = a->model.habits[bh[i]].start;
  int row = a->focus_row;
  ad_day day = a->focus_day;
  ad_view_move(v, starts, dir, &row, &day);
  a->focus_row = row;
  a->focus_day = day;
  a->kb_visible = true;
  ad_view_ensure_visible(v, day);
}

static void board_hotkeys(App *a) {
  ui_state *u = &a->ui;
  if (u->text_active) {
    return; /* typing in a field: letters are letters */
  }
  if (action_pressed(a, AD_ACT_HABITS)) {
    if (a->popup_open) popup_close(a);
    else popup_open_now(a);
    return;
  }
  if (a->popup_open) {
    if (ui_take_key(u, SDLK_ESCAPE, 0)) {
      if (a->edit != EDIT_NONE || a->full_prompt) {
        a->edit = EDIT_NONE;
        a->full_prompt = false;
      } else popup_close(a);
    }
    return;
  }
  if (ui_take_key(u, SDLK_ESCAPE, 0)) u->focus = 0;
  if (action_pressed(a, AD_ACT_LAYOUT)) {
    a->model.settings.layout = a->model.settings.layout == AD_LAYOUT_HORIZONTAL ? AD_LAYOUT_VERTICAL : AD_LAYOUT_HORIZONTAL;
    commit(a);
    layout_view(a);
    ad_view_center_on(&a->view, a->model.today);
  }
  if (action_pressed(a, AD_ACT_TODAY)) {
    layout_view(a);
    ad_view_center_on(&a->view, a->model.today);
    if (a->cell_focus) a->focus_day = a->model.today;
  }
  if (u->focus != 0) return; /* a button has the keyboard: Enter and Space are for it */
  if (action_pressed(a, AD_ACT_LEFT)) move_focus(a, 0);
  if (action_pressed(a, AD_ACT_RIGHT)) move_focus(a, 1);
  if (action_pressed(a, AD_ACT_UP)) move_focus(a, 2);
  if (action_pressed(a, AD_ACT_DOWN)) move_focus(a, 3);
  if (a->cell_focus && a->kb_visible) {
    int bh[AD_MAX_BOARD];
    const int nb = board_habits(a, bh);
    if (a->focus_row < nb) {
      if (action_pressed(a, AD_ACT_FILL)) click_cell(a, bh[a->focus_row], a->focus_day, true);
      if (action_pressed(a, AD_ACT_SKIP)) click_cell(a, bh[a->focus_row], a->focus_day, false);
    }
  }
}

static void frame(App *a) {
  if (a->screen == SCREEN_BOARD) {
    bool need = false;
    update_clock(a, &need);
    if (need) commit(a);
  }
  gfx_refresh_scale(&a->g);
  gfx_begin_frame(&a->g);
  ui_begin_frame(&a->ui);
  a->modal = a->popup_open;
  if (a->screen == SCREEN_FATAL) {
    fatal_frame(a);
  } else if (a->screen == SCREEN_FIRST) {
    first_frame(a);
  } else {
    board_hotkeys(a);
    board_frame(a);
    corner_buttons_frame(a);
    if (a->popup_open) habit_popup_frame(a);
    banners_frame(a);
  }
  ui_end_frame(&a->ui);
  gfx_end_frame(&a->g);
}

static void queue_key(ui_state *u, const SDL_KeyboardEvent *k) {
  if (u->nkeys < UI_MAX_KEYS) u->keys[u->nkeys++] = (ui_key){k->key, k->mod, k->repeat, false};
}

static void handle_event(App *a, const SDL_Event *e) {
  ui_state *u = &a->ui;
  const float d = a->g.density;
  switch (e->type) {
    case SDL_EVENT_QUIT: a->quit = true; break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE: u->mouse_in = false; break;
    case SDL_EVENT_MOUSE_MOTION:
      u->mx = e->motion.x * d;
      u->my = e->motion.y * d;
      u->mouse_in = true;
      u->mouse_moved = true;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      u->mx = e->button.x * d;
      u->my = e->button.y * d;
      u->mouse_in = true;
      if (e->button.button == SDL_BUTTON_LEFT) u->lpress = u->ldown = true;
      if (e->button.button == SDL_BUTTON_RIGHT) u->rpress = u->rdown = true;
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (e->button.button == SDL_BUTTON_LEFT) {
        u->lrelease = true;
        u->ldown = false;
      }
      if (e->button.button == SDL_BUTTON_RIGHT) {
        u->rrelease = true;
        u->rdown = false;
      }
      break;
    case SDL_EVENT_MOUSE_WHEEL: {
      float dy = e->wheel.y, dx = e->wheel.x;
      if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
        dy = -dy;
        dx = -dx;
      }
      u->wheel_x += dx;
      u->wheel_y += dy;
      break;
    }
    case SDL_EVENT_KEY_DOWN: queue_key(u, &e->key); break;
    case SDL_EVENT_TEXT_INPUT: SDL_strlcat(u->text, e->text.text, sizeof u->text); break;
    default: break;
  }
}

/* ---- entry ------------------------------------------------------------- */

int app_run(int argc, char **argv) {
  App a;
  memset(&a, 0, sizeof a);
  const char *shot = NULL;
  const char *data_dir = SDL_getenv("ADDERALL_DATA_DIR");
  int win_w = 1000, win_h = 600;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) shot = argv[++i];
    else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) sscanf(argv[++i], "%dx%d", &win_w, &win_h);
    else if (strcmp(argv[i], "--data-dir") == 0 && i + 1 < argc) data_dir = argv[++i];
  }
  if (data_dir && !data_dir[0]) data_dir = NULL;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "Adderall: could not start SDL: %s\n", SDL_GetError());
    return 1;
  }
  if (!TTF_Init()) {
    fprintf(stderr, "Adderall: could not start text drawing: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  if (!SDL_CreateWindowAndRenderer("Adderall", win_w, win_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &a.win, &a.ren)) {
    fprintf(stderr, "Adderall: could not open a window: %s\n", SDL_GetError());
    TTF_Quit();
    SDL_Quit();
    return 1;
  }
  SDL_SetWindowMinimumSize(a.win, 480, 320);
  SDL_SetRenderVSync(a.ren, 1);
  gfx_init(&a.g, a.win, a.ren);
  a.ui.g = &a.g;
  if (!resource_path("fonts/Inter-Regular.otf", a.bundled_font, sizeof a.bundled_font) ||
      !gfx_set_font(&a.g, a.bundled_font, NULL, 16)) {
    fprintf(stderr, "Adderall: could not load the bundled font.\n");
    return 1;
  }

  /* open the data file */
  char msg[400];
  ad_parse_error perr;
  const ad_open_status st = ad_store_open(&a.store, data_dir, &a.model, &perr, msg, sizeof msg);
  a.store_open = true;
  {
    char zf[1100];
    if (resource_path("zoneinfo/zone.tab", zf, sizeof zf)) {
      zf[strlen(zf) - strlen("/zone.tab")] = 0;
      SDL_snprintf(a.zoneinfo_dir, sizeof a.zoneinfo_dir, "%s", zf);
      a.nzones = ad_zones_load(a.zoneinfo_dir, &a.zones);
    }
    if (a.nzones == 0) {
      fprintf(stderr, "Adderall: could not find its bundled time zone data.\n");
      return 1;
    }
    apply_timezone(&a);
    ad_model_set_clock(&a.model, clock_day(&a));
  }
  switch (st) {
    case AD_OPEN_OK: break;
    case AD_OPEN_FRESH: a.screen = SCREEN_FIRST; break;
    case AD_OPEN_FROM_BACKUP: SDL_snprintf(a.notice, sizeof a.notice, "%s", msg); break;
    case AD_OPEN_LOCKED: set_fatal(&a, "Adderall is already running", msg, false); break;
    case AD_OPEN_IO_ERROR: set_fatal(&a, "Adderall can't open its data", msg, false); break;
    case AD_OPEN_PARSE_ERROR: {
      char why[300], text[1200];
      ad_parse_error_describe(&perr, why, sizeof why);
      if (msg[0]) SDL_snprintf(text, sizeof text, "%s\n\n%s", msg, why);
      else if (perr.status == AD_PARSE_LINE)
        SDL_snprintf(text, sizeof text, "Adderall can't read line %d of your data file, so it has left the file exactly as it is.\n\n    %s\n\nProblem: %s.\n\nFix that line in a text editor, or move the file away, then start Adderall again.", perr.line, perr.text, perr.message);
      else SDL_snprintf(text, sizeof text, "%s\n\nYour data file has been left exactly as it is.", why);
      set_fatal(&a, "Your data file needs a look", text, ad_plat_file_exists(a.store.backup));
      break;
    }
  }

  layout_view(&a);
  ad_view_center_on(&a.view, a.model.today);

  if (shot) {
    frame(&a);
    frame(&a);
    SDL_Surface *s = SDL_RenderReadPixels(a.ren, NULL);
    if (s) {
      SDL_SaveBMP(s, shot);
      SDL_DestroySurface(s);
    }
  } else {
    while (!a.quit) {
      SDL_Event e;
      if (SDL_WaitEventTimeout(&e, 500)) {
        do handle_event(&a, &e);
        while (SDL_PollEvent(&e));
      }
      frame(&a);
      for (int again = 0; a.redraw && again < 3; again++) {
        a.redraw = false;
        frame(&a);
      }
      if (a.ui.text_active != SDL_TextInputActive(a.win)) {
        if (a.ui.text_active) SDL_StartTextInput(a.win);
        else SDL_StopTextInput(a.win);
      }
    }
  }

  if (a.store_open) ad_store_close(&a.store);
  ad_tz_free(a.tz);
  free(a.zones);
  ad_model_free(&a.model);
  gfx_shutdown(&a.g);
  SDL_DestroyRenderer(a.ren);
  SDL_DestroyWindow(a.win);
  TTF_Quit();
  SDL_Quit();
  return 0;
}
