#include "model.h"
#include "test.h"
#include "platform.h"
#include "tz.h"

#include <stdlib.h>
#include <string.h>

static int64_t utc(int y, int m, int d, int hh, int mm, int ss) {
  return (int64_t)ad_day_from_ymd(y, m, d) * 86400 + hh * 3600 + mm * 60 + ss;
}

static ad_tz *zone(const char *name) {
  ad_tz *tz = ad_tz_load(ADDERALL_TEST_DATA_DIR "/zoneinfo", name);
  if (!tz) printf("  could not load zone %s\n", name);
  CHECK(tz != NULL);
  return tz;
}

static int off(const ad_tz *tz, int64_t t) { return ad_tz_offset(tz, t, NULL); }

static void test_real_zones(void) {
  ad_tz *ny = zone("America/New_York");
  CHECK_EQ_INT(off(ny, utc(2026, 1, 15, 12, 0, 0)), -5 * 3600);
  CHECK_EQ_INT(off(ny, utc(2026, 7, 15, 12, 0, 0)), -4 * 3600);
  /* Spring forward: 2026-03-08 02:00 EST == 07:00 UTC. */
  CHECK_EQ_INT(off(ny, utc(2026, 3, 8, 6, 59, 59)), -5 * 3600);
  CHECK_EQ_INT(off(ny, utc(2026, 3, 8, 7, 0, 0)), -4 * 3600);
  /* Fall back: 2026-11-01 02:00 EDT == 06:00 UTC. */
  CHECK_EQ_INT(off(ny, utc(2026, 11, 1, 5, 59, 59)), -4 * 3600);
  CHECK_EQ_INT(off(ny, utc(2026, 11, 1, 6, 0, 0)), -5 * 3600);
  bool dst;
  ad_tz_offset(ny, utc(2026, 7, 1, 0, 0, 0), &dst);
  CHECK(dst);
  ad_tz_offset(ny, utc(2026, 1, 1, 0, 0, 0), &dst);
  CHECK(!dst);
  /* Long before and long after the table: the old and the footer rules. */
  CHECK_EQ_INT(off(ny, utc(1950, 1, 15, 12, 0, 0)), -5 * 3600);
  CHECK_EQ_INT(off(ny, utc(2090, 7, 15, 12, 0, 0)), -4 * 3600);
  CHECK_EQ_INT(off(ny, utc(2090, 1, 15, 12, 0, 0)), -5 * 3600);
  /* 2090: second Sunday of March is the 12th. */
  CHECK_EQ_INT(off(ny, utc(2090, 3, 12, 6, 59, 59)), -5 * 3600);
  CHECK_EQ_INT(off(ny, utc(2090, 3, 12, 7, 0, 0)), -4 * 3600);
  ad_tz_free(ny);

  ad_tz *syd = zone("Australia/Sydney"); /* southern hemisphere */
  CHECK_EQ_INT(off(syd, utc(2026, 1, 15, 0, 0, 0)), 11 * 3600);
  CHECK_EQ_INT(off(syd, utc(2026, 7, 15, 0, 0, 0)), 10 * 3600);
  CHECK_EQ_INT(off(syd, utc(2090, 1, 15, 0, 0, 0)), 11 * 3600);
  CHECK_EQ_INT(off(syd, utc(2090, 7, 15, 0, 0, 0)), 10 * 3600);
  ad_tz_free(syd);

  ad_tz *kol = zone("Asia/Kolkata"); /* half-hour offset, no DST */
  CHECK_EQ_INT(off(kol, utc(2026, 7, 15, 0, 0, 0)), 5 * 3600 + 1800);
  ad_tz_free(kol);
  ad_tz *lh = zone("Australia/Lord_Howe"); /* 30-minute DST */
  CHECK_EQ_INT(off(lh, utc(2026, 1, 15, 0, 0, 0)), 11 * 3600);
  CHECK_EQ_INT(off(lh, utc(2026, 7, 15, 0, 0, 0)), 10 * 3600 + 1800);
  ad_tz_free(lh);
  ad_tz *lon = zone("Europe/London");
  CHECK_EQ_INT(off(lon, utc(2026, 7, 15, 0, 0, 0)), 3600);
  CHECK_EQ_INT(off(lon, utc(2026, 1, 15, 0, 0, 0)), 0);
  ad_tz_free(lon);
  ad_tz *utcz = zone("UTC");
  CHECK_EQ_INT(off(utcz, utc(2026, 7, 15, 0, 0, 0)), 0);
  ad_tz_free(utcz);

  CHECK(ad_tz_load(ADDERALL_TEST_DATA_DIR "/zoneinfo", "Mars/Olympus") == NULL);
  CHECK(ad_tz_load(ADDERALL_TEST_DATA_DIR "/zoneinfo", "../zoneinfo/UTC") == NULL);
  CHECK(ad_tz_load(ADDERALL_TEST_DATA_DIR "/zoneinfo", "/etc/passwd") == NULL);
  CHECK(ad_tz_load(ADDERALL_TEST_DATA_DIR "/zoneinfo", "") == NULL);
  CHECK(ad_tz_from_bytes((const uint8_t *)"nope", 4) == NULL);
}

