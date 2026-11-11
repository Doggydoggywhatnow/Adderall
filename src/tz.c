#include "tz.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

/* ---- POSIX TZ rules (the part of a TZif file after the last transition) -- */

typedef struct {
  char kind; /* 'M' month.week.day, 'J' julian 1..365 (no leap day), 'N' zero-based day */
  int m, w, d;
  int n;
  int time; /* seconds after local midnight; may be negative or past 24h */
} rule_date;

typedef struct {
  bool valid;
  int std_off, dst_off; /* seconds east of UTC */
  bool has_dst;
  rule_date start, end;
} posix_rule;

struct ad_tz {
  int64_t *trans;
  uint8_t *idx;
  int ntrans;
  struct {
    int32_t off;
    bool dst;
  } *types;
  int ntypes;
  int first_type; /* used before the first transition */
  posix_rule rule;
};

static bool rd_name(const char **p) {
  const char *s = *p;
  if (*s == '<') {
    while (*s && *s != '>') s++;
    if (*s != '>') return false;
    s++;
  } else {
    int n = 0;
    while (isalpha((unsigned char)*s)) {
      s++;
      n++;
    }
    if (n < 3) return false;
  }
  *p = s;
  return true;
}

/* [+-]hh[:mm[:ss]] -> seconds */
static bool rd_hms(const char **p, int *out) {
  const char *s = *p;
  int sign = 1;
  if (*s == '+') s++;
  else if (*s == '-') {
    sign = -1;
    s++;
  }
  if (!isdigit((unsigned char)*s)) return false;
  int v[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
    int x = 0, digits = 0;
    while (isdigit((unsigned char)*s) && digits < 3) {
      x = x * 10 + (*s - '0');
      s++;
      digits++;
    }
    if (digits == 0) return false;
    v[i] = x;
    if (*s == ':' && i < 2) s++;
    else break;
  }
  *out = sign * (v[0] * 3600 + v[1] * 60 + v[2]);
  *p = s;
  return true;
}

static bool rd_rule_date(const char **p, rule_date *r) {
  const char *s = *p;
  r->time = 7200;
  if (*s == 'M') {
    int m, w, d, used;
    if (sscanf(s, "M%d.%d.%d%n", &m, &w, &d, &used) != 3) return false;
    if (m < 1 || m > 12 || w < 1 || w > 5 || d < 0 || d > 6) return false;
    r->kind = 'M';
    r->m = m;
    r->w = w;
    r->d = d;
    s += used;
  } else if (*s == 'J') {
    int n, used;
    if (sscanf(s, "J%d%n", &n, &used) != 1 || n < 1 || n > 365) return false;
    r->kind = 'J';
    r->n = n;
    s += used;
  } else if (isdigit((unsigned char)*s)) {
    int n, used;
    if (sscanf(s, "%d%n", &n, &used) != 1 || n < 0 || n > 365) return false;
    r->kind = 'N';
    r->n = n;
    s += used;
  } else {
    return false;
  }
  if (*s == '/') {
    s++;
    if (!rd_hms(&s, &r->time)) return false;
  }
  *p = s;
  return true;
}

static bool parse_posix(const char *s, posix_rule *r) {
  memset(r, 0, sizeof *r);
  if (!rd_name(&s)) return false;
  int off;
  if (!rd_hms(&s, &off)) return false;
  r->std_off = -off;
  if (*s == 0) {
    r->valid = true;
    return true;
  }
  if (!rd_name(&s)) return false;
  r->dst_off = r->std_off + 3600;
  if (*s && *s != ',') {
    if (!rd_hms(&s, &off)) return false;
    r->dst_off = -off;
  }
  r->has_dst = true;
  if (*s != ',') {
    /* No rule given: the US rule of the time zone database's default. */
    r->start = (rule_date){'M', 3, 2, 0, 0, 7200};
    r->end = (rule_date){'M', 11, 1, 0, 0, 7200};
    r->valid = true;
    return true;
  }
  s++;
  if (!rd_rule_date(&s, &r->start) || *s != ',') return false;
  s++;
  if (!rd_rule_date(&s, &r->end) || *s != 0) return false;
  r->valid = true;
  return true;
}

static bool is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

/* The local wall-clock seconds (since 1970, as if local were UTC) at which a rule fires in a year. */
static int64_t rule_local(const rule_date *r, int year) {
  ad_day day;
  if (r->kind == 'M') {
    const ad_day first = ad_day_from_ymd(year, r->m, 1);
    const int wd = (ad_day_weekday(first) + 1) % 7; /* 0 = Sunday */
    int dom = 1 + ((r->d - wd + 7) % 7) + (r->w - 1) * 7;
    static const int dm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int len = (r->m == 2 && is_leap(year)) ? 29 : dm[r->m - 1];
    while (dom > len) dom -= 7;
    day = ad_day_from_ymd(year, r->m, dom);
  } else if (r->kind == 'J') {
    int n = r->n;
    if (is_leap(year) && n >= 60) n++; /* Jn never counts February 29 */
    day = ad_day_from_ymd(year, 1, 1) + (n - 1);
  } else {
    day = ad_day_from_ymd(year, 1, 1) + r->n;
  }
  return (int64_t)day * 86400 + r->time;
}

