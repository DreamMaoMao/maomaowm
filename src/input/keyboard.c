#include "mango/input/keyboard.h"
#include "mango/ipc/ipc.h"
#include "mango/input/device.h"
#include "mango/common/server.h"
#include "mango/common/log.h"
#include "mango/manage/client.h"
#include "mango/dispatch/bind.h"
#include "mango/switcher/switcher.h"
#include "mango/input/pointer.h"
#include "ctype.h"
#include "mango/ext-protocol/text-input.h"
#include "mango/common/util.h"
#include "mango/manage/monitor.h"
#include <wlr/backend/libinput.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#ifdef XWAYLAND
#include <wlr/xwayland.h>
#endif

void handle_keyboard_key_watch(struct wl_listener *listener, void *data) {
	InputDevice *id = wl_container_of(listener, id, key_watch);
	ipc_notify_device_event(id->wlr_device);
}
/* Device matching: exact match on name / vendor:product:name identifier first,
 * then match by type. */
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

void keyboard_create(struct wlr_keyboard *keyboard) {

	struct libinput_device *device = NULL;
	InputDevice *input_dev = NULL;

	if (wlr_input_device_is_libinput(&keyboard->base) &&
		(device = wlr_libinput_get_device_handle(&keyboard->base))) {

		input_dev = calloc(1, sizeof(InputDevice));
		input_dev->wlr_device = &keyboard->base;
		input_dev->libinput_device = device;
		input_dev->device_data = keyboard;
		input_dev->key_watch.notify = handle_keyboard_key_watch;
		wl_signal_add(&keyboard->events.key, &input_dev->key_watch);

		input_dev->destroy_listener.notify = handle_input_device_destroy;
		wl_signal_add(&keyboard->base.events.destroy,
					  &input_dev->destroy_listener);

		wl_list_insert(&server.input_devices, &input_dev->link);
	}

	ConfigDeviceRule *rule = find_device_rule(&keyboard->base);
	if (rule && device_rule_has_keyboard_settings(rule) && input_dev) {
		/* Devicerule with keyboard parameters matched: create it standalone
		 * with its own keymap. */
		input_dev->standalone = true;
		create_standalone_keyboard(input_dev, keyboard, rule);
		return;
	}

	/* Set the keymap to match the group keymap */
	wlr_keyboard_set_keymap(keyboard, server.keyboard_group->keyboard->keymap);

	wlr_keyboard_notify_modifiers(keyboard, 0, 0, server.locked_modifiers, 0);

	/* Add the new keyboard to the group */
	wlr_keyboard_group_add_keyboard(server.keyboard_group->wlr_group, keyboard);
}

bool device_rule_has_keyboard_settings(ConfigDeviceRule *rule) {
	return rule &&
		   (rule->repeat_rate != -1 || rule->repeat_delay != -1 ||
			rule->kb_rules[0] || rule->kb_model[0] || rule->kb_layout[0] ||
			rule->kb_variant[0] || rule->kb_options[0]);
}

/* Applies the standalone keyboard keymap and repeat rate; unset fields fall
 * back to the global defaults. */
/* Compiles the keymap from the devicerule: fully self-contained, does not
 * inherit global xkb_rules_*. */
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

int32_t keyboard_repeat(void *data) {
	KeyboardGroup *group = data;
	int32_t i;
	if (!group->nsyms || group->keyboard->repeat_info.rate <= 0)
		return 0;

	wl_event_source_timer_update(group->key_repeat_source,
								 1000 / group->keyboard->repeat_info.rate);

	for (i = 0; i < group->nsyms; i++)
		keyboard_check_keybinding(WL_KEYBOARD_KEY_STATE_PRESSED, false,
								  group->mods, group->keysyms[i],
								  group->keycode);

	return 0;
}

bool is_keyboard_shortcut_inhibitor(struct wlr_surface *surface) {
	KeyboardShortcutsInhibitor *kbsinhibitor;

	wl_list_for_each(kbsinhibitor, &server.keyboard_shortcut_inhibitors, link) {
		if (kbsinhibitor->inhibitor->surface == surface) {
			return true;
		}
	}
	return false;
}

