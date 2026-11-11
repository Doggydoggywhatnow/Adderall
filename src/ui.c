#include "ui.h"

#include <math.h>
#include <string.h>

void ui_begin_frame(ui_state *u) {
  u->n_cur = 0;
  u->want_text = false;
  u->want_visible = false;
  u->use_clip = false;
  u->consumed_click = false;
  u->line = fmaxf(1.0f, roundf(u->g->scale * (u->g->high_contrast ? 2.0f : 1.0f)));

  /* Tab / Shift+Tab move through the widgets drawn last frame. */
  for (int i = 0; i < u->nkeys; i++) {
    ui_key *k = &u->keys[i];
    if (k->used || k->key != SDLK_TAB) continue;
    k->used = true;
    if (u->n_prev == 0) continue;
    int idx = -1;
    for (int j = 0; j < u->n_prev; j++)
      if (u->prev_ids[j] == u->focus) idx = j;
    const bool back = (k->mod & SDL_KMOD_SHIFT) != 0;
    if (idx < 0) idx = back ? 0 : -1;
    idx = back ? (idx - 1 + u->n_prev) % u->n_prev : (idx + 1) % u->n_prev;
    u->focus = u->prev_ids[idx];
    u->focus_visible = true;
    u->cursor = 1 << 20; /* end of whatever text field gets it */
  }
  if (u->mouse_moved || u->lpress || u->rpress) u->focus_visible = false;
}

void ui_end_frame(ui_state *u) {
  u->text_active = u->want_text;
  memcpy(u->prev_ids, u->cur_ids, sizeof(int) * (size_t)u->n_cur);
  u->n_prev = u->n_cur;
  u->nkeys = 0;
  u->text[0] = 0;
  u->lpress = u->rpress = u->lrelease = false;
  u->wheel_x = u->wheel_y = 0;
  u->mouse_moved = false;
}

bool ui_take_key(ui_state *u, SDL_Keycode key, SDL_Keymod required_mods) {
  for (int i = 0; i < u->nkeys; i++) {
    ui_key *k = &u->keys[i];
    if (k->used || k->key != key) continue;
    if (required_mods && (k->mod & required_mods) == 0) continue;
    if (!required_mods && (k->mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI))) continue;
    k->used = true;
    return true;
  }
  return false;
}

bool ui_any_key(const ui_state *u) {
  for (int i = 0; i < u->nkeys; i++)
    if (!u->keys[i].used) return true;
  return false;
}

bool ui_focusable(ui_state *u, int id) {
  if (u->n_cur < UI_MAX_IDS) u->cur_ids[u->n_cur++] = id;
  return u->focus == id;
}

bool ui_hover(const ui_state *u, ad_rect r) {
  if (!u->mouse_in || !ad_rect_has(r, u->mx, u->my)) return false;
  return !u->use_clip || ad_rect_has(u->clip, u->mx, u->my);
}

static void note_focus(ui_state *u, ad_rect r) {
  if (u->use_clip && u->focus_visible) {
    u->want_visible = true;
    u->want_y0 = r.y;
    u->want_y1 = r.y + r.h;
  }
}

float ui_button_width(ui_state *u, const char *label) { return gfx_text_width(u->g, label) + 28 * u->g->scale; }

static bool activated(ui_state *u, int id, bool focused) {
  if (!focused) return false;
  for (int i = 0; i < u->nkeys; i++) {
    ui_key *k = &u->keys[i];
    if (k->used || k->repeat) continue;
    if ((k->key == SDLK_RETURN || k->key == SDLK_KP_ENTER || k->key == SDLK_SPACE) && !(k->mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI))) {
      k->used = true;
      return true;
    }
  }
  (void)id;
  return false;
}

static void focus_ring(ui_state *u, ad_rect r) {
  const float o = u->line * 2;
  gfx_stroke_rect(u->g, u->g->theme.fg, r.x - o - u->line, r.y - o - u->line, r.w + 2 * (o + u->line), r.h + 2 * (o + u->line), u->line);
}

bool ui_button(ui_state *u, int id, ad_rect r, const char *label, int flags) {
  gfx *g = u->g;
  const bool disabled = (flags & UI_BTN_DISABLED) != 0;
  const bool focused = !disabled && ui_focusable(u, id);
  const bool hover = !disabled && ui_hover(u, r);
  bool clicked = false;
  if (!disabled) {
    if (hover && u->lpress && !u->consumed_click) {
      clicked = true;
      u->consumed_click = true;
      u->focus = id;
    }
    if (activated(u, id, focused)) clicked = true;
  }
  const bool solid = (flags & (UI_BTN_PRIMARY | UI_BTN_SELECTED)) != 0 && !disabled;
  if (solid) {
    gfx_fill_rect(g, g->theme.fg, r.x, r.y, r.w, r.h);
  } else if (hover) {
    gfx_fill_rect(g, g->theme.faint, r.x, r.y, r.w, r.h);
  }
  gfx_stroke_rect(g, disabled ? g->theme.dim : g->theme.fg, r.x, r.y, r.w, r.h, u->line);
  gfx_color tc = disabled ? g->theme.dim : (solid ? g->theme.bg : g->theme.fg);
  const float tw = gfx_text_width(g, label);
  if (tw > r.w - 8 * g->scale) gfx_text_fit(g, label, r.x + 4 * g->scale, r.y + (r.h - gfx_text_height(g)) * 0.5f, r.w - 8 * g->scale, tc);
  else gfx_text_center(g, label, r.x, r.y, r.w, r.h, tc);
  if (focused && u->focus_visible) {
    focus_ring(u, r);
    note_focus(u, r);
  }
  return clicked;
}

