#include "format.h"
#include "model.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static const ad_day D = 20768; /* 2026-11-11 */

static ad_model new_model(void) {
  ad_model m;
  ad_model_init(&m);
  ad_model_set_clock(&m, D);
  return m;
}

static int create(ad_model *m, const char *name) {
  int i = -1;
  CHECK_EQ_INT(ad_model_create(m, name, &i, NULL), AD_HABIT_OK);
  return i;
}

static void test_create_rules(void) {
  ad_model m = new_model();
  int i = create(&m, "  Drink water ");
  CHECK_EQ_STR(m.habits[i].name, "Drink water");
  CHECK_EQ_INT(m.habits[i].start, D);          /* starts today, mid-day or not */
  CHECK_EQ_INT(m.habits[i].symbol, AD_SYM_STAR);
  CHECK(!m.habits[i].archived);

  ad_name_result nr;
  CHECK_EQ_INT(ad_model_create(&m, "", NULL, &nr), AD_HABIT_BAD_NAME);
  CHECK_EQ_INT(nr, AD_NAME_EMPTY);
  CHECK_EQ_INT(ad_model_create(&m, "a\nb", NULL, &nr), AD_HABIT_BAD_NAME);
  CHECK_EQ_INT(nr, AD_NAME_BAD_CHAR);
  CHECK_EQ_INT(ad_model_create(&m, "12345678901234567890123456789012345678901", NULL, &nr), AD_HABIT_BAD_NAME);
  CHECK_EQ_INT(nr, AD_NAME_TOO_LONG);
  CHECK_EQ_INT(ad_model_create(&m, "DRINK WATER", NULL, NULL), AD_HABIT_NAME_TAKEN);
  CHECK_EQ_INT(m.n_habits, 1);
  CHECK(strlen(ad_habit_result_message(AD_HABIT_BAD_NAME, AD_NAME_TOO_LONG)) > 0);

  /* The tracked day also moves on: a habit created tomorrow starts tomorrow. */
  ad_model_set_clock(&m, D + 3);
  const int j = create(&m, "Read");
  CHECK_EQ_INT(m.habits[j].start, D + 3);
  CHECK_EQ_INT(m.habits[j].symbol, AD_SYM_TRIANGLE); /* next free symbol */
  ad_model_free(&m);
}

static void test_board_limit_and_archive(void) {
  ad_model m = new_model();
  char name[16];
  for (int i = 0; i < AD_MAX_BOARD; i++) {
    snprintf(name, sizeof name, "H%d", i);
    create(&m, name);
  }
  CHECK_EQ_INT(ad_model_board_count(&m), 8);
  /* All eight symbols are different. */
  for (int i = 0; i < 8; i++)
    for (int j = i + 1; j < 8; j++) CHECK(m.habits[i].symbol != m.habits[j].symbol);
  CHECK_EQ_INT(ad_model_create(&m, "Ninth", NULL, NULL), AD_HABIT_BOARD_FULL);
  CHECK_EQ_INT(m.n_habits, 8);

  /* Archive frees a slot and keeps the history. */
  ad_habit_put(&m.habits[2], D, AD_FILLED);
  CHECK_EQ_INT(ad_model_archive(&m, 2), AD_HABIT_OK);
  CHECK_EQ_INT(ad_model_archive(&m, 2), AD_HABIT_ALREADY);
  CHECK_EQ_INT(ad_model_board_count(&m), 7);
  CHECK_EQ_INT(ad_habit_get(&m.habits[2], D), AD_FILLED);
  CHECK_EQ_INT(ad_model_board_habit(&m, 2), 3); /* the board skips the archived one */
  CHECK_EQ_INT(ad_model_board_habit(&m, 7), -1);
  /* Archived days are sealed, even today's. */
  CHECK_EQ_INT(ad_model_set_day(&m, 2, D, AD_EMPTY), AD_ERR_ARCHIVED);

  int idx;
  CHECK_EQ_INT(ad_model_create(&m, "Ninth", &idx, NULL), AD_HABIT_OK);
  CHECK_EQ_INT(m.habits[idx].symbol, m.habits[2].symbol); /* reuses the freed symbol */

  /* The names of archived habits stay reserved. */
  CHECK_EQ_INT(ad_model_create(&m, "h2", NULL, NULL), AD_HABIT_NAME_TAKEN);

  /* Restore needs room... */
  CHECK_EQ_INT(ad_model_restore(&m, 2, NULL), AD_HABIT_BOARD_FULL);
  /* ...and when there is, goes to the end of the board; a clashing symbol is replaced. */
  CHECK_EQ_INT(ad_model_archive(&m, 0), AD_HABIT_OK);
  int where = -1;
  CHECK_EQ_INT(ad_model_restore(&m, 2, &where), AD_HABIT_OK);
  CHECK_EQ_INT(where, m.n_habits - 1);
  CHECK_EQ_STR(m.habits[where].name, "H2");
  CHECK_EQ_INT(ad_habit_get(&m.habits[where], D), AD_FILLED);
  for (int i = 0; i < m.n_habits; i++)
    for (int j = i + 1; j < m.n_habits; j++)
      if (!m.habits[i].archived && !m.habits[j].archived) CHECK(m.habits[i].symbol != m.habits[j].symbol);
  CHECK_EQ_INT(ad_model_restore(&m, where, NULL), AD_HABIT_ALREADY);
  ad_model_free(&m);
}

