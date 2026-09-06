#include "mango/input/keyboard.h"
#include "mango/ipc/ipc.h"
#include "mango/input/device.h"
#include "mango/common/globals.h"
#include "mango/common/log.h"
#include "mango/manage/client.h"
#include "mango/dispatch/bind.h"
#include "mango/switcher/switcher.h"
#include "mango/input/pointer.h"
#include "ctype.h"
#include "mango/ext-protocol/text-input.h"
#include "mango/common/util.h"

void ipc_key_watch_notify(struct wl_listener *listener, void *data) {
	InputDevice *id = wl_container_of(listener, id, key_watch);
	ipc_notify_device_event(id->wlr_device);
}
/* 设备匹配：优先精确匹配 name / vendor:product:name 标识符，其次匹配类型 */
ConfigDeviceRule *find_device_rule(struct wlr_input_device *device) {
	if (!device || !config.device_rules || config.device_rules_count <= 0)
		return NULL;

	int32_t vendor = 0, product = 0;
	if (wlr_input_device_is_libinput(device)) {
		struct libinput_device *libinput_dev =
			wlr_libinput_get_device_handle(device);
		if (libinput_dev) {
			vendor = libinput_device_get_id_vendor(libinput_dev);
			product = libinput_device_get_id_product(libinput_dev);
		}
	}
	char identifier[512];
	snprintf(identifier, sizeof(identifier), "%d:%d:%s", vendor, product,
			 device->name ? device->name : "");

	const char *type = NULL;
	ConfigDeviceRule *rule;
	int32_t i;

	for (i = 0; i < config.device_rules_count; i++) {
		rule = &config.device_rules[i];
		if (!rule->name)
			continue;
		if (strcmp(rule->name, device->name) == 0 ||
			strcmp(rule->name, identifier) == 0)
			return rule;
	}

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		type = "keyboard";
		break;
	case WLR_INPUT_DEVICE_POINTER:
		if (wlr_input_device_is_libinput(device)) {
			struct libinput_device *libinput_dev =
				wlr_libinput_get_device_handle(device);
			if (libinput_dev &&
				libinput_device_config_tap_get_finger_count(libinput_dev) > 0)
				type = "touchpad";
			else
				type = "pointer";
		} else {
			type = "pointer";
		}
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		type = "touch";
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		type = "switch";
		break;
	case WLR_INPUT_DEVICE_TABLET:
		type = "tablet";
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		type = "pad";
		break;
	default:
		return NULL;
	}

	for (i = 0; i < config.device_rules_count; i++) {
		rule = &config.device_rules[i];
		if (rule->type[0] && strcmp(rule->type, type) == 0)
			return rule;
	}

	return NULL;
}

void createkeyboard(struct wlr_keyboard *keyboard) {

	struct libinput_device *device = NULL;
	InputDevice *input_dev = NULL;

	if (wlr_input_device_is_libinput(&keyboard->base) &&
		(device = wlr_libinput_get_device_handle(&keyboard->base))) {

		input_dev = calloc(1, sizeof(InputDevice));
		input_dev->wlr_device = &keyboard->base;
		input_dev->libinput_device = device;
		input_dev->device_data = keyboard;
		input_dev->key_watch.notify = ipc_key_watch_notify;
		wl_signal_add(&keyboard->events.key, &input_dev->key_watch);

		input_dev->destroy_listener.notify = destroyinputdevice;
		wl_signal_add(&keyboard->base.events.destroy,
					  &input_dev->destroy_listener);

		wl_list_insert(&inputdevices, &input_dev->link);
	}

	ConfigDeviceRule *rule = find_device_rule(&keyboard->base);
	if (rule && device_rule_has_keyboard_settings(rule) && input_dev) {
		/* 命中带键盘参数的 devicerule：独立创建，使用独立 keymap */
		input_dev->standalone = true;
		create_standalone_keyboard(input_dev, keyboard, rule);
		return;
	}

	/* Set the keymap to match the group keymap */
	wlr_keyboard_set_keymap(keyboard, kb_group->keyboard->keymap);

	wlr_keyboard_notify_modifiers(keyboard, 0, 0, locked_mods, 0);

	/* Add the new keyboard to the group */
	wlr_keyboard_group_add_keyboard(kb_group->wlr_group, keyboard);
}

