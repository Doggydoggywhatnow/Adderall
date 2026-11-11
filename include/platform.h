/* The few places where the operating systems differ: where the data folder
 * is, durable file writes, and the OS-level lock. Paths are UTF-8 everywhere;
 * the Windows build converts to wide characters at the edge. */
#ifndef ADDERALL_PLATFORM_H
#define ADDERALL_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AD_PATH_MAX 1100

/* Adderall's own data folder: ~/.local/share/adderall (or $XDG_DATA_HOME),
 * ~/Library/Application Support/Adderall, or %APPDATA%\Adderall. */
bool ad_plat_data_dir(char *out, size_t cap);

/* Create a folder (and its parents) if it is missing. */
bool ad_plat_mkdir_p(const char *path);

bool ad_plat_file_exists(const char *path);

/* Read a whole file. On failure *missing tells "does not exist" apart from
 * other errors. The buffer has one extra NUL byte after `len`. Free with free(). */
bool ad_plat_read_file(const char *path, char **data, size_t *len, bool *missing);

/* Create or truncate `path`, write, and flush to disk before returning. */
bool ad_plat_write_durable(const char *path, const void *data, size_t len);

/* Copy a file, flushing the copy to disk. */
bool ad_plat_copy_durable(const char *from, const char *to);

/* Atomically make `to` be `from`, replacing it, and flush the folder entry. */
bool ad_plat_replace(const char *from, const char *to);

bool ad_plat_remove(const char *path);

/* OS-held exclusive lock on a file (flock / LockFileEx). The lock belongs to
 * the open file, so the OS drops it when the program exits or crashes. A
 * leftover lock file does not matter. Returns 0 = got it, 1 = somebody else
 * holds it, -1 = error. */
typedef struct ad_lock ad_lock;
int ad_plat_lock(const char *path, ad_lock **out);
void ad_plat_unlock(ad_lock *lock);

/* The computer's idea of time, for detecting its time zone. */
int64_t ad_plat_now(void);                 /* seconds since 1970-01-01 UTC */
int ad_plat_local_offset(int64_t utc);     /* seconds east of UTC, by the system's rules */
/* The IANA name of the computer's time zone, if the system states one
 * ($TZ, /etc/localtime, /etc/timezone). False where it does not (Windows). */
bool ad_plat_system_tz_name(char *out, size_t cap);

#endif
