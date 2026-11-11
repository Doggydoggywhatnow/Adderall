/* Opening, locking and saving the data file. */
#ifndef ADDERALL_STORE_H
#define ADDERALL_STORE_H

#include "format.h"
#include "model.h"
#include "platform.h"

typedef enum {
  AD_OPEN_OK,          /* loaded the file */
  AD_OPEN_FRESH,       /* no file yet: a new, empty model */
  AD_OPEN_FROM_BACKUP, /* the main file was missing or damaged; loaded the backup */
  AD_OPEN_LOCKED,      /* another Adderall is already running */
  AD_OPEN_PARSE_ERROR, /* a line can't be read; the file is left alone */
  AD_OPEN_IO_ERROR,    /* could not create the folder, read or lock */
} ad_open_status;

typedef struct {
  char dir[AD_PATH_MAX];
  char path[AD_PATH_MAX * 2 + 32];     /* adderall.md */
  char backup[AD_PATH_MAX * 2 + 32];   /* adderall.md.bak */
  char tmp[AD_PATH_MAX * 2 + 32];      /* adderall.md.tmp */
  char damaged[AD_PATH_MAX * 2 + 32];  /* adderall.md.damaged */
  char lockpath[AD_PATH_MAX * 2 + 32]; /* adderall.lock */
  ad_lock *lock;
  bool read_only;       /* set when the file could not be read: never write over it */
  bool keep_damaged;    /* the main file is a damaged one that must be kept aside before it is replaced */
} ad_store;

/* Test hook: called after each step of a save with the step number (1 = new
 * file written, 2 = backup written, 3 = backup in place, 4 = swapped in).
 * Returning non-zero abandons the save on the spot, as a crash would. */
extern int (*ad_store_crash_hook)(int step);

/* `dir` NULL means the OS's standard data directory. `msg` explains anything
 * that is not plain success, in words fit to show the person. `perr` is filled
 * for AD_OPEN_PARSE_ERROR. The model is initialised either way. */
ad_open_status ad_store_open(ad_store *s, const char *dir, ad_model *model, ad_parse_error *perr, char *msg, size_t msgcap);

/* Replace the file with the model, crash-safely. */
bool ad_store_save(ad_store *s, const ad_model *model, char *msg, size_t msgcap);

/* After AD_OPEN_PARSE_ERROR: load the backup instead (the unreadable file is
 * kept aside as adderall.md.damaged). */
ad_open_status ad_store_use_backup(ad_store *s, ad_model *model, char *msg, size_t msgcap);

void ad_store_close(ad_store *s);

#endif