bool keyboard_process_global_keypress(struct wlr_surface *last_surface,
									  struct wlr_keyboard *keyboard,
									  struct wlr_keyboard_key_event *event,
									  uint32_t mods, xkb_keysym_t keysym,
									  uint32_t keycode) {
	Client *c = NULL, *lastc = client_focus_top(server.selected_monitor);
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
			wl_list_for_each(c, &server.clients, link) {
				if (c && c != lastc) {
					appid = client_get_appid(c);
					title = client_get_title(c);

					if ((r->title && regex_match(r->title, title) && !r->id) ||
						(r->id && regex_match(r->id, appid) && !r->title) ||
						(r->id && regex_match(r->id, appid) && r->title &&
						 regex_match(r->title, title))) {
						reset = true;
						wlr_seat_keyboard_enter(server.seat, client_surface(c),
												keycodes, 0,
												&keyboard->modifiers);
						wlr_seat_keyboard_send_key(
							server.seat, event->time_msec, event->keycode,
							event->state);
						goto done;
					}
				}
			}
		}
	}

done:
	if (reset)
		wlr_seat_keyboard_enter(server.seat, last_surface, keycodes, 0,
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

	wl_list_insert(&server.keyboard_shortcut_inhibitors, &kbsinhibitor->link);

	wlr_keyboard_shortcuts_inhibitor_v1_activate(inhibitor);
}

#ifdef XWAYLAND
int32_t keyboard_sync_keymap(void *data) {
	reset_keyboard_layout();
	// we only need to sync keymap once
	mango_error(true, WLR_INFO, "timer to synckeymap done");
	wl_event_source_timer_update(server.sync_keymap, 0);
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
	/* If the global layout also fails to compile, fall back to the default
	 * keyboard group keymap. */
	bool borrowed = false;
	if (!keymap && server.keyboard_group && server.keyboard_group->keyboard &&
		server.keyboard_group->keyboard->keymap) {
		keymap = server.keyboard_group->keyboard->keymap;
		borrowed = true;
	}
	if (!keymap)
		return;

	wlr_keyboard_set_keymap(group->keyboard, keymap);
	if (!borrowed)
		xkb_keymap_unref(keymap);
	wlr_keyboard_notify_modifiers(group->keyboard, 0, 0,
								  server.locked_modifiers, 0);
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

	LISTEN(&keyboard->events.key, &group->key, handle_keyboard_key);
	LISTEN(&keyboard->events.modifiers, &group->modifiers,
		   handle_keyboard_modifiers);
	LISTEN(&keyboard->base.events.destroy, &group->destroy,
		   handle_standalone_keyboard_destroy);

	group->key_repeat_source =
		wl_event_loop_add_timer(server.event_loop, keyboard_repeat, group);
	wl_list_insert(&server.standalone_keyboards, &group->link);

	input_dev->device_data = group;
}

// Invalidates all groups holding this keyboard so restore never uses a
// destroyed keyboard.
static void invalidate_saved_seat_keyboard(struct wlr_keyboard *keyboard) {
	KeyboardGroup *group;
	wl_list_for_each(group, &server.virtual_keyboards, link) {
		if (group->prev_seat_keyboard == keyboard)
			group->prev_seat_keyboard = NULL;
	}
	wl_list_for_each(group, &server.standalone_keyboards, link) {
		if (group->prev_seat_keyboard == keyboard)
			group->prev_seat_keyboard = NULL;
	}
}

// When the destroyed keyboard currently owns the seat (or the seat has no
// keyboard), restore the keyboard that was in effect before it took over:
// either keyboard_group or a devicerule standalone keyboard.
static void restore_seat_keyboard(KeyboardGroup *group) {
	struct wlr_keyboard *active = wlr_seat_get_keyboard(server.seat);
	if (active && active != group->keyboard)
		return;
	struct wlr_keyboard *fallback = group->prev_seat_keyboard;
	if (!fallback && server.keyboard_group)
		fallback = server.keyboard_group->keyboard;
	if (fallback)
		wlr_seat_set_keyboard(server.seat, fallback);
}