static int rule_offset(const posix_rule *r, int64_t utc, bool *dst) {
  if (dst) *dst = false;
  if (!r->has_dst) return r->std_off;
  int y, m, d;
  ad_day_to_ymd((ad_day)ad_floor_div(utc + r->std_off, 86400), &y, &m, &d);
  const int64_t start = rule_local(&r->start, y) - r->std_off; /* starts in standard time */
  const int64_t end = rule_local(&r->end, y) - r->dst_off;     /* ends in daylight time */
  bool in_dst = (start < end) ? (utc >= start && utc < end) : !(utc >= end && utc < start);
  if (dst) *dst = in_dst;
  return in_dst ? r->dst_off : r->std_off;
}

/* ---- TZif ---------------------------------------------------------------- */

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static int64_t be64(const uint8_t *p) { return (int64_t)(((uint64_t)be32(p) << 32) | be32(p + 4)); }

typedef struct {
  uint32_t isut, isstd, leap, time, type, chars;
} counts;

static bool read_header(const uint8_t *p, size_t len, counts *c, int *version) {
  if (len < 44 || memcmp(p, "TZif", 4) != 0) return false;
  *version = p[4] == 0 ? 1 : p[4] - '0';
  c->isut = be32(p + 20);
  c->isstd = be32(p + 24);
  c->leap = be32(p + 28);
  c->time = be32(p + 32);
  c->type = be32(p + 36);
  c->chars = be32(p + 40);
  return c->type > 0 && c->type <= 256 && c->time <= 100000 && c->chars <= 1024;
}

ad_tz *ad_tz_from_bytes(const uint8_t *data, size_t len) {
  counts c;
  int version;
  if (!read_header(data, len, &c, &version)) return NULL;
  const size_t v1 = (size_t)c.time * 5 + (size_t)c.type * 6 + c.chars + (size_t)c.leap * 8 + c.isstd + c.isut;
  const uint8_t *p = data + 44;
  size_t left = len - 44;
  int tsize = 4, lsize = 8;
  if (version >= 2) {
    if (left < v1 + 44) return NULL;
    p += v1;
    left -= v1;
    if (!read_header(p, left, &c, &version)) return NULL;
    p += 44;
    left -= 44;
    tsize = 8;
    lsize = 12;
  }
  const size_t body = (size_t)c.time * (size_t)(tsize + 1) + (size_t)c.type * 6 + c.chars + (size_t)c.leap * (size_t)lsize + c.isstd + c.isut;
  if (left < body) return NULL;

  ad_tz *tz = (ad_tz *)calloc(1, sizeof *tz);
  if (!tz) return NULL;
  tz->ntrans = (int)c.time;
  tz->ntypes = (int)c.type;
  tz->trans = (int64_t *)malloc(sizeof(int64_t) * (c.time + 1));
  tz->idx = (uint8_t *)malloc(c.time + 1);
  tz->types = malloc(sizeof(*tz->types) * c.type);
  if (!tz->trans || !tz->idx || !tz->types) {
    ad_tz_free(tz);
    return NULL;
  }
  for (uint32_t i = 0; i < c.time; i++) tz->trans[i] = tsize == 8 ? be64(p + i * 8) : (int32_t)be32(p + i * 4);
  p += (size_t)c.time * (size_t)tsize;
  for (uint32_t i = 0; i < c.time; i++) {
    tz->idx[i] = p[i];
    if (p[i] >= c.type) {
      ad_tz_free(tz);
      return NULL;
    }
  }
  p += c.time;
  tz->first_type = 0;
  bool found_std = false;
  for (uint32_t i = 0; i < c.type; i++) {
    tz->types[i].off = (int32_t)be32(p + i * 6);
    tz->types[i].dst = p[i * 6 + 4] != 0;
    if (!found_std && !tz->types[i].dst) {
      tz->first_type = (int)i;
      found_std = true;
    }
  }
  p += (size_t)c.type * 6 + c.chars + (size_t)c.leap * (size_t)lsize + c.isstd + c.isut;
  left -= body;

  /* The footer: "\n<POSIX TZ string>\n" for times after the last transition. */
  if (version >= 2 && left >= 2 && p[0] == '\n') {
    char foot[128];
    size_t n = 0;
    p++;
    while (n < left - 1 && n < sizeof foot - 1 && p[n] != '\n') {
      foot[n] = (char)p[n];
      n++;
    }
    foot[n] = 0;
    if (n > 0) parse_posix(foot, &tz->rule);
  }
  return tz;
}

