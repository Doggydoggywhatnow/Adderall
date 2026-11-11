#include "map.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

void ad_map_free(ad_map *m) {
  for (int i = 0; i < m->n; i++) free(m->rings[i].xy);
  free(m->rings);
  m->rings = NULL;
  m->n = 0;
}

bool ad_map_load(const char *path, ad_map *out) {
  memset(out, 0, sizeof *out);
  char *data;
  size_t len;
  if (!ad_plat_read_file(path, &data, &len, NULL)) return false;
  int cap = 256;
  out->rings = (ad_ring *)malloc(sizeof(ad_ring) * (size_t)cap);
  char *line = data;
  bool ok = out->rings != NULL;
  while (ok && line && *line) {
    char *nl = strchr(line, '\n');
    if (nl) *nl = 0;
    if (line[0] == 'L' || line[0] == 'Z') {
      char *p = line + 1;
      char *e;
      ad_ring r;
      memset(&r, 0, sizeof r);
      r.kind = line[0];
      if (r.kind == 'Z') {
        r.quarters = (int)strtol(p, &e, 10);
        p = e;
      }
      const long n = strtol(p, &e, 10);
      p = e;
      if (n < 3 || n > 100000) {
        ok = false;
        break;
      }
      r.n = (int)n;
      r.xy = (int16_t *)malloc(sizeof(int16_t) * 2 * (size_t)n);
      if (!r.xy) {
        ok = false;
        break;
      }
      for (long i = 0; i < 2 * n; i++) {
        const long v = strtol(p, &e, 10);
        if (e == p) {
          ok = false;
          break;
        }
        p = e;
        r.xy[i] = (int16_t)v;
      }
      if (!ok) {
        free(r.xy);
        break;
      }
      if (out->n == cap) {
        cap *= 2;
        ad_ring *nr = (ad_ring *)realloc(out->rings, sizeof(ad_ring) * (size_t)cap);
        if (!nr) {
          free(r.xy);
          ok = false;
          break;
        }
        out->rings = nr;
      }
      out->rings[out->n++] = r;
    }
    line = nl ? nl + 1 : NULL;
  }
  free(data);
  if (!ok || out->n == 0) {
    ad_map_free(out);
    return false;
  }
  return true;
}

static int cmp_double(const void *a, const void *b) {
  const double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}

void ad_map_fill(const ad_map *m, char kind, int32_t quarters, int w, int h, uint8_t *alpha, uint8_t value) {
  const double sx = (double)w / AD_MAP_W, sy = (double)h / AD_MAP_H;
  size_t cap = 256;
  double *xs = (double *)malloc(sizeof(double) * cap);
  if (!xs) return;
  for (int row = 0; row < h; row++) {
    const double y = (row + 0.5) / sy; /* in map units */
    size_t nx = 0;
    for (int i = 0; i < m->n; i++) {
      const ad_ring *r = &m->rings[i];
      if (r->kind != kind) continue;
      if (kind == 'Z' && quarters != INT32_MIN && r->quarters != quarters) continue;
      for (int k = 0, j = r->n - 1; k < r->n; j = k++) {
        const double y0 = r->xy[j * 2 + 1], y1 = r->xy[k * 2 + 1];
        if ((y0 > y) == (y1 > y)) continue;
        const double x0 = r->xy[j * 2], x1 = r->xy[k * 2];
        if (nx == cap) {
          cap *= 2;
          double *nxs = (double *)realloc(xs, sizeof(double) * cap);
          if (!nxs) {
            free(xs);
            return;
          }
          xs = nxs;
        }
        xs[nx++] = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
      }
    }
    qsort(xs, nx, sizeof(double), cmp_double);
    for (size_t k = 0; k + 1 < nx; k += 2) {
      int a = (int)lround(xs[k] * sx), b = (int)lround(xs[k + 1] * sx);
      if (a < 0) a = 0;
      if (b > w) b = w;
      for (int x = a; x < b; x++) alpha[row * w + x] = value;
    }
  }
  free(xs);
}

static void line(uint8_t *alpha, int w, int h, int x0, int y0, int x1, int y1, uint8_t value) {
  const int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    if (x0 >= 0 && x0 < w && y0 >= 0 && y0 < h && alpha[y0 * w + x0] < value) alpha[y0 * w + x0] = value;
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void ad_map_outline(const ad_map *m, char kind, int w, int h, uint8_t *alpha, uint8_t value) {
  const double sx = (double)w / AD_MAP_W, sy = (double)h / AD_MAP_H;
  for (int i = 0; i < m->n; i++) {
    const ad_ring *r = &m->rings[i];
    if (r->kind != kind) continue;
    for (int k = 0, j = r->n - 1; k < r->n; j = k++) {
      /* skip the long edges that run along the map's own border */
      const int ax = r->xy[j * 2], bx = r->xy[k * 2], ay = r->xy[j * 2 + 1], by = r->xy[k * 2 + 1];
      if ((ax <= 1 && bx <= 1) || (ax >= AD_MAP_W - 1 && bx >= AD_MAP_W - 1) || (ay <= 1 && by <= 1) || (ay >= AD_MAP_H - 1 && by >= AD_MAP_H - 1)) continue;
      line(alpha, w, h, (int)lround(ax * sx), (int)lround(ay * sy), (int)lround(bx * sx), (int)lround(by * sy), value);
    }
  }
}

static bool inside(const ad_ring *r, double x, double y) {
  bool in = false;
  for (int k = 0, j = r->n - 1; k < r->n; j = k++) {
    const double y0 = r->xy[j * 2 + 1], y1 = r->xy[k * 2 + 1];
    if ((y0 > y) != (y1 > y)) {
      const double x0 = r->xy[j * 2], x1 = r->xy[k * 2];
      if (x < x0 + (y - y0) * (x1 - x0) / (y1 - y0)) in = !in;
    }
  }
  return in;
}

bool ad_map_band_at(const ad_map *m, double lon, double lat, int *quarters) {
  const double x = (lon + 180.0) * 10.0, y = (90.0 - lat) * 10.0;
  for (int i = 0; i < m->n; i++) {
    const ad_ring *r = &m->rings[i];
    if (r->kind == 'Z' && inside(r, x, y)) {
      *quarters = r->quarters;
      return true;
    }
  }
  return false;
}

void ad_map_to_lonlat(int w, int h, float px, float py, double *lon, double *lat) {
  *lon = px / (double)w * 360.0 - 180.0;
  *lat = 90.0 - py / (double)h * 180.0;
}

void ad_map_from_lonlat(int w, int h, double lon, double lat, float *px, float *py) {
  *px = (float)((lon + 180.0) / 360.0 * w);
  *py = (float)((90.0 - lat) / 180.0 * h);
}
