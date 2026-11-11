/* Time zones from the bundled IANA database (TZif files), with no help from
 * the operating system, so Windows behaves exactly like everyone else. */
#ifndef ADDERALL_TZ_H
#define ADDERALL_TZ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "date.h"

typedef struct ad_tz ad_tz;

/* Load zone `name` (e.g. "Europe/Paris") from `zoneinfo_dir`. NULL if unknown. */
ad_tz *ad_tz_load(const char *zoneinfo_dir, const char *name);
/* Parse the bytes of a TZif file. */
ad_tz *ad_tz_from_bytes(const uint8_t *data, size_t len);
void ad_tz_free(ad_tz *tz);

/* Seconds east of UTC at a UTC instant, daylight saving included. */
int ad_tz_offset(const ad_tz *tz, int64_t utc, bool *is_dst);

/* ---- the day, as Adderall sees it -------------------------------------- */

/* Which day is it at this instant? The day changes at `daystart_min` minutes
 * past local midnight (so at 04:00 for 240), by the wall clock of the zone. */
ad_day ad_tz_day(const ad_tz *tz, int64_t utc, int daystart_min);

/* Seconds from `utc` until the day changes next (at most one day). */
int64_t ad_tz_seconds_to_next_day(const ad_tz *tz, int64_t utc, int daystart_min);

/* ---- the list of zones, for the map and for detection ------------------- */

typedef struct {
  char name[48];
  char cc[3];
  double lat, lon; /* degrees; north and east positive */
} ad_zone_info;

/* Reads zone.tab from the folder. Returns the count; *out is malloc'ed. */
int ad_zones_load(const char *zoneinfo_dir, ad_zone_info **out);

/* The zone nearest to a point on the globe. -1 if there are none. */
int ad_zones_nearest(const ad_zone_info *z, int n, double lat, double lon);

int ad_zones_find(const ad_zone_info *z, int n, const char *name);

/* "UTC+5:30" style label for an offset in seconds. */
void ad_tz_format_offset(int offset_s, char *out, size_t cap);

/* Work out the computer's own time zone: the TZ variable, /etc/localtime,
 * and as a last resort by comparing offsets with the system clock. Writes an
 * IANA name that exists in the list (or "UTC"). */
void ad_tz_detect_system(const char *zoneinfo_dir, const ad_zone_info *z, int n, char *out, size_t cap);

#endif