bool device_rule_has_keyboard_settings(ConfigDeviceRule *rule) {
	return rule &&
		   (rule->repeat_rate != -1 || rule->repeat_delay != -1 ||
			rule->kb_rules[0] || rule->kb_model[0] || rule->kb_layout[0] ||
			rule->kb_variant[0] || rule->kb_options[0]);
}

/* 应用独立键盘的 keymap 与重复率，未设置项回退全局默认 */
/* 按 devicerule 编译 keymap：字段完全自包含，不继承全局 xkb_rules_* */
struct xkb_keymap *compile_rule_keymap(ConfigDeviceRule *rule) {
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context)
		return NULL;

	struct xkb_rule_names names = {0};
	if (rule) {
		if (rule->kb_rules[0])
			names.rules = rule->kb_rules;
		if (rule->kb_model[0])
			names.model = rule->kb_model;
		if (rule->kb_layout[0])
			names.layout = rule->kb_layout;
		if (rule->kb_variant[0])
			names.variant = rule->kb_variant;
		if (rule->kb_options[0])
			names.options = rule->kb_options;
	}

	struct xkb_keymap *keymap =
		xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
	xkb_context_unref(context);
	return keymap;
}

int32_t keyrepeat(void *data) {
	KeyboardGroup *group = data;
	int32_t i;
	if (!group->nsyms || group->keyboard->repeat_info.rate <= 0)
		return 0;

	wl_event_source_timer_update(group->key_repeat_source,
								 1000 / group->keyboard->repeat_info.rate);

	for (i = 0; i < group->nsyms; i++)
		keybinding(WL_KEYBOARD_KEY_STATE_PRESSED, false, group->mods,
				   group->keysyms[i], group->keycode);

	return 0;
}

bool is_keyboard_shortcut_inhibitor(struct wlr_surface *surface) {
	KeyboardShortcutsInhibitor *kbsinhibitor;

	wl_list_for_each(kbsinhibitor, &keyboard_shortcut_inhibitors, link) {
		if (kbsinhibitor->inhibitor->surface == surface) {
			return true;
		}
	}
	return false;
}

bool keypressglobal(struct wlr_surface *last_surface,
					struct wlr_keyboard *keyboard,
					struct wlr_keyboard_key_event *event, uint32_t mods,
					xkb_keysym_t keysym, uint32_t keycode) {
	Client *c = NULL, *lastc = focustop(selmon);
	uint32_t keycodes[32] = {0};
	int32_t reset = false;
	const char *appid = NULL;
	const char *title = NULL;
	int32_t ji;
	const ConfigWinRule *r;

	for (ji = 0; ji < config.window_rules_count; ji++) {
		r = &config.window_rules[ji];

		if (!r->globalkeybinding.mod ||
			(!r->globalkeybinding.keysymcode.keysym &&
			 !r->globalkeybinding.keysymcode.keycode.keycode1 &&
			 !r->globalkeybinding.keysymcode.keycode.keycode2 &&
			 !r->globalkeybinding.keysymcode.keycode.keycode3))
			continue;

		/* match key only (case insensitive) ignoring mods */
		if (((r->globalkeybinding.keysymcode.type == KEY_TYPE_SYM &&
			  r->globalkeybinding.keysymcode.keysym == keysym) ||
			 (r->globalkeybinding.keysymcode.type == KEY_TYPE_CODE &&
			  (r->globalkeybinding.keysymcode.keycode.keycode1 == keycode ||
			   r->globalkeybinding.keysymcode.keycode.keycode2 == keycode ||
			   r->globalkeybinding.keysymcode.keycode.keycode3 == keycode))) &&
			r->globalkeybinding.mod == mods) {
			wl_list_for_each(c, &clients, link) {
				if (c && c != lastc) {
					appid = client_get_appid(c);
					title = client_get_title(c);

					if ((r->title && regex_match(r->title, title) && !r->id) ||
						(r->id && regex_match(r->id, appid) && !r->title) ||
						(r->id && regex_match(r->id, appid) && r->title &&
						 regex_match(r->title, title))) {
						reset = true;
						wlr_seat_keyboard_enter(seat, client_surface(c),
												keycodes, 0,
												&keyboard->modifiers);
						wlr_seat_keyboard_send_key(seat, event->time_msec,
												   event->keycode,
												   event->state);
						goto done;
					}
				}
			}
		}
	}

done:
	if (reset)
		wlr_seat_keyboard_enter(seat, last_surface, keycodes, 0,
								&keyboard->modifiers);
	return reset;
}

