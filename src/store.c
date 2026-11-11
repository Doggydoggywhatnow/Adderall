#include "store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int (*ad_store_crash_hook)(int step) = NULL;

static void setmsg(char *msg, size_t cap, const char *fmt, const char *a) {
  if (msg && cap) snprintf(msg, cap, fmt, a ? a : "");
}

static bool hook(int step) { return ad_store_crash_hook && ad_store_crash_hook(step) != 0; }

void ad_store_close(ad_store *s) {
  ad_plat_unlock(s->lock);
  s->lock = NULL;
}

/* Read and parse one file. Returns the parse status; *io is set when the file
 * could not be read at all and *missing when it does not exist. */
static ad_parse_status load_file(const char *path, ad_model *model, ad_parse_error *perr, bool *io, bool *missing) {
  char *data;
  size_t len;
  *io = false;
  if (!ad_plat_read_file(path, &data, &len, missing)) {
    *io = !*missing;
    ad_model_init(model);
    return AD_PARSE_DAMAGED;
  }
  const ad_parse_status st = ad_format_parse(data, len, model, perr);
  free(data);
  return st;
}

ad_open_status ad_store_open(ad_store *s, const char *dir, ad_model *model, ad_parse_error *perr, char *msg, size_t msgcap) {
  memset(s, 0, sizeof *s);
  ad_model_init(model);
  if (msg && msgcap) msg[0] = 0;
  ad_parse_error local;
  if (!perr) perr = &local;
  memset(perr, 0, sizeof *perr);

  if (dir) snprintf(s->dir, sizeof s->dir, "%s", dir);
  else if (!ad_plat_data_dir(s->dir, sizeof s->dir)) {
    setmsg(msg, msgcap, "Adderall could not work out where its data folder should be.", NULL);
    return AD_OPEN_IO_ERROR;
  }
  const char sep = (strchr(s->dir, '\\') && !strchr(s->dir, '/')) ? '\\' : '/';
  snprintf(s->path, sizeof s->path, "%s%cadderall.md", s->dir, sep);
  snprintf(s->backup, sizeof s->backup, "%s%cadderall.md.bak", s->dir, sep);
  snprintf(s->tmp, sizeof s->tmp, "%s%cadderall.md.tmp", s->dir, sep);
  snprintf(s->damaged, sizeof s->damaged, "%s%cadderall.md.damaged", s->dir, sep);
  snprintf(s->lockpath, sizeof s->lockpath, "%s%cadderall.lock", s->dir, sep);

  if (!ad_plat_mkdir_p(s->dir)) {
    setmsg(msg, msgcap, "Adderall could not create its data folder:\n%s", s->dir);
    return AD_OPEN_IO_ERROR;
  }
  const int lk = ad_plat_lock(s->lockpath, &s->lock);
  if (lk == 1) {
    setmsg(msg, msgcap, "Adderall is already running. Only one window can use your habits at a time.", NULL);
    return AD_OPEN_LOCKED;
  }
  if (lk != 0) {
    setmsg(msg, msgcap, "Adderall could not lock its data folder:\n%s", s->dir);
    return AD_OPEN_IO_ERROR;
  }

  bool io, missing;
  const ad_parse_status st = load_file(s->path, model, perr, &io, &missing);
  if (st == AD_PARSE_OK) return AD_OPEN_OK;
  if (io) {
    s->read_only = true;
    setmsg(msg, msgcap, "Adderall could not read your data file:\n%s", s->path);
    return AD_OPEN_IO_ERROR;
  }

  if (st == AD_PARSE_LINE || st == AD_PARSE_NEWER) {
    /* An Adderall file with something unreadable in it: report, don't touch. */
    s->read_only = true;
    return AD_OPEN_PARSE_ERROR;
  }

  /* Missing or damaged main file: is there a backup? */
  ad_model_free(model);
  ad_model_init(model);
  const bool have_backup = ad_plat_file_exists(s->backup);
  if (!have_backup) {
    if (missing) return AD_OPEN_FRESH;
    s->read_only = true;
    setmsg(msg, msgcap, "Your data file is damaged and there is no backup to fall back on:\n%s", perr->message);
    return AD_OPEN_PARSE_ERROR;
  }
  ad_parse_error berr;
  const ad_parse_status bs = load_file(s->backup, model, &berr, &io, &missing);
  if (bs != AD_PARSE_OK) {
    s->read_only = true;
    *perr = berr;
    setmsg(msg, msgcap, "Your data file and its backup are both unreadable.", NULL);
    return AD_OPEN_PARSE_ERROR;
  }
  if (!missing && ad_plat_file_exists(s->path)) {
    s->keep_damaged = true;
    setmsg(msg, msgcap, "Your data file was damaged, so Adderall loaded your last backup. The damaged file is kept as adderall.md.damaged next to it.", NULL);
  } else {
    setmsg(msg, msgcap, "Your data file was missing, so Adderall loaded your last backup.", NULL);
  }
  return AD_OPEN_FROM_BACKUP;
}

