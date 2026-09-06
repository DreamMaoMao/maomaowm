#include <wlr/types/wlr_touch.h>
#include "mango/input/touch.h"
#include "mango/common/server.h"
#include "mango/common/log.h"
#include "mango/manage/monitor.h"
#include "mango/manage/misc.h"
#include "mango/manage/client.h"
#include "mango/input/pointer.h"
#include <wayland-client-core.h>
#include "mango/ipc/ipc.h"
#include "mango/common/util.h"
#include <linux/input-event-codes.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_compositor.h>

void handle_cursor_touch_up(struct wl_listener *listener, void *data);
void handle_cursor_touch_cancel(struct wl_listener *listener, void *data);
void handle_cursor_touch_frame(struct wl_listener *listener, void *data);
struct wlr_surface *touch_get_coords(struct wlr_touch *touch, double x,
									 double y, double *x_offset,
									 double *y_offset, Client **pc);
void touch_apply_xwayland_scale(struct wlr_surface *surface, double *sx,
								double *sy);
void touch_finish_all(void);

struct touch_point {
	int32_t touch_id;
	double x_offset;
	double y_offset;
	struct wlr_surface *surface;
	struct wl_listener surface_destroy;
	bool touch_protocol; // true = touch protocol, false = mouse emulation.
	struct wl_list link;
};

// Pointer emulation state: only the first touch point that does not support
// touch emulates the pointer; pointer_touch_id isolates multiple fingers.
static bool simulating_pointer_from_touch = false;
static int32_t pointer_touch_id = -1;

// Maps the touch device to the monitor specified by touch_map_to_mon.
// Supports hot-plugging outputs and config reload; reapplied on every touch
// down.
void touch_apply_monitor_mapping(struct wlr_touch *touch) {
	if (!config.touch_map_to_mon)
		return;

	Monitor *m = NULL;
	wl_list_for_each(m, &server.monitors, link) {
		if (match_monitor_spec(config.touch_map_to_mon, m)) {
			wlr_cursor_map_input_to_output(server.cursor, &touch->base,
										   m->wlr_output);
			mango_error(true, WLR_DEBUG, "Mapping touch %s to output %s",
						touch->base.name, config.touch_map_to_mon);
			return;
		}
	}
}

void touch_create(struct wlr_touch *touch) {
	struct libinput_device *device = NULL;

	if (wlr_input_device_is_libinput(&touch->base) &&
		(device = wlr_libinput_get_device_handle(&touch->base))) {
		if (libinput_device_config_send_events_get_modes(device))
			libinput_device_config_send_events_set_mode(
				device, config.send_events_mode);
	}
	wlr_cursor_attach_input_device(server.cursor, &touch->base);
	touch_apply_monitor_mapping(touch);
}

void handle_touch_point_surface_destroy(struct wl_listener *listener,
										void *data) {
	struct touch_point *point =
		wl_container_of(listener, point, surface_destroy);
	point->surface = NULL;
	wl_list_remove(&listener->link);
	wl_list_init(&listener->link);
}

// Converts [0,1] normalized coordinates to layout coordinates, finds the
// surface under the touch point, and computes the layout -> surface offset so
// later events can be reported in relative coordinates.
struct wlr_surface *touch_get_coords(struct wlr_touch *touch, double x,
									 double y, double *x_offset,
									 double *y_offset, Client **pc) {
	double lx, ly, sx, sy;
	struct wlr_surface *surface = NULL;
	Client *c = NULL;

	wlr_cursor_absolute_to_layout_coords(server.cursor, &touch->base, x, y, &lx,
										 &ly);

	node_at_point(lx, ly, &surface, &c, NULL, NULL, &sx, &sy);

	*x_offset = lx - sx;
	*y_offset = ly - sy;

	if (pc)
		*pc = c;

	return surface;
}

// Touch emulates a mouse with absolute motion; fallback for touchscreens
// without a pointer device.
void touch_emulate_move_absolute(struct wlr_touch *touch, double x, double y,
								 uint32_t time) {
	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(server.cursor, &touch->base, x, y, &lx,
										 &ly);
	double dx = lx - server.cursor->x;
	double dy = ly - server.cursor->y;
	pointer_process_motion(time, &touch->base, dx, dy, dx, dy);
	wlr_seat_pointer_notify_frame(server.seat);
}

