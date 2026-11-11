/* Hotkeys: actions, key names, and the defaults. Keys are stored as readable
 * text ("T", "Ctrl+,", "Enter | Space") so the data file stays friendly. */
#ifndef ADDERALL_HOTKEYS_H
#define ADDERALL_HOTKEYS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  AD_ACT_LEFT,
  AD_ACT_RIGHT,
  AD_ACT_UP,
  AD_ACT_DOWN,
  AD_ACT_TODAY,
  AD_ACT_FILL,
  AD_ACT_SKIP,
  AD_ACT_HABITS,
  AD_ACT_SETTINGS,
  AD_ACT_LAYOUT,
  AD_ACT_COUNT
} ad_action;

const char *ad_action_name(ad_action a);  /* file name: "today" */
const char *ad_action_label(ad_action a); /* for people: "Jump to today" */
bool ad_action_from_name(const char *name, ad_action *out);

enum { AD_MOD_CTRL = 1, AD_MOD_ALT = 2, AD_MOD_SHIFT = 4, AD_MOD_CMD = 8 };

typedef struct {
  uint8_t mods;
  char key[12]; /* canonical: "T", ",", "Enter", "Space", "Left", "F5", ... */
} ad_key;

#define AD_MAX_BINDINGS 3
typedef struct {
  ad_key keys[AD_MAX_BINDINGS];
  int n;
} ad_binding;

/* Parse one key like "ctrl+,", "T", "Enter". Names are case-insensitive. */
bool ad_key_parse(const char *s, ad_key *out);
void ad_key_format(const ad_key *k, char *out, size_t cap);
bool ad_key_equal(const ad_key *a, const ad_key *b);

/* A binding is one to AD_MAX_BINDINGS keys separated by " | ". */
bool ad_binding_parse(const char *s, ad_binding *out);
void ad_binding_format(const ad_binding *b, char *out, size_t cap);
bool ad_binding_has(const ad_binding *b, const ad_key *k);

/* The built-in binding for an action (Cmd+, on macOS, Ctrl+, elsewhere). */
ad_binding ad_hotkey_default(ad_action a);

/* Is this a key name that makes sense as a canonical key? */
bool ad_key_name_valid(const char *name);

#endif
