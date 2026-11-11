/* Platform-specific file, folder, and lock code. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "platform.h"

#include "date.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
/* ======================================================================== */
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>

static wchar_t *to_wide(const char *s) {
  const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  if (n <= 0) return NULL;
  wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
  if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
  return w;
}

static bool from_wide(const wchar_t *w, char *out, size_t cap) {
  return WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL) > 0;
}

bool ad_plat_data_dir(char *out, size_t cap) {
  const wchar_t *base = _wgetenv(L"APPDATA");
  if (!base || !*base) return false;
  char b[AD_PATH_MAX];
  if (!from_wide(base, b, sizeof b)) return false;
  return snprintf(out, cap, "%s\\Adderall", b) < (int)cap;
}

bool ad_plat_mkdir_p(const char *path) {
  wchar_t *w = to_wide(path);
  if (!w) return false;
  size_t n = wcslen(w);
  for (size_t i = 1; i <= n; i++) {
    if (w[i] == L'\\' || w[i] == L'/' || w[i] == 0) {
      const wchar_t c = w[i];
      w[i] = 0;
      if (!(i == 2 && w[1] == L':')) {
        if (!CreateDirectoryW(w, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
          free(w);
          return false;
        }
      }
      w[i] = c;
    }
  }
  free(w);
  return true;
}

bool ad_plat_file_exists(const char *path) {
  wchar_t *w = to_wide(path);
  if (!w) return false;
  const DWORD a = GetFileAttributesW(w);
  free(w);
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool ad_plat_read_file(const char *path, char **data, size_t *len, bool *missing) {
  if (missing) *missing = false;
  wchar_t *w = to_wide(path);
  if (!w) return false;
  FILE *f = _wfopen(w, L"rb");
  free(w);
  if (!f) {
    if (missing && errno == ENOENT) *missing = true;
    return false;
  }
  size_t cap = 4096, n = 0;
  char *buf = (char *)malloc(cap + 1);
  while (buf) {
    const size_t got = fread(buf + n, 1, cap - n, f);
    n += got;
    if (n < cap) break;
    cap *= 2;
    char *nb = (char *)realloc(buf, cap + 1);
    if (!nb) {
      free(buf);
      buf = NULL;
    } else buf = nb;
  }
  const bool err = ferror(f) != 0;
  fclose(f);
  if (!buf || err) {
    free(buf);
    return false;
  }
  buf[n] = 0;
  *data = buf;
  *len = n;
  return true;
}

bool ad_plat_write_durable(const char *path, const void *data, size_t len) {
  wchar_t *w = to_wide(path);
  if (!w) return false;
  HANDLE h = CreateFileW(w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  free(w);
  if (h == INVALID_HANDLE_VALUE) return false;
  bool ok = true;
  const char *p = (const char *)data;
  while (len > 0 && ok) {
    DWORD wrote = 0;
    const DWORD chunk = len > (1u << 30) ? (1u << 30) : (DWORD)len;
    if (!WriteFile(h, p, chunk, &wrote, NULL) || wrote == 0) ok = false;
    p += wrote;
    len -= wrote;
  }
  if (ok && !FlushFileBuffers(h)) ok = false;
  if (!CloseHandle(h)) ok = false;
  return ok;
}

bool ad_plat_copy_durable(const char *from, const char *to) {
  char *data;
  size_t len;
  if (!ad_plat_read_file(from, &data, &len, NULL)) return false;
  const bool ok = ad_plat_write_durable(to, data, len);
  free(data);
  return ok;
}

bool ad_plat_replace(const char *from, const char *to) {
  wchar_t *a = to_wide(from), *b = to_wide(to);
  bool ok = false;
  if (a && b) ok = MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
  free(a);
  free(b);
  return ok;
}

bool ad_plat_remove(const char *path) {
  wchar_t *w = to_wide(path);
  if (!w) return false;
  const bool ok = DeleteFileW(w) != 0 || GetLastError() == ERROR_FILE_NOT_FOUND;
  free(w);
  return ok;
}

bool ad_plat_system_tz_name(char *out, size_t cap) {
  (void)out;
  (void)cap;
  return false; /* Windows keeps its own zone names: detected by offsets instead */
}

struct ad_lock {
  HANDLE h;
};

int ad_plat_lock(const char *path, ad_lock **out) {
  wchar_t *w = to_wide(path);
  if (!w) return -1;
  HANDLE h = CreateFileW(w, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  free(w);
  if (h == INVALID_HANDLE_VALUE) return -1;
  OVERLAPPED ov;
  memset(&ov, 0, sizeof ov);
  if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov)) {
    const DWORD e = GetLastError();
    CloseHandle(h);
    return (e == ERROR_LOCK_VIOLATION || e == ERROR_IO_PENDING) ? 1 : -1;
  }
  ad_lock *l = (ad_lock *)malloc(sizeof *l);
  if (!l) {
    CloseHandle(h);
    return -1;
  }
  l->h = h;
  *out = l;
  return 0;
}

void ad_plat_unlock(ad_lock *l) {
  if (!l) return;
  CloseHandle(l->h);
  free(l);
}

#else
/* ======================================================================== */
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

bool ad_plat_data_dir(char *out, size_t cap) {
#ifdef __APPLE__
  const char *home = getenv("HOME");
  if (!home || !*home) return false;
  return snprintf(out, cap, "%s/Library/Application Support/Adderall", home) < (int)cap;
#else
  const char *xdg = getenv("XDG_DATA_HOME");
  if (xdg && xdg[0] == '/') return snprintf(out, cap, "%s/adderall", xdg) < (int)cap;
  const char *home = getenv("HOME");
  if (!home || !*home) return false;
  return snprintf(out, cap, "%s/.local/share/adderall", home) < (int)cap;
#endif
}

bool ad_plat_mkdir_p(const char *path) {
  char tmp[AD_PATH_MAX];
  if (strlen(path) >= sizeof tmp) return false;
  strcpy(tmp, path);
  for (char *p = tmp + 1; ; p++) {
    if (*p == '/' || *p == 0) {
      const char c = *p;
      *p = 0;
      if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return false;
      *p = c;
      if (c == 0) break;
    }
  }
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool ad_plat_file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool ad_plat_read_file(const char *path, char **data, size_t *len, bool *missing) {
  if (missing) *missing = false;
  FILE *f = fopen(path, "rb");
  if (!f) {
    if (missing && errno == ENOENT) *missing = true;
    return false;
  }
  size_t cap = 4096, n = 0;
  char *buf = (char *)malloc(cap + 1);
  while (buf) {
    const size_t got = fread(buf + n, 1, cap - n, f);
    n += got;
    if (n < cap) break;
    cap *= 2;
    char *nb = (char *)realloc(buf, cap + 1);
    if (!nb) {
      free(buf);
      buf = NULL;
    } else buf = nb;
  }
  const bool err = ferror(f) != 0;
  fclose(f);
  if (!buf || err) {
    free(buf);
    return false;
  }
  buf[n] = 0;
  *data = buf;
  *len = n;
  return true;
}

bool ad_plat_write_durable(const char *path, const void *data, size_t len) {
  const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) return false;
  const char *p = (const char *)data;
  bool ok = true;
  while (len > 0 && ok) {
    const ssize_t w = write(fd, p, len);
    if (w < 0) {
      if (errno == EINTR) continue;
      ok = false;
    } else {
      p += w;
      len -= (size_t)w;
    }
  }
  if (ok && fsync(fd) != 0) ok = false;
  if (close(fd) != 0) ok = false;
  return ok;
}

bool ad_plat_copy_durable(const char *from, const char *to) {
  char *data;
  size_t len;
  if (!ad_plat_read_file(from, &data, &len, NULL)) return false;
  const bool ok = ad_plat_write_durable(to, data, len);
  free(data);
  return ok;
}

static void sync_parent_dir(const char *path) {
  char dir[AD_PATH_MAX];
  if (strlen(path) >= sizeof dir) return;
  strcpy(dir, path);
  char *slash = strrchr(dir, '/');
  if (!slash) return;
  if (slash == dir) slash[1] = 0;
  else *slash = 0;
  const int fd = open(dir, O_RDONLY);
  if (fd >= 0) {
    fsync(fd);
    close(fd);
  }
}

bool ad_plat_replace(const char *from, const char *to) {
  if (rename(from, to) != 0) return false;
  sync_parent_dir(to);
  return true;
}

bool ad_plat_remove(const char *path) {
  return unlink(path) == 0 || errno == ENOENT;
}

bool ad_plat_system_tz_name(char *out, size_t cap) {
  const char *tz = getenv("TZ");
  if (tz && *tz) {
    if (*tz == ':') tz++;
    if (*tz && *tz != '/') {
      snprintf(out, cap, "%s", tz);
      return true;
    }
  }
  char link[AD_PATH_MAX];
  const ssize_t n = readlink("/etc/localtime", link, sizeof link - 1);
  if (n > 0) {
    link[n] = 0;
    const char *z = strstr(link, "zoneinfo/");
    if (z) {
      snprintf(out, cap, "%s", z + 9);
      return true;
    }
  }
  FILE *f = fopen("/etc/timezone", "r");
  if (f) {
    char line[128];
    const bool ok = fgets(line, sizeof line, f) != NULL;
    fclose(f);
    if (ok) {
      line[strcspn(line, "\r\n")] = 0;
      if (line[0]) {
        snprintf(out, cap, "%s", line);
        return true;
      }
    }
  }
  return false;
}

struct ad_lock {
  int fd;
};

int ad_plat_lock(const char *path, ad_lock **out) {
  const int fd = open(path, O_RDWR | O_CREAT, 0600);
  if (fd < 0) return -1;
  if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
    const int e = errno;
    close(fd);
    return (e == EWOULDBLOCK || e == EAGAIN) ? 1 : -1;
  }
  ad_lock *l = (ad_lock *)malloc(sizeof *l);
  if (!l) {
    close(fd);
    return -1;
  }
  l->fd = fd;
  *out = l;
  return 0;
}

void ad_plat_unlock(ad_lock *l) {
  if (!l) return;
  flock(l->fd, LOCK_UN);
  close(l->fd);
  free(l);
}
#endif

/* ======================================================================== */
/* Shared: the system clock. */
#include <time.h>

int64_t ad_plat_now(void) { return (int64_t)time(NULL); }

int ad_plat_local_offset(int64_t utc) {
  time_t t = (time_t)utc;
  struct tm lt, gt;
#ifdef _WIN32
  localtime_s(&lt, &t);
  gmtime_s(&gt, &t);
#else
  localtime_r(&t, &lt);
  gmtime_r(&t, &gt);
#endif
  /* Compare the two wall clocks as if both were UTC. */
  const int64_t ld = ad_day_from_ymd(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
  const int64_t gd = ad_day_from_ymd(gt.tm_year + 1900, gt.tm_mon + 1, gt.tm_mday);
  const long diff = (long)((ld - gd) * 86400 + (lt.tm_hour - gt.tm_hour) * 3600 + (lt.tm_min - gt.tm_min) * 60 + (lt.tm_sec - gt.tm_sec));
  return (int)diff;
}