/* A minimal TZif v2 file with no transitions and only a footer rule. */
static ad_tz *footer_only(const char *rule) {
  uint8_t buf[512];
  size_t n = 0;
  memset(buf, 0, sizeof buf);
  for (int block = 0; block < 2; block++) {
    memcpy(buf + n, "TZif2", 5);
    n += 20;               /* magic, version, reserved */
    n += 12;               /* isut, isstd, leap counts = 0 */
    buf[n + 3] = 0;        /* timecnt 0 */
    n += 4;
    buf[n + 3] = 1;        /* typecnt 1 */
    n += 4;
    buf[n + 3] = 4;        /* charcnt 4 */
    n += 4;
    n += 6;                /* one ttinfo: offset 0, std, abbr 0 */
    memcpy(buf + n, "UTC\0", 4);
    n += 4;
  }
  buf[n++] = '\n';
  memcpy(buf + n, rule, strlen(rule));
  n += strlen(rule);
  buf[n++] = '\n';
  return ad_tz_from_bytes(buf, n);
}

static void test_posix_rules(void) {
  ad_tz *t = footer_only("EST5EDT,M3.2.0,M11.1.0");
  CHECK(t != NULL);
  CHECK_EQ_INT(off(t, utc(2026, 7, 1, 0, 0, 0)), -4 * 3600);
  CHECK_EQ_INT(off(t, utc(2026, 12, 1, 0, 0, 0)), -5 * 3600);
  ad_tz_free(t);

  t = footer_only("<+0330>-3:30");
  CHECK(t != NULL);
  CHECK_EQ_INT(off(t, utc(2026, 7, 1, 0, 0, 0)), 3 * 3600 + 1800);
  ad_tz_free(t);

  /* southern: DST from the first Sunday of October at 02:00 to the first Sunday of April at 03:00 */
  t = footer_only("AEST-10AEDT,M10.1.0,M4.1.0/3");
  CHECK(t != NULL);
  CHECK_EQ_INT(off(t, utc(2026, 1, 1, 0, 0, 0)), 11 * 3600);
  CHECK_EQ_INT(off(t, utc(2026, 6, 1, 0, 0, 0)), 10 * 3600);
  CHECK_EQ_INT(off(t, utc(2026, 12, 1, 0, 0, 0)), 11 * 3600);
  ad_tz_free(t);

  /* last Sunday: week 5 means "the last one". CET: 2026-03-29 01:00 UTC. */
  t = footer_only("CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK_EQ_INT(off(t, utc(2026, 3, 29, 0, 59, 59)), 3600);
  CHECK_EQ_INT(off(t, utc(2026, 3, 29, 1, 0, 0)), 7200);
  CHECK_EQ_INT(off(t, utc(2026, 10, 25, 0, 59, 59)), 7200);
  CHECK_EQ_INT(off(t, utc(2026, 10, 25, 1, 0, 0)), 3600);
  ad_tz_free(t);

  /* Julian-day and zero-based-day rules, and a time of day past 24h. */
  t = footer_only("XXX3YYY,J100/0,200/26");
  CHECK(t != NULL);
  CHECK_EQ_INT(off(t, utc(2026, 3, 1, 0, 0, 0)), -3 * 3600);  /* before day 100 */
  CHECK_EQ_INT(off(t, utc(2026, 6, 1, 0, 0, 0)), -2 * 3600);  /* between */
  CHECK_EQ_INT(off(t, utc(2026, 12, 1, 0, 0, 0)), -3 * 3600); /* after day 200? no: 200 is in July */
  ad_tz_free(t);

  CHECK(footer_only("garbage") != NULL); /* an unreadable footer is ignored, not fatal */
}

static void test_days(void) {
  ad_tz *ny = zone("America/New_York");
  /* Midnight rollover, New York in winter (UTC-5): local midnight is 05:00 UTC. */
  const ad_day nov10 = ad_day_from_ymd(2026, 12, 10);
  CHECK_EQ_INT(ad_tz_day(ny, utc(2026, 12, 10, 4, 59, 59), 0), nov10 - 1);
  CHECK_EQ_INT(ad_tz_day(ny, utc(2026, 12, 10, 5, 0, 0), 0), nov10);
  /* A night owl's day starts at 04:00: until then it is still "yesterday". */
  CHECK_EQ_INT(ad_tz_day(ny, utc(2026, 12, 10, 8, 59, 59), 240), nov10 - 1);
  CHECK_EQ_INT(ad_tz_day(ny, utc(2026, 12, 10, 9, 0, 0), 240), nov10);
  /* Daylight saving: the same wall-clock rule holds in summer (UTC-4). */
  const ad_day jul = ad_day_from_ymd(2026, 7, 10);
  CHECK_EQ_INT(ad_tz_day(ny, utc(2026, 7, 10, 7, 59, 59), 240), jul - 1);
  CHECK_EQ_INT(ad_tz_day(ny, utc(2026, 7, 10, 8, 0, 0), 240), jul);
  /* The day of the spring-forward change still has one cell. */
  CHECK_EQ_INT(ad_tz_day(ny, utc(2026, 3, 8, 4, 59, 59), 0), ad_day_from_ymd(2026, 3, 7));
  CHECK_EQ_INT(ad_tz_day(ny, utc(2026, 3, 8, 5, 0, 0), 0), ad_day_from_ymd(2026, 3, 8));
  CHECK_EQ_INT(ad_tz_day(ny, utc(2026, 3, 9, 3, 59, 59), 0), ad_day_from_ymd(2026, 3, 8));
  CHECK_EQ_INT(ad_tz_day(ny, utc(2026, 3, 9, 4, 0, 0), 0), ad_day_from_ymd(2026, 3, 9));

  CHECK_EQ_INT(ad_tz_seconds_to_next_day(ny, utc(2026, 12, 10, 4, 59, 59), 0), 1);
  CHECK_EQ_INT(ad_tz_seconds_to_next_day(ny, utc(2026, 12, 10, 5, 0, 0), 0), 86400);
  CHECK_EQ_INT(ad_tz_seconds_to_next_day(ny, utc(2026, 12, 10, 8, 0, 0), 240), 3600);
  ad_tz_free(ny);

  /* The same instant is a different day on the other side of the world. */
  ad_tz *tokyo = zone("Asia/Tokyo");
  ad_tz *la = zone("America/Los_Angeles");
  const int64_t t = utc(2026, 11, 11, 20, 0, 0);
  CHECK_EQ_INT(ad_tz_day(tokyo, t, 0), ad_day_from_ymd(2026, 11, 12));
  CHECK_EQ_INT(ad_tz_day(la, t, 0), ad_day_from_ymd(2026, 11, 11));
  ad_tz_free(tokyo);
  ad_tz_free(la);
}

static ad_model fresh(ad_day today) {
  ad_model m;
  ad_model_init(&m);
  ad_model_set_clock(&m, today);
  ad_model_add_raw(&m, "Water", AD_SYM_STAR, today - 10);
  return m;
}

static void test_sealing(void) {
  const ad_day d0 = ad_day_from_ymd(2026, 11, 11);
  ad_model m = fresh(d0);

  /* Today and the future can be filled and skipped; the past can't. */
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0, AD_FILLED), AD_OK);
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0 + 1, AD_SKIPPED), AD_OK);
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0 + 400, AD_FILLED), AD_OK);
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0 - 1, AD_FILLED), AD_ERR_SEALED);
  CHECK_EQ_INT(ad_model_toggle_fill(&m, 0, d0 - 3), AD_ERR_SEALED);
  CHECK_EQ_INT(ad_model_toggle_skip(&m, 0, d0 - 3), AD_ERR_SEALED);
  CHECK_EQ_INT(ad_habit_get(&m.habits[0], d0 - 1), AD_EMPTY);
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0 + 99999, AD_FILLED), AD_ERR_RANGE);
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0 - 11, AD_FILLED), AD_ERR_BEFORE_START); /* before start wins? sealed too */
  CHECK_EQ_INT(ad_model_set_day(&m, 3, d0, AD_FILLED), AD_ERR_NO_HABIT);

  /* Toggle semantics. */
  CHECK_EQ_INT(ad_model_toggle_fill(&m, 0, d0), AD_OK); /* filled -> empty */
  CHECK_EQ_INT(ad_habit_get(&m.habits[0], d0), AD_EMPTY);
  ad_model_toggle_fill(&m, 0, d0);
  CHECK_EQ_INT(ad_habit_get(&m.habits[0], d0), AD_FILLED);
  ad_model_toggle_skip(&m, 0, d0); /* filled -> skipped */
  CHECK_EQ_INT(ad_habit_get(&m.habits[0], d0), AD_SKIPPED);
  ad_model_toggle_skip(&m, 0, d0); /* skipped -> empty */
  CHECK_EQ_INT(ad_habit_get(&m.habits[0], d0), AD_EMPTY);

  /* Fill tomorrow, then let the day roll over: tomorrow is today, today is sealed. */
  ad_model_set_day(&m, 0, d0, AD_FILLED);
  ad_model_set_day(&m, 0, d0 + 1, AD_FILLED);
  CHECK(ad_model_set_clock(&m, d0 + 1));
  CHECK_EQ_INT(m.today, d0 + 1);
  CHECK(ad_model_is_sealed(&m, d0));
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0, AD_EMPTY), AD_ERR_SEALED);       /* can't un-fill yesterday */
  CHECK_EQ_INT(ad_habit_get(&m.habits[0], d0), AD_FILLED);
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0 + 1, AD_EMPTY), AD_OK);           /* the new today is open */
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0 + 2, AD_FILLED), AD_OK);

  /* A day that was never touched stays empty and sealed: no quiet rewriting. */
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0 - 2, AD_FILLED), AD_ERR_SEALED);

  /* Archived habits are read-only. */
  m.habits[0].archived = true;
  CHECK_EQ_INT(ad_model_set_day(&m, 0, d0 + 2, AD_EMPTY), AD_ERR_ARCHIVED);
  ad_model_free(&m);
}

