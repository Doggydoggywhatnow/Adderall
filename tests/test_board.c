#include "board.h"
#include "test.h"

static ad_view make(ad_layout l, int rows, ad_day origin, ad_day last) {
  ad_view v = {0};
  v.layout = l;
  v.x = 10;
  v.y = 20;
  v.w = 410;   /* label + 8 columns of 50 */
  v.h = 300;
  v.pitch = 50;
  v.head_h = 20;
  v.rows = rows;
  v.origin = origin;
  v.last = last;
  ad_view_update(&v);
  return v;
}

static void check_roundtrip(const ad_view *v) {
  /* Every cell's centre hits that very cell. */
  ad_day a, b;
  ad_view_visible_days(v, &a, &b);
  for (int r = 0; r < v->rows; r++) {
    for (ad_day d = a; d <= b; d++) {
      ad_rect rc;
      CHECK(ad_view_cell_rect(v, r, d, &rc));
      const float cx = rc.x + rc.w / 2, cy = rc.y + rc.h / 2;
      if (cx < v->x + v->pitch || cx >= v->x + v->w || cy < v->y || cy >= v->y + v->h) continue;
      int hr;
      ad_day hd;
      CHECK(ad_view_hit_cell(v, cx, cy, &hr, &hd));
      CHECK_EQ_INT(hr, r);
      CHECK_EQ_INT(hd, d);
    }
  }
}

int main(void) {
  const ad_day o = 1000;

  /* ---- horizontal ---- */
  ad_view h = make(AD_LAYOUT_HORIZONTAL, 3, o, o + 100);
  ad_rect r;
  CHECK(ad_view_cell_rect(&h, 0, o, &r));
  CHECK_EQ_INT((int)r.x, 60);               /* after the label column */
  CHECK_EQ_INT((int)r.y, 40);               /* below the header */
  CHECK(ad_view_cell_rect(&h, 2, o + 3, &r));
  CHECK_EQ_INT((int)r.x, 210);
  CHECK_EQ_INT((int)r.y, 140);
  CHECK(!ad_view_cell_rect(&h, 3, o, &r));  /* no such row */
  CHECK(!ad_view_cell_rect(&h, 0, o - 1, &r));
  CHECK(!ad_view_cell_rect(&h, 0, o + 101, &r));
  check_roundtrip(&h);

  /* The label column and header are not cells. */
  int row;
  ad_day day;
  CHECK(!ad_view_hit_cell(&h, 30, 50, &row, &day));
  CHECK(!ad_view_hit_cell(&h, 100, 25, &row, &day));
  CHECK(ad_view_hit_label(&h, 30, 50, &row));
  CHECK_EQ_INT(row, 0);
  CHECK(ad_view_hit_label(&h, 30, 100, &row));
  CHECK_EQ_INT(row, 1);
  CHECK(!ad_view_hit_label(&h, 100, 100, &row));

  /* Scrolling: clamps, and moves cells left. */
  h.scroll = -50;
  ad_view_update(&h);
  CHECK_EQ_INT((int)h.scroll, 0);
  h.scroll = 1e9f;
  ad_view_update(&h);
  CHECK_EQ_INT((int)h.scroll, (int)ad_view_max_scroll(&h));
  h.scroll = 100;
  ad_view_update(&h);
  CHECK(ad_view_cell_rect(&h, 0, o + 2, &r));
  CHECK_EQ_INT((int)r.x, 60);
  check_roundtrip(&h);

  /* Centering and ensuring visibility. */
  ad_view_center_on(&h, o + 50);
  CHECK(ad_view_cell_rect(&h, 0, o + 50, &r));
  CHECK(r.x > 60 && r.x + r.w < 420);
  h.scroll = 0;
  ad_view_ensure_visible(&h, o + 60);
  CHECK(ad_view_cell_rect(&h, 0, o + 60, &r));
  CHECK(r.x >= 60 && r.x + r.w <= 420);
  ad_view_ensure_visible(&h, o);
  CHECK(ad_view_cell_rect(&h, 0, o, &r));
  CHECK(r.x >= 60);

  /* Few days: nothing to scroll. */
  ad_view tiny = make(AD_LAYOUT_HORIZONTAL, 1, o, o + 2);
  CHECK_EQ_INT((int)ad_view_max_scroll(&tiny), 0);

  /* ---- vertical ---- */
  ad_view v = make(AD_LAYOUT_VERTICAL, 2, o, o + 100);
  CHECK_EQ_INT(v.per_line, 7);              /* (410 - 50) / 50 = 7.2 */
  CHECK_EQ_INT(v.chunks, 15);               /* ceil(101 / 7) */
  CHECK(ad_view_cell_rect(&v, 0, o, &r));
  CHECK_EQ_INT((int)r.x, 60);
  CHECK_EQ_INT((int)r.y, 40);
  CHECK(ad_view_cell_rect(&v, 0, o + 7, &r)); /* wraps to the second line */
  CHECK_EQ_INT((int)r.x, 60);
  CHECK(r.y > 40 + v.pitch * 2);
  CHECK(ad_view_cell_rect(&v, 1, o + 8, &r));
  CHECK_EQ_INT((int)r.x, 110);
  check_roundtrip(&v);
  CHECK(ad_view_hit_label(&v, 30, 45, &row));
  CHECK_EQ_INT(row, 0);
  /* The same day, one chunk later, still has its label. */
  ad_rect lr;
  v.scroll = 2 * v.chunk_h;
  ad_view_update(&v);
  CHECK(ad_view_label_rect(&v, 1, 2, &lr));
  CHECK(ad_view_hit_label(&v, lr.x + 5, lr.y + 5, &row));
  CHECK_EQ_INT(row, 1);
  /* Cells past the end of a wrapped line are not cells. */
  CHECK(!ad_view_hit_cell(&v, v.x + v.pitch + 7 * v.pitch + 5, 50, &row, &day));

  v.scroll = 3 * v.chunk_h;
  ad_view_update(&v);
  CHECK(ad_view_cell_rect(&v, 0, o + 21, &r));
  CHECK_EQ_INT((int)r.y, (int)(v.y + v.head_h));
  check_roundtrip(&v);
  ad_view_center_on(&v, o + 50);
  int chunk, col;
  ad_view_chunk_of(&v, o + 50, &chunk, &col);
  CHECK_EQ_INT(chunk, 7);
  CHECK_EQ_INT(col, 1);
  CHECK(ad_view_cell_rect(&v, 0, o + 50, &r));
  CHECK(r.y >= v.y && r.y + v.chunk_h <= v.y + v.h + v.chunk_h);
  ad_view_ensure_visible(&v, o);
  CHECK_EQ_INT((int)v.scroll, 0);

  /* Narrow window: still one column per line. */
  ad_view n = make(AD_LAYOUT_VERTICAL, 1, o, o + 10);
  n.w = 60;
  ad_view_update(&n);
  CHECK_EQ_INT(n.per_line, 1);

  /* No habits: no cells at all. */
  ad_view e = make(AD_LAYOUT_HORIZONTAL, 0, o, o + 10);
  CHECK(!ad_view_hit_cell(&e, 100, 50, &row, &day));
  TEST_DONE();
}
