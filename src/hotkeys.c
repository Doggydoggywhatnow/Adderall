#include "hotkeys.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *const NAMES[AD_ACT_COUNT] = {"left", "right", "up", "down", "today", "fill", "skip", "habits", "settings", "layout"};
static const char *const LABELS[AD_ACT_COUNT] = {
    "Move left", "Move right", "Move up", "Move down", "Jump to today", "Fill the selected cell",
    "Skip the selected cell", "Open the habit pop-up", "Open settings", "Switch horizontal / vertical"};

const char *ad_action_name(ad_action a) { return (a >= 0 && a < AD_ACT_COUNT) ? NAMES[a] : "?"; }
const char *ad_action_label(ad_action a) { return (a >= 0 && a < AD_ACT_COUNT) ? LABELS[a] : "?"; }

bool ad_action_from_name(const char *name, ad_action *out) {
  for (int i = 0; i < AD_ACT_COUNT; i++) {
    if (strcmp(name, NAMES[i]) == 0) {
      *out = (ad_action)i;
      return true;
    }
  }
  return false;
}

static const char *const SPECIAL[] = {"Enter", "Space", "Left", "Right", "Up", "Down", "Tab", "Escape", "Backspace", "Delete",
                                      "Home", "End", "PageUp", "PageDown", "Insert"};

static bool ieq(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    a++;
    b++;
  }
  return *a == *b;
}

bool ad_key_name_valid(const char *name) {
  const size_t n = strlen(name);
  if (n == 1) {
    const unsigned char c = (unsigned char)name[0];
    return c > 32 && c < 127 && !(c >= 'a' && c <= 'z');
  }
  for (size_t i = 0; i < sizeof SPECIAL / sizeof SPECIAL[0]; i++)
    if (strcmp(name, SPECIAL[i]) == 0) return true;
  if (name[0] == 'F' && n >= 2 && n <= 3) {
    int v = 0;
    for (size_t i = 1; i < n; i++) {
      if (!isdigit((unsigned char)name[i])) return false;
      v = v * 10 + (name[i] - '0');
    }
    return v >= 1 && v <= 24;
  }
  return false;
}

static bool canonical_key(const char *tok, char out[12]) {
  const size_t n = strlen(tok);
  if (n == 0 || n >= 12) return false;
  if (n == 1) {
    const unsigned char c = (unsigned char)tok[0];
    if (c <= 32 || c >= 127) return false;
    out[0] = (char)toupper(c);
    out[1] = 0;
    return true;
  }
  if (ieq(tok, "return")) tok = "Enter";
  if (ieq(tok, "esc")) tok = "Escape";
  if (ieq(tok, "del")) tok = "Delete";
  if (ieq(tok, "pgup") || ieq(tok, "pageup")) tok = "PageUp";
  if (ieq(tok, "pgdn") || ieq(tok, "pagedown")) tok = "PageDown";
  for (size_t i = 0; i < sizeof SPECIAL / sizeof SPECIAL[0]; i++) {
    if (ieq(tok, SPECIAL[i])) {
      strcpy(out, SPECIAL[i]);
      return true;
    }
  }
  if ((tok[0] == 'f' || tok[0] == 'F') && n <= 3) {
    char buf[12];
    snprintf(buf, sizeof buf, "F%s", tok + 1);
    if (ad_key_name_valid(buf)) {
      strcpy(out, buf);
      return true;
    }
  }
  return false;
}

static bool starts_with_nocase(const char *s, const char *prefix) {
  while (*prefix) {
    if (tolower((unsigned char)*s) != *prefix) return false;
    s++;
    prefix++;
  }
  return true;
}

/* Consume a leading "ctrl+" style prefix, if there is one and something
 * follows it (so that "Ctrl++" is Ctrl and "+"). */
