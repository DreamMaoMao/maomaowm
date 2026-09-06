#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include <wlr/types/wlr_keyboard.h>
#include "mango/mango.h"
#include "mango/config/parse_config.h"

void create_standalone_keyboard(InputDevice *input_dev,
								struct wlr_keyboard *keyboard,
								ConfigDeviceRule *rule);

bool device_rule_has_keyboard_settings(ConfigDeviceRule *rule);

void createkeyboard(struct wlr_keyboard *keyboard);

void destroy_standalone_keyboard(struct wl_listener *listener, void *data);

KeyboardGroup *createkeyboardgroup(void);

void destroykeyboardgroup(struct wl_listener *listener, void *data);

int32_t keyrepeat(void *data);

bool is_keyboard_shortcut_inhibitor(struct wlr_surface *surface);

int32_t keybinding(uint32_t state, bool locked, uint32_t mods, xkb_keysym_t sym,
				   uint32_t keycode);

bool keypressglobal(struct wlr_surface *last_surface,
					struct wlr_keyboard *keyboard,
					struct wlr_keyboard_key_event *event, uint32_t mods,
					xkb_keysym_t keysym, uint32_t keycode);

void keypress(struct wl_listener *listener, void *data);

void keypressmod(struct wl_listener *listener, void *data);

void reset_keyboard_layout(void);

void handle_keyboard_shortcuts_inhibit_new_inhibitor(
	struct wl_listener *listener, void *data);

void virtualkeyboard(struct wl_listener *listener, void *data);

int32_t synckeymap(void *data);
// 物理键盘（kb_group + devicerule 独立键盘）当前的修饰键并集，
// 供鼠标绑定这类需要看硬件按键状态的地方使用。虚拟键盘（比如输入法
// 的）不算在内，它的修饰状态可能残留或锁死，不该触发鼠标绑定
uint32_t keyboard_hard_modifiers(void);

#endif
