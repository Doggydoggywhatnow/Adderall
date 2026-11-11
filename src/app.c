/* Adderall: window, event loop and the board. */
#include "app.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "date.h"
#include "gfx.h"
#include "model.h"

typedef struct {
  SDL_Window *win;
  SDL_Renderer *ren;
  gfx g;
  ad_model model;
  ad_view view;
  char bundled_font[1100];
  float mx, my; /* mouse, in pixels */
  bool mouse_in;
  bool quit;
  bool dirty;
} App;

/* ---- resources --------------------------------------------------------- */

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

/* ---- today (replaced by the time zone logic in a later slice) ----------- */

static ad_day system_today(void) {
  SDL_Time now;
  SDL_DateTime dt;
  if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &dt, true)) return ad_day_from_ymd(2026, 1, 1);
  return ad_day_from_ymd(dt.year, dt.month, dt.day);
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

static void draw_tooltip(App *a, const char *line1, const char *line2) {
  gfx *g = &a->g;
  const float pad = 8 * g->scale;
  float w = gfx_text_width(g, line1);
  const float lh = gfx_text_height(g);
  float h = lh + pad * 2;
  if (line2) {
    const float w2 = gfx_text_width(g, line2);
    if (w2 > w) w = w2;
    h += lh;
  }
  int ww, wh;
  SDL_GetWindowSizeInPixels(a->win, &ww, &wh);
  float x = a->mx + 14 * g->scale, y = a->my + 18 * g->scale;
  if (x + w + pad * 2 > ww) x = ww - w - pad * 2 - 2;
  if (y + h > wh) y = a->my - h - 8 * g->scale;
  gfx_fill_rect(g, g->theme.bg, x, y, w + pad * 2, h);
  gfx_stroke_rect(g, g->theme.fg, x, y, w + pad * 2, h, fmaxf(1.0f, roundf(g->scale)));
  gfx_text(g, line1, x + pad, y + pad, g->theme.fg);
  if (line2) gfx_text(g, line2, x + pad, y + pad + lh, g->theme.dim);
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

static void draw_board(App *a) {
  gfx *g = &a->g;
  ad_view *v = &a->view;
  ad_day first, last;
  ad_view_visible_days(v, &first, &last);
  const ad_model *m = &a->model;

  int hover_row = -1;
  ad_day hover_day = 0;
  bool hover_cell = a->mouse_in && ad_view_hit_cell(v, a->mx, a->my, &hover_row, &hover_day);
  if (hover_cell && hover_day < m->habits[hover_row].start) hover_cell = false;

  /* Cells and headers are clipped so they slide under the symbol column. */
  SDL_Rect clip = {(int)(v->x + (v->layout == AD_LAYOUT_HORIZONTAL ? v->pitch : 0)), (int)v->y, (int)(v->w - (v->layout == AD_LAYOUT_HORIZONTAL ? v->pitch : 0)), (int)v->h};
  gfx_clip(g, &clip);
  for (ad_day d = first; d <= last; d++) {
    int y, mo, dd;
    ad_day_to_ymd(d, &y, &mo, &dd);
    ad_rect hr;
    if (v->rows == 0) break;
    if (!ad_view_cell_rect(v, 0, d, &hr)) continue;
    int chunk, col;
    ad_view_chunk_of(v, d, &chunk, &col);
    /* header text: months (horizontal) or the first day of each line (vertical) */
    const float ty = ad_view_chunk_top(v, chunk) + (v->head_h - gfx_text_height(g)) * 0.5f;
    char buf[48];
    if (v->layout == AD_LAYOUT_HORIZONTAL && (dd == 1 || d == v->origin)) {
      if (mo == 1 || d == v->origin) SDL_snprintf(buf, sizeof buf, "%s %d", ad_month_short(mo), y);
      else SDL_snprintf(buf, sizeof buf, "%s", ad_month_short(mo));
      gfx_text(g, buf, hr.x + v->pitch * 0.19f, ty, g->theme.dim);
    } else if (v->layout == AD_LAYOUT_VERTICAL && col == 0) {
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
  if (a->mouse_in && ad_view_hit_label(v, a->mx, a->my, &lrow)) {
    draw_tooltip(a, m->habits[lrow].name, NULL);
  } else if (hover_cell) {
    char buf[64];
    format_long_date(hover_day, buf, sizeof buf);
    draw_tooltip(a, buf, hover_day == m->today ? "Today" : NULL);
  }
}

static void render(App *a) {
  gfx_begin_frame(&a->g);
  layout_view(a);
  draw_board(a);
  gfx_end_frame(&a->g);
}

/* ---- input ------------------------------------------------------------- */

static void click_cell(App *a, int button) {
  int row;
  ad_day day;
  if (!ad_view_hit_cell(&a->view, a->mx, a->my, &row, &day)) return;
  ad_habit *h = &a->model.habits[row];
  if (day < h->start) return;
  const ad_state cur = ad_habit_get(h, day);
  ad_state next;
  if (button == SDL_BUTTON_LEFT) next = (cur == AD_FILLED) ? AD_EMPTY : AD_FILLED;
  else next = (cur == AD_SKIPPED) ? AD_EMPTY : AD_SKIPPED;
  ad_habit_put(h, day, next);
}

static void handle_event(App *a, const SDL_Event *e) {
  const float d = a->g.density;
  switch (e->type) {
    case SDL_EVENT_QUIT: a->quit = true; break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: gfx_refresh_scale(&a->g); break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE: a->mouse_in = false; break;
    case SDL_EVENT_MOUSE_MOTION:
      a->mx = e->motion.x * d;
      a->my = e->motion.y * d;
      a->mouse_in = true;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      a->mx = e->button.x * d;
      a->my = e->button.y * d;
      if (e->button.button == SDL_BUTTON_LEFT || e->button.button == SDL_BUTTON_RIGHT) click_cell(a, e->button.button);
      break;
    case SDL_EVENT_MOUSE_WHEEL: {
      ad_view *v = &a->view;
      const float step = v->pitch * 1.5f;
      float dy = e->wheel.y, dx = e->wheel.x;
      if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
        dy = -dy;
        dx = -dx;
      }
      if (v->layout == AD_LAYOUT_HORIZONTAL) v->scroll += (dx != 0 ? dx : -dy) * step;
      else v->scroll += -dy * step;
      ad_view_update(v);
      break;
    }
    default: break;
  }
}

/* ---- entry ------------------------------------------------------------- */

int app_run(int argc, char **argv) {
  App a;
  memset(&a, 0, sizeof a);
  const char *shot = NULL;
  int win_w = 1000, win_h = 600;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) shot = argv[++i];
    else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) sscanf(argv[++i], "%dx%d", &win_w, &win_h);
  }

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
  if (!resource_path("fonts/Inter-Regular.otf", a.bundled_font, sizeof a.bundled_font) ||
      !gfx_set_font(&a.g, a.bundled_font, NULL, 16)) {
    fprintf(stderr, "Adderall: could not load the bundled font.\n");
    return 1;
  }

  ad_model_init(&a.model);
  a.model.today = system_today();
  ad_model_add_raw(&a.model, "Drink water", AD_SYM_STAR, a.model.today);
  layout_view(&a);
  ad_view_center_on(&a.view, a.model.today);

  if (shot) {
    render(&a);
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
      render(&a);
    }
  }

  ad_model_free(&a.model);
  gfx_shutdown(&a.g);
  SDL_DestroyRenderer(a.ren);
  SDL_DestroyWindow(a.win);
  TTF_Quit();
  SDL_Quit();
  return 0;
}
