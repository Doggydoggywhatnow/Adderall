#include "model.h"

#include <stdlib.h>
#include <string.h>

void ad_settings_defaults(ad_settings *s) {
  memset(s, 0, sizeof *s);
  s->daystart = 0;
  s->layout = AD_LAYOUT_HORIZONTAL;
  s->textsize = 16;
  s->scale = 100;
  s->lastday = AD_DAY_NONE;
}

void ad_model_init(ad_model *m) {
  memset(m, 0, sizeof *m);
  ad_settings_defaults(&m->settings);
}

void ad_model_free(ad_model *m) {
  for (int i = 0; i < m->n_habits; i++) free(m->habits[i].marks);
  free(m->habits);
  memset(m, 0, sizeof *m);
}

/* First index whose day >= `day`. */
static size_t lower_bound(const ad_habit *h, ad_day day) {
  size_t lo = 0, hi = h->n_marks;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    if (h->marks[mid].day < day) lo = mid + 1;
    else hi = mid;
  }
  return lo;
}

ad_state ad_habit_get(const ad_habit *h, ad_day day) {
  const size_t i = lower_bound(h, day);
  if (i < h->n_marks && h->marks[i].day == day) return (ad_state)h->marks[i].state;
  return AD_EMPTY;
}

bool ad_habit_put(ad_habit *h, ad_day day, ad_state s) {
  const size_t i = lower_bound(h, day);
  const bool exists = i < h->n_marks && h->marks[i].day == day;
  if (s == AD_EMPTY) {
    if (exists) {
      memmove(&h->marks[i], &h->marks[i + 1], (h->n_marks - i - 1) * sizeof(ad_mark));
      h->n_marks--;
    }
    return true;
  }
  if (exists) {
    h->marks[i].state = (uint8_t)s;
    return true;
  }
  if (h->n_marks == h->cap_marks) {
    const size_t cap = h->cap_marks ? h->cap_marks * 2 : 64;
    ad_mark *p = (ad_mark *)realloc(h->marks, cap * sizeof(ad_mark));
    if (!p) return false;
    h->marks = p;
    h->cap_marks = cap;
  }
  memmove(&h->marks[i + 1], &h->marks[i], (h->n_marks - i) * sizeof(ad_mark));
  h->marks[i].day = day;
  h->marks[i].state = (uint8_t)s;
  h->n_marks++;
  return true;
}

int ad_model_add_raw(ad_model *m, const char *name, ad_symbol sym, ad_day start) {
  if (m->n_habits == m->cap_habits) {
    const int cap = m->cap_habits ? m->cap_habits * 2 : 8;
    ad_habit *p = (ad_habit *)realloc(m->habits, (size_t)cap * sizeof(ad_habit));
    if (!p) return -1;
    m->habits = p;
    m->cap_habits = cap;
  }
  ad_habit *h = &m->habits[m->n_habits];
  memset(h, 0, sizeof *h);
  strncpy(h->name, name, AD_NAME_MAX_BYTES);
  h->symbol = sym;
  h->start = start;
  return m->n_habits++;
}

/* ---- names ------------------------------------------------------------- */

/* Decode one code point; returns the byte length, or 0 if invalid. */
static int utf8_decode(const unsigned char *p, size_t avail, uint32_t *cp) {
  if (avail == 0) return 0;
  const unsigned char c = p[0];
  if (c < 0x80) {
    *cp = c;
    return 1;
  }
  int n;
  uint32_t v, min;
  if ((c & 0xE0) == 0xC0) {
    n = 2;
    v = c & 0x1F;
    min = 0x80;
  } else if ((c & 0xF0) == 0xE0) {
    n = 3;
    v = c & 0x0F;
    min = 0x800;
  } else if ((c & 0xF8) == 0xF0) {
    n = 4;
    v = c & 0x07;
    min = 0x10000;
  } else {
    return 0;
  }
  if ((size_t)n > avail) return 0;
  for (int i = 1; i < n; i++) {
    if ((p[i] & 0xC0) != 0x80) return 0;
    v = (v << 6) | (p[i] & 0x3F);
  }
  if (v < min || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) return 0;
  *cp = v;
  return n;
}

bool ad_utf8_valid(const char *s, size_t len) {
  const unsigned char *p = (const unsigned char *)s;
  size_t i = 0;
  while (i < len) {
    uint32_t cp;
    const int n = utf8_decode(p + i, len - i, &cp);
    if (n == 0) return false;
    i += (size_t)n;
  }
  return true;
}

ad_name_result ad_name_normalize(const char *in, char *out) {
  size_t len = strlen(in);
  if (!ad_utf8_valid(in, len)) return AD_NAME_BAD_CHAR;
  size_t a = 0, b = len;
  while (a < b && (in[a] == ' ' || in[a] == '\t')) a++;
  while (b > a && (in[b - 1] == ' ' || in[b - 1] == '\t')) b--;
  if (a == b) {
    /* All blanks: but a line break is still a bad character, not "empty". */
    for (size_t i = 0; i < len; i++)
      if ((unsigned char)in[i] < 0x20) return AD_NAME_BAD_CHAR;
    return AD_NAME_EMPTY;
  }
  int chars = 0;
  const unsigned char *p = (const unsigned char *)in;
  for (size_t i = a; i < b;) {
    uint32_t cp;
    const int n = utf8_decode(p + i, b - i, &cp);
    if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp < 0xA0) || cp == 0x2028 || cp == 0x2029 || cp == 0xFEFF)
      return AD_NAME_BAD_CHAR;
    chars++;
    i += (size_t)n;
  }
  if (chars > AD_NAME_MAX_CHARS) return AD_NAME_TOO_LONG;
  memcpy(out, in + a, b - a);
  out[b - a] = 0;
  return AD_NAME_OK;
}

const char *ad_name_message(ad_name_result r) {
  switch (r) {
    case AD_NAME_OK: return "";
    case AD_NAME_EMPTY: return "A habit needs a name.";
    case AD_NAME_TOO_LONG: return "Names can be up to 40 characters.";
    case AD_NAME_BAD_CHAR: return "Names are a single line of plain text.";
  }
  return "";
}

static uint32_t fold(uint32_t c) {
  if (c < 0x80) return (c >= 'A' && c <= 'Z') ? c + 32 : c;
  if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
  if (c >= 0x100 && c <= 0x137) return (c % 2 == 0) ? c + 1 : c;
  if (c >= 0x139 && c <= 0x148) return (c % 2 == 1) ? c + 1 : c;
  if (c >= 0x14A && c <= 0x177) return (c % 2 == 0) ? c + 1 : c;
  if (c == 0x178) return 0xFF;
  if (c >= 0x179 && c <= 0x17E) return (c % 2 == 1) ? c + 1 : c;
  if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return c + 32;
  if (c >= 0x410 && c <= 0x42F) return c + 32;
  if (c >= 0x400 && c <= 0x40F) return c + 80;
  return c;
}

bool ad_name_equal(const char *a, const char *b) {
  const unsigned char *pa = (const unsigned char *)a, *pb = (const unsigned char *)b;
  size_t la = strlen(a), lb = strlen(b), i = 0, j = 0;
  while (i < la && j < lb) {
    uint32_t ca, cb;
    const int na = utf8_decode(pa + i, la - i, &ca), nb = utf8_decode(pb + j, lb - j, &cb);
    if (na == 0 || nb == 0) return strcmp(a, b) == 0;
    if (fold(ca) != fold(cb)) return false;
    i += (size_t)na;
    j += (size_t)nb;
  }
  return i == la && j == lb;
}