// Union of currently locked modifiers across physical keyboards (keyboard_group
// + devicerule standalone keyboards). Used by places that need the raw hardware
// key state, such as mouse bindings. Virtual keyboards (e.g. input method
// keyboards) are excluded; their modifier state may linger or lock and should
// not trigger mouse bindings.
uint32_t keyboard_hard_modifiers(void) {
	uint32_t mods = 0;
	KeyboardGroup *group;

	if (server.keyboard_group && server.keyboard_group->keyboard)
		mods |= wlr_keyboard_get_modifiers(server.keyboard_group->keyboard);
	wl_list_for_each(group, &server.standalone_keyboards, link) {
		if (group->keyboard)
			mods |= wlr_keyboard_get_modifiers(group->keyboard);
	}
	return mods;
}

void handle_standalone_keyboard_destroy(struct wl_listener *listener,
										void *data) {
	KeyboardGroup *group = wl_container_of(listener, group, destroy);
	if (group->keyboard == server.last_active_keyboard)
		server.last_active_keyboard = NULL;
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

KeyboardGroup *keyboard_group_create(void) {
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
			server.locked_modifiers |= (uint32_t)1 << mod_index;
	}

	if (config.capslock) {
		xkb_mod_index_t mod_index =
			xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
		if (mod_index != XKB_MOD_INVALID)
			server.locked_modifiers |= (uint32_t)1 << mod_index;
	}

	if (server.locked_modifiers)
		wlr_keyboard_notify_modifiers(group->keyboard, 0, 0,
									  server.locked_modifiers, 0);

	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	wlr_keyboard_set_repeat_info(group->keyboard, config.repeat_rate,
								 config.repeat_delay);

	/* Set up listeners for keyboard events */
	LISTEN(&group->keyboard->events.key, &group->key, handle_keyboard_key);
	LISTEN(&group->keyboard->events.modifiers, &group->modifiers,
		   handle_keyboard_modifiers);

	group->key_repeat_source =
		wl_event_loop_add_timer(server.event_loop, keyboard_repeat, group);

	/* A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same wlr_keyboard_group, which provides a single wlr_keyboard interface
	 * for all of them. Set this combined wlr_keyboard as the seat keyboard.
	 */
	wlr_seat_set_keyboard(server.seat, group->keyboard);
	return group;
}

void keyboard_group_destroy(struct wl_listener *listener, void *data) {
	KeyboardGroup *group = wl_container_of(listener, group, destroy);
	if (group->keyboard == server.last_active_keyboard)
		server.last_active_keyboard = NULL;
	wl_list_remove(&group->link);
	invalidate_saved_seat_keyboard(group->keyboard);
	wl_event_source_remove(group->key_repeat_source);
	wl_list_remove(&group->key.link);
	wl_list_remove(&group->modifiers.link);
	wl_list_remove(&group->destroy.link);
	wlr_keyboard_group_destroy(group->wlr_group);
	server.keyboard_group = NULL;
	free(group);
}