void handle_keyboard_shortcuts_inhibitor_destroy(struct wl_listener *listener,
												 void *data) {
	KeyboardShortcutsInhibitor *inhibitor =
		wl_container_of(listener, inhibitor, destroy);

	mango_error(true, WLR_DEBUG, "Removing keyboard shortcuts inhibitor");

	wl_list_remove(&inhibitor->link);
	wl_list_remove(&inhibitor->destroy.link);
	free(inhibitor);
}

void handle_keyboard_shortcuts_inhibit_new_inhibitor(
	struct wl_listener *listener, void *data) {

	struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor = data;

	if (config.allow_shortcuts_inhibit == SHORTCUTS_INHIBIT_DISABLE) {
		return;
	}

	// per-view, seat-agnostic config via criteria
	Client *c = NULL;
	LayerSurface *l = NULL;

	int32_t type = toplevel_from_wlr_surface(inhibitor->surface, &c, &l);

	if (type < 0)
		return;

	if (type != LayerShell && c && !c->allow_shortcuts_inhibit) {
		return;
	}

	mango_error(true, WLR_DEBUG, "Adding keyboard shortcuts inhibitor");

	KeyboardShortcutsInhibitor *kbsinhibitor =
		calloc(1, sizeof(KeyboardShortcutsInhibitor));

	kbsinhibitor->inhibitor = inhibitor;

	kbsinhibitor->destroy.notify = handle_keyboard_shortcuts_inhibitor_destroy;
	wl_signal_add(&inhibitor->events.destroy, &kbsinhibitor->destroy);

	wl_list_insert(&keyboard_shortcut_inhibitors, &kbsinhibitor->link);

	wlr_keyboard_shortcuts_inhibitor_v1_activate(inhibitor);
}

#ifdef XWAYLAND
int32_t synckeymap(void *data) {
	reset_keyboard_layout();
	// we only need to sync keymap once
	mango_error(true, WLR_INFO, "timer to synckeymap done");
	wl_event_source_timer_update(sync_keymap, 0);
	return 0;
}
#endif

void standalone_keyboard_apply_config(KeyboardGroup *group,
									  ConfigDeviceRule *rule) {
	if (!group || !group->keyboard)
		return;

	wlr_keyboard_set_repeat_info(
		group->keyboard,
		rule && rule->repeat_rate != -1 ? rule->repeat_rate
										: config.repeat_rate,
		rule && rule->repeat_delay != -1 ? rule->repeat_delay
										 : config.repeat_delay);

	struct xkb_keymap *keymap = compile_rule_keymap(rule);
	if (!keymap && rule) {
		mango_error(false, WLR_ERROR,
					"Failed to compile devicerule keymap for %s, "
					"falling back to the global layout",
					group->keyboard->base.name ? group->keyboard->base.name
											   : "unknown device");
		struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		if (context) {
			keymap = xkb_keymap_new_from_names(context, &config.xkb_rules,
											   XKB_KEYMAP_COMPILE_NO_FLAGS);
			xkb_context_unref(context);
		}
	}
	/* 全局布局也编译失败时，回退到默认键盘组的 keymap */
	bool borrowed = false;
	if (!keymap && kb_group && kb_group->keyboard &&
		kb_group->keyboard->keymap) {
		keymap = kb_group->keyboard->keymap;
		borrowed = true;
	}
	if (!keymap)
		return;

	wlr_keyboard_set_keymap(group->keyboard, keymap);
	if (!borrowed)
		xkb_keymap_unref(keymap);
	wlr_keyboard_notify_modifiers(group->keyboard, 0, 0, locked_mods, 0);
}

