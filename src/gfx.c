#include "gfx.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

gfx_color gfx_mix(gfx_color a, gfx_color b, float t) {
  gfx_color o;
  o.r = (uint8_t)lroundf(a.r + (b.r - a.r) * t);
  o.g = (uint8_t)lroundf(a.g + (b.g - a.g) * t);
  o.b = (uint8_t)lroundf(a.b + (b.b - a.b) * t);
  o.a = 255;
  return o;
}

gfx_color gfx_alpha(gfx_color c, int a) {
  c.a = (uint8_t)(a < 0 ? 0 : (a > 255 ? 255 : a));
  return c;
}

void gfx_set_theme(gfx *g, bool dark, bool high_contrast) {
  g->dark = dark;
  g->high_contrast = high_contrast;
  gfx_theme t;
  if (high_contrast) {
    const uint8_t bg = dark ? 0 : 255, fg = dark ? 255 : 0;
    t.bg = (gfx_color){bg, bg, bg, 255};
    t.fg = (gfx_color){fg, fg, fg, 255};
    t.dim = t.fg;
    t.faint = gfx_mix(t.bg, t.fg, 0.18f);
  } else {
    t.bg = dark ? (gfx_color){14, 14, 14, 255} : (gfx_color){255, 255, 255, 255};
    t.fg = dark ? (gfx_color){240, 240, 240, 255} : (gfx_color){17, 17, 17, 255};
    t.dim = gfx_mix(t.fg, t.bg, 0.45f);
    t.faint = gfx_mix(t.bg, t.fg, 0.08f);
  }
  g->theme = t;
}

bool gfx_init(gfx *g, SDL_Window *win, SDL_Renderer *ren) {
  memset(g, 0, sizeof *g);
  g->win = win;
  g->ren = ren;
  g->ui_scale = 1.0f;
  g->text_size = 16;
  g->text_factor = 1.0f;
  gfx_set_theme(g, false, false);
  gfx_refresh_scale(g);
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  return true;
}

void gfx_clear_caches(gfx *g) {
  for (int i = 0; i < g->n_masks; i++) SDL_DestroyTexture(g->masks[i].tex);
  g->n_masks = 0;
  for (int i = 0; i < g->n_texts; i++) {
    SDL_DestroyTexture(g->texts[i].tex);
    free(g->texts[i].text);
  }
  g->n_texts = 0;
}

void gfx_shutdown(gfx *g) {
  gfx_clear_caches(g);
  if (g->font) TTF_CloseFont(g->font);
  g->font = NULL;
}

static void apply_font_size(gfx *g) {
  if (!g->font) return;
  int px = (int)lroundf(g->text_size * g->scale * (g->text_factor > 0 ? g->text_factor : 1.0f));
  if (px < 6) px = 6;
  if (px != g->font_px) {
    TTF_SetFontSize(g->font, (float)px);
    g->font_px = px;
  }
}

void gfx_refresh_scale(gfx *g) {
  float d = SDL_GetWindowPixelDensity(g->win);
  if (d <= 0) d = 1.0f;
  g->density = d;
  const float s = d * g->ui_scale;
  if (fabsf(s - g->scale) > 0.001f) {
    g->scale = s;
    gfx_clear_caches(g);
    apply_font_size(g);
  }
}

void gfx_set_ui_scale(gfx *g, float s) {
  if (s < 0.5f) s = 0.5f;
  if (s > 3.0f) s = 3.0f;
  g->ui_scale = s;
  gfx_refresh_scale(g);
}

bool gfx_set_font(gfx *g, const char *bundled_path, const char *user_path, int text_size) {
  if (text_size < 8) text_size = 8;
  if (text_size > 48) text_size = 48;
  g->text_size = text_size;
  g->font_error[0] = 0;
  const char *want = (user_path && user_path[0]) ? user_path : bundled_path;
  if (g->font && strcmp(want, g->font_path) == 0) {
    apply_font_size(g);
    gfx_clear_caches(g);
    return true;
  }
  float px = (float)lround(text_size * g->scale);
  TTF_Font *f = TTF_OpenFont(want, px);
  bool ok = f != NULL;
  if (!f) {
    if (want != bundled_path) {
      SDL_snprintf(g->font_error, sizeof g->font_error, "Could not use that font file, so the built-in font is shown.");
    }
    want = bundled_path;
    f = TTF_OpenFont(want, px);
    if (!f) return false;
  }
  if (g->font) TTF_CloseFont(g->font);
  g->font = f;
  SDL_strlcpy(g->font_path, want, sizeof g->font_path);
  g->font_px = (int)px;
  TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
  gfx_clear_caches(g);
  return ok || g->font_error[0];
}

