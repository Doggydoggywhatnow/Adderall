#include <math.h>
#include "map.h"
#include "test.h"
#include "tz.h"

#include <stdlib.h>
#include <string.h>

static int band(const ad_map *m, double lat, double lon) {
  int q = -9999;
  if (!ad_map_band_at(m, lon, lat, &q)) return -9999;
  return q;
}

int main(void) {
  ad_map m;
  CHECK(ad_map_load(ADDERALL_TEST_DATA_DIR "/map/world.txt", &m));
  CHECK(m.n > 100);

  /* The region under a point is that place's standard UTC offset (in quarter hours). */
  CHECK_EQ_INT(band(&m, 48.85, 2.35), 4);      /* Paris        UTC+1  */
  CHECK_EQ_INT(band(&m, 40.7, -74.0), -20);    /* New York     UTC-5  */
  CHECK_EQ_INT(band(&m, 35.7, 139.7), 36);     /* Tokyo        UTC+9  */
  CHECK_EQ_INT(band(&m, 28.6, 77.2), 22);      /* Delhi        UTC+5:30 */
  CHECK_EQ_INT(band(&m, 27.7, 85.3), 23);      /* Kathmandu    UTC+5:45 */
  CHECK_EQ_INT(band(&m, 34.0, -118.2), -32);   /* Los Angeles  UTC-8  */
  CHECK_EQ_INT(band(&m, -33.9, 151.2), 40);    /* Sydney       UTC+10 */
  CHECK_EQ_INT(band(&m, 51.5, -0.1), 0);       /* London       UTC+0  */

  /* Filling land gives a picture with both land and sea in it. */
  const int w = 360, h = 180;
  uint8_t *a = (uint8_t *)calloc((size_t)w * h, 1);
  ad_map_fill(&m, 'L', INT32_MIN, w, h, a, 255);
  float px, py;
  ad_map_from_lonlat(w, h, 2.35, 48.85, &px, &py); /* Paris: land */
  CHECK(a[(int)py * w + (int)px] == 255);
  ad_map_from_lonlat(w, h, -30.0, 30.0, &px, &py); /* mid-Atlantic: sea */
  CHECK(a[(int)py * w + (int)px] == 0);
  /* Selecting one offset fills only that region. */
  uint8_t *z = (uint8_t *)calloc((size_t)w * h, 1);
  ad_map_fill(&m, 'Z', 36, w, h, z, 255);
  ad_map_from_lonlat(w, h, 139.7, 35.7, &px, &py);
  CHECK(z[(int)py * w + (int)px] == 255);
  ad_map_from_lonlat(w, h, 2.35, 48.85, &px, &py);
  CHECK(z[(int)py * w + (int)px] == 0);
  /* Outlines draw something. */
  uint8_t *o = (uint8_t *)calloc((size_t)w * h, 1);
  ad_map_outline(&m, 'Z', w, h, o, 200);
  int lit = 0;
  for (int i = 0; i < w * h; i++) lit += o[i] != 0;
  CHECK(lit > 500);

  /* Round trip of the projection. */
  double lon, lat;
  ad_map_from_lonlat(w, h, 12.5, 41.9, &px, &py);
  ad_map_to_lonlat(w, h, px, py, &lon, &lat);
  CHECK(fabs(lon - 12.5) < 1e-3 && fabs(lat - 41.9) < 1e-3);

  /* Picking a zone: a click is turned into a region, then into the nearest
   * zone of that region. */
  ad_zone_info *zones;
  const int nz = ad_zones_load(ADDERALL_TEST_DATA_DIR "/zoneinfo", &zones);
  CHECK(nz > 400);
  int *std = (int *)malloc(sizeof(int) * (size_t)nz);
  ad_zones_std_quarters(ADDERALL_TEST_DATA_DIR "/zoneinfo", zones, nz, 2026, std);
  const struct {
    double lat, lon;
    const char *want;
  } clicks[] = {{48.9, 2.4, "Europe/Paris"}, {27.7, 85.3, "Asia/Kathmandu"}, {35.7, 139.7, "Asia/Tokyo"}, {40.7, -74.0, "America/New_York"}, {-33.9, 151.2, "Australia/Sydney"}, {28.6, 77.2, "Asia/Kolkata"}};
  for (size_t i = 0; i < sizeof clicks / sizeof clicks[0]; i++) {
    int q;
    CHECK(ad_map_band_at(&m, clicks[i].lon, clicks[i].lat, &q));
    const int z = ad_zones_nearest_in(zones, nz, std, q, clicks[i].lat, clicks[i].lon);
    CHECK(z >= 0);
    if (z >= 0) {
      if (strcmp(zones[z].name, clicks[i].want) != 0) printf("  click %zu picked %s, wanted %s\n", i, zones[z].name, clicks[i].want);
      CHECK_EQ_STR(zones[z].name, clicks[i].want);
    }
  }
  /* Standard offsets: New York is UTC-5 even in summer. */
  const int ny = ad_zones_find(zones, nz, "America/New_York");
  CHECK_EQ_INT(std[ny], -20);
  const int kol = ad_zones_find(zones, nz, "Asia/Kolkata");
  CHECK_EQ_INT(std[kol], 22);
  free(std);
  free(zones);

  free(a);
  free(z);
  free(o);
  ad_map_free(&m);
  TEST_DONE();
}