static void test_rename_symbol_delete(void) {
  ad_model m = new_model();
  const int a = create(&m, "Alpha");
  const int b = create(&m, "Beta");
  CHECK_EQ_INT(ad_model_rename(&m, a, " Gamma ", NULL), AD_HABIT_OK);
  CHECK_EQ_STR(m.habits[a].name, "Gamma");
  CHECK_EQ_INT(ad_model_rename(&m, a, "BETA", NULL), AD_HABIT_NAME_TAKEN);
  CHECK_EQ_INT(ad_model_rename(&m, a, "gamma", NULL), AD_HABIT_OK); /* same habit, new capitals */
  CHECK_EQ_STR(m.habits[a].name, "gamma");
  CHECK_EQ_INT(ad_model_rename(&m, a, "", NULL), AD_HABIT_BAD_NAME);
  CHECK_EQ_STR(m.habits[a].name, "gamma");
  CHECK_EQ_INT(ad_model_rename(&m, 9, "x", NULL), AD_HABIT_NOT_FOUND);

  /* Symbols: free ones only. */
  CHECK_EQ_INT(ad_model_set_symbol(&m, a, m.habits[b].symbol), AD_HABIT_SYMBOL_TAKEN);
  CHECK_EQ_INT(ad_model_set_symbol(&m, a, AD_SYM_HEART), AD_HABIT_OK);
  CHECK_EQ_INT(m.habits[a].symbol, AD_SYM_HEART);
  CHECK_EQ_INT(ad_model_set_symbol(&m, a, AD_SYM_HEART), AD_HABIT_OK); /* its own symbol is fine */
  CHECK_EQ_INT(ad_model_set_symbol(&m, a, AD_SYM_COUNT), AD_HABIT_NOT_FOUND);

  /* Delete is for good. */
  ad_habit_put(&m.habits[a], D, AD_FILLED);
  CHECK_EQ_INT(ad_model_delete(&m, a), AD_HABIT_OK);
  CHECK_EQ_INT(m.n_habits, 1);
  CHECK_EQ_STR(m.habits[0].name, "Beta");
  CHECK_EQ_INT(ad_model_create(&m, "gamma", NULL, NULL), AD_HABIT_OK); /* the name is free again */
  CHECK_EQ_INT(ad_model_delete(&m, 5), AD_HABIT_NOT_FOUND);
  ad_model_free(&m);
}