static bool strip_mod(const char **s, uint8_t *mods) {
  static const struct {
    const char *name;
    uint8_t bit;
  } M[] = {{"ctrl+", AD_MOD_CTRL},   {"control+", AD_MOD_CTRL}, {"alt+", AD_MOD_ALT},   {"option+", AD_MOD_ALT},
           {"shift+", AD_MOD_SHIFT}, {"cmd+", AD_MOD_CMD},      {"command+", AD_MOD_CMD}, {"meta+", AD_MOD_CMD},
           {"super+", AD_MOD_CMD}};
  for (size_t i = 0; i < sizeof M / sizeof M[0]; i++) {
    const size_t n = strlen(M[i].name);
    if (strlen(*s) > n && starts_with_nocase(*s, M[i].name)) {
      *mods |= M[i].bit;
      *s += n;
      return true;
    }
  }
  return false;
}

bool ad_key_parse(const char *s, ad_key *out) {
  memset(out, 0, sizeof *out);
  while (*s == ' ') s++;
  while (strip_mod(&s, &out->mods)) {
  }
  char tok[32];
  size_t n = strlen(s);
  while (n > 0 && s[n - 1] == ' ') n--;
  if (n == 0 || n >= sizeof tok) return false;
  memcpy(tok, s, n);
  tok[n] = 0;
  return canonical_key(tok, out->key);
}

void ad_key_format(const ad_key *k, char *out, size_t cap) {
  snprintf(out, cap, "%s%s%s%s%s", (k->mods & AD_MOD_CTRL) ? "Ctrl+" : "", (k->mods & AD_MOD_ALT) ? "Alt+" : "",
           (k->mods & AD_MOD_SHIFT) ? "Shift+" : "", (k->mods & AD_MOD_CMD) ? "Cmd+" : "", k->key);
}

bool ad_key_equal(const ad_key *a, const ad_key *b) { return a->mods == b->mods && strcmp(a->key, b->key) == 0; }

bool ad_binding_parse(const char *s, ad_binding *out) {
  memset(out, 0, sizeof *out);
  const char *p = s;
  for (;;) {
    const char *sep = strstr(p, " | ");
    char part[40];
    size_t n = sep ? (size_t)(sep - p) : strlen(p);
    if (n == 0 || n >= sizeof part || out->n >= AD_MAX_BINDINGS) return false;
    memcpy(part, p, n);
    part[n] = 0;
    ad_key k;
    if (!ad_key_parse(part, &k)) return false;
    if (!ad_binding_has(out, &k)) out->keys[out->n++] = k;
    if (!sep) break;
    p = sep + 3;
  }
  return out->n > 0;
}

void ad_binding_format(const ad_binding *b, char *out, size_t cap) {
  out[0] = 0;
  for (int i = 0; i < b->n; i++) {
    char one[40];
    ad_key_format(&b->keys[i], one, sizeof one);
    if (i) strncat(out, " | ", cap - strlen(out) - 1);
    strncat(out, one, cap - strlen(out) - 1);
  }
}

bool ad_binding_has(const ad_binding *b, const ad_key *k) {
  for (int i = 0; i < b->n; i++)
    if (ad_key_equal(&b->keys[i], k)) return true;
  return false;
}

ad_binding ad_hotkey_default(ad_action a) {
  const char *spec = "";
  switch (a) {
    case AD_ACT_LEFT: spec = "Left"; break;
    case AD_ACT_RIGHT: spec = "Right"; break;
    case AD_ACT_UP: spec = "Up"; break;
    case AD_ACT_DOWN: spec = "Down"; break;
    case AD_ACT_TODAY: spec = "T"; break;
    case AD_ACT_FILL: spec = "Enter | Space"; break;
    case AD_ACT_SKIP: spec = "S"; break;
    case AD_ACT_HABITS: spec = "H"; break;
    case AD_ACT_SETTINGS:
#ifdef __APPLE__
      spec = "Cmd+,";
#else
      spec = "Ctrl+,";
#endif
      break;
    case AD_ACT_LAYOUT: spec = "L"; break;
    default: break;
  }
  ad_binding b;
  if (!ad_binding_parse(spec, &b)) memset(&b, 0, sizeof b);
  return b;
}
