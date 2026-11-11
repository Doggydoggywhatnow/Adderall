#include "format.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

/* The example from the README. */
static const char *README_EXAMPLE =
    "# Adderall\n\nadderall-format: 1\n\n## Settings\n\n- timezone: America/New_York\n- daystart: 04:00\n"
    "- layout: horizontal\n- font: default\n- textsize: 16\n- hotkey today: t\n\n## Habit: Drink water\n\n"
    "- symbol: star\n- start: 2026-11-11\n\n- [x] 2026-11-11\n- [x] 2026-11-12\n- [/] 2026-11-13\n\n"
    "## Habit: Read\n\n- symbol: triangle\n- start: 2026-11-12\n- archived: yes\n\n- [x] 2026-11-12\n";

static ad_parse_status parse(const char *text, ad_model *m, ad_parse_error *e) {
  return ad_format_parse(text, strlen(text), m, e);
}

/* Header shared by the error cases. */
#define HEAD "# Adderall\n\nadderall-format: 2\n\n"
#define HAB1 "## Habit: A\n\n- symbol: star\n- start: 2026-11-11\n"

static void expect_error(const char *text, ad_parse_status st, int line) {
  ad_model m;
  ad_parse_error e;
  const ad_parse_status got = parse(text, &m, &e);
  if (got != st || (line >= 0 && e.line != line)) {
    printf("  case did not give status %d line %d (got %d line %d: %s)\n---\n%s---\n", st, line, got, e.line, e.message, text);
  }
  CHECK_EQ_INT(got, st);
  if (line >= 0) CHECK_EQ_INT(e.line, line);
  CHECK(e.message[0] != 0);
  CHECK_EQ_INT(m.n_habits, 0); /* nothing half-loaded is handed back */
  ad_model_free(&m);
}

static void test_readme_example(void) {
  ad_model m;
  ad_parse_error e;
  CHECK_EQ_INT(parse(README_EXAMPLE, &m, &e), AD_PARSE_OK);
  CHECK_EQ_STR(m.settings.timezone, "America/New_York");
  CHECK_EQ_INT(m.settings.daystart, 240);
  CHECK_EQ_INT(m.settings.layout, AD_LAYOUT_HORIZONTAL);
  CHECK_EQ_INT(m.settings.textsize, 16);
  CHECK(m.settings.hotkey_set[AD_ACT_TODAY]);
  CHECK_EQ_INT(m.settings.hotkey[AD_ACT_TODAY].n, 1);
  CHECK_EQ_STR(m.settings.hotkey[AD_ACT_TODAY].keys[0].key, "T");
  CHECK_EQ_INT(m.n_habits, 2);
  CHECK_EQ_STR(m.habits[0].name, "Drink water");
  CHECK_EQ_INT(m.habits[0].symbol, AD_SYM_STAR);
  CHECK_EQ_INT(m.habits[0].start, ad_day_from_ymd(2026, 11, 11));
  CHECK(!m.habits[0].archived);
  CHECK_EQ_INT(ad_habit_get(&m.habits[0], ad_day_from_ymd(2026, 11, 11)), AD_FILLED);
  CHECK_EQ_INT(ad_habit_get(&m.habits[0], ad_day_from_ymd(2026, 11, 13)), AD_SKIPPED);
  CHECK_EQ_INT(ad_habit_get(&m.habits[0], ad_day_from_ymd(2026, 11, 14)), AD_EMPTY);
  CHECK_EQ_STR(m.habits[1].name, "Read");
  CHECK(m.habits[1].archived);
  CHECK_EQ_INT(m.habits[1].symbol, AD_SYM_TRIANGLE);
  ad_model_free(&m);
}

