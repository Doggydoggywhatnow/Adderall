#include "format.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- errors ------------------------------------------------------------ */

static ad_parse_status fail(ad_parse_error *e, ad_parse_status st, int line, const char *text, size_t tlen, const char *fmt, ...) {
  if (!e) return st;
  e->status = st;
  e->line = line;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(e->message, sizeof e->message, fmt, ap);
  va_end(ap);
  if (tlen >= sizeof e->text) tlen = sizeof e->text - 4;
  if (text) {
    memcpy(e->text, text, tlen);
    e->text[tlen] = 0;
    if (tlen == sizeof e->text - 4) strcat(e->text, "...");
  } else {
    e->text[0] = 0;
  }
  return st;
}

void ad_parse_error_describe(const ad_parse_error *e, char *out, size_t cap) {
  if (e->status == AD_PARSE_LINE) snprintf(out, cap, "Line %d: %s", e->line, e->message);
  else snprintf(out, cap, "%s", e->message);
}

/* ---- small helpers ----------------------------------------------------- */

typedef struct {
  const char *p;
  size_t n;
} span;

static bool span_eq(span s, const char *lit) { return s.n == strlen(lit) && memcmp(s.p, lit, s.n) == 0; }
static bool span_starts(span s, const char *lit) { return s.n >= strlen(lit) && memcmp(s.p, lit, strlen(lit)) == 0; }

static bool parse_int(span s, int lo, int hi, int *out) {
  if (s.n == 0 || s.n > 6) return false;
  int v = 0;
  for (size_t i = 0; i < s.n; i++) {
    if (s.p[i] < '0' || s.p[i] > '9') return false;
    v = v * 10 + (s.p[i] - '0');
  }
  if (v < lo || v > hi) return false;
  *out = v;
  return true;
}

static bool copy_span(span s, char *dst, size_t cap) {
  if (s.n >= cap) return false;
  memcpy(dst, s.p, s.n);
  dst[s.n] = 0;
  return true;
}

#define MAX_LINE 4096
#define MAX_FILE (32u * 1024u * 1024u)

/* ---- settings ---------------------------------------------------------- */

enum {
  K_TIMEZONE, K_DAYSTART, K_LAYOUT, K_FONT, K_TEXTSIZE, K_THEME, K_SCALE, K_CONTRAST, K_MOTION, K_LASTDAY, K_COUNT
};
static const char *const KEYS[K_COUNT] = {"timezone", "daystart", "layout", "font", "textsize", "theme", "scale", "contrast", "motion", "lastday"};

static bool zone_name_ok(span v) {
  if (v.n == 0 || v.n > 63) return false;
  for (size_t i = 0; i < v.n; i++) {
    const char c = v.p[i];
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '+' || c == '/';
    if (!ok) return false;
  }
  return true;
}

