/* mapgen: turns two Natural Earth GeoJSON files into Adderall's small map file.
 *
 *   mapgen land.geojson timezones.geojson > data/map/world.txt
 *
 * land.geojson      ne_110m_land       (country outlines, for the picture)
 * timezones.geojson ne_10m_time_zones  (the zones, one polygon per UTC offset)
 *
 * Natural Earth is in the public domain (https://www.naturalearthdata.com).
 * The output is plain text, one ring per line:
 *
 *   L <n> x y x y ...          a land ring
 *   Z <quarter-hours> <n> x y ... the outer ring of a time zone region
 *
 * with x = (longitude + 180) * 10 and y = (90 - latitude) * 10, both integers.
 * Rings are simplified (Douglas-Peucker) so the whole map stays small.
 *
 * Not part of the program itself: build it with -DADDERALL_BUILD_TOOLS=ON. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  double x, y;
} pt;

static char *slurp(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    perror(path);
    exit(1);
  }
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *b = (char *)malloc((size_t)n + 1);
  if (fread(b, 1, (size_t)n, f) != (size_t)n) exit(1);
  b[n] = 0;
  fclose(f);
  return b;
}

static double seg_dist(pt p, pt a, pt b) {
  const double dx = b.x - a.x, dy = b.y - a.y;
  const double l2 = dx * dx + dy * dy;
  double t = l2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2 : 0;
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  const double ex = a.x + t * dx - p.x, ey = a.y + t * dy - p.y;
  return sqrt(ex * ex + ey * ey);
}

static void dp(const pt *p, char *keep, int lo, int hi, double tol) {
  if (hi <= lo + 1) return;
  double best = -1;
  int bi = lo;
  for (int i = lo + 1; i < hi; i++) {
    const double d = seg_dist(p[i], p[lo], p[hi]);
    if (d > best) {
      best = d;
      bi = i;
    }
  }
  if (best > tol) {
    keep[bi] = 1;
    dp(p, keep, lo, bi, tol);
    dp(p, keep, bi, hi, tol);
  }
}

static void emit(char kind, int q, const pt *p, int n, double tol) {
  if (n < 4) return;
  char *keep = (char *)calloc((size_t)n, 1);
  int *ox = (int *)malloc(sizeof(int) * (size_t)n * 2);
  keep[0] = keep[n - 1] = 1;
  dp(p, keep, 0, n - 1, tol);
  int m = 0, lx = -1, ly = -1;
  for (int i = 0; i < n; i++) {
    if (!keep[i]) continue;
    const int x = (int)lround((p[i].x + 180.0) * 10.0), y = (int)lround((90.0 - p[i].y) * 10.0);
    if (x == lx && y == ly) continue; /* simplification can land two points on one grid spot */
    ox[m * 2] = x;
    ox[m * 2 + 1] = y;
    m++;
    lx = x;
    ly = y;
  }
  if (m >= 4) {
    if (kind == 'Z') printf("Z %d %d", q, m);
    else printf("L %d", m);
    for (int i = 0; i < m; i++) printf(" %d %d", ox[i * 2], ox[i * 2 + 1]);
    printf("\n");
  }
  free(ox);
  free(keep);
}

/* Read the rings of one feature's "coordinates" array starting at `s`. */
static const char *rings(const char *s, char kind, int quarters, int ring_depth, double tol, int outer_only) {
  int depth = 0;
  pt *buf = NULL;
  int n = 0, cap = 0, ring_index = 0;
  double num[2];
  int nn = 0;
  while (*s) {
    const char c = *s;
    if (c == '[') {
      depth++;
      s++;
    } else if (c == ']') {
      if (depth == ring_depth) {
        if (!outer_only || ring_index == 0) emit(kind, quarters, buf, n, tol);
        ring_index++;
        n = 0;
      }
      depth--;
      s++;
      if (depth == 0) break;
      if (depth == ring_depth - 2) ring_index = 0; /* a polygon closed: the next ring is an outer ring */
    } else if ((c >= '0' && c <= '9') || c == '-' || c == '.') {
      char *e;
      const double v = strtod(s, &e);
      s = e;
      num[nn++] = v;
      if (nn == 2) {
        if (n == cap) {
          cap = cap ? cap * 2 : 1024;
          buf = (pt *)realloc(buf, sizeof(pt) * (size_t)cap);
        }
        buf[n].x = num[0];
        buf[n].y = num[1];
        n++;
        nn = 0;
      }
      /* a third number (elevation) is skipped by the caller's data: none in Natural Earth */
    } else {
      s++;
    }
  }
  free(buf);
  return s;
}

static void process(const char *json, char kind, double tol) {
  const char *p = json;
  while ((p = strstr(p, "{\"type\":\"Feature\"")) != NULL) {
    const char *next = strstr(p + 10, "{\"type\":\"Feature\"");
    const char *zone = strstr(p, "\"zone\":");
    int quarters = 0;
    if (kind == 'Z') {
      if (!zone || (next && zone > next)) {
        p += 10;
        continue;
      }
      quarters = (int)lround(atof(zone + 7) * 4.0);
    }
    const char *geom = strstr(p, "\"geometry\":");
    if (!geom || (next && geom > next)) {
      p += 10;
      continue;
    }
    const int multi = strstr(geom, "\"MultiPolygon\"") && (!next || strstr(geom, "\"MultiPolygon\"") < next);
    const char *coords = strstr(geom, "\"coordinates\":");
    if (!coords) break;
    coords += 14;
    rings(coords, kind, quarters, multi ? 3 : 2, tol, kind == 'Z');
    p += 10;
  }
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: mapgen land.geojson timezones.geojson > world.txt\n");
    return 1;
  }
  char *land = slurp(argv[1]), *zones = slurp(argv[2]);
  printf("# Adderall world map: land outlines and time zone regions.\n");
  printf("# Natural Earth (public domain): ne_110m_land and ne_10m_time_zones, simplified.\n");
  printf("# x = (lon+180)*10, y = (90-lat)*10. L = land ring, Z <quarter-hours> = time zone region.\n");
  process(land, 'L', 0.05);
  process(zones, 'Z', 0.22);
  free(land);
  free(zones);
  return 0;
}
