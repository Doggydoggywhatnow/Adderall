#include "date.h"
#include "test.h"

int main(void) {
  ad_day d;
  char buf[11];

  CHECK_EQ_INT(ad_day_from_ymd(1970, 1, 1), 0);
  CHECK_EQ_INT(ad_day_from_ymd(1970, 1, 2), 1);
  CHECK_EQ_INT(ad_day_from_ymd(1969, 12, 31), -1);
  CHECK_EQ_INT(ad_day_from_ymd(2000, 3, 1) - ad_day_from_ymd(2000, 2, 28), 2); /* leap */
  CHECK_EQ_INT(ad_day_from_ymd(1900, 3, 1) - ad_day_from_ymd(1900, 2, 28), 1); /* not leap */
  CHECK_EQ_INT(ad_day_from_ymd(2026, 11, 11), 20768);

  /* Round trip over a wide range. */
  for (ad_day x = -30000; x < 60000; x += 7) {
    int y, m, dd;
    ad_day_to_ymd(x, &y, &m, &dd);
    CHECK(ad_date_valid(y, m, dd));
    CHECK_EQ_INT(ad_day_from_ymd(y, m, dd), x);
  }

  CHECK(ad_date_valid(2024, 2, 29));
  CHECK(!ad_date_valid(2023, 2, 29));
  CHECK(!ad_date_valid(2026, 13, 1));
  CHECK(!ad_date_valid(2026, 4, 31));
  CHECK(!ad_date_valid(2026, 0, 10));

  /* 2026-11-11 is a Wednesday. */
  CHECK_EQ_INT(ad_day_weekday(ad_day_from_ymd(2026, 11, 11)), 2);
  CHECK_EQ_INT(ad_day_weekday(0), 3); /* 1970-01-01 Thursday */
  CHECK_EQ_INT(ad_day_weekday(-1), 2);

  CHECK(ad_day_parse("2026-11-11", 10, &d));
  CHECK_EQ_INT(d, ad_day_from_ymd(2026, 11, 11));
  CHECK(!ad_day_parse("2026-11-1", 9, &d));
  CHECK(!ad_day_parse("2026-02-30", 10, &d));
  CHECK(!ad_day_parse("2026/11/11", 10, &d));
  CHECK(!ad_day_parse("2026-11-11 ", 11, &d));
  CHECK(!ad_day_parse("20x6-11-11", 10, &d));
  CHECK(!ad_day_parse("1800-01-01", 10, &d));
  CHECK(!ad_day_parse("", 0, &d));

  ad_day_format(ad_day_from_ymd(2026, 1, 5), buf);
  CHECK_EQ_STR(buf, "2026-01-05");
  ad_day_format(ad_day_from_ymd(2099, 12, 31), buf);
  CHECK_EQ_STR(buf, "2099-12-31");

  CHECK_EQ_INT(ad_floor_div(-1, 86400), -1);
  CHECK_EQ_INT(ad_floor_mod(-1, 86400), 86399);
  CHECK_EQ_INT(ad_floor_div(86400, 86400), 1);
  CHECK_EQ_INT(ad_floor_div(7, 2), 3);
  CHECK_EQ_INT(ad_floor_div(-7, 2), -4);

  CHECK_EQ_STR(ad_month_short(11), "Nov");
  CHECK_EQ_STR(ad_weekday_name(2), "Wednesday");
  TEST_DONE();
}
