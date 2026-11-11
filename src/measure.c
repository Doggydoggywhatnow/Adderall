#include "measure.h"

ad_measure ad_measure_range(const ad_habit *h, ad_day from, ad_day to, ad_day today) {
  ad_measure m = {0};
  m.percent = -1;
  if (from > to) {
    const ad_day t = from;
    from = to;
    to = t;
  }
  if (from < h->start) from = h->start;
  if (to < from) return m;

  int run = 0;
  for (ad_day d = from; d <= to; d++) {
    m.cells++;
    switch (ad_habit_get(h, d)) {
      case AD_FILLED:
        m.filled++;
        if (run == 0) m.streaks++;
        run++;
        if (run > m.longest) m.longest = run;
        break;
      case AD_SKIPPED:
        m.skipped++; /* bridges: the run carries on past it */
        break;
      default:
        if (d > today) m.ahead++;
        else m.empty++;
        run = 0;
        break;
    }
  }
  const int happened = m.cells - m.ahead;
  if (happened > 0) m.percent = (m.filled * 100 + happened / 2) / happened;
  return m;
}