static void test_roundtrip(void) {
  ad_model m;
  ad_model_init(&m);
  m.settings.timezone[0] = 0;
  strcpy(m.settings.timezone, "Europe/Paris");
  m.settings.daystart = 4 * 60 + 30;
  m.settings.layout = AD_LAYOUT_VERTICAL;
  strcpy(m.settings.font, "/usr/share/fonts/some: odd font.ttf");
  m.settings.textsize = 22;
  m.settings.dark = true;
  m.settings.scale = 125;
  m.settings.high_contrast = true;
  m.settings.reduced_motion = true;
  m.settings.lastday = ad_day_from_ymd(2026, 11, 20);
  ad_binding_parse("Enter | Space | X", &m.settings.hotkey[AD_ACT_FILL]);
  m.settings.hotkey_set[AD_ACT_FILL] = true;
  ad_binding_parse("Ctrl+,", &m.settings.hotkey[AD_ACT_SETTINGS]);
  m.settings.hotkey_set[AD_ACT_SETTINGS] = true;

  const int a = ad_model_add_raw(&m, "Drink water", AD_SYM_STAR, ad_day_from_ymd(2026, 11, 1));
  const int b = ad_model_add_raw(&m, "Zażółć gęślą jaźń \xE2\x9C\x93", AD_SYM_HEART, ad_day_from_ymd(2026, 11, 5));
  const int c = ad_model_add_raw(&m, "Old one", AD_SYM_STAR, ad_day_from_ymd(2026, 10, 1)); /* archived: may reuse a symbol */
  m.habits[c].archived = true;
  for (int i = 0; i < 30; i += 2) ad_habit_put(&m.habits[a], ad_day_from_ymd(2026, 11, 1) + i, AD_FILLED);
  ad_habit_put(&m.habits[a], ad_day_from_ymd(2026, 11, 2), AD_SKIPPED);
  ad_habit_put(&m.habits[b], ad_day_from_ymd(2026, 11, 7), AD_FILLED);

  size_t len1, len2;
  char *t1 = ad_format_write(&m, &len1);
  CHECK(t1 != NULL);
  ad_model back;
  ad_parse_error e;
  CHECK_EQ_INT(ad_format_parse(t1, len1, &back, &e), AD_PARSE_OK);
  char *t2 = ad_format_write(&back, &len2);
  CHECK_EQ_INT(len1, len2);
  CHECK_EQ_STR(t1, t2);
  CHECK_EQ_STR(back.settings.font, "/usr/share/fonts/some: odd font.ttf");
  CHECK_EQ_INT(back.habits[0].n_marks, m.habits[0].n_marks);
  CHECK(back.settings.dark && back.settings.high_contrast && back.settings.reduced_motion);
  CHECK_EQ_INT(back.settings.hotkey[AD_ACT_FILL].n, 3);
  /* The file reads like a document. */
  CHECK(strstr(t1, "## Habit: Drink water\n") != NULL);
  CHECK(strstr(t1, "- [x] 2026-11-01\n") != NULL);
  CHECK(strstr(t1, "- [/] 2026-11-02\n") != NULL);
  CHECK(strstr(t1, "- archived: yes\n") != NULL);
  free(t1);
  free(t2);
  ad_model_free(&back);
  ad_model_free(&m);
}

static void test_empty_model(void) {
  ad_model m;
  ad_model_init(&m);
  size_t len;
  char *t = ad_format_write(&m, &len);
  ad_model back;
  ad_parse_error e;
  CHECK_EQ_INT(ad_format_parse(t, len, &back, &e), AD_PARSE_OK);
  CHECK_EQ_INT(back.n_habits, 0);
  CHECK_EQ_INT(back.settings.lastday, AD_DAY_NONE);
  CHECK_EQ_STR(back.settings.timezone, "");
  free(t);
  ad_model_free(&back);
  ad_model_free(&m);
}