// Touch emulates mouse buttons (reuses the normal pointer button handling).
void touch_emulate_button(uint32_t button, enum wl_pointer_button_state state,
						  uint32_t time) {
	struct wlr_pointer_button_event ev = {
		.button = button,
		.state = state,
		.time_msec = time,
	};
	if (!pointer_process_button_press(&ev))
		wlr_seat_pointer_notify_button(server.seat, ev.time_msec, ev.button,
									   ev.state);
	wlr_seat_pointer_notify_frame(server.seat);
}

// X11 window surface-local coordinate mapping: with xwayland_ignore_scale, X11
// windows render at physical sizes, so surface-local coordinates must be
// multiplied by xwayland_scale to hit the right spot.
void touch_apply_xwayland_scale(struct wlr_surface *surface, double *sx,
								double *sy) {
#ifdef XWAYLAND
	Client *c = NULL;
	toplevel_from_wlr_surface(surface, &c, NULL);
	if (c && client_is_x11(c) && config.xwayland_ignore_scale &&
		c->xwayland_scale > 0.f) {
		*sx *= c->xwayland_scale;
		*sy *= c->xwayland_scale;
	}
#endif
}

void handle_cursor_touch_down(struct wl_listener *listener, void *data) {
	struct wlr_touch_down_event *event = data;
	struct touch_point *point = ecalloc(1, sizeof(*point));
	double x_offset = 0.0, y_offset = 0.0;
	Client *c = NULL;

	ipc_notify_device_event(&event->touch->base);

	// Ignores touch events when touch is globally disabled; if emulating a
	// pointer, clean up and stop it too.
	if (!config.touch_enable) {
		touch_finish_all();
		return;
	}

	touch_apply_monitor_mapping(event->touch);

	wlr_idle_notifier_v1_notify_activity(server.idle_notifier, server.seat);

	point->surface = touch_get_coords(event->touch, event->x, event->y,
									  &x_offset, &y_offset, &c);
	point->touch_id = event->touch_id;
	point->x_offset = x_offset;
	point->y_offset = y_offset;
	// Touch points over surfaces that accept touch use the touch protocol;
	// otherwise fall back to mouse emulation.
	point->touch_protocol = point->surface && wlr_surface_accepts_touch(
												  point->surface, server.seat);

	wl_list_insert(&server.touch_points, &point->link);
	int touch_point_count = wl_list_length(&server.touch_points);

	// Hides the cursor during touch input.
	pointer_hide_cursor(NULL);

	if (point->touch_protocol) {
		// Touch protocol touch point: exit pointer emulation and clear pointer
		// focus to avoid interference.
		simulating_pointer_from_touch = false;
		wlr_seat_pointer_notify_clear_focus(server.seat);

		double lx, ly, sx, sy;
		wlr_cursor_absolute_to_layout_coords(server.cursor, &event->touch->base,
											 event->x, event->y, &lx, &ly);
		sx = lx - x_offset;
		sy = ly - y_offset;

		// X11 window surface-local coordinate mapping (physical size under
		// xwayland_ignore_scale).
		touch_apply_xwayland_scale(point->surface, &sx, &sy);

		if (touch_point_count == 1)
			wlr_cursor_warp_absolute(server.cursor, &event->touch->base,
									 event->x, event->y);

		wl_signal_add(&point->surface->events.destroy, &point->surface_destroy);
		point->surface_destroy.notify = handle_touch_point_surface_destroy;

		wlr_seat_touch_notify_down(server.seat, point->surface,
								   event->time_msec, event->touch_id, sx, sy);
	} else if (config.touch_enable_mouse_emulation &&
			   !simulating_pointer_from_touch) {
		// Pointer emulation fallback: only the first touch point emulates the
		// pointer; pointer_touch_id isolates multiple fingers.
		simulating_pointer_from_touch = true;
		pointer_touch_id = event->touch_id;
		touch_emulate_move_absolute(event->touch, event->x, event->y,
									event->time_msec);
		touch_emulate_button(BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED,
							 event->time_msec);
	}
	// Otherwise: the surface does not accept touch and emulation is disabled,
	// or another touch point is already emulating; ignore this point.
}

