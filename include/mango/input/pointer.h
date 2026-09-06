#ifndef __INPUT_POINTER_H__
#define __INPUT_POINTER_H__

#include "mango/mango.h"
#include <stdint.h>

void toggle_hotarea(int32_t x_root, int32_t y_root);
bool pointer_is_trackpad(struct wlr_pointer *pointer);
void // 鼠标滚轮事件
axisnotify(struct wl_listener *listener, void *data);
int32_t ongesture(struct wlr_pointer_swipe_end_event *event);
void swipe_begin(struct wl_listener *listener, void *data);
void swipe_update(struct wl_listener *listener, void *data);
void swipe_end(struct wl_listener *listener, void *data);
void pinch_begin(struct wl_listener *listener, void *data);
void pinch_update(struct wl_listener *listener, void *data);
void pinch_end(struct wl_listener *listener, void *data);
void hold_begin(struct wl_listener *listener, void *data);
void hold_end(struct wl_listener *listener, void *data);
Client *find_closest_tiled_client(Client *c);
void place_drag_tile_client(Client *c);
bool check_trackpad_disabled(struct wlr_pointer *pointer);
void // 鼠标按键事件
buttonpress(struct wl_listener *listener, void *data);
bool handle_buttonpress(struct wlr_pointer_button_event *event);
void last_cursor_surface_destroy(struct wl_listener *listener, void *data);
void setcursorshape(struct wl_listener *listener, void *data);

void pointer_set_accel(struct libinput_device *device, bool natural_scrolling,
					   uint32_t mouse_accel_profile, double mouse_accel_speed);
void configure_pointer(struct wlr_input_device *wlr_device,
					   struct libinput_device *device);
void createpointer(struct wlr_pointer *pointer);
void createpointerconstraint(struct wl_listener *listener, void *data);
void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
void cursorframe(struct wl_listener *listener, void *data);
void cursorwarptohint(void);
void destroydragicon(struct wl_listener *listener, void *data);
void destroypointerconstraint(struct wl_listener *listener, void *data);
void motionabsolute(struct wl_listener *listener, void *data);
void resize_floating_window(Client *grabc);
void motionnotify(uint32_t time, struct wlr_input_device *device, double dx,
				  double dy, double dx_unaccel, double dy_unaccel);
void motionrelative(struct wl_listener *listener, void *data);
void pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
				  uint32_t time);
void requeststartdrag(struct wl_listener *listener, void *data);
void setcursor(struct wl_listener *listener, void *data);
void startdrag(struct wl_listener *listener, void *data);
void handlecursoractivity(void);
int32_t hidecursor(void *data);
void warp_cursor(const Client *c);
void warp_cursor_to_selmon(Monitor *m);
void virtualpointer(struct wl_listener *listener, void *data);

#endif