void create_standalone_keyboard(InputDevice *input_dev,
								struct wlr_keyboard *keyboard,
								ConfigDeviceRule *rule) {
	KeyboardGroup *group = ecalloc(1, sizeof(*group));
	group->wlr_group = NULL;
	group->keyboard = keyboard;
	group->virtual_keyboard = NULL;
	wl_list_init(&group->link);

	standalone_keyboard_apply_config(group, rule);

	LISTEN(&keyboard->events.key, &group->key, keypress);
	LISTEN(&keyboard->events.modifiers, &group->modifiers, keypressmod);
	LISTEN(&keyboard->base.events.destroy, &group->destroy,
		   destroy_standalone_keyboard);

	group->key_repeat_source =
		wl_event_loop_add_timer(event_loop, keyrepeat, group);
	wl_list_insert(&standalone_keyboards, &group->link);

	input_dev->device_data = group;
}

// 让所有保存了该键盘的组失效，避免恢复时用到已销毁的键盘
static void invalidate_saved_seat_keyboard(struct wlr_keyboard *keyboard) {
	KeyboardGroup *group;
	wl_list_for_each(group, &virtual_keyboards, link) {
		if (group->prev_seat_keyboard == keyboard)
			group->prev_seat_keyboard = NULL;
	}
	wl_list_for_each(group, &standalone_keyboards, link) {
		if (group->prev_seat_keyboard == keyboard)
			group->prev_seat_keyboard = NULL;
	}
}

// 被销毁的键盘正占着 seat（或 seat 已经没键盘）时，恢复它接管前生效的
// 键盘——可能是 kb_group，也可能是 devicerule 的独立键盘
static void restore_seat_keyboard(KeyboardGroup *group) {
	struct wlr_keyboard *active = wlr_seat_get_keyboard(seat);
	if (active && active != group->keyboard)
		return;
	struct wlr_keyboard *fallback = group->prev_seat_keyboard;
	if (!fallback && kb_group)
		fallback = kb_group->keyboard;
	if (fallback)
		wlr_seat_set_keyboard(seat, fallback);
}

// 物理键盘（kb_group + devicerule 独立键盘）当前的修饰键并集，
// 供鼠标绑定这类需要看硬件按键状态的地方使用。虚拟键盘（比如输入法
// 的）不算在内，它的修饰状态可能残留或锁死，不该触发鼠标绑定
uint32_t keyboard_hard_modifiers(void) {
	uint32_t mods = 0;
	KeyboardGroup *group;

	if (kb_group && kb_group->keyboard)
		mods |= wlr_keyboard_get_modifiers(kb_group->keyboard);
	wl_list_for_each(group, &standalone_keyboards, link) {
		if (group->keyboard)
			mods |= wlr_keyboard_get_modifiers(group->keyboard);
	}
	return mods;
}

void destroy_standalone_keyboard(struct wl_listener *listener, void *data) {
	KeyboardGroup *group = wl_container_of(listener, group, destroy);
	if (group->keyboard == last_active_keyboard)
		last_active_keyboard = NULL;
	invalidate_saved_seat_keyboard(group->keyboard);
	restore_seat_keyboard(group);
	wl_list_remove(&group->key.link);
	wl_list_remove(&group->modifiers.link);
	wl_list_remove(&group->destroy.link);
	if (group->key_repeat_source) {
		wl_event_source_remove(group->key_repeat_source);
		group->key_repeat_source = NULL;
	}
	wl_list_remove(&group->link);
	free(group);
}

