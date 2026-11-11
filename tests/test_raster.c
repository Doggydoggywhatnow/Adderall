#include "raster.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static int total(const ad_mask *m) {
  int t = 0;
  for (int i = 0; i < m->size * m->size; i++) t += m->a[i];
  return t;
}

static int px(const ad_mask *m, int x, int y) { return m->a[y * m->size + x]; }

int main(void) {
  const int S = 48;

  /* Empty cell: an outline, hollow in the middle, with its edges drawn. */
  ad_mask sq = ad_raster_square(S, 2.0f);
  CHECK(px(&sq, S / 2, S / 2) == 0);
  CHECK(px(&sq, 0, S / 2) == 255);
  CHECK(px(&sq, S / 2, S - 1) == 255);

  /* Filled: a big dot, solid in the middle, empty in the corners. */
  ad_mask dot = ad_raster_dot(S, S * 0.6f);
  CHECK(px(&dot, S / 2, S / 2) == 255);
  CHECK(px(&dot, 1, 1) == 0);
  CHECK(total(&dot) > total(&sq));

  /* Skipped: one diagonal, "/" : bottom-left to top-right, nothing at the
   * other two corners and a mark through the middle. */
  ad_mask sl = ad_raster_slash(S, 2.0f);
  CHECK(px(&sl, 2, S - 3) > 100);
  CHECK(px(&sl, S - 3, 2) > 100);
  CHECK(px(&sl, S / 2, S / 2) > 100);
  CHECK(px(&sl, 2, 2) == 0);
  CHECK(px(&sl, S - 3, S - 3) == 0);

  /* Today: a squircle ring, hollow, reaching further out than a circle of the
   * same size would (its corners are filled in). */
  ad_mask sc = ad_raster_squircle(S, 2.0f);
  CHECK(px(&sc, S / 2, S / 2) == 0);
  CHECK(px(&sc, 0, S / 2) > 200);
  CHECK(px(&sc, 4, 4) > 100);

  ad_mask_free(&sq);
  ad_mask_free(&dot);
  ad_mask_free(&sl);
  ad_mask_free(&sc);

  /* Symbols: at least 8, all different from each other, none a circle. */
  CHECK(AD_SYM_COUNT >= 8);
  ad_mask syms[AD_SYM_COUNT];
  for (int i = 0; i < AD_SYM_COUNT; i++) {
    syms[i] = ad_raster_symbol((ad_symbol)i, 40);
    CHECK(total(&syms[i]) > 40 * 40 * 255 / 12); /* visible */
    ad_symbol back;
    CHECK(ad_symbol_from_name(ad_symbol_name((ad_symbol)i), &back));
    CHECK_EQ_INT(back, i);
    /* A disc of this size fills about pi/4 of the box; no symbol may match
     * the filled-dot mark closely. */
    ad_mask d = ad_raster_dot(40, 40.0f * 0.88f);
    long diff = 0;
    for (int k = 0; k < 40 * 40; k++) diff += labs((long)d.a[k] - (long)syms[i].a[k]);
    CHECK(diff > 40 * 40 * 255 / 12);
    ad_mask_free(&d);
  }
  for (int i = 0; i < AD_SYM_COUNT; i++) {
    for (int j = i + 1; j < AD_SYM_COUNT; j++) {
      CHECK(memcmp(syms[i].a, syms[j].a, 40 * 40) != 0);
      CHECK(strcmp(ad_symbol_name((ad_symbol)i), ad_symbol_name((ad_symbol)j)) != 0);
    }
  }
  for (int i = 0; i < AD_SYM_COUNT; i++) ad_mask_free(&syms[i]);

  ad_symbol s;
  CHECK(!ad_symbol_from_name("circle", &s));
  CHECK(!ad_symbol_from_name("", &s));
  TEST_DONE();
}