static void test_tolerance(void) {
  ad_model m;
  ad_parse_error e;
  /* Windows line endings, a BOM, blank lines anywhere, and "[ ]" for empty. */
  const char *crlf = "\xEF\xBB\xBF# Adderall\r\n\r\nadderall-format: 2\r\n\r\n\r\n## Habit: A\r\n- start: 2026-11-11\r\n- [x] 2026-11-11\r\n- [ ] 2026-11-12\r\n";
  CHECK_EQ_INT(parse(crlf, &m, &e), AD_PARSE_OK);
  CHECK_EQ_INT(m.n_habits, 1);
  CHECK_EQ_STR(m.habits[0].name, "A");
  CHECK_EQ_INT(m.habits[0].n_marks, 1);
  ad_model_free(&m);
  /* A missing symbol gets the next free one. */
  const char *nosym = HEAD HAB1 "\n## Habit: B\n\n- start: 2026-11-11\n";
  CHECK_EQ_INT(parse(nosym, &m, &e), AD_PARSE_OK);
  CHECK(m.habits[0].symbol != m.habits[1].symbol);
  ad_model_free(&m);
  /* Names are trimmed. */
  const char *spaced = HEAD "## Habit:    Padded name   \n- start: 2026-11-11\n";
  CHECK_EQ_INT(parse(spaced, &m, &e), AD_PARSE_OK);
  CHECK_EQ_STR(m.habits[0].name, "Padded name");
  ad_model_free(&m);
  /* No trailing newline. */
  CHECK_EQ_INT(parse("# Adderall\n\nadderall-format: 2", &m, &e), AD_PARSE_OK);
  ad_model_free(&m);
}

