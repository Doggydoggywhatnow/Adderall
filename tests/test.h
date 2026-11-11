/* A tiny test harness: no dependencies, works the same on every OS. */
#ifndef ADDERALL_TEST_H
#define ADDERALL_TEST_H
#include <stdio.h>
#include <string.h>

static int t_failures = 0;
static int t_checks = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    t_checks++;                                                                \
    if (!(cond)) {                                                             \
      t_failures++;                                                            \
      printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
    }                                                                          \
  } while (0)

#define CHECK_EQ_INT(a, b)                                                     \
  do {                                                                         \
    long long t_a = (long long)(a), t_b = (long long)(b);                      \
    t_checks++;                                                                \
    if (t_a != t_b) {                                                          \
      t_failures++;                                                            \
      printf("  FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__,    \
             #a, #b, t_a, t_b);                                                \
    }                                                                          \
  } while (0)

#define CHECK_EQ_STR(a, b)                                                     \
  do {                                                                         \
    const char *t_a = (a), *t_b = (b);                                         \
    t_checks++;                                                                \
    if (!t_a || !t_b || strcmp(t_a, t_b) != 0) {                               \
      t_failures++;                                                            \
      printf("  FAIL %s:%d: %s == %s (\"%s\" vs \"%s\")\n", __FILE__,          \
             __LINE__, #a, #b, t_a ? t_a : "(null)", t_b ? t_b : "(null)");    \
    }                                                                          \
  } while (0)

#define TEST_DONE()                                                            \
  do {                                                                         \
    printf("%s: %d checks, %d failed\n", __FILE__, t_checks, t_failures);      \
    return t_failures ? 1 : 0;                                                 \
  } while (0)

#endif
