/* The measure tool's arithmetic. The person drags across some cells of one
 * habit; this says what they grabbed. Streaks only ever come from here. */
#ifndef ADDERALL_MEASURE_H
#define ADDERALL_MEASURE_H

#include "model.h"

typedef struct {
  int cells;     /* days grabbed (days before the habit started are not counted) */
  int filled;
  int skipped;
  int empty;     /* unmarked days up to and including today */
  int ahead;     /* unmarked days after today: they haven't happened, so they aren't misses */
  int streaks;   /* runs of filled days */
  int longest;   /* the longest of them */
  int percent;   /* filled out of the days that have happened, rounded; -1 if none have */
} ad_measure;

/* Measure `from`..`to` (either order, inclusive) of a habit.
 *
 * A streak is a run of filled days. A skipped day is not a miss, so it
 * neither counts toward a streak nor breaks one; an empty day on or before
 * today ends it. */
ad_measure ad_measure_range(const ad_habit *h, ad_day from, ad_day to, ad_day today);

#endif
