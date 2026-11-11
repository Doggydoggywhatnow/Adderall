/* Board geometry: where every cell is, and which cell is under the mouse.
 * Pure maths, no SDL, so layout and hit-testing are covered by tests.
 *
 * Both layouts keep one calendar day per column position, shared by every
 * habit, so today's squircles line up across the rows:
 *
 *   horizontal  one line per habit, days run to the right forever.
 *               Scroll left/right.
 *   vertical    the same line, cut into pieces as wide as the window, laid
 *               down one under another like wrapped text; each piece shows
 *               all the habits. Scroll up/down.
 *
 * Each row starts with a fixed symbol cell (the label column). */
#ifndef ADDERALL_BOARD_H
#define ADDERALL_BOARD_H

#include <stdbool.h>

#include "date.h"

typedef enum { AD_LAYOUT_HORIZONTAL = 0, AD_LAYOUT_VERTICAL = 1 } ad_layout;

typedef struct {
  float x, y, w, h;
} ad_rect;

typedef struct {
  ad_layout layout;
  float x, y, w, h;   /* the board's viewport, in window pixels */
  float pitch;        /* one cell plus its gap */
  float head_h;       /* header strip: month names / the caption of a piece */
  int rows;           /* habits on the board */
  ad_day origin;      /* the day shown in the first column */
  ad_day last;        /* the last day that can be scrolled to */
  float scroll;       /* pixels scrolled along the layout's axis */
  /* derived by ad_view_update */
  int per_line;       /* vertical: days per line */
  int chunks;         /* vertical: number of lines of days */
  float chunk_h;      /* vertical: height of one chunk */
} ad_view;

/* How far past today the board can be scrolled (and so planned): 10 years. */
#define AD_HORIZON_DAYS 3650

static inline float ad_rect_right(ad_rect r) { return r.x + r.w; }
static inline float ad_rect_bottom(ad_rect r) { return r.y + r.h; }
static inline bool ad_rect_has(ad_rect r, float px, float py) {
  return px >= r.x && py >= r.y && px < r.x + r.w && py < r.y + r.h;
}

/* Recompute derived fields and clamp the scroll. Call after changing anything. */
void ad_view_update(ad_view *v);

float ad_view_max_scroll(const ad_view *v);

/* The cell for `day` on row `row`, in window pixels (pitch box, unclipped). */
bool ad_view_cell_rect(const ad_view *v, int row, ad_day day, ad_rect *out);

/* The symbol cell of a row. In the vertical layout there is one per chunk. */
bool ad_view_label_rect(const ad_view *v, int row, int chunk, ad_rect *out);

/* Which cell is at this point? False in gaps, headers and the label column. */
bool ad_view_hit_cell(const ad_view *v, float px, float py, int *row, ad_day *day);

/* Which row's symbol cell is at this point? */
bool ad_view_hit_label(const ad_view *v, float px, float py, int *row);

/* Top of a chunk (vertical layout), or of the row block (horizontal, chunk 0). */
float ad_view_chunk_top(const ad_view *v, int chunk);

/* The range of days that might be on screen, for drawing loops. */
void ad_view_visible_days(const ad_view *v, ad_day *first, ad_day *last);

/* Scroll so `day` is on screen (just enough), or centred. */
void ad_view_ensure_visible(ad_view *v, ad_day day);
void ad_view_center_on(ad_view *v, ad_day day);

/* Which chunk and column a day falls in (vertical layout). */
void ad_view_chunk_of(const ad_view *v, ad_day day, int *chunk, int *col);

#endif