void ad_tz_free(ad_tz *tz) {
  if (!tz) return;
  free(tz->trans);
  free(tz->idx);
  free(tz->types);
  free(tz);
}

ad_tz *ad_tz_load(const char *dir, const char *name) {
  /* Only plain zone names: no "..", no absolute paths, nothing odd. */
  if (!name || !name[0] || strlen(name) > 63 || name[0] == '/' || strstr(name, "..")) return NULL;
  for (const char *c = name; *c; c++) {
    if (!(isalnum((unsigned char)*c) || *c == '/' || *c == '_' || *c == '-' || *c == '+')) return NULL;
  }
  char path[AD_PATH_MAX + 80];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  char *data;
  size_t len;
  if (!ad_plat_read_file(path, &data, &len, NULL)) return NULL;
  ad_tz *tz = ad_tz_from_bytes((const uint8_t *)data, len);
  free(data);
  return tz;
}

int ad_tz_offset(const ad_tz *tz, int64_t utc, bool *is_dst) {
  if (is_dst) *is_dst = false;
  if (!tz) return 0;
  if (tz->ntrans == 0 || utc < tz->trans[0]) {
    if (tz->ntrans == 0 && tz->rule.valid) return rule_offset(&tz->rule, utc, is_dst);
    if (is_dst) *is_dst = tz->types[tz->first_type].dst;
    return tz->types[tz->first_type].off;
  }
  if (utc >= tz->trans[tz->ntrans - 1] && tz->rule.valid) return rule_offset(&tz->rule, utc, is_dst);
  int lo = 0, hi = tz->ntrans - 1; /* last transition <= utc */
  while (lo < hi) {
    const int mid = (lo + hi + 1) / 2;
    if (tz->trans[mid] <= utc) lo = mid;
    else hi = mid - 1;
  }
  const int t = tz->idx[lo];
  if (is_dst) *is_dst = tz->types[t].dst;
  return tz->types[t].off;
}

/* ---- days ---------------------------------------------------------------- */

ad_day ad_tz_day(const ad_tz *tz, int64_t utc, int daystart_min) {
  const int64_t local = utc + ad_tz_offset(tz, utc, NULL) - (int64_t)daystart_min * 60;
  return (ad_day)ad_floor_div(local, 86400);
}

int64_t ad_tz_seconds_to_next_day(const ad_tz *tz, int64_t utc, int daystart_min) {
  const int64_t local = utc + ad_tz_offset(tz, utc, NULL) - (int64_t)daystart_min * 60;
  int64_t left = 86400 - ad_floor_mod(local, 86400);
  /* A daylight saving change can move the wall clock inside this stretch; the
   * caller re-checks at least every minute, so being a little off is harmless. */
  if (left < 1) left = 1;
  return left;
}

/* ---- the zone list -------------------------------------------------------- */

static bool parse_iso6709(const char *s, double *lat, double *lon) {
  /* +DDMM+DDDMM, +DDMMSS+DDDMMSS (signs may also be -) */
  size_t split = 1;
  while (s[split] && s[split] != '+' && s[split] != '-') split++;
  if (!s[split]) return false;
  const size_t la = split, lo = strlen(s) - split;
  if (!(la == 5 || la == 7) || !(lo == 6 || lo == 8)) return false;
  for (size_t i = 1; i < la; i++)
    if (!isdigit((unsigned char)s[i])) return false;
  for (size_t i = 1; i < lo; i++)
    if (!isdigit((unsigned char)s[split + i])) return false;
  int d, m, sec = 0;
  char b[4];
  memcpy(b, s + 1, 2);
  b[2] = 0;
  d = atoi(b);
  memcpy(b, s + 3, 2);
  b[2] = 0;
  m = atoi(b);
  if (la == 7) {
    memcpy(b, s + 5, 2);
    b[2] = 0;
    sec = atoi(b);
  }
  *lat = (d + m / 60.0 + sec / 3600.0) * (s[0] == '-' ? -1 : 1);
  const char *q = s + split;
  memcpy(b, q + 1, 3);
  b[3] = 0;
  d = atoi(b);
  memcpy(b, q + 4, 2);
  b[2] = 0;
  m = atoi(b);
  sec = 0;
  if (lo == 8) {
    memcpy(b, q + 6, 2);
    b[2] = 0;
    sec = atoi(b);
  }
  *lon = (d + m / 60.0 + sec / 3600.0) * (q[0] == '-' ? -1 : 1);
  return true;
}

