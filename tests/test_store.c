#include "store.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ADDERALL_TEST_TMP
#define ADDERALL_TEST_TMP "."
#endif

static char dir[AD_PATH_MAX];

/* A fresh, empty folder for each test. */
static void fresh_dir(const char *name) {
  snprintf(dir, sizeof dir, "%s/store_%s", ADDERALL_TEST_TMP, name);
  ad_plat_mkdir_p(dir);
  static const char *const files[] = {"adderall.md", "adderall.md.bak", "adderall.md.tmp", "adderall.md.bak.tmp", "adderall.md.damaged"};
  for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) {
    char p[AD_PATH_MAX + 64];
    snprintf(p, sizeof p, "%s/%s", dir, files[i]);
    ad_plat_remove(p);
  }
}

static char *slurp(const char *name) {
  char p[AD_PATH_MAX + 64], *d;
  size_t n;
  snprintf(p, sizeof p, "%s/%s", dir, name);
  if (!ad_plat_read_file(p, &d, &n, NULL)) return NULL;
  return d;
}

static void spit(const char *name, const char *text) {
  char p[AD_PATH_MAX + 64];
  snprintf(p, sizeof p, "%s/%s", dir, name);
  ad_plat_write_durable(p, text, strlen(text));
}

static bool exists(const char *name) {
  char p[AD_PATH_MAX + 64];
  snprintf(p, sizeof p, "%s/%s", dir, name);
  return ad_plat_file_exists(p);
}

static void make_model(ad_model *m, int habits, int marks) {
  ad_model_init(m);
  for (int i = 0; i < habits; i++) {
    char name[32];
    snprintf(name, sizeof name, "Habit %d", i);
    const int h = ad_model_add_raw(m, name, (ad_symbol)i, ad_day_from_ymd(2026, 11, 1));
    for (int k = 0; k < marks; k++) ad_habit_put(&m->habits[h], ad_day_from_ymd(2026, 11, 1) + k * 2, AD_FILLED);
  }
}

static char *text_of(const ad_model *m) {
  size_t n;
  return ad_format_write(m, &n);
}

static void test_fresh_and_reopen(void) {
  fresh_dir("basic");
  ad_store s;
  ad_model m;
  char msg[256];
  CHECK_EQ_INT(ad_store_open(&s, dir, &m, NULL, msg, sizeof msg), AD_OPEN_FRESH);
  CHECK_EQ_INT(m.n_habits, 0);
  CHECK(!exists("adderall.md"));

  ad_model v1;
  make_model(&v1, 2, 3);
  CHECK(ad_store_save(&s, &v1, msg, sizeof msg));
  CHECK(exists("adderall.md"));
  CHECK(!exists("adderall.md.bak")); /* nothing to back up yet */
  CHECK(!exists("adderall.md.tmp"));
  char *t1 = text_of(&v1);
  char *on_disk = slurp("adderall.md");
  CHECK_EQ_STR(on_disk, t1);
  free(on_disk);

  /* A second save keeps the first version as the backup. */
  ad_model v2;
  make_model(&v2, 3, 5);
  CHECK(ad_store_save(&s, &v2, msg, sizeof msg));
  char *bak = slurp("adderall.md.bak");
  CHECK_EQ_STR(bak, t1);
  free(bak);
  char *t2 = text_of(&v2);
  char *cur = slurp("adderall.md");
  CHECK_EQ_STR(cur, t2);
  free(cur);
  CHECK(!exists("adderall.md.tmp"));
  CHECK(!exists("adderall.md.bak.tmp"));
  ad_store_close(&s);
  ad_model_free(&m);

  ad_model back;
  CHECK_EQ_INT(ad_store_open(&s, dir, &back, NULL, msg, sizeof msg), AD_OPEN_OK);
  char *tb = text_of(&back);
  CHECK_EQ_STR(tb, t2);
  free(tb);
  ad_store_close(&s);
  ad_model_free(&back);
  ad_model_free(&v1);
  ad_model_free(&v2);
  free(t1);
  free(t2);
}

static void test_lock(void) {
  fresh_dir("lock");
  ad_store a, b;
  ad_model ma, mb;
  char msg[256];
  CHECK_EQ_INT(ad_store_open(&a, dir, &ma, NULL, msg, sizeof msg), AD_OPEN_FRESH);
  /* A second launch is told Adderall is already running. */
  CHECK_EQ_INT(ad_store_open(&b, dir, &mb, NULL, msg, sizeof msg), AD_OPEN_LOCKED);
  CHECK(strstr(msg, "already running") != NULL);
  /* Once the first is gone (as if it quit or crashed), the lock is free,
   * even though the lock file is still on disk. */
  ad_store_close(&a);
  CHECK_EQ_INT(ad_store_open(&b, dir, &mb, NULL, msg, sizeof msg), AD_OPEN_FRESH);
  ad_store_close(&b);
  ad_model_free(&ma);
  ad_model_free(&mb);
}