static void test_errors(void) {
  expect_error("", AD_PARSE_DAMAGED, -1);
  expect_error("hello there\n", AD_PARSE_DAMAGED, -1);
  {
    ad_model m;
    ad_parse_error e;
    const char bin[] = "# Adderall\n\nadderall-format: 2\n\x01\0\x02";
    CHECK_EQ_INT(ad_format_parse(bin, sizeof bin - 1, &m, &e), AD_PARSE_DAMAGED);
    const char bad8[] = "# Adderall\n\nadderall-format: 2\n\n## Habit: \xC3\x28\n";
    CHECK_EQ_INT(ad_format_parse(bad8, sizeof bad8 - 1, &m, &e), AD_PARSE_DAMAGED);
  }
  expect_error("# Adderall\n\n## Settings\n", AD_PARSE_LINE, 3);
  expect_error("# Adderall\n\nadderall-format: 99\n", AD_PARSE_NEWER, 3);
  expect_error("# Adderall\n\nadderall-format: 0\n", AD_PARSE_LINE, 3);
  expect_error("# Adderall\n\nadderall-format: x\n", AD_PARSE_LINE, 3);

  /* Line numbers point at the problem. */
  expect_error(HEAD "## Settings\n\n- layout: sideways\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "## Settings\n\n- colour: red\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "## Settings\n\n- daystart: 12:00\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "## Settings\n\n- daystart: 4am\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "## Settings\n\n- textsize: 99\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "## Settings\n\n- scale: 10\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "## Settings\n\n- timezone: Not A Zone\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "## Settings\n\n- layout: vertical\n- layout: horizontal\n", AD_PARSE_LINE, 8);
  expect_error(HEAD "## Settings\n\n- hotkey fly: F\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "## Settings\n\n- hotkey today: Ctrl+Nope\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "## Settings\n\n- lastday: 2026-13-01\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "## Settings\n\n## Settings\n", AD_PARSE_LINE, 7);
  expect_error(HEAD "random note\n", AD_PARSE_LINE, 5);
  expect_error(HEAD "## Other\n", AD_PARSE_LINE, 5);
  expect_error(HEAD "## Settings\n\njust text\n", AD_PARSE_LINE, 7);

  expect_error(HEAD HAB1 "- [x] 2026-11-31\n", AD_PARSE_LINE, 9);          /* not a date */
  expect_error(HEAD HAB1 "- [x] 2026-11-10\n", AD_PARSE_LINE, 9);          /* before the start */
  expect_error(HEAD HAB1 "- [x] 2026-11-12\n- [/] 2026-11-12\n", AD_PARSE_LINE, 10); /* twice */
  expect_error(HEAD HAB1 "- [?] 2026-11-12\n", AD_PARSE_LINE, 9);
  expect_error(HEAD "## Habit: A\n\n- [x] 2026-11-12\n- start: 2026-11-11\n", AD_PARSE_LINE, 7); /* start first */
  expect_error(HEAD "## Habit: A\n\n- symbol: star\n", AD_PARSE_LINE, -1);                     /* no start */
  expect_error(HEAD HAB1 "- symbol: hexagon\n", AD_PARSE_LINE, 9);
  expect_error(HEAD HAB1 "- start: 2026-11-12\n", AD_PARSE_LINE, 9);
  expect_error(HEAD HAB1 "- symbol: circle\n", AD_PARSE_LINE, 9);
  expect_error(HEAD HAB1 "- archived: maybe\n", AD_PARSE_LINE, 9);
  expect_error(HEAD HAB1 "- colour: red\n", AD_PARSE_LINE, 9);
  expect_error(HEAD "## Habit: \n- start: 2026-11-11\n", AD_PARSE_LINE, 5);                  /* empty name */
  expect_error(HEAD HAB1 "\n## Habit: a\n- start: 2026-11-11\n", AD_PARSE_LINE, 10);        /* same name */
  expect_error(HEAD "## Habit: Zero\n- symbol: star\n- start: 2026-11-11\n\n## Habit: ZERO\n- start: 2026-11-11\n", AD_PARSE_LINE, 9);
  expect_error(HEAD HAB1 "\n## Habit: B\n- symbol: star\n- start: 2026-11-11\n", AD_PARSE_LINE, -1); /* same symbol on the board */

  /* 40 characters is fine, 41 is not. */
  {
    ad_model m;
    ad_parse_error e;
    char t[512];
    snprintf(t, sizeof t, HEAD "## Habit: %s\n- start: 2026-11-11\n", "1234567890123456789012345678901234567890");
    CHECK_EQ_INT(parse(t, &m, &e), AD_PARSE_OK);
    ad_model_free(&m);
    snprintf(t, sizeof t, HEAD "## Habit: %s\n- start: 2026-11-11\n", "12345678901234567890123456789012345678901");
    expect_error(t, AD_PARSE_LINE, 5);
  }

  /* Nine habits on the board is one too many; archived ones don't count. */
  {
    char t[4096] = HEAD;
    for (int i = 0; i < 9; i++) {
      char h[96];
      snprintf(h, sizeof h, "## Habit: H%d\n- symbol: %s\n- start: 2026-11-11\n\n", i, ad_symbol_name((ad_symbol)i));
      strcat(t, h);
    }
    expect_error(t, AD_PARSE_LINE, -1);
    ad_model m;
    ad_parse_error e;
    char *arch = strstr(t, "## Habit: H8");
    strcat(arch, "- archived: yes\n");
    CHECK_EQ_INT(parse(t, &m, &e), AD_PARSE_OK);
    CHECK_EQ_INT(m.n_habits, 9);
    ad_model_free(&m);
  }
}

static void test_names(void) {
  char out[AD_NAME_MAX_BYTES + 1];
  CHECK_EQ_INT(ad_name_normalize("  Read  ", out), AD_NAME_OK);
  CHECK_EQ_STR(out, "Read");
  CHECK_EQ_INT(ad_name_normalize("", out), AD_NAME_EMPTY);
  CHECK_EQ_INT(ad_name_normalize("    ", out), AD_NAME_EMPTY);
  CHECK_EQ_INT(ad_name_normalize("two\nlines", out), AD_NAME_BAD_CHAR);
  CHECK_EQ_INT(ad_name_normalize("tab\there", out), AD_NAME_BAD_CHAR);
  CHECK_EQ_INT(ad_name_normalize("\n", out), AD_NAME_BAD_CHAR);
  CHECK_EQ_INT(ad_name_normalize("bad\xC3\x28", out), AD_NAME_BAD_CHAR);
  CHECK_EQ_INT(ad_name_normalize("sep\xE2\x80\xA8" "arator", out), AD_NAME_BAD_CHAR);
  CHECK_EQ_INT(ad_name_normalize("1234567890123456789012345678901234567890", out), AD_NAME_OK);
  CHECK_EQ_INT(ad_name_normalize("12345678901234567890123456789012345678901", out), AD_NAME_TOO_LONG);
  /* Characters, not bytes: 40 two-byte letters fit. */
  CHECK_EQ_INT(ad_name_normalize("\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9"
                                 "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9"
                                 "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9"
                                 "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", out), AD_NAME_OK);
  CHECK(ad_name_equal("Read", "read"));
  CHECK(ad_name_equal("READ", "rEaD"));
  CHECK(!ad_name_equal("Read", "Reads"));
  CHECK(ad_name_equal("\xC3\x89t\xC3\xA9", "\xC3\xA9t\xC3\xA9"));  /* Été */
  CHECK(ad_name_equal("\xD0\x96", "\xD0\xB6"));                      /* Ж ж */
  CHECK(ad_name_equal("\xCE\x91", "\xCE\xB1"));                      /* Α α */
  CHECK(!ad_name_equal("a", "b"));
}

static void test_hotkeys(void) {
  ad_key k;
  CHECK(ad_key_parse("t", &k));
  CHECK_EQ_STR(k.key, "T");
  CHECK_EQ_INT(k.mods, 0);
  CHECK(ad_key_parse("ctrl+,", &k));
  CHECK_EQ_STR(k.key, ",");
  CHECK_EQ_INT(k.mods, AD_MOD_CTRL);
  CHECK(ad_key_parse("Ctrl++", &k));
  CHECK_EQ_STR(k.key, "+");
  CHECK(ad_key_parse("Cmd+Shift+Left", &k));
  CHECK_EQ_INT(k.mods, AD_MOD_CMD | AD_MOD_SHIFT);
  CHECK_EQ_STR(k.key, "Left");
  CHECK(ad_key_parse("return", &k));
  CHECK_EQ_STR(k.key, "Enter");
  CHECK(ad_key_parse("f5", &k));
  CHECK_EQ_STR(k.key, "F5");
  CHECK(!ad_key_parse("", &k));
  CHECK(!ad_key_parse("Ctrl+", &k) || strcmp(k.key, "+") == 0);
  CHECK(!ad_key_parse("Wibble", &k));
  CHECK(!ad_key_parse("F99", &k));
  char buf[40];
  ad_key_parse("alt+ctrl+x", &k);
  ad_key_format(&k, buf, sizeof buf);
  CHECK_EQ_STR(buf, "Ctrl+Alt+X");
  ad_binding b;
  CHECK(ad_binding_parse("Enter | Space", &b));
  CHECK_EQ_INT(b.n, 2);
  ad_binding_format(&b, buf, sizeof buf);
  CHECK_EQ_STR(buf, "Enter | Space");
  CHECK(!ad_binding_parse("Enter |  | Space", &b));
  CHECK(!ad_binding_parse("A | B | C | D", &b));
  for (int a = 0; a < AD_ACT_COUNT; a++) {
    ad_binding d = ad_hotkey_default((ad_action)a);
    CHECK(d.n >= 1);
    ad_action back;
    CHECK(ad_action_from_name(ad_action_name((ad_action)a), &back));
    CHECK_EQ_INT(back, a);
  }
  /* Defaults from the README. */
  ad_binding t = ad_hotkey_default(AD_ACT_TODAY);
  CHECK_EQ_STR(t.keys[0].key, "T");
  ad_binding f = ad_hotkey_default(AD_ACT_FILL);
  CHECK_EQ_INT(f.n, 2);
  ad_key sp;
  ad_key_parse("Space", &sp);
  CHECK(ad_binding_has(&f, &sp));
}

int main(void) {
  test_readme_example();
  test_roundtrip();
  test_empty_model();
  test_tolerance();
  test_errors();
  test_names();
  test_hotkeys();
  TEST_DONE();
}