int ad_zones_load(const char *dir, ad_zone_info **out) {
  *out = NULL;
  char path[AD_PATH_MAX + 32];
  snprintf(path, sizeof path, "%s/zone.tab", dir);
  char *data;
  size_t len;
  if (!ad_plat_read_file(path, &data, &len, NULL)) return 0;
  int cap = 512, n = 0;
  ad_zone_info *z = (ad_zone_info *)malloc(sizeof *z * (size_t)cap);
  char *line = data;
  while (z && line && *line) {
    char *nl = strchr(line, '\n');
    if (nl) *nl = 0;
    {
      size_t ll = strlen(line);
      if (ll && line[ll - 1] == '\r') line[ll - 1] = 0; /* checked out with Windows line endings */
    }
    if (line[0] != '#' && line[0]) {
      char *f[3] = {line, NULL, NULL};
      char *p = line;
      int k = 1;
      while (*p && k < 3) {
        if (*p == '\t') {
          *p = 0;
          f[k++] = p + 1;
        }
        p++;
      }
      if (k == 3) {
        char *tzname = f[2];
        char *t = strchr(tzname, '\t');
        if (t) *t = 0;
        ad_zone_info zi;
        memset(&zi, 0, sizeof zi);
        if (strlen(tzname) < sizeof zi.name && strlen(f[0]) == 2 && parse_iso6709(f[1], &zi.lat, &zi.lon)) {
          strcpy(zi.name, tzname);
          strcpy(zi.cc, f[0]);
          if (n == cap) {
            cap *= 2;
            ad_zone_info *nz = (ad_zone_info *)realloc(z, sizeof *z * (size_t)cap);
            if (!nz) break;
            z = nz;
          }
          z[n++] = zi;
        }
      }
    }
    line = nl ? nl + 1 : NULL;
  }
  free(data);
  *out = z;
  return n;
}

int ad_zones_find(const ad_zone_info *z, int n, const char *name) {
  for (int i = 0; i < n; i++)
    if (strcmp(z[i].name, name) == 0) return i;
  return -1;
}

int ad_zones_nearest(const ad_zone_info *z, int n, double lat, double lon) {
  int best = -1;
  double bd = 1e18;
  const double rad = 3.14159265358979323846 / 180.0;
  for (int i = 0; i < n; i++) {
    const double a1 = lat * rad, a2 = z[i].lat * rad, dl = (z[i].lon - lon) * rad;
    /* haversine, good enough to rank places */
    const double h = sin((a2 - a1) / 2) * sin((a2 - a1) / 2) + cos(a1) * cos(a2) * sin(dl / 2) * sin(dl / 2);
    if (h < bd) {
      bd = h;
      best = i;
    }
  }
  return best;
}

void ad_tz_format_offset(int off, char *out, size_t cap) {
  const int a = off < 0 ? -off : off;
  const int h = a / 3600, m = (a % 3600) / 60;
  if (m) snprintf(out, cap, "UTC%c%d:%02d", off < 0 ? '-' : '+', h, m);
  else snprintf(out, cap, "UTC%c%d", off < 0 ? '-' : '+', h);
}

void ad_tz_detect_system(const char *dir, const ad_zone_info *z, int n, char *out, size_t cap) {
  char name[96];
  /* 1-3: the system says its zone by name. */
  if (ad_plat_system_tz_name(name, sizeof name)) {
    if (ad_zones_find(z, n, name) >= 0 || strcmp(name, "UTC") == 0) {
      snprintf(out, cap, "%s", name);
      return;
    }
  }
  /* 4: find a zone that agrees with the system clock all year round. */
  const int64_t now = ad_plat_now();
  int y, m, d;
  ad_day_to_ymd((ad_day)ad_floor_div(now, 86400), &y, &m, &d);
  int64_t samples[6];
  int ns = 0;
  for (int yy = y - 2; yy <= y; yy++) {
    samples[ns++] = (int64_t)ad_day_from_ymd(yy, 1, 15) * 86400 + 43200;
    samples[ns++] = (int64_t)ad_day_from_ymd(yy, 7, 15) * 86400 + 43200;
  }
  int sys[6];
  for (int i = 0; i < ns; i++) sys[i] = ad_plat_local_offset(samples[i]);
  for (int i = 0; i < n; i++) {
    ad_tz *tz = ad_tz_load(dir, z[i].name);
    if (!tz) continue;
    bool ok = true;
    for (int k = 0; k < ns && ok; k++)
      if (ad_tz_offset(tz, samples[k], NULL) != sys[k]) ok = false;
    ad_tz_free(tz);
    if (ok) {
      snprintf(out, cap, "%s", z[i].name);
      return;
    }
  }
  snprintf(out, cap, "UTC");
}