static int crash_at;
static int hook_fn(int step) { return step == crash_at; }

static void test_interrupted_saves(void) {
  for (crash_at = 1; crash_at <= 3; crash_at++) {
    fresh_dir("crash");
    ad_store s;
    ad_model m, v1, v2;
    char msg[256];
    CHECK_EQ_INT(ad_store_open(&s, dir, &m, NULL, msg, sizeof msg), AD_OPEN_FRESH);
    make_model(&v1, 2, 3);
    CHECK(ad_store_save(&s, &v1, msg, sizeof msg)); /* the last good state */
    char *t1 = text_of(&v1);

    make_model(&v2, 4, 6);
    ad_store_crash_hook = hook_fn;
    CHECK(!ad_store_save(&s, &v2, msg, sizeof msg)); /* "power cut" */
    ad_store_crash_hook = NULL;
    ad_store_close(&s);

    /* Whatever step it died at, the real file is complete and unchanged, and
     * it opens normally. */
    char *cur = slurp("adderall.md");
    CHECK_EQ_STR(cur, t1);
    free(cur);
    ad_model back;
    CHECK_EQ_INT(ad_store_open(&s, dir, &back, NULL, msg, sizeof msg), AD_OPEN_OK);
    char *tb = text_of(&back);
    CHECK_EQ_STR(tb, t1);
    free(tb);

    /* And saving again simply works, leaving no strays behind. */
    CHECK(ad_store_save(&s, &v2, msg, sizeof msg));
    char *t2 = text_of(&v2);
    cur = slurp("adderall.md");
    CHECK_EQ_STR(cur, t2);
    free(cur);
    CHECK(!exists("adderall.md.tmp"));
    ad_store_close(&s);
    free(t1);
    free(t2);
    ad_model_free(&m);
    ad_model_free(&back);
    ad_model_free(&v1);
    ad_model_free(&v2);
  }
}

static void test_half_written_temp_file(void) {
  fresh_dir("torn");
  ad_store s;
  ad_model m, v1;
  char msg[256];
  ad_store_open(&s, dir, &m, NULL, msg, sizeof msg);
  make_model(&v1, 2, 3);
  ad_store_save(&s, &v1, msg, sizeof msg);
  ad_store_close(&s);
  ad_model_free(&m);
  char *t1 = text_of(&v1);

  /* A crash mid-write leaves half a file under the temporary name. */
  spit("adderall.md.tmp", "# Adderall\n\nadderall-format: 2\n\n## Settings\n\n- timez");
  ad_model back;
  CHECK_EQ_INT(ad_store_open(&s, dir, &back, NULL, msg, sizeof msg), AD_OPEN_OK);
  char *tb = text_of(&back);
  CHECK_EQ_STR(tb, t1);
  free(tb);
  CHECK(ad_store_save(&s, &back, msg, sizeof msg));
  CHECK(!exists("adderall.md.tmp"));
  ad_store_close(&s);
  ad_model_free(&back);
  ad_model_free(&v1);
  free(t1);
}

static void test_damaged_main_falls_back(void) {
  fresh_dir("damaged");
  ad_store s;
  ad_model m, v1, v2;
  char msg[256];
  ad_store_open(&s, dir, &m, NULL, msg, sizeof msg);
  make_model(&v1, 1, 2);
  make_model(&v2, 2, 4);
  ad_store_save(&s, &v1, msg, sizeof msg);
  ad_store_save(&s, &v2, msg, sizeof msg); /* bak = v1, main = v2 */
  ad_store_close(&s);
  ad_model_free(&m);
  char *t1 = text_of(&v1);
  char *t2 = text_of(&v2);

  /* The main file turns to garbage. */
  spit("adderall.md", "\x01\x02 not an adderall file at all");
  ad_model back;
  CHECK_EQ_INT(ad_store_open(&s, dir, &back, NULL, msg, sizeof msg), AD_OPEN_FROM_BACKUP);
  CHECK(strstr(msg, "damaged") != NULL);
  char *tb = text_of(&back);
  CHECK_EQ_STR(tb, t1); /* the backup's content */
  free(tb);
  CHECK(!exists("adderall.md.damaged")); /* nothing touched until the next save */

  CHECK(ad_store_save(&s, &back, msg, sizeof msg));
  char *dmg = slurp("adderall.md.damaged");
  CHECK(dmg != NULL && strstr(dmg, "not an adderall file") != NULL); /* kept, not lost */
  free(dmg);
  char *bak = slurp("adderall.md.bak");
  CHECK_EQ_STR(bak, t1); /* the good backup was not replaced by the damaged file */
  free(bak);
  ad_store_close(&s);
  ad_model_free(&back);

  /* An empty main file counts as damaged too. */
  spit("adderall.md", "");
  CHECK_EQ_INT(ad_store_open(&s, dir, &back, NULL, msg, sizeof msg), AD_OPEN_FROM_BACKUP);
  ad_store_close(&s);
  ad_model_free(&back);

  /* A missing main file with a backup also falls back. */
  char p[AD_PATH_MAX + 64];
  snprintf(p, sizeof p, "%s/adderall.md", dir);
  ad_plat_remove(p);
  CHECK_EQ_INT(ad_store_open(&s, dir, &back, NULL, msg, sizeof msg), AD_OPEN_FROM_BACKUP);
  CHECK(strstr(msg, "missing") != NULL);
  ad_store_close(&s);
  ad_model_free(&back);
  ad_model_free(&v1);
  ad_model_free(&v2);
  free(t1);
  free(t2);
}

