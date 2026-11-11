/* Adderall: window, event loop, screens. */
#include "app.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "date.h"
#include "gfx.h"
#include "model.h"
#include "store.h"
#include "tz.h"
#include "ui.h"

typedef enum { SCREEN_BOARD, SCREEN_FATAL } Screen;

enum { ID_FATAL_FOLDER = 1, ID_FATAL_COPY, ID_FATAL_BACKUP, ID_FATAL_QUIT, ID_NOTICE_OK, ID_ERR_RETRY };

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

static void draw_label_cell(App *a, ad_rect r, ad_symbol sym) {
  gfx *g = &a->g;
  const float s = roundf(r.w * 0.62f);
  const float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
  gfx_fill_rect(g, g->theme.fg, roundf(cx - s / 2), roundf(cy - s / 2), s, s);
  const float m = roundf(s * 0.72f);
  gfx_mask(g, GFX_MASK_SYMBOL, (int)sym, 0, cx - m / 2, cy - m / 2, m, g->theme.bg);
}

static void format_long_date(ad_day d, char *out, size_t cap) {
  int y, m, dd;
  ad_day_to_ymd(d, &y, &m, &dd);
  SDL_snprintf(out, cap, "%s %d %s %d", ad_weekday_name(ad_day_weekday(d)), dd, ad_month_name(m), y);
}

static void layout_view(App *a) {
  gfx *g = &a->g;
  int w, h;
  SDL_GetWindowSizeInPixels(a->win, &w, &h);
  const float margin = 28 * g->scale;
  const float bar = 64 * g->scale;
  ad_view *v = &a->view;
  v->x = margin;
  v->y = margin;
  v->w = w - 2 * margin;
  v->h = h - margin - bar;
  v->pitch = roundf(46 * g->scale);
  v->head_h = roundf(gfx_text_height(g) + 10 * g->scale);
  v->rows = a->model.n_habits;
  v->origin = a->model.n_habits ? a->model.habits[0].start : a->model.today;
  for (int i = 1; i < a->model.n_habits; i++)
    if (a->model.habits[i].start < v->origin) v->origin = a->model.habits[i].start;
  v->last = a->model.today + AD_HORIZON_DAYS;
  ad_view_update(v);
}

static void click_cell(App *a, int row, ad_day day, bool left) {
  ad_habit *h = &a->model.habits[row];
  if (day < h->start) return;
  /* Sealed days simply ignore the click (the data layer refuses it anyway). */
  const ad_result r = left ? ad_model_toggle_fill(&a->model, row, day) : ad_model_toggle_skip(&a->model, row, day);
  if (r == AD_OK) commit(a);
}

static void board_frame(App *a) {
  gfx *g = &a->g;
  ui_state *u = &a->ui;
  ad_view *v = &a->view;
  layout_view(a);

  /* scrolling */
  if (u->wheel_x != 0 || u->wheel_y != 0) {
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
  bool hover_cell = u->mouse_in && ad_view_hit_cell(v, u->mx, u->my, &hover_row, &hover_day);
  if (hover_cell && hover_day < m->habits[hover_row].start) hover_cell = false;

  if (hover_cell && !u->consumed_click) {
    if (u->lpress) click_cell(a, hover_row, hover_day, true);
    else if (u->rpress) click_cell(a, hover_row, hover_day, false);
  }

  /* Cells and headers are clipped so they slide under the symbol column. */
  const bool horiz = v->layout == AD_LAYOUT_HORIZONTAL;
  SDL_Rect clip = {(int)(v->x + (horiz ? v->pitch : 0)), (int)v->y, (int)(v->w - (horiz ? v->pitch : 0)), (int)v->h};
  gfx_clip(g, &clip);
  for (ad_day d = first; d <= last && v->rows > 0; d++) {
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
      const ad_habit *h = &m->habits[r];
      if (d < h->start) continue;
      ad_rect rc;
      ad_view_cell_rect(v, r, d, &rc);
      if (rc.y + rc.h < v->y || rc.y > v->y + v->h) continue;
      draw_cell(a, rc, ad_habit_get(h, d), d == m->today, hover_cell && r == hover_row && d == hover_day, false);
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
      if (ad_view_label_rect(v, r, c, &lr)) draw_label_cell(a, lr, m->habits[r].symbol);
    }
    gfx_clip(g, NULL);
  }

  /* tooltips */
  int lrow;
  if (u->mouse_in && ad_view_hit_label(v, u->mx, u->my, &lrow)) {
    ui_tooltip(u, m->habits[lrow].name, NULL, u->mx, u->my);
  } else if (hover_cell) {
    char buf[64];
    format_long_date(hover_day, buf, sizeof buf);
    ui_tooltip(u, buf, hover_day == m->today ? "Today" : NULL, u->mx, u->my);
  }
}

/* ---- banners and the fatal screen -------------------------------------- */

static void banners_frame(App *a) {
  ui_state *u = &a->ui;
  gfx *g = &a->g;
  int w, h;
  SDL_GetWindowSizeInPixels(a->win, &w, &h);
  const float pad = 12 * g->scale;
  const char *text = a->save_error[0] ? a->save_error : (a->notice[0] ? a->notice : NULL);
  if (!text) return;
  const float bw = fminf(w - 2 * pad, 720 * g->scale);
  const float th = ui_wrapped_text(u, text, 0, 0, bw - 2 * pad - 90 * g->scale, g->theme.fg, false);
  const float bh = fmaxf(th, gfx_text_height(g)) + 2 * pad;
  ad_rect r = {(w - bw) * 0.5f, h - bh - 12 * g->scale, bw, bh};
  ui_panel(u, r);
  ui_wrapped_text(u, text, r.x + pad, r.y + pad, bw - 2 * pad - 90 * g->scale, g->theme.fg, true);
  ad_rect b = {r.x + bw - pad - 70 * g->scale, r.y + (bh - 32 * g->scale) * 0.5f, 70 * g->scale, 32 * g->scale};
  if (a->save_error[0]) {
    b.w = 80 * g->scale;
    b.x = r.x + bw - pad - b.w;
    if (ui_button(u, ID_ERR_RETRY, b, "Retry", UI_BTN_NORMAL)) commit(a);
  } else if (ui_button(u, ID_NOTICE_OK, b, "OK", UI_BTN_NORMAL)) {
    a->notice[0] = 0;
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

/* ---- the frame ---------------------------------------------------------- */

static void frame(App *a) {
  if (a->screen == SCREEN_BOARD) {
    bool need = false;
    update_clock(a, &need);
    if (need) commit(a);
  }
  gfx_refresh_scale(&a->g);
  gfx_begin_frame(&a->g);
  ui_begin_frame(&a->ui);
  if (a->screen == SCREEN_FATAL) {
    fatal_frame(a);
  } else {
    banners_frame(a); /* first, so a banner's button wins over a cell behind it */
    board_frame(a);
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
      if (e->button.button == SDL_BUTTON_RIGHT) u->rpress = true;
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (e->button.button == SDL_BUTTON_LEFT) {
        u->lrelease = true;
        u->ldown = false;
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
    case AD_OPEN_FRESH:
      ad_model_add_raw(&a.model, "Habit", AD_SYM_STAR, a.model.today);
      commit(&a);
      break;
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

  a.view.layout = a.model.settings.layout;
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