void handle_cursor_touch_motion(struct wl_listener *listener, void *data) {
	struct wlr_touch_motion_event *event = data;
	struct touch_point *point;

	ipc_notify_device_event(&event->touch->base);

	if (!config.touch_enable) {
		touch_finish_all();
		return;
	}

	wlr_idle_notifier_v1_notify_activity(server.idle_notifier, server.seat);

	wl_list_for_each(point, &server.touch_points, link) {
		if (point->touch_id != event->touch_id)
			continue;

		if (point->touch_protocol && point->surface) {
			double lx, ly;
			wlr_cursor_absolute_to_layout_coords(server.cursor,
												 &event->touch->base, event->x,
												 event->y, &lx, &ly);
			double sx = lx - point->x_offset;
			double sy = ly - point->y_offset;

			touch_apply_xwayland_scale(point->surface, &sx, &sy);

			wlr_seat_touch_notify_motion(server.seat, event->time_msec,
										 event->touch_id, sx, sy);
		} else if (config.touch_enable_mouse_emulation &&
				   simulating_pointer_from_touch &&
				   point->touch_id == pointer_touch_id) {
			touch_emulate_move_absolute(event->touch, event->x, event->y,
										event->time_msec);
		}
		return;
	}
}

void handle_cursor_touch_up(struct wl_listener *listener, void *data) {
	struct wlr_touch_up_event *event = data;
	struct touch_point *point, *tmp;

	ipc_notify_device_event(&event->touch->base);

	if (!config.touch_enable) {
		touch_finish_all();
		return;
	}

	wlr_idle_notifier_v1_notify_activity(server.idle_notifier, server.seat);

	wl_list_for_each_safe(point, tmp, &server.touch_points, link) {
		if (point->touch_id != event->touch_id)
			continue;

		if (point->touch_protocol && point->surface) {
			wlr_seat_touch_notify_up(server.seat, event->time_msec,
									 event->touch_id);
			wl_list_remove(&point->surface_destroy.link);
		} else if (config.touch_enable_mouse_emulation &&
				   simulating_pointer_from_touch &&
				   point->touch_id == pointer_touch_id) {
			touch_emulate_button(BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED,
								 event->time_msec);
			simulating_pointer_from_touch = false;
			pointer_touch_id = -1;
		}

		wl_list_remove(&point->link);
		free(point);
		break;
	}
}

// Touch cancelled by the system (e.g. a global gesture takes over):
// touch-protocol points cancel the whole session; pointer-emulation points
// release the mouse and end emulation.
void handle_cursor_touch_cancel(struct wl_listener *listener, void *data) {
	struct wlr_touch_cancel_event *event = data;
	struct touch_point *point, *tmp;

	ipc_notify_device_event(&event->touch->base);

	if (!config.touch_enable) {
		touch_finish_all();
		return;
	}

	wlr_idle_notifier_v1_notify_activity(server.idle_notifier, server.seat);

	wl_list_for_each_safe(point, tmp, &server.touch_points, link) {
		if (point->touch_id != event->touch_id)
			continue;

		if (point->touch_protocol && point->surface) {
			// NULL clears all touch focus on the seat.
			wlr_seat_touch_notify_cancel(server.seat, NULL);
			wl_list_remove(&point->surface_destroy.link);
		} else if (config.touch_enable_mouse_emulation &&
				   simulating_pointer_from_touch &&
				   point->touch_id == pointer_touch_id) {
			touch_emulate_button(BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED,
								 event->time_msec);
			simulating_pointer_from_touch = false;
			pointer_touch_id = -1;
		}

		wl_list_remove(&point->link);
		free(point);
		break;
	}
}

void handle_cursor_touch_frame(struct wl_listener *listener, void *data) {
	// Sends a pointer frame during pointer emulation; otherwise sends a touch
	// frame.
	if (simulating_pointer_from_touch)
		wlr_seat_pointer_notify_frame(server.seat);
	else
		wlr_seat_touch_notify_frame(server.seat);
}

// Clears all in-progress touch points; releases the left mouse button first
// when emulating a pointer.
void touch_finish_all(void) {
	struct touch_point *point, *tmp;

	if (simulating_pointer_from_touch && config.touch_enable_mouse_emulation)
		touch_emulate_button(BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED, 0);
	simulating_pointer_from_touch = false;
	pointer_touch_id = -1;

	wl_list_for_each_safe(point, tmp, &server.touch_points, link) {
		if (point->touch_protocol && point->surface)
			wl_list_remove(&point->surface_destroy.link);
		wl_list_remove(&point->link);
		free(point);
	}
}