ad_open_status ad_store_use_backup(ad_store *s, ad_model *model, char *msg, size_t msgcap) {
  ad_model_free(model);
  ad_model_init(model);
  if (msg && msgcap) msg[0] = 0;
  bool io, missing;
  ad_parse_error e;
  if (!ad_plat_file_exists(s->backup) || load_file(s->backup, model, &e, &io, &missing) != AD_PARSE_OK) {
    setmsg(msg, msgcap, "There is no readable backup to use.", NULL);
    ad_model_free(model);
    ad_model_init(model);
    return AD_OPEN_PARSE_ERROR;
  }
  s->read_only = false;
  s->keep_damaged = ad_plat_file_exists(s->path);
  setmsg(msg, msgcap, "Loaded your last backup. The file that could not be read is kept as adderall.md.damaged.", NULL);
  return AD_OPEN_FROM_BACKUP;
}

bool ad_store_save(ad_store *s, const ad_model *model, char *msg, size_t msgcap) {
  if (s->read_only) {
    setmsg(msg, msgcap, "Adderall will not save over a data file it could not read.", NULL);
    return false;
  }
  size_t len;
  char *text = ad_format_write(model, &len);
  if (!text) {
    setmsg(msg, msgcap, "Out of memory while saving.", NULL);
    return false;
  }
  bool ok = false;
  char bak_tmp[sizeof s->backup + 8];
  snprintf(bak_tmp, sizeof bak_tmp, "%s.tmp", s->backup);

  /* 1. Write the new version to a temporary file and flush it to disk. */
  if (!ad_plat_write_durable(s->tmp, text, len)) {
    setmsg(msg, msgcap, "Could not write a temporary file in your data folder:\n%s", s->dir);
    goto done;
  }
  if (hook(1)) goto crashed;

  /* 2. Keep the previous version as the backup. If the current file is a
   *    damaged one, keep it aside instead and leave the good backup alone. */
  if (ad_plat_file_exists(s->path)) {
    if (s->keep_damaged) {
      if (!ad_plat_copy_durable(s->path, s->damaged)) {
        setmsg(msg, msgcap, "Could not keep the damaged data file aside:\n%s", s->damaged);
        goto done;
      }
      if (hook(2)) goto crashed;
      if (hook(3)) goto crashed;
    } else {
      if (!ad_plat_copy_durable(s->path, bak_tmp)) {
        setmsg(msg, msgcap, "Could not write the backup copy:\n%s", s->backup);
        goto done;
      }
      if (hook(2)) goto crashed;
      if (!ad_plat_replace(bak_tmp, s->backup)) {
        setmsg(msg, msgcap, "Could not update the backup copy:\n%s", s->backup);
        goto done;
      }
      if (hook(3)) goto crashed;
    }
  }

  /* 3. Swap the new file over the real one in a single step. */
  if (!ad_plat_replace(s->tmp, s->path)) {
    setmsg(msg, msgcap, "Could not replace your data file:\n%s", s->path);
    goto done;
  }
  s->keep_damaged = false;
  ok = true;
  (void)hook(4); /* the save is complete: nothing left to interrupt */
  goto done;

crashed:
  setmsg(msg, msgcap, "The save was interrupted.", NULL);
  ok = false;
done:
  free(text);
  return ok;
}
