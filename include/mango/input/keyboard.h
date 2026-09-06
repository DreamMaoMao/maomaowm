#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include <wlr/types/wlr_keyboard.h>
#include "mango/mango.h"
#include "mango/config/parse_config.h"

void create_standalone_keyboard(InputDevice *input_dev,
								struct wlr_keyboard *keyboard,
								ConfigDeviceRule *rule);

bool device_rule_has_keyboard_settings(ConfigDeviceRule *rule);

void keyboard_create(struct wlr_keyboard *keyboard);

void handle_standalone_keyboard_destroy(struct wl_listener *listener, void *data);

KeyboardGroup *keyboard_group_create(void);

void keyboard_group_destroy(struct wl_listener *listener, void *data);

int32_t keyboard_repeat(void *data);

bool is_keyboard_shortcut_inhibitor(struct wlr_surface *surface);

int32_t keyboard_check_keybinding(uint32_t state, bool locked, uint32_t mods,
								  xkb_keysym_t sym, uint32_t keycode);

bool keyboard_process_global_keypress(struct wlr_surface *last_surface,
									  struct wlr_keyboard *keyboard,
									  struct wlr_keyboard_key_event *event,
									  uint32_t mods, xkb_keysym_t keysym,
									  uint32_t keycode);

void handle_keyboard_key(struct wl_listener *listener, void *data);

void handle_keyboard_modifiers(struct wl_listener *listener, void *data);

void reset_keyboard_layout(void);

void handle_keyboard_shortcuts_inhibit_new_inhibitor(
	struct wl_listener *listener, void *data);

void handle_new_virtual_keyboard(struct wl_listener *listener, void *data);

int32_t keyboard_sync_keymap(void *data);
// Union of currently locked modifiers across physical keyboards (keyboard_group
// + devicerule standalone keyboards). Used by places that need the raw hardware
// key state, such as mouse bindings. Virtual keyboards (e.g. input method
// keyboards) are excluded; their modifier state may linger or lock and should
// not trigger mouse bindings.
uint32_t keyboard_hard_modifiers(void);

#endif
