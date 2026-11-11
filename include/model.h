/* The habits and their marked days. This is the data the whole program is
 * about; the rules that protect it (sealing, names, limits) live here, not in
 * the interface. */
#ifndef ADDERALL_MODEL_H
#define ADDERALL_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "date.h"
#include "hotkeys.h"
#include "platform.h"
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

/* The board holds up to 8 habits; archived ones live in the file only. */
#define AD_MAX_BOARD 8
#define AD_MAX_HABITS 128

typedef struct {
  char timezone[64];      /* IANA name; "" follows the computer's time zone */
  int daystart;           /* when a new day begins, minutes after midnight (0..719) */
  ad_layout layout;       /* board-wide */
  char font[AD_PATH_MAX]; /* path to a .ttf/.otf; "" is the bundled font */
  int textsize;           /* 8..48 */
  bool dark;              /* white on black */
  int scale;              /* interface scale in percent, 50..300 */
  bool high_contrast;
  bool reduced_motion;
  ad_day lastday;         /* the latest "today" ever seen; AD_DAY_NONE if none yet */
  ad_binding hotkey[AD_ACT_COUNT]; /* only meaningful where hotkey_set is true */
  bool hotkey_set[AD_ACT_COUNT];
} ad_settings;

#define AD_DAYSTART_MAX 719 /* 11:59 */

typedef struct {
  ad_habit *habits;
  int n_habits, cap_habits;
  ad_settings settings;
  ad_day today;      /* the current day; days before it are sealed */
} ad_model;

void ad_settings_defaults(ad_settings *s);

void ad_model_init(ad_model *m);
void ad_model_free(ad_model *m);

/* The state of one day of one habit (AD_EMPTY if nothing is recorded). */
ad_state ad_habit_get(const ad_habit *h, ad_day day);

/* Store a state, no rules applied. Used by the parser and by tests. */
bool ad_habit_put(ad_habit *h, ad_day day, ad_state s);

/* ---- names ------------------------------------------------------------- */

typedef enum {
  AD_NAME_OK,
  AD_NAME_EMPTY,
  AD_NAME_TOO_LONG,
  AD_NAME_BAD_CHAR, /* a control character, a line break, or broken UTF-8 */
} ad_name_result;

/* Trim leading and trailing spaces and check the rules: a single line of at
 * most 40 characters. `out` must hold AD_NAME_MAX_BYTES + 1 bytes. */
ad_name_result ad_name_normalize(const char *in, char *out);
const char *ad_name_message(ad_name_result r);

/* Names are the same if they differ only by capitals. */
bool ad_name_equal(const char *a, const char *b);

/* Strict UTF-8 check (no overlong forms, no surrogates, nothing above U+10FFFF). */
bool ad_utf8_valid(const char *s, size_t len);

/* Add a habit at the end. Returns its index, or -1 if out of memory. */
int ad_model_add_raw(ad_model *m, const char *name, ad_symbol sym, ad_day start);

#endif
