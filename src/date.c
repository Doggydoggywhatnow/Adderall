#include "date.h"


int64_t ad_floor_div(int64_t a, int64_t b) {
  int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
  return q;
}

int64_t ad_floor_mod(int64_t a, int64_t b) {
  return a - ad_floor_div(a, b) * b;
}

static bool is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static int days_in_month(int y, int m) {
  static const int dm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (m == 2 && is_leap(y)) ? 29 : dm[m - 1];
}

bool ad_date_valid(int y, int m, int d) {
  if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1) return false;
  return d <= days_in_month(y, m);
}

/* Howard Hinnant's days_from_civil / civil_from_days. */
ad_day ad_day_from_ymd(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = ad_floor_div(y, 400);
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (ad_day)(era * 146097 + doe - 719468);
}

void ad_day_to_ymd(ad_day day, int *y, int *m, int *d) {
  int64_t z = (int64_t)day + 719468;
  const int64_t era = ad_floor_div(z, 146097);
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t yy = yoe + era * 400;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  const int dd = (int)(doy - (153 * mp + 2) / 5 + 1);
  const int mm = (int)(mp < 10 ? mp + 3 : mp - 9);
  if (mm <= 2) yy++;
  if (y) *y = (int)yy;
  if (m) *m = mm;
  if (d) *d = dd;
}

int ad_day_weekday(ad_day day) {
  /* 1970-01-01 was a Thursday (index 3 with Monday = 0). */
  return (int)ad_floor_mod((int64_t)day + 3, 7);
}

static bool digits(const char *s, int n, int *out) {
  int v = 0;
  for (int i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10 + (s[i] - '0');
  }
  *out = v;
  return true;
}

bool ad_day_parse(const char *s, size_t len, ad_day *out) {
  int y, m, d;
  if (len != 10 || s[4] != '-' || s[7] != '-') return false;
  if (!digits(s, 4, &y) || !digits(s + 5, 2, &m) || !digits(s + 8, 2, &d)) return false;
  if (y < 1900 || !ad_date_valid(y, m, d)) return false;
  *out = ad_day_from_ymd(y, m, d);
  return true;
}

void ad_day_format(ad_day day, char out[11]) {
  int y, m, d;
  ad_day_to_ymd(day, &y, &m, &d);
  if (y < 0) y = 0;
  if (y > 9999) y = 9999;
  out[0] = (char)('0' + y / 1000);
  out[1] = (char)('0' + y / 100 % 10);
  out[2] = (char)('0' + y / 10 % 10);
  out[3] = (char)('0' + y % 10);
  out[4] = '-';
  out[5] = (char)('0' + m / 10);
  out[6] = (char)('0' + m % 10);
  out[7] = '-';
  out[8] = (char)('0' + d / 10);
  out[9] = (char)('0' + d % 10);
  out[10] = 0;
}

static const char *const MONTHS[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
static const char *const MONTHS_S[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char *const WEEKDAYS[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
static const char *const WEEKDAYS_S[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

const char *ad_month_name(int m) { return (m >= 1 && m <= 12) ? MONTHS[m - 1] : "?"; }
const char *ad_month_short(int m) { return (m >= 1 && m <= 12) ? MONTHS_S[m - 1] : "?"; }
const char *ad_weekday_name(int wd) { return (wd >= 0 && wd <= 6) ? WEEKDAYS[wd] : "?"; }
const char *ad_weekday_short(int wd) { return (wd >= 0 && wd <= 6) ? WEEKDAYS_S[wd] : "?"; }