static void test_persists(void) {
  ad_model m = new_model();
  create(&m, "One");
  create(&m, "Two");
  ad_model_archive(&m, 0);
  size_t len;
  char *t = ad_format_write(&m, &len);
  ad_model back;
  ad_parse_error e;
  CHECK_EQ_INT(ad_format_parse(t, len, &back, &e), AD_PARSE_OK);
  CHECK(back.habits[0].archived && !back.habits[1].archived);
  CHECK_EQ_INT(back.settings.lastday, D);
  free(t);
  ad_model_free(&back);
  ad_model_free(&m);
}

static void test_backfill(void) {
  ad_model m = new_model();
  int i = -1;
  /* A habit that has really been going for 30 days. */
  CHECK_EQ_INT(ad_model_create_from(&m, "Run", D - 30, &i, NULL), AD_HABIT_OK);
  CHECK_EQ_INT(m.habits[i].start, D - 30);
  CHECK(ad_model_in_setup(&m));
  /* During setup the past can be filled and skipped, one day at a time. */
  CHECK_EQ_INT(ad_model_set_day(&m, i, D - 30, AD_FILLED), AD_OK);
  CHECK_EQ_INT(ad_model_toggle_fill(&m, i, D - 29), AD_OK);
  CHECK_EQ_INT(ad_model_toggle_skip(&m, i, D - 5), AD_OK);
  CHECK_EQ_INT(ad_model_set_day(&m, i, D - 31, AD_FILLED), AD_ERR_BEFORE_START);
  /* Another habit is not opened up by that. */
  int j = create(&m, "Read");
  CHECK_EQ_INT(ad_model_set_day(&m, j, D - 1, AD_FILLED), AD_ERR_BEFORE_START);
  /* It survives a save and reload as ordinary, sealed history. */
  size_t len;
  char *t = ad_format_write(&m, &len);
  ad_model back;
  ad_parse_error e;
  CHECK_EQ_INT(ad_format_parse(t, len, &back, &e), AD_PARSE_OK);
  ad_model_set_clock(&back, D);
  CHECK(!ad_model_in_setup(&back));
  CHECK_EQ_INT(back.habits[0].start, D - 30);
  CHECK_EQ_INT(ad_habit_get(&back.habits[0], D - 29), AD_FILLED);
  CHECK_EQ_INT(ad_model_set_day(&back, 0, D - 29, AD_EMPTY), AD_ERR_SEALED);
  free(t);
  ad_model_free(&back);
  /* Confirming setup seals the past. */
  ad_model_finish_setup(&m);
  CHECK(!ad_model_in_setup(&m));
  CHECK_EQ_INT(ad_model_set_day(&m, i, D - 10, AD_FILLED), AD_ERR_SEALED);
  CHECK_EQ_INT(ad_habit_get(&m.habits[i], D - 29), AD_FILLED);
  CHECK_EQ_INT(ad_model_set_day(&m, i, D, AD_FILLED), AD_OK); /* today stays open */
  /* Dates that make no sense are refused. */
  CHECK_EQ_INT(ad_model_create_from(&m, "Future", D + 1, NULL, NULL), AD_HABIT_BAD_DATE);
  CHECK_EQ_INT(ad_model_create_from(&m, "Ancient", D - AD_BACKFILL_MAX_DAYS - 1, NULL, NULL), AD_HABIT_BAD_DATE);
  CHECK_EQ_INT(ad_model_create_from(&m, "Decade", D - AD_BACKFILL_MAX_DAYS, NULL, NULL), AD_HABIT_OK);
  CHECK_EQ_INT(ad_model_create_from(&m, "run", D - 3, NULL, NULL), AD_HABIT_NAME_TAKEN);
  /* A habit started today is never in setup. */
  CHECK(!m.habits[j].setup);
  ad_model_free(&m);
}

int main(void) {
  test_create_rules();
  test_board_limit_and_archive();
  test_rename_symbol_delete();
  test_persists();
  test_backfill();
  TEST_DONE();
}
