#include "raster.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SS 4 /* sub-samples per axis */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ad_mask ad_mask_new(int size) {
  ad_mask m;
  m.size = size;
  m.a = (uint8_t *)calloc((size_t)size * (size_t)size, 1);
  return m;
}

void ad_mask_free(ad_mask *m) {
  if (m && m->a) free(m->a);
  if (m) {
    m->a = NULL;
    m->size = 0;
  }
}

/* A shape is a predicate over points relative to the mask centre, in pixels. */
typedef bool (*inside_fn)(const void *ctx, float x, float y);

static ad_mask render(int size, inside_fn fn, const void *ctx) {
  ad_mask m = ad_mask_new(size);
  if (!m.a) return m;
  const float c = size * 0.5f;
  for (int py = 0; py < size; py++) {
    for (int px = 0; px < size; px++) {
      int hits = 0;
      for (int sy = 0; sy < SS; sy++) {
        for (int sx = 0; sx < SS; sx++) {
          const float x = px + (sx + 0.5f) / SS - c;
          const float y = py + (sy + 0.5f) / SS - c;
          if (fn(ctx, x, y)) hits++;
        }
      }
      m.a[py * size + px] = (uint8_t)((hits * 255 + SS * SS / 2) / (SS * SS));
    }
  }
  return m;
}

/* ---- primitive shapes -------------------------------------------------- */

typedef struct {
  float outer, inner; /* half extents; inner <= 0 means solid */
} band_ctx;

static bool in_square(const void *p, float x, float y) {
  const band_ctx *c = (const band_ctx *)p;
  const float m = fmaxf(fabsf(x), fabsf(y));
  return m <= c->outer && (c->inner <= 0 || m > c->inner);
}

static bool in_squircle_r(float x, float y, float r) {
  if (r <= 0) return false;
  const float a = fabsf(x) / r, b = fabsf(y) / r;
  const float a2 = a * a, b2 = b * b;
  return a2 * a2 + b2 * b2 <= 1.0f;
}

static bool in_squircle(const void *p, float x, float y) {
  const band_ctx *c = (const band_ctx *)p;
  return in_squircle_r(x, y, c->outer) && !in_squircle_r(x, y, c->inner);
}

typedef struct {
  float r;
} disc_ctx;

static bool in_disc(const void *p, float x, float y) {
  const disc_ctx *c = (const disc_ctx *)p;
  return x * x + y * y <= c->r * c->r;
}

typedef struct {
  float x0, y0, x1, y1, half;
} seg_ctx;

static bool in_segment(const void *p, float x, float y) {
  const seg_ctx *c = (const seg_ctx *)p;
  const float dx = c->x1 - c->x0, dy = c->y1 - c->y0;
  float t = ((x - c->x0) * dx + (y - c->y0) * dy) / (dx * dx + dy * dy);
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  const float px = c->x0 + t * dx - x, py = c->y0 + t * dy - y;
  return px * px + py * py <= c->half * c->half;
}

ad_mask ad_raster_square(int size, float thickness) {
  band_ctx c = {size * 0.5f, size * 0.5f - thickness};
  return render(size, in_square, &c);
}

ad_mask ad_raster_dot(int size, float diameter) {
  disc_ctx c = {diameter * 0.5f};
  return render(size, in_disc, &c);
}

ad_mask ad_raster_slash(int size, float thickness) {
  /* Corner to corner, bottom-left to top-right, like a "/". */
  const float h = size * 0.5f;
  seg_ctx c = {-h + thickness * 0.5f, h - thickness * 0.5f, h - thickness * 0.5f, -h + thickness * 0.5f, thickness * 0.5f};
  return render(size, in_segment, &c);
}

ad_mask ad_raster_squircle(int size, float thickness) {
  band_ctx c = {size * 0.5f, size * 0.5f - thickness};
  return render(size, in_squircle, &c);
}

ad_mask ad_raster_squircle_fill(int size) {
  band_ctx c = {size * 0.5f, 0};
  return render(size, in_squircle, &c);
}

/* ---- symbols ----------------------------------------------------------- */

typedef struct {
  const float *pts; /* x,y pairs in the unit square [-1, 1], y pointing down */
  int n;
  float scale;      /* pixels per unit */
} poly_ctx;

static bool in_poly(const void *p, float x, float y) {
  const poly_ctx *c = (const poly_ctx *)p;
  const float px = x / c->scale, py = y / c->scale;
  bool inside = false;
  for (int i = 0, j = c->n - 1; i < c->n; j = i++) {
    const float xi = c->pts[i * 2], yi = c->pts[i * 2 + 1];
    const float xj = c->pts[j * 2], yj = c->pts[j * 2 + 1];
    if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) inside = !inside;
  }
  return inside;
}