static void test_clock(void) {
  const ad_day d0 = ad_day_from_ymd(2026, 11, 11);
  ad_model m;
  ad_model_init(&m);
  CHECK(ad_model_set_clock(&m, d0)); /* first time: records lastday */
  CHECK_EQ_INT(m.settings.lastday, d0);
  CHECK(!ad_model_set_clock(&m, d0)); /* nothing changed */
  CHECK(ad_model_set_clock(&m, d0 + 1));
  CHECK_EQ_INT(m.today, d0 + 1);

  /* A trip west: the clock says an earlier date, but sealed days stay sealed. */
  CHECK(!ad_model_set_clock(&m, d0));
  CHECK_EQ_INT(m.today, d0 + 1);
  CHECK(ad_model_is_sealed(&m, d0));
  CHECK(!ad_model_set_clock(&m, d0 + 1 - 1));
  CHECK_EQ_INT(m.today, d0 + 1);
  /* ...until the local clock catches up. */
  CHECK(ad_model_set_clock(&m, d0 + 2));
  CHECK_EQ_INT(m.today, d0 + 2);

  /* A clock that was wildly wrong is not allowed to freeze the calendar. */
  ad_model_set_clock(&m, d0 + 5000);
  CHECK_EQ_INT(m.today, d0 + 5000);
  CHECK(ad_model_set_clock(&m, d0 + 3));
  CHECK_EQ_INT(m.today, d0 + 3);
  ad_model_free(&m);
}