/* Returns NULL when the setting was applied, else the reason it was refused. */
static const char *apply_setting(ad_settings *s, span key, span val, unsigned *seen) {
  int k = -1;
  for (int i = 0; i < K_COUNT; i++)
    if (span_eq(key, KEYS[i])) k = i;
  if (k >= 0) {
    if (*seen & (1u << k)) return "this setting appears twice";
    *seen |= 1u << k;
  }
  int v;
  switch (k) {
    case K_TIMEZONE:
      if (!zone_name_ok(val)) return "the time zone should look like Europe/Paris";
      copy_span(val, s->timezone, sizeof s->timezone);
      return NULL;
    case K_DAYSTART:
      if (val.n != 5 || val.p[2] != ':') return "the new-day time should look like 04:00";
      {
        int h, m;
        if (!parse_int((span){val.p, 2}, 0, 23, &h) || !parse_int((span){val.p + 3, 2}, 0, 59, &m)) return "the new-day time should look like 04:00";
        v = h * 60 + m;
        if (v > AD_DAYSTART_MAX) return "the new-day time can be anywhere from 00:00 to 11:59";
        s->daystart = v;
      }
      return NULL;
    case K_LAYOUT:
      if (span_eq(val, "horizontal")) s->layout = AD_LAYOUT_HORIZONTAL;
      else if (span_eq(val, "vertical")) s->layout = AD_LAYOUT_VERTICAL;
      else return "the layout is either horizontal or vertical";
      return NULL;
    case K_FONT:
      if (span_eq(val, "default")) {
        s->font[0] = 0;
        return NULL;
      }
      for (size_t i = 0; i < val.n; i++)
        if ((unsigned char)val.p[i] < 0x20) return "the font path has a control character in it";
      if (val.n == 0 || !copy_span(val, s->font, sizeof s->font)) return "the font should be \"default\" or a file path";
      return NULL;
    case K_TEXTSIZE:
      if (!parse_int(val, 8, 48, &v)) return "the text size should be a number from 8 to 48";
      s->textsize = v;
      return NULL;
    case K_THEME:
      if (span_eq(val, "light")) s->dark = false;
      else if (span_eq(val, "dark")) s->dark = true;
      else return "the theme is either light or dark";
      return NULL;
    case K_SCALE:
      if (!parse_int(val, 50, 300, &v)) return "the scale should be a percentage from 50 to 300";
      s->scale = v;
      return NULL;
    case K_CONTRAST:
      if (span_eq(val, "normal")) s->high_contrast = false;
      else if (span_eq(val, "high")) s->high_contrast = true;
      else return "contrast is either normal or high";
      return NULL;
    case K_MOTION:
      if (span_eq(val, "full")) s->reduced_motion = false;
      else if (span_eq(val, "reduced")) s->reduced_motion = true;
      else return "motion is either full or reduced";
      return NULL;
    case K_LASTDAY: {
      ad_day d;
      if (!ad_day_parse(val.p, val.n, &d)) return "the date should look like 2026-11-11";
      s->lastday = d;
      return NULL;
    }
    default: break;
  }
  if (span_starts(key, "hotkey ")) {
    char name[24], spec[120];
    span an = {key.p + 7, key.n - 7};
    ad_action act;
    if (!copy_span(an, name, sizeof name) || !ad_action_from_name(name, &act)) return "that is not a hotkey action Adderall knows";
    if (s->hotkey_set[act]) return "this hotkey appears twice";
    if (!copy_span(val, spec, sizeof spec) || !ad_binding_parse(spec, &s->hotkey[act])) return "the key should look like T, Ctrl+, or Enter | Space";
    s->hotkey_set[act] = true;
    return NULL;
  }
  return "Adderall does not know this setting";
}

/* ---- the parser -------------------------------------------------------- */

typedef enum { SEC_NONE, SEC_SETTINGS, SEC_HABIT } section;