KeyboardGroup *createkeyboardgroup(void) {
	KeyboardGroup *group = ecalloc(1, sizeof(*group));
	struct xkb_context *context;
	struct xkb_keymap *keymap;

	group->wlr_group = wlr_keyboard_group_create();
	group->wlr_group->data = group;
	group->keyboard = &group->wlr_group->keyboard;
	group->virtual_keyboard = NULL;

	wl_list_init(&group->link);

	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context)
		die("failed to create xkb context");
	keymap = xkb_keymap_new_from_names(context, &config.xkb_rules,
									   XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap) {
		mango_error(false, WLR_ERROR,
					"Failed to compile keymap (layout=\"%s\" variant=\"%s\" "
					"options=\"%s\"), falling back to the default layout",
					config.xkb_rules.layout ? config.xkb_rules.layout : "",
					config.xkb_rules.variant ? config.xkb_rules.variant : "",
					config.xkb_rules.options ? config.xkb_rules.options : "");
		keymap = xkb_keymap_new_from_names(context, &xkb_fallback_rules,
										   XKB_KEYMAP_COMPILE_NO_FLAGS);
		if (!keymap)
			die("failed to compile keymap");
	}

	wlr_keyboard_set_keymap(group->keyboard, keymap);

	if (config.numlockon) {
		xkb_mod_index_t mod_index =
			xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_NUM);
		if (mod_index != XKB_MOD_INVALID)
			locked_mods |= (uint32_t)1 << mod_index;
	}

	if (config.capslock) {
		xkb_mod_index_t mod_index =
			xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
		if (mod_index != XKB_MOD_INVALID)
			locked_mods |= (uint32_t)1 << mod_index;
	}

	if (locked_mods)
		wlr_keyboard_notify_modifiers(group->keyboard, 0, 0, locked_mods, 0);

	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	wlr_keyboard_set_repeat_info(group->keyboard, config.repeat_rate,
								 config.repeat_delay);

	/* Set up listeners for keyboard events */
	LISTEN(&group->keyboard->events.key, &group->key, keypress);
	LISTEN(&group->keyboard->events.modifiers, &group->modifiers, keypressmod);

	group->key_repeat_source =
		wl_event_loop_add_timer(event_loop, keyrepeat, group);

	/* A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same wlr_keyboard_group, which provides a single wlr_keyboard interface
	 * for all of them. Set this combined wlr_keyboard as the seat keyboard.
	 */
	wlr_seat_set_keyboard(seat, group->keyboard);
	return group;
}

void destroykeyboardgroup(struct wl_listener *listener, void *data) {
	KeyboardGroup *group = wl_container_of(listener, group, destroy);
	if (group->keyboard == last_active_keyboard)
		last_active_keyboard = NULL;
	wl_list_remove(&group->link);
	invalidate_saved_seat_keyboard(group->keyboard);
	wl_event_source_remove(group->key_repeat_source);
	wl_list_remove(&group->key.link);
	wl_list_remove(&group->modifiers.link);
	wl_list_remove(&group->destroy.link);
	wlr_keyboard_group_destroy(group->wlr_group);
	kb_group = NULL;
	free(group);
}

