/* Tiny anti-aliased shape rasteriser. Every shape is produced as a square
 * coverage mask (one byte per pixel), which the renderer turns into a texture
 * and tints with the current theme colour. No SDL in here, so it is testable. */
#ifndef ADDERALL_RASTER_H
#define ADDERALL_RASTER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int size;     /* width == height, in pixels */
  uint8_t *a;   /* size*size coverage values, 0..255 */
} ad_mask;

void ad_mask_free(ad_mask *m);

/* Cell marks. All masks are `size` pixels square, shapes centred in them. */
ad_mask ad_raster_square(int size, float thickness);   /* empty cell outline   */
ad_mask ad_raster_dot(int size, float diameter);       /* filled: a big dot    */
ad_mask ad_raster_slash(int size, float thickness);    /* skipped: "/"         */
ad_mask ad_raster_squircle(int size, float thickness); /* today's outline      */
ad_mask ad_raster_squircle_fill(int size);        /* solid squircle       */

/* Habit symbols. None of them is a circle, so they never read as a cell mark. */
typedef enum {
  AD_SYM_STAR,
  AD_SYM_TRIANGLE,
  AD_SYM_HEXAGON,
  AD_SYM_DIAMOND,
  AD_SYM_PENTAGON,
  AD_SYM_PLUS,
  AD_SYM_CROSS,
  AD_SYM_HOURGLASS,
  AD_SYM_ARROW,
  AD_SYM_BOLT,
  AD_SYM_HEART,
  AD_SYM_HOUSE,
  AD_SYM_COUNT
} ad_symbol;

const char *ad_symbol_name(ad_symbol s);                /* "star", ... */
bool ad_symbol_from_name(const char *name, ad_symbol *out);
ad_mask ad_raster_symbol(ad_symbol s, int size);

ad_mask ad_mask_new(int size);

#endif