ad_parse_status ad_format_parse(const char *text, size_t len, ad_model *out, ad_parse_error *err) {
  ad_model_init(out);
  if (err) memset(err, 0, sizeof *err);
  if (len == 0) return fail(err, AD_PARSE_DAMAGED, 0, NULL, 0, "The file is empty.");
  if (len > MAX_FILE) return fail(err, AD_PARSE_DAMAGED, 0, NULL, 0, "The file is far larger than an Adderall file should be.");
  if (memchr(text, 0, len)) return fail(err, AD_PARSE_DAMAGED, 0, NULL, 0, "The file contains binary data.");
  if (!ad_utf8_valid(text, len)) return fail(err, AD_PARSE_DAMAGED, 0, NULL, 0, "The file is not valid UTF-8 text.");

  const char *p = text, *end = text + len;
  if (len >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;

  ad_model *m = out;
  section sec = SEC_NONE;
  bool seen_header = false, seen_version = false, seen_settings = false;
  unsigned seen_keys = 0;
  bool have_start = false;
  int lineno = 0;
  ad_habit *cur = NULL;
  ad_parse_status result = AD_PARSE_OK;

  while (p < end) {
    const char *nl = memchr(p, '\n', (size_t)(end - p));
    const char *le = nl ? nl : end;
    const char *next = nl ? nl + 1 : end;
    span line = {p, (size_t)(le - p)};
    p = next;
    lineno++;
    if (line.n > 0 && line.p[line.n - 1] == '\r') line.n--;
    while (line.n > 0 && (line.p[line.n - 1] == ' ' || line.p[line.n - 1] == '\t')) line.n--;
    if (line.n == 0) continue;
    if (line.n > MAX_LINE) {
      result = fail(err, AD_PARSE_LINE, lineno, line.p, line.n, "this line is too long to be right");
      goto bad;
    }
    const char *lp = line.p;
    const size_t ln = line.n;

    if (!seen_header) {
      if (!span_eq(line, "# Adderall")) {
        result = fail(err, AD_PARSE_DAMAGED, lineno, line.p, ln, "This does not look like an Adderall file (it should start with \"# Adderall\").");
        goto bad;
      }
      seen_header = true;
      continue;
    }
    if (!seen_version) {
      int ver;
      if (!span_starts(line, "adderall-format: ") || !parse_int((span){lp + 17, ln - 17}, 0, 99999, &ver)) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "expected the version line, like \"adderall-format: %d\"", AD_FORMAT_VERSION);
        goto bad;
      }
      if (ver > AD_FORMAT_VERSION) {
        result = fail(err, AD_PARSE_NEWER, lineno, lp, ln, "This file was written by a newer Adderall (format %d). Update Adderall to open it.", ver);
        goto bad;
      }
      if (ver < 1) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "the format version should be 1 or higher");
        goto bad;
      }
      seen_version = true;
      continue;
    }

    /* section headings */
    if (span_starts(line, "## ")) {
      if (span_eq(line, "## Settings")) {
        if (seen_settings) {
          result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "there is a second Settings section");
          goto bad;
        }
        seen_settings = true;
        sec = SEC_SETTINGS;
        cur = NULL;
        continue;
      }
      if (span_starts(line, "## Habit: ")) {
        char raw[AD_NAME_MAX_BYTES * 2 + 8], name[AD_NAME_MAX_BYTES + 1];
        span nm = {lp + 10, ln - 10};
        if (nm.n >= sizeof raw) {
          result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "the habit name is too long");
          goto bad;
        }
        copy_span(nm, raw, sizeof raw);
        const ad_name_result nr = ad_name_normalize(raw, name);
        if (nr != AD_NAME_OK) {
          result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "the habit name is not allowed: %s", ad_name_message(nr));
          goto bad;
        }
        for (int i = 0; i < m->n_habits; i++) {
          if (ad_name_equal(m->habits[i].name, name)) {
            result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "another habit already has this name (capitals do not count)");
            goto bad;
          }
        }
        if (m->n_habits >= AD_MAX_HABITS) {
          result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "there are too many habits in this file");
          goto bad;
        }
        const int idx = ad_model_add_raw(m, name, AD_SYM_COUNT, AD_DAY_NONE);
        if (idx < 0) {
          result = fail(err, AD_PARSE_DAMAGED, lineno, NULL, 0, "Out of memory.");
          goto bad;
        }
        cur = &m->habits[idx];
        have_start = false;
        sec = SEC_HABIT;
        continue;
      }
      result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "expected \"## Settings\" or \"## Habit: <name>\"");
      goto bad;
    }

    if (sec == SEC_NONE || !span_starts(line, "- ")) {
      result = fail(err, AD_PARSE_LINE, lineno, lp, ln, sec == SEC_NONE ? "this line is outside any section" : "expected a list item starting with \"- \"");
      goto bad;
    }
    span item = {lp + 2, ln - 2};

    if (sec == SEC_HABIT && item.n >= 4 && item.p[0] == '[' && item.p[2] == ']' && item.p[3] == ' ') {
      const char mark = item.p[1];
      ad_state st;
      if (mark == 'x' || mark == 'X') st = AD_FILLED;
      else if (mark == '/') st = AD_SKIPPED;
      else if (mark == ' ') st = AD_EMPTY;
      else {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "a day is marked [x] (filled), [/] (skipped) or [ ] (empty)");
        goto bad;
      }
      ad_day d;
      if (!ad_day_parse(item.p + 4, item.n - 4, &d)) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "the date should look like 2026-11-11");
        goto bad;
      }
      if (!have_start) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "a habit's \"- start:\" line has to come before its days");
        goto bad;
      }
      if (d < cur->start) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "this day is before the habit's start day");
        goto bad;
      }
      if (ad_habit_get(cur, d) != AD_EMPTY) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "this day is listed twice");
        goto bad;
      }
      if (st != AD_EMPTY && !ad_habit_put(cur, d, st)) {
        result = fail(err, AD_PARSE_DAMAGED, lineno, NULL, 0, "Out of memory.");
        goto bad;
      }
      continue;
    }

    /* "- key: value" */
    const char *colon = memchr(item.p, ':', item.n);
    if (!colon) {
      result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "expected \"- name: value\"");
      goto bad;
    }
    span key = {item.p, (size_t)(colon - item.p)};
    span val = {colon + 1, item.n - key.n - 1};
    if (val.n > 0 && val.p[0] == ' ') {
      val.p++;
      val.n--;
    }
    if (sec == SEC_SETTINGS) {
      const char *why = apply_setting(&m->settings, key, val, &seen_keys);
      if (why) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "%s", why);
        goto bad;
      }
      continue;
    }
    /* habit properties */
    if (span_eq(key, "symbol")) {
      char nm[24];
      ad_symbol sy;
      if (!copy_span(val, nm, sizeof nm) || !ad_symbol_from_name(nm, &sy)) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "that is not a symbol Adderall knows (try star, triangle, hexagon...)");
        goto bad;
      }
      if (cur->symbol != AD_SYM_COUNT) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "this habit already has a symbol");
        goto bad;
      }
      cur->symbol = sy;
    } else if (span_eq(key, "start")) {
      if (have_start) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "this habit already has a start day");
        goto bad;
      }
      if (!ad_day_parse(val.p, val.n, &cur->start)) {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "the date should look like 2026-11-11");
        goto bad;
      }
      have_start = true;
    } else if (span_eq(key, "archived")) {
      if (span_eq(val, "yes")) cur->archived = true;
      else if (span_eq(val, "no")) cur->archived = false;
      else {
        result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "archived is either yes or no");
        goto bad;
      }
    } else {
      result = fail(err, AD_PARSE_LINE, lineno, lp, ln, "Adderall does not know this habit property");
      goto bad;
    }
  }

  if (!seen_header) return fail(err, AD_PARSE_DAMAGED, 0, NULL, 0, "The file has no content.");
  if (!seen_version) {
    result = fail(err, AD_PARSE_LINE, lineno, NULL, 0, "the version line (\"adderall-format: %d\") is missing", AD_FORMAT_VERSION);
    goto bad;
  }

  /* Whole-file checks that need every habit to be read first. */
  {
    int board = 0;
    for (int i = 0; i < m->n_habits; i++) {
      ad_habit *h = &m->habits[i];
      if (h->start == AD_DAY_NONE) {
        result = fail(err, AD_PARSE_LINE, lineno, NULL, 0, "the habit \"%s\" has no \"- start:\" line", h->name);
        goto bad;
      }
      if (!h->archived && ++board > AD_MAX_BOARD) {
        result = fail(err, AD_PARSE_LINE, lineno, NULL, 0, "more than %d habits are on the board; mark the extra ones \"- archived: yes\"", AD_MAX_BOARD);
        goto bad;
      }
    }
    /* Symbols: assign free ones to habits without, and refuse duplicates on the board. */
    bool used[AD_SYM_COUNT] = {false};
    for (int i = 0; i < m->n_habits; i++) {
      ad_habit *h = &m->habits[i];
      if (h->archived || h->symbol == AD_SYM_COUNT) continue;
      if (used[h->symbol]) {
        result = fail(err, AD_PARSE_LINE, lineno, NULL, 0, "two habits on the board share the symbol \"%s\" (\"%s\" is one of them)", ad_symbol_name(h->symbol), h->name);
        goto bad;
      }
      used[h->symbol] = true;
    }
    for (int i = 0; i < m->n_habits; i++) {
      ad_habit *h = &m->habits[i];
      if (h->symbol != AD_SYM_COUNT) continue;
      int s = 0;
      while (s < AD_SYM_COUNT && used[s]) s++;
      h->symbol = (ad_symbol)(s < AD_SYM_COUNT ? s : 0);
      if (!h->archived && s < AD_SYM_COUNT) used[s] = true;
    }
  }
  return AD_PARSE_OK;