int32_t // 17
keybinding(uint32_t state, bool locked, uint32_t mods, xkb_keysym_t sym,
		   uint32_t keycode) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its
	 * own processing.
	 */
	int32_t handled = 0;
	const KeyBinding *k;
	int32_t ji;

	if (is_keyboard_shortcut_inhibitor(seat->keyboard_state.focused_surface)) {
		return false;
	}

	for (ji = 0; ji < config.key_bindings_count; ji++) {
		if (locked && config.key_bindings[ji].islockapply == false)
			continue;

		if (state == WL_KEYBOARD_KEY_STATE_RELEASED &&
			config.key_bindings[ji].isreleaseapply == false)
			continue;

		if (state == WL_KEYBOARD_KEY_STATE_PRESSED &&
			config.key_bindings[ji].isreleaseapply == true)
			continue;

		if (state != WL_KEYBOARD_KEY_STATE_PRESSED &&
			state != WL_KEYBOARD_KEY_STATE_RELEASED)
			continue;

		if (state == WL_KEYBOARD_KEY_STATE_RELEASED &&
			keycode != last_hold_keycode) {
			continue;
		}

		k = &config.key_bindings[ji];
		if ((k->iscommonmode || (k->isdefaultmode && keymode.isdefault) ||
			 (strcmp(keymode.mode, k->mode) == 0)) &&
			CLEANMASK(mods) == CLEANMASK(k->mod) &&
			((k->keysymcode.type == KEY_TYPE_SYM &&
			  xkb_keysym_to_lower(sym) ==
				  xkb_keysym_to_lower(k->keysymcode.keysym)) ||
			 (k->keysymcode.type == KEY_TYPE_CODE &&
			  (keycode == k->keysymcode.keycode.keycode1 ||
			   keycode == k->keysymcode.keycode.keycode2 ||
			   keycode == k->keysymcode.keycode.keycode3))) &&
			k->func) {

			if (!k->ispassapply)
				handled = 1;
			else
				handled = 0;

			k->func(&k->arg);

			// only match the first keybind
			if (!k->isallowconflict)
				break;
		}
	}
	return handled;
}
void keypress(struct wl_listener *listener, void *data) {
	int32_t i;
	/* This event is raised when a key is pressed or released. */
	KeyboardGroup *group = wl_container_of(listener, group, key);
	struct wlr_keyboard_key_event *event = data;

	struct wlr_surface *last_surface = seat->keyboard_state.focused_surface;
	struct wlr_xdg_surface *xdg_surface =
		last_surface ? wlr_xdg_surface_try_from_wlr_surface(last_surface)
					 : NULL;
	int32_t pass = 0;
	bool hit_global = false;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface =
		last_surface ? wlr_xwayland_surface_try_from_wlr_surface(last_surface)
					 : NULL;
#endif

	if (!group->keyboard->xkb_state)
		return;

	last_active_keyboard = group->keyboard;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int32_t nsyms =
		xkb_state_key_get_syms(group->keyboard->xkb_state, keycode, &syms);

	int32_t handled = 0;
	uint32_t mods = wlr_keyboard_get_modifiers(group->keyboard);

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	// overcircle 模式下松开模式键退出 overview
	if (selmon && selmon->ov_tab_layout && !selmon->is_jump_mode &&
		selmon->isoverview && selmon->sel && !locked &&
		!group->virtual_keyboard &&
		event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
		ISMODEKEYCODE(keycode)) {
		toggleoverview(&(Arg){0});
	}

	if (switcher_is_active()) {
		if (locked) {
			switcher_close();
		} else if (!group->virtual_keyboard &&
				   event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
				   ISMODEKEYCODE(keycode)) {
			switcher_commit();
			group->nsyms = 0;
			wl_event_source_timer_update(group->key_repeat_source, 0);
		}
	}

	if (config.cursor_hide_on_keypress && !cursor_hidden &&
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		hidecursor(NULL);
	}

	if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		tag_combo = false;
	} else {
		// 避免普通绑定影响单独mod键绑定
		last_hold_keycode = keycode;
	}

	for (i = 0; i < nsyms; i++)
		handled =
			keybinding(event->state, locked, mods, syms[i], keycode) || handled;

	if (handled && group->keyboard->repeat_info.delay > 0 &&
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		group->mods = mods;
		group->keysyms = syms;
		group->keycode = keycode;
		group->nsyms = nsyms;
		wl_event_source_timer_update(group->key_repeat_source,
									 group->keyboard->repeat_info.delay);
	} else {
		group->nsyms = 0;
		wl_event_source_timer_update(group->key_repeat_source, 0);
	}

	if (handled)
		return;

	if (selmon && selmon->is_jump_mode &&
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (i = 0; i < nsyms; i++) {
			if (syms[i] == XKB_KEY_Escape) {
				togglejump(&(Arg){0});
				return;
			}
			// keysym 转字符，与 jump_labels 匹配（字母忽略大小写）
			uint32_t cp = xkb_keysym_to_utf32(syms[i]);
			if (!cp || cp >= 0x80)
				continue;
			char c_char = (char)cp;
			Client *c;
			wl_list_for_each(c, &clients, link) {
				if (c->mon == selmon && c->jump_char != '\0' &&
					!c->is_logic_hide &&
					(c_char == c->jump_char ||
					 toupper((unsigned char)c_char) ==
						 toupper((unsigned char)c->jump_char))) {
					focusclient(c, 1);
					toggleoverview(&(Arg){.tc = c});
					return;
				}
			}
		}
	}

	/* don't pass when popup is focused
	 * this is better than having popups (like fuzzel or wmenu) closing
	 * while typing in a passed keybind */
	pass = (xdg_surface && xdg_surface->role != WLR_XDG_SURFACE_ROLE_POPUP) ||
		   !last_surface
