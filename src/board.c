#include "board.h"

#include <math.h>

static float row_block_h(const ad_view *v) { return v->rows * v->pitch; }

void ad_view_update(ad_view *v) {
  if (v->pitch < 1) v->pitch = 1;
  if (v->last < v->origin) v->last = v->origin;
  const int days = v->last - v->origin + 1;
  if (v->layout == AD_LAYOUT_VERTICAL) {
    int n = (int)floorf((v->w - v->pitch) / v->pitch);
    if (n < 1) n = 1;
    v->per_line = n;
    v->chunks = (days + n - 1) / n;
    v->chunk_h = v->head_h + row_block_h(v) + v->pitch * 0.6f;
  } else {
    v->per_line = days;
    v->chunks = 1;
    v->chunk_h = v->head_h + row_block_h(v);
  }
  const float mx = ad_view_max_scroll(v);
  if (v->scroll > mx) v->scroll = mx;
  if (v->scroll < 0) v->scroll = 0;
}

float ad_view_max_scroll(const ad_view *v) {
  float m;
  if (v->layout == AD_LAYOUT_VERTICAL) {
    m = v->chunks * v->chunk_h - v->h;
  } else {
    m = (float)(v->last - v->origin + 1) * v->pitch - (v->w - v->pitch);
  }
  return m > 0 ? m : 0;
}

void ad_view_chunk_of(const ad_view *v, ad_day day, int *chunk, int *col) {
  const int idx = day - v->origin;
  if (v->layout == AD_LAYOUT_VERTICAL) {
    *chunk = idx / v->per_line;
    *col = idx % v->per_line;
  } else {
    *chunk = 0;
    *col = idx;
  }
}

float ad_view_chunk_top(const ad_view *v, int chunk) {
  if (v->layout == AD_LAYOUT_VERTICAL) return v->y + chunk * v->chunk_h - v->scroll;
  return v->y;
}

bool ad_view_cell_rect(const ad_view *v, int row, ad_day day, ad_rect *out) {
  if (row < 0 || row >= v->rows || day < v->origin || day > v->last) return false;
  int chunk, col;
  ad_view_chunk_of(v, day, &chunk, &col);
  ad_rect r;
  r.w = r.h = v->pitch;
  r.y = ad_view_chunk_top(v, chunk) + v->head_h + row * v->pitch;
  if (v->layout == AD_LAYOUT_VERTICAL) r.x = v->x + v->pitch + col * v->pitch;
  else r.x = v->x + v->pitch + col * v->pitch - v->scroll;
  *out = r;
  return true;
}

bool ad_view_label_rect(const ad_view *v, int row, int chunk, ad_rect *out) {
  if (row < 0 || row >= v->rows || chunk < 0 || chunk >= v->chunks) return false;
  ad_rect r;
  r.x = v->x;
  r.w = r.h = v->pitch;
  r.y = ad_view_chunk_top(v, chunk) + v->head_h + row * v->pitch;
  *out = r;
  return true;
}

bool ad_view_hit_cell(const ad_view *v, float px, float py, int *row, ad_day *day) {
  if (v->rows <= 0 || px < v->x + v->pitch || px >= v->x + v->w || py < v->y || py >= v->y + v->h) return false;
  const float cx = px - (v->x + v->pitch);
  int chunk = 0, col;
  float cy;
  if (v->layout == AD_LAYOUT_VERTICAL) {
    const float ry = py - v->y + v->scroll;
    chunk = (int)floorf(ry / v->chunk_h);
    cy = ry - chunk * v->chunk_h - v->head_h;
    col = (int)floorf(cx / v->pitch);
    if (col >= v->per_line) return false;
  } else {
    col = (int)floorf((cx + v->scroll) / v->pitch);
    cy = py - v->y - v->head_h;
  }
  if (chunk < 0 || chunk >= v->chunks || cy < 0 || col < 0) return false;
  const int r = (int)floorf(cy / v->pitch);
  if (r < 0 || r >= v->rows) return false;
  const ad_day d = v->origin + (v->layout == AD_LAYOUT_VERTICAL ? chunk * v->per_line + col : col);
  if (d < v->origin || d > v->last) return false;
  *row = r;
  *day = d;
  return true;
}

bool ad_view_hit_label(const ad_view *v, float px, float py, int *row) {
  if (v->rows <= 0 || px < v->x || px >= v->x + v->pitch || py < v->y || py >= v->y + v->h) return false;
  float cy;
  if (v->layout == AD_LAYOUT_VERTICAL) {
    const float ry = py - v->y + v->scroll;
    const int chunk = (int)floorf(ry / v->chunk_h);
    if (chunk < 0 || chunk >= v->chunks) return false;
    cy = ry - chunk * v->chunk_h - v->head_h;
  } else {
    cy = py - v->y - v->head_h;
  }
  if (cy < 0) return false;
  const int r = (int)floorf(cy / v->pitch);
  if (r < 0 || r >= v->rows) return false;
  *row = r;
  return true;
}

void ad_view_visible_days(const ad_view *v, ad_day *first, ad_day *last) {
  if (v->layout == AD_LAYOUT_VERTICAL) {
    int c0 = (int)floorf(v->scroll / v->chunk_h);
    int c1 = (int)floorf((v->scroll + v->h) / v->chunk_h);
    if (c0 < 0) c0 = 0;
    if (c1 >= v->chunks) c1 = v->chunks - 1;
    *first = v->origin + c0 * v->per_line;
    *last = v->origin + (c1 + 1) * v->per_line - 1;
  } else {
    const int c0 = (int)floorf(v->scroll / v->pitch);
    const int c1 = (int)ceilf((v->scroll + v->w - v->pitch) / v->pitch);
    *first = v->origin + c0;
    *last = v->origin + c1;
  }
  if (*first < v->origin) *first = v->origin;
  if (*last > v->last) *last = v->last;
}

void ad_view_ensure_visible(ad_view *v, ad_day day) {
  if (day < v->origin) day = v->origin;
  if (day > v->last) day = v->last;
  int chunk, col;
  ad_view_chunk_of(v, day, &chunk, &col);
  if (v->layout == AD_LAYOUT_VERTICAL) {
    const float top = chunk * v->chunk_h;
    const float bottom = top + v->chunk_h;
    if (top < v->scroll) v->scroll = top;
    else if (bottom > v->scroll + v->h) v->scroll = bottom - v->h;
  } else {
    const float left = col * v->pitch;
    const float right = left + v->pitch;
    const float view = v->w - v->pitch;
    if (left < v->scroll + v->pitch * 0.5f) v->scroll = left - v->pitch * 0.5f;
    else if (right > v->scroll + view - v->pitch * 0.5f) v->scroll = right - view + v->pitch * 0.5f;
  }
  ad_view_update(v);
}

void ad_view_center_on(ad_view *v, ad_day day) {
  if (day < v->origin) day = v->origin;
  if (day > v->last) day = v->last;
  int chunk, col;
  ad_view_chunk_of(v, day, &chunk, &col);
  if (v->layout == AD_LAYOUT_VERTICAL) {
    v->scroll = chunk * v->chunk_h + v->chunk_h * 0.5f - v->h * 0.5f;
  } else {
    v->scroll = col * v->pitch + v->pitch * 0.5f - (v->w - v->pitch) * 0.5f;
  }
  ad_view_update(v);
}