bad:
  ad_model_free(out);
  ad_model_init(out);
  return result;
}

/* ---- the writer -------------------------------------------------------- */

typedef struct {
  char *p;
  size_t n, cap;
  bool oom;
} sbuf;

static void put(sbuf *b, const char *fmt, ...) {
  if (b->oom) return;
  va_list ap;
  va_start(ap, fmt);
  char tmp[1400];
  int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if ((size_t)n >= sizeof tmp) n = (int)sizeof tmp - 1;
  if (b->n + (size_t)n + 1 > b->cap) {
    size_t cap = b->cap ? b->cap : 1024;
    while (cap < b->n + (size_t)n + 1) cap *= 2;
    char *np = (char *)realloc(b->p, cap);
    if (!np) {
      b->oom = true;
      return;
    }
    b->p = np;
    b->cap = cap;
  }
  memcpy(b->p + b->n, tmp, (size_t)n);
  b->n += (size_t)n;
  b->p[b->n] = 0;
}

char *ad_format_write(const ad_model *m, size_t *len) {
  sbuf b = {0};
  const ad_settings *s = &m->settings;
  char d[11];
  put(&b, "# Adderall\n\nadderall-format: %d\n\n## Settings\n\n", AD_FORMAT_VERSION);
  if (s->timezone[0]) put(&b, "- timezone: %s\n", s->timezone);
  put(&b, "- daystart: %02d:%02d\n", s->daystart / 60, s->daystart % 60);
  put(&b, "- layout: %s\n", s->layout == AD_LAYOUT_VERTICAL ? "vertical" : "horizontal");
  put(&b, "- font: %s\n", s->font[0] ? s->font : "default");
  put(&b, "- textsize: %d\n", s->textsize);
  put(&b, "- theme: %s\n", s->dark ? "dark" : "light");
  put(&b, "- scale: %d\n", s->scale);
  put(&b, "- contrast: %s\n", s->high_contrast ? "high" : "normal");
  put(&b, "- motion: %s\n", s->reduced_motion ? "reduced" : "full");
  if (s->lastday != AD_DAY_NONE) {
    ad_day_format(s->lastday, d);
    put(&b, "- lastday: %s\n", d);
  }
  for (int a = 0; a < AD_ACT_COUNT; a++) {
    if (!s->hotkey_set[a]) continue;
    char spec[120];
    ad_binding_format(&s->hotkey[a], spec, sizeof spec);
    put(&b, "- hotkey %s: %s\n", ad_action_name((ad_action)a), spec);
  }
  for (int i = 0; i < m->n_habits; i++) {
    const ad_habit *h = &m->habits[i];
    put(&b, "\n## Habit: %s\n\n- symbol: %s\n", h->name, ad_symbol_name(h->symbol));
    ad_day_format(h->start, d);
    put(&b, "- start: %s\n", d);
    if (h->archived) put(&b, "- archived: yes\n");
    if (h->n_marks) put(&b, "\n");
    for (size_t k = 0; k < h->n_marks; k++) {
      ad_day_format(h->marks[k].day, d);
      put(&b, "- [%c] %s\n", h->marks[k].state == AD_FILLED ? 'x' : '/', d);
    }
  }
  if (b.oom) {
    free(b.p);
    return NULL;
  }
  if (len) *len = b.n;
  return b.p;
}