#ifdef XWAYLAND
		   || xsurface
#endif
		;
	/* passed keys don't get repeated */
	if (pass && syms)
		hit_global = keypressglobal(last_surface, group->keyboard, event, mods,
									syms[0], keycode);

	if (hit_global) {
		return;
	}
	if (!mango_im_keyboard_grab_forward_key(group, event)) {
		// prev_seat_keyboard 记的是“这把键盘接管 seat 之前”seat 上的键盘，
		// 这把键盘销毁时靠它把 seat 还回去。所以只有 seat 还不是本键盘时
		// 才算接管、才需要更新；要是无条件赋值，同一把键盘连按第二次就会
		// 把 prev 覆盖成自己，销毁时“恢复”到的就是正在销毁的这把键盘
		struct wlr_keyboard *active = wlr_seat_get_keyboard(seat);
		if (active != group->keyboard)
			group->prev_seat_keyboard = active;
		wlr_seat_set_keyboard(seat, group->keyboard);
		/* Pass unhandled keycodes along to the client. */
		wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
									 event->state);
	}
}

void keypressmod(struct wl_listener *listener, void *data) {
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	KeyboardGroup *group = wl_container_of(listener, group, modifiers);

	if (!group->keyboard->xkb_state)
		return;

	if (!mango_im_keyboard_grab_forward_modifiers(group)) {

		// prev_seat_keyboard 记的是“这把键盘接管 seat 之前”seat 上的键盘，
		// 这把键盘销毁时靠它把 seat 还回去。所以只有 seat 还不是本键盘时
		// 才算接管、才需要更新；要是无条件赋值，同一把键盘连按第二次就会
		// 把 prev 覆盖成自己，销毁时“恢复”到的就是正在销毁的这把键盘
		struct wlr_keyboard *active = wlr_seat_get_keyboard(seat);
		if (active != group->keyboard)
			group->prev_seat_keyboard = active;
		wlr_seat_set_keyboard(seat, group->keyboard);
		/* Send modifiers to the client. */
		wlr_seat_keyboard_notify_modifiers(seat, &group->keyboard->modifiers);
	}

	xkb_layout_index_t current = xkb_state_serialize_layout(
		group->keyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);

	if (current != group->layout_index) {
		group->layout_index = current;
		printstatus(IPC_WATCH_KB_LAYOUT);
	}
}

