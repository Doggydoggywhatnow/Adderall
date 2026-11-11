#include "model.h"

#include <stdlib.h>
#include <string.h>

void ad_model_init(ad_model *m) {
  memset(m, 0, sizeof *m);
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