bool ui_icon_button(ui_state *u, int id, ad_rect r, gfx_mask_kind kind, int param, int flags) {
  gfx *g = u->g;
  const bool disabled = (flags & UI_BTN_DISABLED) != 0;
  const bool focused = !disabled && ui_focusable(u, id);
  const bool hover = !disabled && ui_hover(u, r);
  bool clicked = false;
  if (!disabled) {
    if (hover && u->lpress && !u->consumed_click) {
      clicked = true;
      u->consumed_click = true;
      u->focus = id;
    }
    if (activated(u, id, focused)) clicked = true;
  }
  const bool solid = (flags & UI_BTN_SELECTED) != 0 && !disabled;
  if (solid) gfx_fill_rect(g, g->theme.fg, r.x, r.y, r.w, r.h);
  else if (hover) gfx_fill_rect(g, g->theme.faint, r.x, r.y, r.w, r.h);
  gfx_stroke_rect(g, g->theme.fg, r.x, r.y, r.w, r.h, u->line);
  const float s = roundf(fminf(r.w, r.h) * 0.62f);
  gfx_mask(g, kind, param, 0, r.x + (r.w - s) * 0.5f, r.y + (r.h - s) * 0.5f, s, solid ? g->theme.bg : g->theme.fg);
  if (focused && u->focus_visible) {
    focus_ring(u, r);
    note_focus(u, r);
  }
  return clicked;
}

void ui_panel(ui_state *u, ad_rect r) {
  gfx *g = u->g;
  gfx_fill_rect(g, g->theme.bg, r.x, r.y, r.w, r.h);
  gfx_stroke_rect(g, g->theme.fg, r.x, r.y, r.w, r.h, u->line);
}

void ui_tooltip(ui_state *u, const char *line1, const char *line2, float x, float y) {
  gfx *g = u->g;
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
  SDL_GetWindowSizeInPixels(g->win, &ww, &wh);
  float tx = x + 14 * g->scale, ty = y + 18 * g->scale;
  if (tx + w + pad * 2 > ww) tx = ww - w - pad * 2 - 2;
  if (ty + h > wh) ty = y - h - 8 * g->scale;
  if (tx < 2) tx = 2;
  if (ty < 2) ty = 2;
  gfx_fill_rect(g, g->theme.bg, tx, ty, w + pad * 2, h);
  gfx_stroke_rect(g, g->theme.fg, tx, ty, w + pad * 2, h, u->line);
  gfx_text(g, line1, tx + pad, ty + pad, g->theme.fg);
  if (line2) gfx_text(g, line2, tx + pad, ty + pad + lh, g->theme.dim);
}

float ui_wrapped_text(ui_state *u, const char *s, float x, float y, float w, gfx_color c, bool draw) {
  gfx *g = u->g;
  const float lh = gfx_text_height(g);
  float cy = y;
  char line[512];
  const char *p = s;
  while (*p) {
    /* one paragraph (up to a newline) at a time, wrapped on spaces */
    const char *nl = strchr(p, '\n');
    size_t plen = nl ? (size_t)(nl - p) : strlen(p);
    size_t start = 0;
    if (plen == 0) cy += lh;
    while (start < plen) {
      size_t end = plen, best = 0;
      /* find the longest run of words that fits */
      size_t i = start;
      while (i <= plen) {
        if (i == plen || p[i] == ' ') {
          size_t n = i - start;
          if (n >= sizeof line) n = sizeof line - 1;
          memcpy(line, p + start, n);
          line[n] = 0;
          if (gfx_text_width(g, line) <= w || best == 0) best = i;
          else break;
        }
        i++;
      }
      end = best ? best : plen;
      size_t n = end - start;
      if (n >= sizeof line) n = sizeof line - 1;
      memcpy(line, p + start, n);
      line[n] = 0;
      if (draw) gfx_text(g, line, x, cy, c);
      cy += lh;
      start = end;
      while (start < plen && p[start] == ' ') start++;
    }
    if (!nl) break;
    p = nl + 1;
  }
  return cy - y;
}

/* ---- text field -------------------------------------------------------- */

static size_t utf8_prev(const char *s, size_t i) {
  if (i == 0) return 0;
  i--;
  while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
  return i;
}