void reset_keyboard_layout(void) {
	if (!kb_group || !kb_group->wlr_group || !seat) {
		mango_error(true, WLR_ERROR, "Invalid keyboard group or seat");
		return;
	}

	struct wlr_keyboard *keyboard = kb_group->keyboard;
	if (!keyboard || !keyboard->keymap) {
		mango_error(true, WLR_ERROR, "Invalid keyboard or keymap");
		return;
	}

	// Get current layout
	xkb_layout_index_t current = xkb_state_serialize_layout(
		keyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	const int32_t num_layouts = xkb_keymap_num_layouts(keyboard->keymap);
	if (num_layouts < 1) {
		mango_error(true, WLR_INFO, "No layouts available");
		return;
	}

	// Create context
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context) {
		mango_error(true, WLR_ERROR, "Failed to create XKB context");
		return;
	}

	struct xkb_keymap *new_keymap = xkb_keymap_new_from_names(
		context, &config.xkb_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!new_keymap) {
		mango_error(true, WLR_ERROR,
					"Failed to compile keymap (invalid layout?), keeping the "
					"current keymap");
		goto cleanup_context;
	}

	// 验证新keymap是否有布局
	const int32_t new_num_layouts = xkb_keymap_num_layouts(new_keymap);
	if (new_num_layouts < 1) {
		mango_error(true, WLR_ERROR, "New keymap has no layouts");
		xkb_keymap_unref(new_keymap);
		goto cleanup_context;
	}

	// 确保当前布局索引在新keymap中有效
	if (current >= new_num_layouts) {
		mango_error(true, WLR_INFO,
					"Current layout index %u out of range for new keymap, "
					"resetting to 0",
					current);
		current = 0;
	}

	// Apply the new keymap
	uint32_t depressed = keyboard->modifiers.depressed;
	uint32_t latched = keyboard->modifiers.latched;
	uint32_t locked = keyboard->modifiers.locked;

	wlr_keyboard_set_keymap(keyboard, new_keymap);

	wlr_keyboard_notify_modifiers(keyboard, depressed, latched, locked, 0);
	keyboard->modifiers.group = current; // Keep the same layout index

	// Update seat
	wlr_seat_set_keyboard(seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(seat, &keyboard->modifiers);

	InputDevice *id;
	wl_list_for_each(id, &inputdevices, link) {
		if (id->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD ||
			id->standalone) {
			/* 独立键盘保留自己的 keymap */
			continue;
		}

		struct wlr_keyboard *tkb = (struct wlr_keyboard *)id->device_data;

		wlr_keyboard_set_keymap(tkb, keyboard->keymap);
		wlr_keyboard_notify_modifiers(tkb, depressed, latched, locked, 0);
		tkb->modifiers.group = 0;

		// 7. 更新 seat
		wlr_seat_set_keyboard(seat, tkb);
		wlr_seat_keyboard_notify_modifiers(seat, &tkb->modifiers);
	}

	// Cleanup
	xkb_keymap_unref(new_keymap);

cleanup_context:
	xkb_context_unref(context);
}
void virtualkeyboard(struct wl_listener *listener, void *data) {
	struct wlr_virtual_keyboard_v1 *kb = data;
	// 虚拟键盘不进物理键盘组，单把键盘也没必要套 wlr group，
	// 直接按独立键盘处理，用 virtual_keyboard 字段区分
	wlr_seat_set_capabilities(seat,
							  seat->capabilities | WL_SEAT_CAPABILITY_KEYBOARD);
	struct wlr_keyboard *prev = wlr_seat_get_keyboard(seat);

	KeyboardGroup *group = ecalloc(1, sizeof(*group));
	group->wlr_group = NULL;
	group->keyboard = &kb->keyboard;
	group->virtual_keyboard = &kb->keyboard;
	group->prev_seat_keyboard = prev;
	wl_list_init(&group->link);
	wl_list_insert(&virtual_keyboards, &group->link);

	// keymap/重复率：命中 devicerule 用规则里的，否则用全局配置；
	// 客户端之后发来的 keymap 会覆盖这里设的默认值
	ConfigDeviceRule *rule = find_device_rule(&kb->keyboard.base);
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap =
		(rule && device_rule_has_keyboard_settings(rule))
			? compile_rule_keymap(rule)
			: (context ? xkb_keymap_new_from_names(context, &config.xkb_rules,
												   XKB_KEYMAP_COMPILE_NO_FLAGS)
					   : NULL);
	if (keymap) {
		wlr_keyboard_set_keymap(&kb->keyboard, keymap);
		xkb_keymap_unref(keymap);
	}
	if (context)
		xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(
		&kb->keyboard,
		rule && rule->repeat_rate != -1 ? rule->repeat_rate
										: config.repeat_rate,
		rule && rule->repeat_delay != -1 ? rule->repeat_delay
										 : config.repeat_delay);

	LISTEN(&kb->keyboard.events.key, &group->key, keypress);
	LISTEN(&kb->keyboard.events.modifiers, &group->modifiers, keypressmod);
	LISTEN(&kb->keyboard.base.events.destroy, &group->destroy,
		   destroy_standalone_keyboard);

	group->key_repeat_source =
		wl_event_loop_add_timer(event_loop, keyrepeat, group);

	wlr_seat_set_keyboard(seat, &kb->keyboard);
}
