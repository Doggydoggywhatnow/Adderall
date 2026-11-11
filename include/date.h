/* Calendar days. Adderall only ever deals in whole calendar days, so a day is
 * just a count of days since 1970-01-01 (the proleptic Gregorian calendar). */
#ifndef ADDERALL_DATE_H
#define ADDERALL_DATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t ad_day;

#define AD_DAY_NONE INT32_MIN

/* True for a real calendar date (handles leap years). */
bool ad_date_valid(int y, int m, int d);

ad_day ad_day_from_ymd(int y, int m, int d);
void ad_day_to_ymd(ad_day day, int *y, int *m, int *d);

/* 0 = Monday ... 6 = Sunday. */
int ad_day_weekday(ad_day day);

/* Strict "YYYY-MM-DD": exactly 10 characters, a real date, years 1900-9999. */
bool ad_day_parse(const char *s, size_t len, ad_day *out);

/* Writes "YYYY-MM-DD" and a terminating NUL. */
void ad_day_format(ad_day day, char out[11]);

/* Floor division and modulo (round toward negative infinity). */
int64_t ad_floor_div(int64_t a, int64_t b);
int64_t ad_floor_mod(int64_t a, int64_t b);

const char *ad_month_name(int m);   /* 1..12 -> "January" */
const char *ad_month_short(int m);  /* 1..12 -> "Jan" */
const char *ad_weekday_name(int wd);/* 0..6  -> "Monday" */
const char *ad_weekday_short(int wd);

#endif
