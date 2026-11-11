/* The world map for the time zone picker: land outlines and time zone
 * regions (one per UTC offset), from Natural Earth (public domain). Pure
 * geometry; the window code only turns the pictures into textures. */
#ifndef ADDERALL_MAP_H
#define ADDERALL_MAP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  char kind;     /* 'L' land, 'Z' time zone region */
  int quarters;  /* Z: UTC offset in quarter hours (5:30 is 22) */
  int n;
  int16_t *xy;   /* n points: x = (lon+180)*10, y = (90-lat)*10 */
} ad_ring;

typedef struct {
  ad_ring *rings;
  int n;
} ad_map;

#define AD_MAP_W 3600
#define AD_MAP_H 1800

bool ad_map_load(const char *path, ad_map *out);
void ad_map_free(ad_map *m);

/* Fill rings of `kind` (even-odd) into a w*h coverage buffer. For 'Z',
 * quarters selects one offset, or INT32_MIN for all. */
void ad_map_fill(const ad_map *m, char kind, int32_t quarters, int w, int h, uint8_t *alpha, uint8_t value);

/* Draw the ring outlines of `kind` as thin lines. */
void ad_map_outline(const ad_map *m, char kind, int w, int h, uint8_t *alpha, uint8_t value);

/* The region (as quarter hours of UTC offset) at a longitude/latitude. */
bool ad_map_band_at(const ad_map *m, double lon, double lat, int *quarters);

/* Window pixel <-> longitude/latitude for a map drawn w*h pixels. */
void ad_map_to_lonlat(int w, int h, float px, float py, double *lon, double *lat);
void ad_map_from_lonlat(int w, int h, double lon, double lat, float *px, float *py);

#endif
