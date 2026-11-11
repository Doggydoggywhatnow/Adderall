#include "measure.h"
#include "test.h"

#include <stdlib.h>

static const ad_day S = 20000;

/* Build a habit from a picture: x filled, / skipped, . empty. */
static ad_habit make(const char *pic) {
  ad_habit h = {0};
  h.start = S;
  for (int i = 0; pic[i]; i++) {
    if (pic[i] == 'x') ad_habit_put(&h, S + i, AD_FILLED);
    if (pic[i] == '/') ad_habit_put(&h, S + i, AD_SKIPPED);
  }
  return h;
}

int main(void) {
  /* The README's example: 14 cells, 10 filled, 2 skipped, 2 empty,
   * longest streak 6, 71% filled. */
  {
    ad_habit h = make("xxxxxx/.xxxx/.");
    ad_measure m = ad_measure_range(&h, S, S + 13, S + 100);
    CHECK_EQ_INT(m.cells, 14);
    CHECK_EQ_INT(m.filled, 10);
    CHECK_EQ_INT(m.skipped, 2);
    CHECK_EQ_INT(m.empty, 2);
    CHECK_EQ_INT(m.ahead, 0);
    CHECK_EQ_INT(m.longest, 6);
    CHECK_EQ_INT(m.streaks, 2);
    CHECK_EQ_INT(m.percent, 71);
    free(h.marks);
  }
  /* The order of the corners does not matter. */
  {
    ad_habit h = make("xxx.xx");
    ad_measure a = ad_measure_range(&h, S, S + 5, S + 100);
    ad_measure b = ad_measure_range(&h, S + 5, S, S + 100);
    CHECK_EQ_INT(a.cells, b.cells);
    CHECK_EQ_INT(a.longest, b.longest);
    CHECK_EQ_INT(a.longest, 3);
    CHECK_EQ_INT(a.streaks, 2);
    free(h.marks);
  }
  /* A skipped day is not a miss: it doesn't break a streak or add to it. */
  {
    ad_habit h = make("xxx/xxx");
    ad_measure m = ad_measure_range(&h, S, S + 6, S + 100);
    CHECK_EQ_INT(m.longest, 6);
    CHECK_EQ_INT(m.streaks, 1);
    CHECK_EQ_INT(m.skipped, 1);
    CHECK_EQ_INT(m.empty, 0);
    CHECK_EQ_INT(m.percent, 86); /* 6 of 7 */
    free(h.marks);
  }
  /* An empty day ends a streak; grabbing less than the whole line is fine. */
  {
    ad_habit h = make("xx.xxxx.x");
    ad_measure m = ad_measure_range(&h, S + 3, S + 6, S + 100);
    CHECK_EQ_INT(m.cells, 4);
    CHECK_EQ_INT(m.longest, 4);
    CHECK_EQ_INT(m.percent, 100);
    m = ad_measure_range(&h, S, S + 8, S + 100);
    CHECK_EQ_INT(m.longest, 4);
    CHECK_EQ_INT(m.streaks, 3);
    free(h.marks);
  }
  /* Days that haven't happened yet are "ahead", not misses. */
  {
    ad_habit h = make("xxxx......");
    ad_measure m = ad_measure_range(&h, S, S + 9, S + 3); /* today is the 4th day */
    CHECK_EQ_INT(m.cells, 10);
    CHECK_EQ_INT(m.ahead, 6);
    CHECK_EQ_INT(m.empty, 0);
    CHECK_EQ_INT(m.percent, 100);
    /* Today itself, still unmarked, counts as empty so far. */
    ad_measure t = ad_measure_range(&h, S + 4, S + 6, S + 4);
    CHECK_EQ_INT(t.empty, 1);
    CHECK_EQ_INT(t.ahead, 2);
    CHECK_EQ_INT(t.percent, 0);
    /* Only the future: nothing to say about it. */
    ad_measure f = ad_measure_range(&h, S + 7, S + 9, S + 3);
    CHECK_EQ_INT(f.percent, -1);
    CHECK_EQ_INT(f.ahead, 3);
    free(h.marks);
  }
  /* Nothing before the habit's first day is counted. */
  {
    ad_habit h = make("xx");
    ad_measure m = ad_measure_range(&h, S - 10, S + 1, S + 100);
    CHECK_EQ_INT(m.cells, 2);
    ad_measure n = ad_measure_range(&h, S - 10, S - 5, S + 100);
    CHECK_EQ_INT(n.cells, 0);
    CHECK_EQ_INT(n.percent, -1);
    free(h.marks);
  }
  /* One cell. */
  {
    ad_habit h = make("x.");
    ad_measure m = ad_measure_range(&h, S, S, S + 100);
    CHECK_EQ_INT(m.cells, 1);
    CHECK_EQ_INT(m.longest, 1);
    CHECK_EQ_INT(m.percent, 100);
    free(h.marks);
  }
  TEST_DONE();
}