static const float P_TRIANGLE[] = {0.0f, -0.95f, 0.95f, 0.72f, -0.95f, 0.72f};
static const float P_DIAMOND[] = {0.0f, -1.0f, 0.78f, 0.0f, 0.0f, 1.0f, -0.78f, 0.0f};
static const float P_PLUS[] = {-0.2f, -0.9f, 0.2f, -0.9f, 0.2f, -0.2f, 0.9f, -0.2f, 0.9f, 0.2f, 0.2f, 0.2f,
                               0.2f, 0.9f, -0.2f, 0.9f, -0.2f, 0.2f, -0.9f, 0.2f, -0.9f, -0.2f, -0.2f, -0.2f};
static const float P_HOURGLASS[] = {-0.8f, -0.95f, 0.8f, -0.95f, 0.1f, 0.0f, 0.8f, 0.95f, -0.8f, 0.95f, -0.1f, 0.0f};
static const float P_ARROW[] = {0.0f, -1.0f, 0.85f, -0.05f, 0.32f, -0.05f, 0.32f, 0.95f, -0.32f, 0.95f, -0.32f, -0.05f, -0.85f, -0.05f};
static const float P_BOLT[] = {0.3f, -1.0f, -0.65f, 0.15f, -0.05f, 0.15f, -0.3f, 1.0f, 0.65f, -0.2f, 0.08f, -0.2f};
static const float P_HOUSE[] = {0.0f, -0.98f, 0.92f, -0.15f, 0.92f, 0.9f, -0.92f, 0.9f, -0.92f, -0.15f};

ad_mask ad_raster_symbol(ad_symbol s, int size) {
  float buf[64]; /* scratch for generated polygons: up to 32 points */
  poly_ctx c;
  c.scale = size * 0.5f * 0.88f;
  c.pts = buf;
  switch (s) {
    case AD_SYM_STAR:
      for (int i = 0; i < 10; i++) {
        const double ang = -M_PI / 2 + i * M_PI / 5;
        const float r = (i % 2 == 0) ? 1.0f : 0.42f;
        buf[i * 2] = (float)(cos(ang) * r);
        buf[i * 2 + 1] = (float)(sin(ang) * r) + 0.05f;
      }
      c.n = 10;
      break;
    case AD_SYM_HEXAGON:
      for (int i = 0; i < 6; i++) {
        const double ang = i * M_PI / 3;
        buf[i * 2] = (float)cos(ang);
        buf[i * 2 + 1] = (float)sin(ang);
      }
      c.n = 6;
      break;
    case AD_SYM_PENTAGON:
      for (int i = 0; i < 5; i++) {
        const double ang = -M_PI / 2 + i * 2 * M_PI / 5;
        buf[i * 2] = (float)cos(ang);
        buf[i * 2 + 1] = (float)sin(ang) + 0.05f;
      }
      c.n = 5;
      break;
    case AD_SYM_CROSS: { /* the plus, turned a quarter of a quarter-turn */
      const int n = (int)(sizeof P_PLUS / sizeof P_PLUS[0] / 2);
      const float cs = 0.70710678f;
      for (int i = 0; i < n; i++) {
        const float x = P_PLUS[i * 2], y = P_PLUS[i * 2 + 1];
        buf[i * 2] = (x - y) * cs * 1.05f;
        buf[i * 2 + 1] = (x + y) * cs * 1.05f;
      }
      c.n = n;
      break;
    }
    case AD_SYM_HEART:
      for (int i = 0; i < 32; i++) {
        const double t = i * 2 * M_PI / 32;
        const double x = 16 * pow(sin(t), 3);
        const double y = -(13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t));
        buf[i * 2] = (float)(x / 17.0);
        buf[i * 2 + 1] = (float)((y + 2.5) / 17.0);
      }
      c.n = 32;
      break;
    case AD_SYM_TRIANGLE: c.pts = P_TRIANGLE; c.n = 3; break;
    case AD_SYM_DIAMOND: c.pts = P_DIAMOND; c.n = 4; break;
    case AD_SYM_PLUS: c.pts = P_PLUS; c.n = 12; break;
    case AD_SYM_HOURGLASS: c.pts = P_HOURGLASS; c.n = 6; break;
    case AD_SYM_ARROW: c.pts = P_ARROW; c.n = 7; break;
    case AD_SYM_BOLT: c.pts = P_BOLT; c.n = 6; break;
    case AD_SYM_HOUSE: c.pts = P_HOUSE; c.n = 5; break;
    default: c.pts = P_TRIANGLE; c.n = 3; break;
  }
  return render(size, in_poly, &c);
}

static const char *const SYMBOL_NAMES[AD_SYM_COUNT] = {
    "star", "triangle", "hexagon", "diamond", "pentagon", "plus",
    "cross", "hourglass", "arrow", "bolt", "heart", "house"};

const char *ad_symbol_name(ad_symbol s) {
  return ((int)s >= 0 && s < AD_SYM_COUNT) ? SYMBOL_NAMES[s] : "star";
}

bool ad_symbol_from_name(const char *name, ad_symbol *out) {
  for (int i = 0; i < AD_SYM_COUNT; i++) {
    if (strcmp(name, SYMBOL_NAMES[i]) == 0) {
      *out = (ad_symbol)i;
      return true;
    }
  }
  return false;
}