static void test_zone_list(void) {
  ad_zone_info *z;
  const int n = ad_zones_load(ADDERALL_TEST_DATA_DIR "/zoneinfo", &z);
  CHECK(n > 400);
  const int paris = ad_zones_find(z, n, "Europe/Paris");
  CHECK(paris >= 0);
  if (paris >= 0) {
    CHECK(z[paris].lat > 48.8 && z[paris].lat < 48.9);
    CHECK(z[paris].lon > 2.3 && z[paris].lon < 2.4);
    CHECK_EQ_STR(z[paris].cc, "FR");
  }
  const int ny = ad_zones_find(z, n, "America/New_York");
  CHECK(ny >= 0 && z[ny].lat > 40 && z[ny].lon < -73 && z[ny].lon > -75);
  /* Every listed zone has a loadable file. */
  for (int i = 0; i < n; i++) {
    ad_tz *tz = ad_tz_load(ADDERALL_TEST_DATA_DIR "/zoneinfo", z[i].name);
    if (!tz) printf("  missing zone file: %s\n", z[i].name);
    CHECK(tz != NULL);
    ad_tz_free(tz);
  }
  /* Picking from the map: a click near Tokyo finds Tokyo. */
  const int near = ad_zones_nearest(z, n, 35.7, 139.7);
  CHECK(near >= 0 && strcmp(z[near].name, "Asia/Tokyo") == 0);
  char buf[32];
  ad_tz_format_offset(5 * 3600 + 1800, buf, sizeof buf);
  CHECK_EQ_STR(buf, "UTC+5:30");
  ad_tz_format_offset(-4 * 3600, buf, sizeof buf);
  CHECK_EQ_STR(buf, "UTC-4");
  ad_tz_format_offset(0, buf, sizeof buf);
  CHECK_EQ_STR(buf, "UTC+0");
  /* Detection always lands on a zone we can load. */
  char name[96];
  ad_tz_detect_system(ADDERALL_TEST_DATA_DIR "/zoneinfo", z, n, name, sizeof name);
  ad_tz *sys = ad_tz_load(ADDERALL_TEST_DATA_DIR "/zoneinfo", name);
  CHECK(sys != NULL);
  ad_tz_free(sys);
  free(z);
}

static void test_zone_tab_crlf(void) {
  /* A checkout with Windows line endings must read the same. */
  char dir[512], path[600];
  snprintf(dir, sizeof dir, "%s/zt_crlf", ADDERALL_TEST_TMP);
  ad_plat_mkdir_p(dir);
  snprintf(path, sizeof path, "%s/zone.tab", dir);
  const char *tab = "# comment\r\nFR\t+4852+00220\tEurope/Paris\r\nJP\t+353916+1394441\tAsia/Tokyo\tHonshu\r\n";
  CHECK(ad_plat_write_durable(path, tab, strlen(tab)));
  ad_zone_info *z;
  const int n = ad_zones_load(dir, &z);
  CHECK_EQ_INT(n, 2);
  if (n == 2) {
    CHECK_EQ_STR(z[0].name, "Europe/Paris");
    CHECK_EQ_STR(z[1].name, "Asia/Tokyo");
  }
  free(z);
}

int main(void) {
  test_zone_tab_crlf();
  test_real_zones();
  test_posix_rules();
  test_days();
  test_sealing();
  test_clock();
  test_zone_list();
  TEST_DONE();
}
