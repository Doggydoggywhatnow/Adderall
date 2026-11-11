/* A thin drawing layer over SDL3: theme colours, tinted shape masks, cached
 * text. Everything is measured in real pixels; the caller multiplies by
 * gfx->scale (display density times the user's interface scale). */
#ifndef ADDERALL_GFX_H
#define ADDERALL_GFX_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "raster.h"

typedef struct {
  uint8_t r, g, b, a;
} gfx_color;

typedef struct {
  gfx_color bg;     /* window background */
  gfx_color fg;     /* marks and text */
  gfx_color dim;    /* secondary text and the hairlines of empty chrome */
  gfx_color faint;  /* very light fills (hover, panels) */
} gfx_theme;

typedef enum {
  GFX_MASK_SQUARE,
  GFX_MASK_DOT,
  GFX_MASK_SLASH,
  GFX_MASK_SQUIRCLE,
  GFX_MASK_SQUIRCLE_FILL,
  GFX_MASK_SYMBOL, /* param = ad_symbol */
} gfx_mask_kind;

#define GFX_MASK_CACHE 192
#define GFX_TEXT_CACHE 384

typedef struct {
  gfx_mask_kind kind;
  int size, param, thick;
  SDL_Texture *tex;
} gfx_mask_entry;

typedef struct {
  char *text;
  int px;
  SDL_Texture *tex;
  int w, h;
} gfx_text_entry;

typedef struct {
  SDL_Window *win;
  SDL_Renderer *ren;
  TTF_Font *font;
  float density;        /* window pixel density (HiDPI) */
  float ui_scale;       /* the user's interface scale, 1.0 = 100% */
  float scale;          /* density * ui_scale */
  int text_size;        /* the user's text size, in points at 100% */
  float text_factor;    /* temporary enlargement (the measure tooltip), 1.0 normally */
  bool dark, high_contrast;
  gfx_theme theme;
  gfx_mask_entry masks[GFX_MASK_CACHE];
  int n_masks;
  gfx_text_entry texts[GFX_TEXT_CACHE];
  int n_texts;
  int font_px;
  char font_path[1024];
  char font_error[160]; /* set when the user's font could not be loaded */
} gfx;

bool gfx_init(gfx *g, SDL_Window *win, SDL_Renderer *ren);
void gfx_shutdown(gfx *g);

/* Recompute density/scale from the window; call on resize and on changes. */
void gfx_refresh_scale(gfx *g);
void gfx_set_ui_scale(gfx *g, float s);
void gfx_set_theme(gfx *g, bool dark, bool high_contrast);

/* Load a font file (NULL or "" = the bundled one) at a text size. Falls back
 * to the bundled font and records why in g->font_error. */
bool gfx_set_font(gfx *g, const char *bundled_path, const char *user_path, int text_size);

void gfx_clear_caches(gfx *g);

/* Draw text bigger (or back at 1.0) until changed again. */
void gfx_text_scale(gfx *g, float factor);

void gfx_begin_frame(gfx *g);
void gfx_end_frame(gfx *g);

void gfx_set_color(gfx *g, gfx_color c);
void gfx_fill_rect(gfx *g, gfx_color c, float x, float y, float w, float h);
void gfx_stroke_rect(gfx *g, gfx_color c, float x, float y, float w, float h, float thick);
void gfx_line(gfx *g, gfx_color c, float x0, float y0, float x1, float y1);
void gfx_clip(gfx *g, const SDL_Rect *r); /* NULL clears */

/* Draw a cached, tinted shape mask into the box at x,y of the given size. */
void gfx_mask(gfx *g, gfx_mask_kind kind, int param, int thick, float x, float y, float size, gfx_color c);

/* Text. Returns the width drawn. */
float gfx_text(gfx *g, const char *s, float x, float y, gfx_color c);
float gfx_text_width(gfx *g, const char *s);
float gfx_text_height(gfx *g);
/* Draw `s`, cut with an ellipsis if wider than max_w. */
float gfx_text_fit(gfx *g, const char *s, float x, float y, float max_w, gfx_color c);
/* Centered in a box. */
void gfx_text_center(gfx *g, const char *s, float x, float y, float w, float h, gfx_color c);

gfx_color gfx_mix(gfx_color a, gfx_color b, float t); /* t = 0 -> a, 1 -> b */
gfx_color gfx_alpha(gfx_color c, int a);

#endif
