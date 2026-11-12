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

/* ---- the rules ------------------------------------------------------------ */

const char *ad_result_message(ad_result r) {
  switch (r) {
    case AD_OK: return "";
    case AD_ERR_SEALED: return "That day is over, so it is sealed.";
    case AD_ERR_BEFORE_START: return "That is before the habit started.";
    case AD_ERR_ARCHIVED: return "Archived habits can't be changed. Restore it first.";
    case AD_ERR_NO_HABIT: return "There is no such habit.";
    case AD_ERR_RANGE: return "That is too far ahead to plan.";
  }
  return "";
}

bool ad_model_is_sealed(const ad_model *m, ad_day day) { return day < m->today; }

ad_result ad_model_set_day(ad_model *m, int habit, ad_day day, ad_state s) {
  if (habit < 0 || habit >= m->n_habits) return AD_ERR_NO_HABIT;
  ad_habit *h = &m->habits[habit];
  if (h->archived) return AD_ERR_ARCHIVED;
  if (day < h->start) return AD_ERR_BEFORE_START;
  if (ad_model_is_sealed(m, day) && !h->setup) return AD_ERR_SEALED;
  if (day > m->today + AD_HORIZON_DAYS) return AD_ERR_RANGE;
  if (ad_habit_get(h, day) == s) return AD_OK;
  return ad_habit_put(h, day, s) ? AD_OK : AD_ERR_RANGE;
}

ad_result ad_model_toggle_fill(ad_model *m, int habit, ad_day day) {
  if (habit < 0 || habit >= m->n_habits) return AD_ERR_NO_HABIT;
  const ad_state cur = ad_habit_get(&m->habits[habit], day);
  return ad_model_set_day(m, habit, day, cur == AD_FILLED ? AD_EMPTY : AD_FILLED);
}

ad_result ad_model_toggle_skip(ad_model *m, int habit, ad_day day) {
  if (habit < 0 || habit >= m->n_habits) return AD_ERR_NO_HABIT;
  const ad_state cur = ad_habit_get(&m->habits[habit], day);
  return ad_model_set_day(m, habit, day, cur == AD_SKIPPED ? AD_EMPTY : AD_SKIPPED);
}

bool ad_model_set_clock(ad_model *m, ad_day clock_day) {
  ad_day high = m->settings.lastday;
  bool changed = false;
  if (high != AD_DAY_NONE && high > clock_day + 2) {
    high = AD_DAY_NONE; /* the clock must have been wrong: trust it now */
    changed = true;
  }
  const ad_day today = (high != AD_DAY_NONE && high > clock_day) ? high : clock_day;
  m->today = today;
  if (m->settings.lastday != today) {
    m->settings.lastday = today;
    changed = true;
  }
  return changed;
}

/* ---- managing habits ------------------------------------------------------ */

const char *ad_habit_result_message(ad_habit_result r, ad_name_result nr) {
  switch (r) {
    case AD_HABIT_OK: return "";
    case AD_HABIT_BAD_NAME: return ad_name_message(nr);
    case AD_HABIT_NAME_TAKEN: return "Another habit already has that name.";
    case AD_HABIT_BOARD_FULL: return "The board holds 8 habits. Archive or delete one first.";
    case AD_HABIT_TOO_MANY: return "That is more habits than Adderall can keep.";
    case AD_HABIT_NOT_FOUND: return "There is no such habit.";
    case AD_HABIT_SYMBOL_TAKEN: return "Another habit on the board already uses that symbol.";
    case AD_HABIT_ALREADY: return "That habit is already like that.";
    case AD_HABIT_BAD_DATE: return "A habit can't start in the future, or more than 10 years back.";
  }
  return "";
}

int ad_model_board_count(const ad_model *m) {
  int n = 0;
  for (int i = 0; i < m->n_habits; i++)
    if (!m->habits[i].archived) n++;
  return n;
}

int ad_model_board_habit(const ad_model *m, int n) {
  for (int i = 0; i < m->n_habits; i++) {
    if (m->habits[i].archived) continue;
    if (n-- == 0) return i;
  }
  return -1;
}

static bool symbol_used_on_board(const ad_model *m, ad_symbol s, int except) {
  for (int i = 0; i < m->n_habits; i++)
    if (i != except && !m->habits[i].archived && m->habits[i].symbol == s) return true;
  return false;
}

ad_symbol ad_model_next_symbol(const ad_model *m) {
  for (int s = 0; s < AD_SYM_COUNT; s++)
    if (!symbol_used_on_board(m, (ad_symbol)s, -1)) return (ad_symbol)s;
  return AD_SYM_STAR;
}

