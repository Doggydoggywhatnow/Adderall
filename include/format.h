/* The data file: Markdown, parsed strictly and written in one fixed layout.
 * See "The file format" in the README. */
#ifndef ADDERALL_FORMAT_H
#define ADDERALL_FORMAT_H

#include <stddef.h>

#include "model.h"

/* Version written to the file. Version 1 files (without the lines added in
 * version 2) are still read. */
#define AD_FORMAT_VERSION 2

typedef enum {
  AD_PARSE_OK = 0,
  AD_PARSE_DAMAGED, /* not an Adderall file at all: empty, binary, wrong header */
  AD_PARSE_LINE,    /* an Adderall file with one line that can't be understood */
  AD_PARSE_NEWER,   /* written by a newer Adderall */
} ad_parse_status;

typedef struct {
  ad_parse_status status;
  int line;           /* 1-based, for AD_PARSE_LINE */
  char message[200];  /* what is wrong, in plain words */
  char text[120];     /* the offending line (cut short) */
} ad_parse_error;

/* Parse `len` bytes. On success `out` is filled in (and must be freed with
 * ad_model_free). On failure `out` is left empty and `err` says why. */
ad_parse_status ad_format_parse(const char *text, size_t len, ad_model *out, ad_parse_error *err);

/* Write the model in the file format. The result is malloc'ed (NUL-terminated;
 * *len excludes the NUL). */
char *ad_format_write(const ad_model *m, size_t *len);

/* "Line 12: message" or the plain message, ready to show to a person. */
void ad_parse_error_describe(const ad_parse_error *err, char *out, size_t cap);

#endif
