/* The habits and their marked days. This is the data the whole program is
 * about; the rules that protect it (sealing, names, limits) live here, not in
 * the interface. */
#ifndef ADDERALL_MODEL_H
#define ADDERALL_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "date.h"
#include "raster.h"

typedef enum { AD_EMPTY = 0, AD_FILLED = 1, AD_SKIPPED = 2 } ad_state;

typedef struct {
  ad_day day;
  uint8_t state; /* AD_FILLED or AD_SKIPPED; empty days are simply not stored */
} ad_mark;

#define AD_NAME_MAX_CHARS 40
#define AD_NAME_MAX_BYTES (AD_NAME_MAX_CHARS * 4)

typedef struct {
  char name[AD_NAME_MAX_BYTES + 1];
  ad_symbol symbol;
  ad_day start;      /* the habit's first day */
  bool archived;
  ad_mark *marks;    /* sorted by day */
  size_t n_marks, cap_marks;
} ad_habit;

typedef struct {
  ad_habit *habits;
  int n_habits, cap_habits;
  ad_day today;      /* the current day; days before it are sealed */
} ad_model;

void ad_model_init(ad_model *m);
void ad_model_free(ad_model *m);

/* The state of one day of one habit (AD_EMPTY if nothing is recorded). */
ad_state ad_habit_get(const ad_habit *h, ad_day day);

/* Store a state, no rules applied. Used by the parser and by tests. */
bool ad_habit_put(ad_habit *h, ad_day day, ad_state s);

/* Add a habit at the end. Returns its index, or -1 if out of memory. */
int ad_model_add_raw(ad_model *m, const char *name, ad_symbol sym, ad_day start);

#endif