static bool name_taken(const ad_model *m, const char *name, int except) {
  for (int i = 0; i < m->n_habits; i++)
    if (i != except && ad_name_equal(m->habits[i].name, name)) return true;
  return false;
}

ad_habit_result ad_model_create_from(ad_model *m, const char *name, ad_day start, int *index_out, ad_name_result *nr) {
  char clean[AD_NAME_MAX_BYTES + 1];
  const ad_name_result r = ad_name_normalize(name, clean);
  if (nr) *nr = r;
  if (r != AD_NAME_OK) return AD_HABIT_BAD_NAME;
  if (start > m->today || start < m->today - AD_BACKFILL_MAX_DAYS) return AD_HABIT_BAD_DATE;
  if (name_taken(m, clean, -1)) return AD_HABIT_NAME_TAKEN;
  if (ad_model_board_count(m) >= AD_MAX_BOARD) return AD_HABIT_BOARD_FULL;
  if (m->n_habits >= AD_MAX_HABITS) return AD_HABIT_TOO_MANY;
  const int i = ad_model_add_raw(m, clean, ad_model_next_symbol(m), start);
  if (i < 0) return AD_HABIT_TOO_MANY;
  m->habits[i].setup = start < m->today;
  if (index_out) *index_out = i;
  return AD_HABIT_OK;
}

ad_habit_result ad_model_create(ad_model *m, const char *name, int *index_out, ad_name_result *nr) {
  return ad_model_create_from(m, name, m->today, index_out, nr);
}

void ad_model_finish_setup(ad_model *m) {
  for (int i = 0; i < m->n_habits; i++) m->habits[i].setup = false;
}

bool ad_model_in_setup(const ad_model *m) {
  for (int i = 0; i < m->n_habits; i++)
    if (m->habits[i].setup) return true;
  return false;
}

ad_habit_result ad_model_rename(ad_model *m, int habit, const char *name, ad_name_result *nr) {
  if (habit < 0 || habit >= m->n_habits) return AD_HABIT_NOT_FOUND;
  char clean[AD_NAME_MAX_BYTES + 1];
  const ad_name_result r = ad_name_normalize(name, clean);
  if (nr) *nr = r;
  if (r != AD_NAME_OK) return AD_HABIT_BAD_NAME;
  if (name_taken(m, clean, habit)) return AD_HABIT_NAME_TAKEN;
  strcpy(m->habits[habit].name, clean);
  return AD_HABIT_OK;
}

ad_habit_result ad_model_set_symbol(ad_model *m, int habit, ad_symbol sym) {
  if (habit < 0 || habit >= m->n_habits) return AD_HABIT_NOT_FOUND;
  if ((int)sym < 0 || sym >= AD_SYM_COUNT) return AD_HABIT_NOT_FOUND;
  ad_habit *h = &m->habits[habit];
  if (!h->archived && symbol_used_on_board(m, sym, habit)) return AD_HABIT_SYMBOL_TAKEN;
  h->symbol = sym;
  return AD_HABIT_OK;
}

ad_habit_result ad_model_archive(ad_model *m, int habit) {
  if (habit < 0 || habit >= m->n_habits) return AD_HABIT_NOT_FOUND;
  if (m->habits[habit].archived) return AD_HABIT_ALREADY;
  m->habits[habit].archived = true;
  return AD_HABIT_OK;
}

ad_habit_result ad_model_restore(ad_model *m, int habit, int *new_index) {
  if (habit < 0 || habit >= m->n_habits) return AD_HABIT_NOT_FOUND;
  if (!m->habits[habit].archived) return AD_HABIT_ALREADY;
  if (ad_model_board_count(m) >= AD_MAX_BOARD) return AD_HABIT_BOARD_FULL;
  ad_habit h = m->habits[habit];
  memmove(&m->habits[habit], &m->habits[habit + 1], (size_t)(m->n_habits - habit - 1) * sizeof(ad_habit));
  m->habits[m->n_habits - 1] = h;
  ad_habit *r = &m->habits[m->n_habits - 1];
  r->archived = false;
  if (symbol_used_on_board(m, r->symbol, m->n_habits - 1)) r->symbol = ad_model_next_symbol(m);
  if (new_index) *new_index = m->n_habits - 1;
  return AD_HABIT_OK;
}

ad_habit_result ad_model_delete(ad_model *m, int habit) {
  if (habit < 0 || habit >= m->n_habits) return AD_HABIT_NOT_FOUND;
  free(m->habits[habit].marks);
  memmove(&m->habits[habit], &m->habits[habit + 1], (size_t)(m->n_habits - habit - 1) * sizeof(ad_habit));
  m->n_habits--;
  return AD_HABIT_OK;
}