int32_t // 17
keyboard_check_keybinding(uint32_t state, bool is_locked, uint32_t mods,
						  xkb_keysym_t sym, uint32_t keycode) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its
	 * own processing.
	 */
	int32_t handled = 0;
	const KeyBinding *k;
	int32_t ji;

	if (is_keyboard_shortcut_inhibitor(
			server.seat->keyboard_state.focused_surface)) {
		return false;
	}

	for (ji = 0; ji < config.key_bindings_count; ji++) {
		if (is_locked && config.key_bindings[ji].islockapply == false)
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
			keycode != server.last_hold_keycode) {
			continue;
		}

		k = &config.key_bindings[ji];
		if ((k->iscommonmode ||
			 (k->isdefaultmode && server.key_mode.isdefault) ||
			 (strcmp(server.key_mode.mode, k->mode) == 0)) &&
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
void handle_keyboard_key(struct wl_listener *listener, void *data) {
	int32_t i;
	/* This event is raised when a key is pressed or released. */
	KeyboardGroup *group = wl_container_of(listener, group, key);
	struct wlr_keyboard_key_event *event = data;

	struct wlr_surface *last_surface =
		server.seat->keyboard_state.focused_surface;
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

	server.last_active_keyboard = group->keyboard;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int32_t nsyms =
		xkb_state_key_get_syms(group->keyboard->xkb_state, keycode, &syms);

	int32_t handled = 0;
	uint32_t mods = wlr_keyboard_get_modifiers(group->keyboard);

	wlr_idle_notifier_v1_notify_activity(server.idle_notifier, server.seat);

	// In overcircle mode, releasing the mode key exits overview.
	if (server.selected_monitor && server.selected_monitor->ov_tab_layout &&
		!server.selected_monitor->is_jump_mode &&
		server.selected_monitor->isoverview && server.selected_monitor->sel &&
		!server.session_locked && !group->virtual_keyboard &&
		event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
		ISMODEKEYCODE(keycode)) {
		toggle_overview(&(Arg){0});
	}

	if (switcher_is_active()) {
		if (server.session_locked) {
			switcher_close();
		} else if (!group->virtual_keyboard &&
				   event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
				   ISMODEKEYCODE(keycode)) {
			switcher_commit();
			group->nsyms = 0;
			wl_event_source_timer_update(group->key_repeat_source, 0);
		}
	}

	if (config.cursor_hide_on_keypress && !server.cursor_hidden &&
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		pointer_hide_cursor(NULL);
	}

	if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		server.tag_combo = false;
	} else {
		// Avoids normal bindings affecting a bare mode-key binding.
		server.last_hold_keycode = keycode;
	}

	for (i = 0; i < nsyms; i++)
		handled = keyboard_check_keybinding(event->state, server.session_locked,
											mods, syms[i], keycode) ||
				  handled;

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

	if (server.selected_monitor && server.selected_monitor->is_jump_mode &&
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (i = 0; i < nsyms; i++) {
			if (syms[i] == XKB_KEY_Escape) {
				toggle_jump(&(Arg){0});
				return;
			}
			// Converts the keysym to a character and matches it against
			// jump_labels (letters ignore case).
			uint32_t cp = xkb_keysym_to_utf32(syms[i]);
			if (!cp || cp >= 0x80)
				continue;
			char c_char = (char)cp;
			Client *c;
			wl_list_for_each(c, &server.clients, link) {
				if (c->mon == server.selected_monitor && c->jump_char != '\0' &&
					!c->is_logic_hide &&
					(c_char == c->jump_char ||
					 toupper((unsigned char)c_char) ==
						 toupper((unsigned char)c->jump_char))) {
					client_focus(c, 1);
					toggle_overview(&(Arg){.tc = c});
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
		hit_global = keyboard_process_global_keypress(
			last_surface, group->keyboard, event, mods, syms[0], keycode);

	if (hit_global) {
		return;
	}
	if (!mango_im_keyboard_grab_forward_key(group, event)) {
		// prev_seat_keyboard records the keyboard that owned the seat before
		// this one took over, so the seat can be returned to it on destroy.
		// Only update it when this keyboard does not already own the seat; an
		// unconditional assignment would overwrite prev with this very keyboard
		// on a second press and "restore" would point back to the keyboard
		// being destroyed.
		struct wlr_keyboard *active = wlr_seat_get_keyboard(server.seat);
		if (active != group->keyboard)
			group->prev_seat_keyboard = active;
		wlr_seat_set_keyboard(server.seat, group->keyboard);
		/* Pass unhandled keycodes along to the client. */
		wlr_seat_keyboard_notify_key(server.seat, event->time_msec,
									 event->keycode, event->state);
	}
}

void handle_keyboard_modifiers(struct wl_listener *listener, void *data) {
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	KeyboardGroup *group = wl_container_of(listener, group, modifiers);

	if (!group->keyboard->xkb_state)
		return;

	if (!mango_im_keyboard_grab_forward_modifiers(group)) {

		// prev_seat_keyboard records the keyboard that owned the seat before
		// this one took over, so the seat can be returned to it on destroy.
		// Only update it when this keyboard does not already own the seat; an
		// unconditional assignment would overwrite prev with this very keyboard
		// on a second press and "restore" would point back to the keyboard
		// being destroyed.
		struct wlr_keyboard *active = wlr_seat_get_keyboard(server.seat);
		if (active != group->keyboard)
			group->prev_seat_keyboard = active;
		wlr_seat_set_keyboard(server.seat, group->keyboard);
		/* Send modifiers to the client. */
		wlr_seat_keyboard_notify_modifiers(server.seat,
										   &group->keyboard->modifiers);
	}

	xkb_layout_index_t current = xkb_state_serialize_layout(
		group->keyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);

	if (current != group->layout_index) {
		group->layout_index = current;
		printstatus(IPC_WATCH_KB_LAYOUT);
	}
}

void reset_keyboard_layout(void) {
	if (!server.keyboard_group || !server.keyboard_group->wlr_group ||
		!server.seat) {
		mango_error(true, WLR_ERROR, "Invalid keyboard group or seat");
		return;
	}

	struct wlr_keyboard *keyboard = server.keyboard_group->keyboard;
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

	// Validates that the new keymap has layouts.
	const int32_t new_num_layouts = xkb_keymap_num_layouts(new_keymap);
	if (new_num_layouts < 1) {
		mango_error(true, WLR_ERROR, "New keymap has no layouts");
		xkb_keymap_unref(new_keymap);
		goto cleanup_context;
	}

	// Ensures the current layout index is valid in the new keymap.
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
	uint32_t locked_mods = keyboard->modifiers.locked;

	wlr_keyboard_set_keymap(keyboard, new_keymap);

	wlr_keyboard_notify_modifiers(keyboard, depressed, latched, locked_mods, 0);
	keyboard->modifiers.group = current; // Keep the same layout index

	// Update seat
	wlr_seat_set_keyboard(server.seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(server.seat, &keyboard->modifiers);

	InputDevice *id;
	wl_list_for_each(id, &server.input_devices, link) {
		if (id->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD ||
			id->standalone) {
			/* Standalone keyboards keep their own keymap. */
			continue;
		}

		struct wlr_keyboard *tkb = (struct wlr_keyboard *)id->device_data;

		wlr_keyboard_set_keymap(tkb, keyboard->keymap);
		wlr_keyboard_notify_modifiers(tkb, depressed, latched, locked_mods, 0);
		tkb->modifiers.group = 0;

		// 7. Update the seat
		wlr_seat_set_keyboard(server.seat, tkb);
		wlr_seat_keyboard_notify_modifiers(server.seat, &tkb->modifiers);
	}

	// Cleanup
	xkb_keymap_unref(new_keymap);

cleanup_context:
	xkb_context_unref(context);
}
void handle_new_virtual_keyboard(struct wl_listener *listener, void *data) {
	struct wlr_virtual_keyboard_v1 *kb = data;
	// Virtual keyboards do not join the physical keyboard group, and a single
	// keyboard does not need a wlr group; handle them as standalone keyboards
	// and distinguish them with the virtual_keyboard field.
	wlr_seat_set_capabilities(server.seat, server.seat->capabilities |
											   WL_SEAT_CAPABILITY_KEYBOARD);
	struct wlr_keyboard *prev = wlr_seat_get_keyboard(server.seat);

	KeyboardGroup *group = ecalloc(1, sizeof(*group));
	group->wlr_group = NULL;
	group->keyboard = &kb->keyboard;
	group->virtual_keyboard = &kb->keyboard;
	group->prev_seat_keyboard = prev;
	wl_list_init(&group->link);
	wl_list_insert(&server.virtual_keyboards, &group->link);

	// Keymap/repeat rate: use the devicerule values when matched, otherwise the
	// global config; keymaps sent later by the client override the defaults set
	// here.
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

	LISTEN(&kb->keyboard.events.key, &group->key, handle_keyboard_key);
	LISTEN(&kb->keyboard.events.modifiers, &group->modifiers,
		   handle_keyboard_modifiers);
	LISTEN(&kb->keyboard.base.events.destroy, &group->destroy,
		   handle_standalone_keyboard_destroy);

	group->key_repeat_source =
		wl_event_loop_add_timer(server.event_loop, keyboard_repeat, group);

	wlr_seat_set_keyboard(server.seat, &kb->keyboard);
}
