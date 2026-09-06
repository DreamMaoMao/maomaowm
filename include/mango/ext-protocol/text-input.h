#ifndef __EXT_PROTOCOL_TEXT_INPUT_H__
#define __EXT_PROTOCOL_TEXT_INPUT_H__ 1

#include "mango/common/types.h"
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_text_input_v3.h>

struct mango_input_method_relay {
	struct wl_list text_inputs;
	struct wlr_input_method_v2 *input_method;
	struct wlr_surface *focused_surface;

	struct wlr_keyboard_modifiers forwarded_modifiers;

	struct text_input *active_text_input;

	struct wl_list popups;
	struct wlr_scene_tree *popup_tree;

	struct wl_listener new_text_input;
	struct wl_listener new_input_method;

	struct wl_listener input_method_commit;
	struct wl_listener input_method_grab_keyboard;
	struct wl_listener input_method_destroy;
	struct wl_listener input_method_new_popup_surface;

	struct wl_listener keyboard_grab_destroy;
	struct wl_listener focused_surface_destroy;
};

struct mango_input_method_popup {
	uint32_t type; // must at first in struct
	struct wlr_input_popup_surface_v2 *popup_surface;
	struct wlr_scene_tree *tree;
	struct wlr_scene_tree *scene_surface;
	struct mango_input_method_relay *relay;
	struct wl_list link;

	struct wl_listener destroy;
	struct wl_listener commit;
};

struct text_input {
	struct mango_input_method_relay *relay;
	struct wlr_text_input_v3 *input;
	struct wl_list link;

	struct wl_listener enable;
	struct wl_listener commit;
	struct wl_listener disable;
	struct wl_listener destroy;
};

/* ------------------- API for external callers -------------------------------
 */
bool mango_im_keyboard_grab_forward_key(KeyboardGroup *keyboard,
										struct wlr_keyboard_key_event *event);

bool mango_im_keyboard_grab_forward_modifiers(KeyboardGroup *keyboard);

struct mango_input_method_relay *mango_im_relay_create();

void mango_im_relay_finish(struct mango_input_method_relay *relay);

void mango_im_relay_set_focus(struct mango_input_method_relay *relay,
							  struct wlr_surface *surface);
/*----------------------------------------------------------*/

/* ------------------ Protocol internal code ------------------------------ */
Monitor *output_from_wlr_output(struct wlr_output *wlr_output);
bool output_is_usable(Monitor *m);
bool is_keyboard_emulated_by_input_method(
	struct wlr_keyboard *keyboard, struct wlr_input_method_v2 *input_method);
struct wlr_input_method_keyboard_grab_v2 *
get_keyboard_grab(KeyboardGroup *keyboard);
bool mango_im_keyboard_grab_forward_modifiers(KeyboardGroup *keyboard);
struct text_input *
get_active_text_input(struct mango_input_method_relay *relay);
bool mango_im_keyboard_grab_forward_key(KeyboardGroup *keyboard,
										struct wlr_keyboard_key_event *event);
void update_active_text_input(struct mango_input_method_relay *relay);
void update_text_inputs_focused_surface(struct mango_input_method_relay *relay);
void update_popup_position(struct mango_input_method_popup *popup);
void update_popups_position(struct mango_input_method_relay *relay);
void handle_input_method_commit(struct wl_listener *listener, void *data);
void handle_keyboard_grab_destroy(struct wl_listener *listener, void *data);
void handle_input_method_grab_keyboard(struct wl_listener *listener,
									   void *data);
void handle_input_method_destroy(struct wl_listener *listener, void *data);
void handle_popup_surface_destroy(struct wl_listener *listener, void *data);
void handle_popup_surface_commit(struct wl_listener *listener, void *data);
void handle_input_method_new_popup_surface(struct wl_listener *listener,
										   void *data);
void handle_new_input_method(struct wl_listener *listener, void *data);
void send_state_to_input_method(struct mango_input_method_relay *relay);
void handle_text_input_enable(struct wl_listener *listener, void *data);
void handle_text_input_disable(struct wl_listener *listener, void *data);
void handle_text_input_commit(struct wl_listener *listener, void *data);
void handle_text_input_destroy(struct wl_listener *listener, void *data);
void handle_new_text_input(struct wl_listener *listener, void *data);
void handle_focused_surface_destroy(struct wl_listener *listener, void *data);
struct mango_input_method_relay *mango_im_relay_create();
void mango_im_relay_finish(struct mango_input_method_relay *relay);
void mango_im_relay_set_focus(struct mango_input_method_relay *relay,
							  struct wlr_surface *surface);

#endif
