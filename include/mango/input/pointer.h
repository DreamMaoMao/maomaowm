#ifndef __INPUT_POINTER_H__
#define __INPUT_POINTER_H__

#include "mango/mango.h"
#include <stdint.h>

void toggle_hotarea(int32_t x_root, int32_t y_root);
bool pointer_is_trackpad(struct wlr_pointer *pointer);
void // Mouse scroll wheel event
handle_cursor_axis(struct wl_listener *listener, void *data);
int32_t pointer_process_swipe_end(struct wlr_pointer_swipe_end_event *event);
void handle_cursor_swipe_begin(struct wl_listener *listener, void *data);
void handle_cursor_swipe_update(struct wl_listener *listener, void *data);
void handle_cursor_swipe_end(struct wl_listener *listener, void *data);
void handle_cursor_pinch_begin(struct wl_listener *listener, void *data);
void handle_cursor_pinch_update(struct wl_listener *listener, void *data);
void handle_cursor_pinch_end(struct wl_listener *listener, void *data);
void handle_cursor_hold_begin(struct wl_listener *listener, void *data);
void handle_cursor_hold_end(struct wl_listener *listener, void *data);
Client *find_closest_tiled_client(Client *c);
void pointer_place_drag_tile(Client *c);
bool check_trackpad_disabled(struct wlr_pointer *pointer);
void // Mouse button event
handle_cursor_button(struct wl_listener *listener, void *data);
bool pointer_process_button_press(struct wlr_pointer_button_event *event);
void handle_last_cursor_surface_destroy(struct wl_listener *listener,
										void *data);
void handle_request_set_cursor_shape(struct wl_listener *listener, void *data);

void pointer_set_accel(struct libinput_device *device, bool natural_scrolling,
					   uint32_t mouse_accel_profile, double mouse_accel_speed);
void configure_pointer(struct wlr_input_device *wlr_device,
					   struct libinput_device *device);
void pointer_create(struct wlr_pointer *pointer);
void handle_new_pointer_constraint(struct wl_listener *listener, void *data);
void pointer_constrain_cursor(struct wlr_pointer_constraint_v1 *constraint);
void handle_cursor_frame(struct wl_listener *listener, void *data);
void pointer_warp_to_constraint_hint(void);
void handle_drag_icon_destroy(struct wl_listener *listener, void *data);
void handle_pointer_constraint_destroy(struct wl_listener *listener,
									   void *data);
void handle_cursor_motion_absolute(struct wl_listener *listener, void *data);
void pointer_resize_floating_window(Client *gc);
void pointer_process_motion(uint32_t time, struct wlr_input_device *device,
							double dx, double dy, double dx_unaccel,
							double dy_unaccel);
void handle_cursor_motion(struct wl_listener *listener, void *data);
void pointer_focus(Client *c, struct wlr_surface *surface, double sx, double sy,
				   uint32_t time);
void handle_request_start_drag(struct wl_listener *listener, void *data);
void handle_request_set_cursor(struct wl_listener *listener, void *data);
void handle_start_drag(struct wl_listener *listener, void *data);
void pointer_cursor_activity(void);
int32_t pointer_hide_cursor(void *data);
void pointer_warp_to_client(const Client *c);
void pointer_warp_to_monitor(Monitor *m);
void handle_new_virtual_pointer(struct wl_listener *listener, void *data);

#endif