static size_t utf8_next(const char *s, size_t i, size_t len) {
  if (i >= len) return len;
  i++;
  while (i < len && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
  return i;
}

static int utf8_count(const char *s) {
  int n = 0;
  for (; *s; s++)
    if (((unsigned char)*s & 0xC0) != 0x80) n++;
  return n;
}

int ui_text_field(ui_state *u, int id, ad_rect r, char *buf, size_t cap, int max_chars, const char *placeholder) {
  gfx *g = u->g;
  const bool focused = ui_focusable(u, id);
  const bool hover = ui_hover(u, r);
  if (hover && u->lpress && !u->consumed_click) {
    u->consumed_click = true;
    u->focus = id;
    u->cursor = (int)strlen(buf);
  }
  int result = 0;
  size_t len = strlen(buf);
  if (focused || u->focus == id) {
    u->want_text = true;
    size_t cur = (size_t)u->cursor;
    if (cur > len) cur = len;
    /* typed text */
    if (u->text[0]) {
      const size_t tl = strlen(u->text);
      /* a field holds one line: drop line breaks from pasted or typed text */
      char clean[256];
      size_t cn = 0;
      for (size_t i = 0; i < tl && cn < sizeof clean - 1; i++)
        if ((unsigned char)u->text[i] >= 0x20) clean[cn++] = u->text[i];
      clean[cn] = 0;
      if (cn && len + cn < cap && utf8_count(buf) + utf8_count(clean) <= max_chars) {
        memmove(buf + cur + cn, buf + cur, len - cur + 1);
        memcpy(buf + cur, clean, cn);
        cur += cn;
        len += cn;
      }
    }
    for (int i = 0; i < u->nkeys; i++) {
      ui_key *k = &u->keys[i];
      if (k->used) continue;
      const bool ctrl = (k->mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
      switch (k->key) {
        case SDLK_BACKSPACE:
          if (cur > 0) {
            const size_t p = utf8_prev(buf, cur);
            memmove(buf + p, buf + cur, len - cur + 1);
            len -= cur - p;
            cur = p;
          }
          k->used = true;
          break;
        case SDLK_DELETE:
          if (cur < len) {
            const size_t n = utf8_next(buf, cur, len);
            memmove(buf + cur, buf + n, len - n + 1);
            len -= n - cur;
          }
          k->used = true;
          break;
        case SDLK_LEFT: cur = utf8_prev(buf, cur); k->used = true; break;
        case SDLK_RIGHT: cur = utf8_next(buf, cur, len); k->used = true; break;
        case SDLK_HOME: cur = 0; k->used = true; break;
        case SDLK_END: cur = len; k->used = true; break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: result = 1; k->used = true; break;
        case SDLK_ESCAPE: result = 2; k->used = true; break;
        case SDLK_V:
          if (ctrl && SDL_HasClipboardText()) {
            char *clip = SDL_GetClipboardText();
            if (clip) {
              char clean[256];
              size_t cn = 0;
              for (const char *c = clip; *c && cn < sizeof clean - 1; c++)
                if ((unsigned char)*c >= 0x20) clean[cn++] = *c;
              clean[cn] = 0;
              if (cn && len + cn < cap && utf8_count(buf) + utf8_count(clean) <= max_chars) {
                memmove(buf + cur + cn, buf + cur, len - cur + 1);
                memcpy(buf + cur, clean, cn);
                cur += cn;
                len += cn;
              }
              SDL_free(clip);
            }
            k->used = true;
          }
          break;
        case SDLK_A:
          if (ctrl) {
            buf[0] = 0;
            len = cur = 0;
            k->used = true;
          }
          break;
        default: break;
      }
    }
    u->cursor = (int)cur;
  }

  gfx_fill_rect(g, g->theme.bg, r.x, r.y, r.w, r.h);
  gfx_stroke_rect(g, g->theme.fg, r.x, r.y, r.w, r.h, focused ? u->line * 2 : u->line);
  const float pad = 8 * g->scale;
  const float ty = r.y + (r.h - gfx_text_height(g)) * 0.5f;
  SDL_Rect clip = {(int)(r.x + u->line * 2), (int)r.y, (int)(r.w - u->line * 4), (int)r.h};
  gfx_clip(g, &clip);
  if (buf[0]) {
    /* scroll horizontally so the caret stays visible */
    char head[512];
    size_t cur = (size_t)u->cursor > len ? len : (size_t)u->cursor;
    size_t n = cur < sizeof head - 1 ? cur : sizeof head - 1;
    memcpy(head, buf, n);
    head[n] = 0;
    const float cw = gfx_text_width(g, head);
    float off = 0;
    if (cw > r.w - pad * 2 - 2) off = cw - (r.w - pad * 2 - 2);
    gfx_text(g, buf, r.x + pad - off, ty, g->theme.fg);
    if (focused) gfx_fill_rect(g, g->theme.fg, r.x + pad - off + cw, ty, fmaxf(1.0f, roundf(g->scale)), gfx_text_height(g));
  } else {
    if (placeholder) gfx_text(g, placeholder, r.x + pad, ty, g->theme.dim);
    if (focused) gfx_fill_rect(g, g->theme.fg, r.x + pad, ty, fmaxf(1.0f, roundf(g->scale)), gfx_text_height(g));
  }
  gfx_clip(g, NULL);
  return result;
}
