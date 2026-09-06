#ifndef __TOUCH_H__
#define __TOUCH_H__

#include <wlr/types/wlr_touch.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

void touch_create(struct wlr_touch *touch);

void handle_touch_point_surface_destroy(struct wl_listener *listener,
										void *data);

void touch_emulate_move_absolute(struct wlr_touch *touch, double x, double y,
								 uint32_t time);

void touch_emulate_button(uint32_t button, enum wl_pointer_button_state state,
						  uint32_t time);

void handle_cursor_touch_down(struct wl_listener *listener, void *data);

void handle_cursor_touch_motion(struct wl_listener *listener, void *data);

void handle_cursor_touch_up(struct wl_listener *listener, void *data);

void handle_cursor_touch_cancel(struct wl_listener *listener, void *data);

void handle_cursor_touch_frame(struct wl_listener *listener, void *data);

void touch_finish_all(void);

#endif