void gfx_text_scale(gfx *g, float factor) {
  g->text_factor = factor;
  apply_font_size(g);
}

void gfx_begin_frame(gfx *g) {
  gfx_set_color(g, g->theme.bg);
  SDL_RenderClear(g->ren);
}

void gfx_end_frame(gfx *g) { SDL_RenderPresent(g->ren); }

SDL_Texture *gfx_alpha_texture(gfx *g, int w, int h, const uint8_t *alpha) {
  uint8_t *px = (uint8_t *)malloc((size_t)w * h * 4);
  if (!px) return NULL;
  for (int i = 0; i < w * h; i++) {
    px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = 255;
    px[i * 4 + 3] = alpha[i];
  }
  SDL_Texture *tex = SDL_CreateTexture(g->ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
  if (tex) {
    SDL_UpdateTexture(tex, NULL, px, w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
  }
  free(px);
  return tex;
}

void gfx_set_color(gfx *g, gfx_color c) { SDL_SetRenderDrawColor(g->ren, c.r, c.g, c.b, c.a); }

void gfx_fill_rect(gfx *g, gfx_color c, float x, float y, float w, float h) {
  gfx_set_color(g, c);
  SDL_FRect r = {x, y, w, h};
  SDL_RenderFillRect(g->ren, &r);
}

void gfx_stroke_rect(gfx *g, gfx_color c, float x, float y, float w, float h, float t) {
  gfx_fill_rect(g, c, x, y, w, t);
  gfx_fill_rect(g, c, x, y + h - t, w, t);
  gfx_fill_rect(g, c, x, y + t, t, h - 2 * t);
  gfx_fill_rect(g, c, x + w - t, y + t, t, h - 2 * t);
}

void gfx_line(gfx *g, gfx_color c, float x0, float y0, float x1, float y1) {
  gfx_set_color(g, c);
  SDL_RenderLine(g->ren, x0, y0, x1, y1);
}

void gfx_clip(gfx *g, const SDL_Rect *r) { SDL_SetRenderClipRect(g->ren, r); }

/* ---- masks ------------------------------------------------------------- */

static SDL_Texture *make_mask_texture(gfx *g, gfx_mask_kind kind, int size, int param, int thick) {
  ad_mask m;
  const float t = thick / 4.0f; /* thickness is carried in quarter-pixels */
  switch (kind) {
    case GFX_MASK_SQUARE: m = ad_raster_square(size, t); break;
    case GFX_MASK_DOT: m = ad_raster_dot(size, size * (param / 100.0f)); break;
    case GFX_MASK_SLASH: m = ad_raster_slash(size, t); break;
    case GFX_MASK_SQUIRCLE: m = ad_raster_squircle(size, t); break;
    case GFX_MASK_SQUIRCLE_FILL: m = ad_raster_squircle_fill(size); break;
    case GFX_MASK_SYMBOL: m = ad_raster_symbol((ad_symbol)param, size); break;
    default: return NULL;
  }
  if (!m.a) return NULL;
  uint32_t *px = (uint32_t *)malloc((size_t)size * size * 4);
  if (!px) {
    ad_mask_free(&m);
    return NULL;
  }
  for (int i = 0; i < size * size; i++) {
    /* RGBA32: bytes R,G,B,A in memory order, whatever the machine's endianness. */
    uint8_t *b = (uint8_t *)&px[i];
    b[0] = b[1] = b[2] = 255;
    b[3] = m.a[i];
  }
  SDL_Texture *tex = SDL_CreateTexture(g->ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, size, size);
  if (tex) {
    SDL_UpdateTexture(tex, NULL, px, size * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
  }
  free(px);
  ad_mask_free(&m);
  return tex;
}

void gfx_mask(gfx *g, gfx_mask_kind kind, int param, int thick, float x, float y, float size, gfx_color c) {
  const int sz = (int)lroundf(size);
  if (sz < 2) return;
  SDL_Texture *tex = NULL;
  for (int i = 0; i < g->n_masks; i++) {
    gfx_mask_entry *e = &g->masks[i];
    if (e->kind == kind && e->size == sz && e->param == param && e->thick == thick) {
      tex = e->tex;
      break;
    }
  }
  if (!tex) {
    if (g->n_masks == GFX_MASK_CACHE) {
      for (int i = 0; i < g->n_masks; i++) SDL_DestroyTexture(g->masks[i].tex);
      g->n_masks = 0;
    }
    tex = make_mask_texture(g, kind, sz, param, thick);
    if (!tex) return;
    g->masks[g->n_masks++] = (gfx_mask_entry){kind, sz, param, thick, tex};
  }
  SDL_SetTextureColorMod(tex, c.r, c.g, c.b);
  SDL_SetTextureAlphaMod(tex, c.a);
  SDL_FRect dst = {roundf(x), roundf(y), (float)sz, (float)sz};
  SDL_RenderTexture(g->ren, tex, NULL, &dst);
}

/* ---- text -------------------------------------------------------------- */

static gfx_text_entry *text_entry(gfx *g, const char *s) {
  if (!g->font || !s || !s[0]) return NULL;
  for (int i = 0; i < g->n_texts; i++) {
    if (g->texts[i].px == g->font_px && strcmp(g->texts[i].text, s) == 0) return &g->texts[i];
  }
  if (g->n_texts == GFX_TEXT_CACHE) {
    for (int i = 0; i < g->n_texts; i++) {
      SDL_DestroyTexture(g->texts[i].tex);
      free(g->texts[i].text);
    }
    g->n_texts = 0;
  }
  const SDL_Color white = {255, 255, 255, 255};
  SDL_Surface *surf = TTF_RenderText_Blended(g->font, s, 0, white);
  if (!surf) return NULL;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(g->ren, surf);
  gfx_text_entry e;
  e.w = surf->w;
  e.h = surf->h;
  SDL_DestroySurface(surf);
  if (!tex) return NULL;
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  e.tex = tex;
  e.px = g->font_px;
  e.text = SDL_strdup(s);
  g->texts[g->n_texts] = e;
  return &g->texts[g->n_texts++];
}

float gfx_text_width(gfx *g, const char *s) {
  gfx_text_entry *e = text_entry(g, s);
  return e ? (float)e->w : 0;
}

float gfx_text_height(gfx *g) { return g->font ? (float)TTF_GetFontHeight(g->font) : 0; }

float gfx_text(gfx *g, const char *s, float x, float y, gfx_color c) {
  gfx_text_entry *e = text_entry(g, s);
  if (!e) return 0;
  SDL_SetTextureColorMod(e->tex, c.r, c.g, c.b);
  SDL_SetTextureAlphaMod(e->tex, c.a);
  SDL_FRect dst = {roundf(x), roundf(y), (float)e->w, (float)e->h};
  SDL_RenderTexture(g->ren, e->tex, NULL, &dst);
  return (float)e->w;
}

float gfx_text_fit(gfx *g, const char *s, float x, float y, float max_w, gfx_color c) {
  if (gfx_text_width(g, s) <= max_w) return gfx_text(g, s, x, y, c);
  char buf[256];
  size_t n = strlen(s);
  if (n >= sizeof buf - 4) n = sizeof buf - 4;
  while (n > 0) {
    n--;
    while (n > 0 && (s[n] & 0xC0) == 0x80) n--; /* do not cut a UTF-8 sequence */
    memcpy(buf, s, n);
    memcpy(buf + n, "...", 4);
    if (gfx_text_width(g, buf) <= max_w) break;
  }
  return gfx_text(g, buf, x, y, c);
}

void gfx_text_center(gfx *g, const char *s, float x, float y, float w, float h, gfx_color c) {
  const float tw = gfx_text_width(g, s), th = gfx_text_height(g);
  gfx_text(g, s, x + (w - tw) * 0.5f, y + (h - th) * 0.5f, c);
}