static void test_unreadable_line_leaves_file_alone(void) {
  fresh_dir("line");
  ad_store s;
  ad_model m, v1;
  char msg[256];
  ad_store_open(&s, dir, &m, NULL, msg, sizeof msg);
  make_model(&v1, 1, 2);
  ad_store_save(&s, &v1, msg, sizeof msg);
  ad_store_save(&s, &v1, msg, sizeof msg);
  ad_store_close(&s);
  ad_model_free(&m);
  char *t1 = text_of(&v1);

  /* The person edits the file by hand and makes a mistake on one line. */
  const char *edited = "# Adderall\n\nadderall-format: 2\n\n## Settings\n\n- layout: sideways\n\n## Habit: A\n- start: 2026-11-01\n";
  spit("adderall.md", edited);
  ad_parse_error pe;
  ad_model back;
  CHECK_EQ_INT(ad_store_open(&s, dir, &back, &pe, msg, sizeof msg), AD_OPEN_PARSE_ERROR);
  CHECK_EQ_INT(pe.status, AD_PARSE_LINE);
  CHECK_EQ_INT(pe.line, 7);
  CHECK(strstr(pe.text, "sideways") != NULL);

  /* Refuses to save, and the file is exactly as the person left it. */
  CHECK(!ad_store_save(&s, &v1, msg, sizeof msg));
  char *cur = slurp("adderall.md");
  CHECK_EQ_STR(cur, edited);
  free(cur);
  CHECK(!exists("adderall.md.damaged"));

  /* The person may choose the backup instead; the odd file is kept aside. */
  CHECK_EQ_INT(ad_store_use_backup(&s, &back, msg, sizeof msg), AD_OPEN_FROM_BACKUP);
  CHECK_EQ_INT(back.n_habits, 1);
  CHECK(ad_store_save(&s, &back, msg, sizeof msg));
  char *dmg = slurp("adderall.md.damaged");
  CHECK_EQ_STR(dmg, edited);
  free(dmg);
  ad_store_close(&s);
  ad_model_free(&back);
  ad_model_free(&v1);
  free(t1);
}

static void test_both_bad_and_no_backup(void) {
  fresh_dir("bad");
  ad_store s;
  ad_model m;
  char msg[256];
  ad_parse_error pe;
  spit("adderall.md", "garbage");
  CHECK_EQ_INT(ad_store_open(&s, dir, &m, &pe, msg, sizeof msg), AD_OPEN_PARSE_ERROR); /* damaged, no backup */
  CHECK(!ad_store_save(&s, &m, msg, sizeof msg));
  char *cur = slurp("adderall.md");
  CHECK_EQ_STR(cur, "garbage");
  free(cur);
  ad_store_close(&s);
  ad_model_free(&m);

  spit("adderall.md.bak", "also garbage");
  CHECK_EQ_INT(ad_store_open(&s, dir, &m, &pe, msg, sizeof msg), AD_OPEN_PARSE_ERROR);
  ad_store_close(&s);
  ad_model_free(&m);

  /* A file from a newer Adderall is left alone as well. */
  spit("adderall.md", "# Adderall\n\nadderall-format: 77\n");
  CHECK_EQ_INT(ad_store_open(&s, dir, &m, &pe, msg, sizeof msg), AD_OPEN_PARSE_ERROR);
  CHECK_EQ_INT(pe.status, AD_PARSE_NEWER);
  ad_store_close(&s);
  ad_model_free(&m);
}

int main(void) {
  test_fresh_and_reopen();
  test_lock();
  test_interrupted_saves();
  test_half_written_temp_file();
  test_damaged_main_falls_back();
  test_unreadable_line_leaves_file_alone();
  test_both_bad_and_no_backup();
  TEST_DONE();
}
