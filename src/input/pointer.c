#include "mango/input/pointer.h"
#include "mango/animation/client.h"
#include "mango/common/server.h"
#include "mango/dispatch/bind.h"
#include "mango/layout/arrange.h"
#include "mango/layout/layout.h"
#include "mango/layout/scroll.h"
#include "mango/layout/dwindle.h"
#include "mango/switcher/switcher.h"
#include "mango/common/util.h"
#include "mango/ipc/ipc.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include "mango/manage/layer.h"
#include "mango/manage/misc.h"
#include "mango/input/device.h"
#include "mango/input/keyboard.h"

static struct LastCursor last_cursor;

void toggle_hotarea(int32_t x_root, int32_t y_root) {
	// Computes the hot-area coordinates in the lower-left corner; supports
	// multiple monitors.
	Arg arg = {0};

	// At startup selected_monitor may be NULL while the mouse is already in the
	// hot area, so this must be checked to avoid a crash.
	if (!server.selected_monitor)
		return;

	if (server.grab_client)
		return;

	// Computes different hot-area coordinates for each hot corner.
	unsigned hx, hy;

	switch (config.hotarea_corner) {
	case BOTTOM_RIGHT: // Bottom-right corner
		hx = server.selected_monitor->m.x + server.selected_monitor->m.width -
			 config.hotarea_size;
		hy = server.selected_monitor->m.y + server.selected_monitor->m.height -
			 config.hotarea_size;
		break;
	case TOP_LEFT: // Top-left corner
		hx = server.selected_monitor->m.x + config.hotarea_size;
		hy = server.selected_monitor->m.y + config.hotarea_size;
		break;
	case TOP_RIGHT: // Top-right corner
		hx = server.selected_monitor->m.x + server.selected_monitor->m.width -
			 config.hotarea_size;
		hy = server.selected_monitor->m.y + config.hotarea_size;
		break;
	case BOTTOM_LEFT: // Bottom-left corner (default)
	default:
		hx = server.selected_monitor->m.x + config.hotarea_size;
		hy = server.selected_monitor->m.y + server.selected_monitor->m.height -
			 config.hotarea_size;
		break;
	}

	// Checks whether the pointer is inside the hot area.
	int in_hotarea = 0;

	switch (config.hotarea_corner) {
	case BOTTOM_RIGHT: // Bottom-right corner
		in_hotarea = (y_root > hy && x_root > hx &&
					  x_root <= (server.selected_monitor->m.x +
								 server.selected_monitor->m.width) &&
					  y_root <= (server.selected_monitor->m.y +
								 server.selected_monitor->m.height));
		break;
	case TOP_LEFT: // Top-left corner
		in_hotarea = (y_root < hy && x_root < hx &&
					  x_root >= server.selected_monitor->m.x &&
					  y_root >= server.selected_monitor->m.y);
		break;
	case TOP_RIGHT: // Top-right corner
		in_hotarea = (y_root < hy && x_root > hx &&
					  x_root <= (server.selected_monitor->m.x +
								 server.selected_monitor->m.width) &&
					  y_root >= server.selected_monitor->m.y);
		break;
	case BOTTOM_LEFT: // Bottom-left corner (default)
	default:
		in_hotarea = (y_root > hy && x_root < hx &&
					  x_root >= server.selected_monitor->m.x &&
					  y_root <= (server.selected_monitor->m.y +
								 server.selected_monitor->m.height));
		break;
	}

	if (config.enable_hotarea == 1 &&
		server.selected_monitor->is_in_hotarea == 0 && in_hotarea) {
		/* Hot-area entry: uses the normal grid layout. */
		server.selected_monitor->ov_normal_mode = 1;
		toggle_overview(&arg);
		server.selected_monitor->is_in_hotarea = 1;
	} else if (config.enable_hotarea == 1 &&
			   server.selected_monitor->is_in_hotarea == 1 && !in_hotarea) {
		server.selected_monitor->is_in_hotarea = 0;
	}
}

bool pointer_is_trackpad(struct wlr_pointer *pointer) {
	struct libinput_device *device;

	if (wlr_input_device_is_libinput(&pointer->base) &&
		(device = wlr_libinput_get_device_handle(&pointer->base))) {
		if (libinput_device_config_tap_get_finger_count(device) > 0) {
			return true;
		}
	}

	return false;
}

void // Mouse scroll wheel event
handle_cursor_axis(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct wlr_pointer_axis_event *event = data;
	ipc_notify_device_event(&event->pointer->base);
	uint32_t mods;
	AxisBinding *a;
	int32_t ji;
	uint32_t adir;
	double target_scroll_factor;
	// IDLE_NOTIFY_ACTIVITY;
	pointer_cursor_activity();
	wlr_idle_notifier_v1_notify_activity(server.idle_notifier, server.seat);

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	mods = keyboard_hard_modifiers();

	if (event->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL)
		adir = event->delta > 0 ? AxisDown : AxisUp;
	else
		adir = event->delta > 0 ? AxisRight : AxisLeft;

	for (ji = 0; ji < config.axis_bindings_count; ji++) {
		a = &config.axis_bindings[ji];
		if ((a->iscommonmode ||
			 (a->isdefaultmode && server.key_mode.isdefault) ||
			 (strcmp(server.key_mode.mode, a->mode) == 0)) &&
			CLEANMASK(mods) == CLEANMASK(a->mod) && // Same modifier set
			adir == a->dir &&
			a->func) { // Wheel direction matches and a handler exists
			if (event->time_msec - server.axis_apply_time >
					config.axis_bind_apply_timeout ||
				server.axis_apply_dir * event->delta < 0) {
				a->func(&a->arg);
				server.axis_apply_time = event->time_msec;
				server.axis_apply_dir = event->delta > 0 ? 1 : -1;
				return; // If matched, do not forward this scroll event to the
						// client.
			} else {
				server.axis_apply_dir = event->delta > 0 ? 1 : -1;
				server.axis_apply_time = event->time_msec;
				return;
			}
		}
	}

	/* TODO: allow usage of scroll whell for mousebindings, it can be
	 * implemented checking the event's orientation and the delta of the event
	 */
	/* Notify the client with pointer focus of the axis event. */

	target_scroll_factor = pointer_is_trackpad(event->pointer)
							   ? config.trackpad_scroll_factor
							   : config.axis_scroll_factor;

	wlr_seat_pointer_notify_axis(
		server.seat, // Forwards the scroll event to the focused client (the
					 // window).
		event->time_msec, event->orientation,
		event->delta * target_scroll_factor,
		roundf(event->delta_discrete * target_scroll_factor), event->source,
		event->relative_direction);
}

void handle_cursor_swipe_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_begin_event *event = data;

	if (config.disable_trackpad) {
		return;
	}

	// Forward swipe begin event to client
	wlr_pointer_gestures_v1_send_swipe_begin(
		server.pointer_gestures, server.seat, event->time_msec, event->fingers);
}

void handle_cursor_swipe_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_update_event *event = data;

	if (config.disable_trackpad) {
		return;
	}

	server.swipe_fingers = event->fingers;
	// Accumulate swipe distance
	server.swipe_dx += event->dx;
	server.swipe_dy += event->dy;

	// Forward swipe update event to client
	wlr_pointer_gestures_v1_send_swipe_update(server.pointer_gestures,
											  server.seat, event->time_msec,
											  event->dx, event->dy);
}

void handle_cursor_swipe_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_end_event *event = data;

	if (config.disable_trackpad) {
		return;
	}

	pointer_process_swipe_end(event);
	server.swipe_dx = 0;
	server.swipe_dy = 0;
	// Forward swipe end event to client
	wlr_pointer_gestures_v1_send_swipe_end(server.pointer_gestures, server.seat,
										   event->time_msec, event->cancelled);
}

void handle_cursor_pinch_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_begin_event *event = data;

	if (config.disable_trackpad) {
		return;
	}

	// Forward pinch begin event to client
	wlr_pointer_gestures_v1_send_pinch_begin(
		server.pointer_gestures, server.seat, event->time_msec, event->fingers);
}

void handle_cursor_pinch_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_update_event *event = data;

	if (config.disable_trackpad) {
		return;
	}

	// Forward pinch update event to client
	wlr_pointer_gestures_v1_send_pinch_update(
		server.pointer_gestures, server.seat, event->time_msec, event->dx,
		event->dy, event->scale, event->rotation);
}

void handle_cursor_pinch_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_end_event *event = data;

	if (config.disable_trackpad) {
		return;
	}

	// Forward pinch end event to client
	wlr_pointer_gestures_v1_send_pinch_end(server.pointer_gestures, server.seat,
										   event->time_msec, event->cancelled);
}

void handle_cursor_hold_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_begin_event *event = data;

	if (config.disable_trackpad) {
		return;
	}

	// Forward hold begin event to client
	wlr_pointer_gestures_v1_send_hold_begin(
		server.pointer_gestures, server.seat, event->time_msec, event->fingers);
}

void handle_cursor_hold_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_end_event *event = data;

	if (config.disable_trackpad) {
		return;
	}

	// Forward hold end event to client
	wlr_pointer_gestures_v1_send_hold_end(server.pointer_gestures, server.seat,
										  event->time_msec, event->cancelled);
}

bool check_trackpad_disabled(struct wlr_pointer *pointer) {
	if (!config.disable_trackpad) {
		return false;
	}

	return pointer_is_trackpad(pointer);
}
void // Mouse button event
handle_cursor_button(struct wl_listener *listener, void *data) {
	struct wlr_pointer_button_event *event = data;

	ipc_notify_device_event(&event->pointer->base);

	if (!pointer_process_button_press(event))
		wlr_seat_pointer_notify_button(server.seat, event->time_msec,
									   event->button, event->state);
}

void handle_last_cursor_surface_destroy(struct wl_listener *listener,
										void *data) {
	last_cursor.surface = NULL;
	wl_list_remove(&listener->link);
}

void handle_request_set_cursor_shape(struct wl_listener *listener, void *data) {
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
	if (server.cursor_mode != CurNormal && server.cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided cursor shape. */
	if (event->seat_client == server.seat->pointer_state.focused_client) {
		/* Remove surface destroy listener if active */
		if (last_cursor.surface &&
			server.last_cursor_surface_destroy_listener.link.prev != NULL)
			wl_list_remove(&server.last_cursor_surface_destroy_listener.link);

		last_cursor.shape = event->shape;
		last_cursor.surface = NULL;
		if (!server.cursor_hidden)
			wlr_cursor_set_xcursor(server.cursor, server.cursor_manager,
								   wlr_cursor_shape_v1_name(event->shape));
	}
}
void pointer_set_accel(struct libinput_device *device, bool natural_scrolling,
					   uint32_t mouse_accel_profile, double mouse_accel_speed) {
	libinput_device_config_scroll_set_natural_scroll_enabled(device,
															 natural_scrolling);
	if (mouse_accel_profile &&
		libinput_device_config_accel_is_available(device)) {
		libinput_device_config_accel_set_profile(device, mouse_accel_profile);
		libinput_device_config_accel_set_speed(device, mouse_accel_speed);
	} else {
		// profile cannot be directly applied to 0, need to set to 1 first
		libinput_device_config_accel_set_profile(device, 1);
		libinput_device_config_accel_set_profile(device, 0);
		libinput_device_config_accel_set_speed(device, 0);
	}
}

void configure_pointer(struct wlr_input_device *wlr_device,
					   struct libinput_device *device) {
	ConfigDeviceRule *rule = find_device_rule(wlr_device);
	bool is_touchpad = libinput_device_config_tap_get_finger_count(device) > 0;

	/*
	 * devicerule takes priority; falls back to the global config when unset
	 * (trackpad_* for touchpads, mouse_* for mice).
	 */
	int32_t tap_to_click = rule && rule->tap_to_click != -1
							   ? rule->tap_to_click
							   : config.tap_to_click;
	int32_t tap_and_drag = rule && rule->tap_and_drag != -1
							   ? rule->tap_and_drag
							   : config.tap_and_drag;
	int32_t drag_lock =
		rule && rule->drag_lock != -1 ? rule->drag_lock : config.drag_lock;
	uint32_t button_map = rule && rule->button_map != UINT32_MAX
							  ? rule->button_map
							  : config.button_map;
	int32_t natural_scrolling =
		rule && rule->natural_scrolling != -1
			? rule->natural_scrolling
			: (is_touchpad ? config.trackpad_natural_scrolling
						   : config.mouse_natural_scrolling);
	uint32_t accel_profile = rule && rule->accel_profile != -1
								 ? (uint32_t)rule->accel_profile
								 : (is_touchpad ? config.trackpad_accel_profile
												: config.mouse_accel_profile);
	double accel_speed = rule && !isnan(rule->accel_speed)
							 ? rule->accel_speed
							 : (is_touchpad ? config.trackpad_accel_speed
											: config.mouse_accel_speed);
	int32_t disable_while_typing = rule && rule->disable_while_typing != -1
									   ? rule->disable_while_typing
									   : config.trackpad_disable_while_typing;
	int32_t left_handed = rule && rule->left_handed != -1 ? rule->left_handed
						  : is_touchpad ? config.trackpad_left_handed
										: config.mouse_left_handed;
	int32_t middle_button_emulation =
		rule && rule->middle_button_emulation != -1
			? rule->middle_button_emulation
		: is_touchpad ? config.trackpad_middle_button_emulation
					  : config.mouse_middle_button_emulation;
	uint32_t scroll_method = rule && rule->scroll_method != UINT32_MAX
								 ? rule->scroll_method
							 : is_touchpad ? config.trackpad_scroll_method
										   : config.mouse_scroll_method;
	uint32_t scroll_button = rule && rule->scroll_button != UINT32_MAX
								 ? rule->scroll_button
							 : is_touchpad ? config.trackpad_scroll_button
										   : config.mouse_scroll_button;
	uint32_t click_method = rule && rule->click_method != UINT32_MAX
								? rule->click_method
							: is_touchpad ? config.trackpad_click_method
										  : config.mouse_click_method;
	uint32_t send_events_mode = rule && rule->send_events_mode != UINT32_MAX
									? rule->send_events_mode
								: is_touchpad ? config.trackpad_send_events_mode
											  : config.mouse_send_events_mode;

	if (libinput_device_config_tap_get_finger_count(device)) {
		libinput_device_config_tap_set_enabled(device, tap_to_click);
		libinput_device_config_tap_set_drag_enabled(device, tap_and_drag);
		libinput_device_config_tap_set_drag_lock_enabled(device, drag_lock);
		libinput_device_config_tap_set_button_map(device, button_map);
	}
	pointer_set_accel(device, natural_scrolling, accel_profile, accel_speed);

	if (libinput_device_config_dwt_is_available(device))
		libinput_device_config_dwt_set_enabled(device, disable_while_typing);

	if (libinput_device_config_left_handed_is_available(device))
		libinput_device_config_left_handed_set(device, left_handed);

	if (libinput_device_config_middle_emulation_is_available(device))
		libinput_device_config_middle_emulation_set_enabled(
			device, middle_button_emulation);

	if (libinput_device_config_scroll_get_methods(device) !=
		LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
		libinput_device_config_scroll_set_method(device, scroll_method);
	if (libinput_device_config_scroll_get_methods(device) ==
		LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN)
		libinput_device_config_scroll_set_button(device, scroll_button);

	if (libinput_device_config_click_get_methods(device) !=
		LIBINPUT_CONFIG_CLICK_METHOD_NONE)
		libinput_device_config_click_set_method(device, click_method);

	if (libinput_device_config_send_events_get_modes(device))
		libinput_device_config_send_events_set_mode(device, send_events_mode);
}

void pointer_create(struct wlr_pointer *pointer) {
	struct libinput_device *device = NULL;

	if (wlr_input_device_is_libinput(&pointer->base) &&
		(device = wlr_libinput_get_device_handle(&pointer->base))) {

		configure_pointer(&pointer->base, device);

		InputDevice *input_dev = calloc(1, sizeof(InputDevice));
		input_dev->wlr_device = &pointer->base;
		input_dev->libinput_device = device;

		input_dev->destroy_listener.notify = handle_input_device_destroy;
		wl_signal_add(&pointer->base.events.destroy,
					  &input_dev->destroy_listener);

		wl_list_insert(&server.input_devices, &input_dev->link);
	}
	wlr_cursor_attach_input_device(server.cursor, &pointer->base);
}

void handle_new_pointer_constraint(struct wl_listener *listener, void *data) {
	PointerConstraint *pointer_constraint =
		ecalloc(1, sizeof(*pointer_constraint));
	pointer_constraint->constraint = data;
	LISTEN(&pointer_constraint->constraint->events.destroy,
		   &pointer_constraint->destroy, handle_pointer_constraint_destroy);

	// layer surfaces are never selected_monitor->sel, so match pointer focus
	// too (e.g. lan-mouse locks the pointer on a 1px layer surface)
	if (server.seat->pointer_state.focused_surface ==
		pointer_constraint->constraint->surface) {
		pointer_constrain_cursor(pointer_constraint->constraint);
		return;
	}

	if (!server.selected_monitor || !server.selected_monitor->sel)
		return;

	struct wlr_surface *focused_surface =
		client_surface(server.selected_monitor->sel);
	if (focused_surface &&
		focused_surface == pointer_constraint->constraint->surface) {
		pointer_constrain_cursor(pointer_constraint->constraint);
	}
}

void pointer_constrain_cursor(struct wlr_pointer_constraint_v1 *constraint) {
	if (server.active_constraint == constraint)
		return;

	if (server.active_constraint) {
		if (constraint == NULL) {
			pointer_warp_to_constraint_hint();
		}
		wlr_pointer_constraint_v1_send_deactivated(server.active_constraint);
	}

	server.active_constraint = constraint;

	if (constraint) {
		wlr_pointer_constraint_v1_send_activated(constraint);
	}
}

void handle_cursor_frame(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at
	 * the same time, in which case a frame event won't be sent in between.
	 */
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server.seat);
}

void pointer_warp_to_constraint_hint(void) {
	Client *c = NULL;
	double sx = server.active_constraint->current.cursor_hint.x;
	double sy = server.active_constraint->current.cursor_hint.y;

	toplevel_from_wlr_surface(server.active_constraint->surface, &c, NULL);
	if (c && server.active_constraint->current.cursor_hint.enabled) {
		wlr_cursor_warp(server.cursor, NULL, sx + c->geom.x + c->bw,
						sy + c->geom.y + c->bw);
		wlr_seat_pointer_warp(server.active_constraint->seat, sx, sy);
	}
}

void handle_drag_icon_destroy(struct wl_listener *listener, void *data) {
	/* Focus enter isn't sent during drag, so refocus the focused node. */
	client_focus(client_focus_top(server.selected_monitor), 1);
	pointer_process_motion(0, NULL, 0, 0, 0, 0);
	wl_list_remove(&listener->link);
	free(listener);
}

void handle_pointer_constraint_destroy(struct wl_listener *listener,
									   void *data) {
	PointerConstraint *pointer_constraint =
		wl_container_of(listener, pointer_constraint, destroy);

	if (server.active_constraint == pointer_constraint->constraint) {
		pointer_warp_to_constraint_hint();
		server.active_constraint = NULL;
	}

	wl_list_remove(&pointer_constraint->destroy.link);
	free(pointer_constraint);
}

void handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an
	 * _absolute_ motion event, from 0..1 on each axis. This happens, for
	 * example, when wlroots is running under a Wayland window rather than
	 * KMS+DRM, and you move the mouse over the window. You could enter the
	 * window from any edge, so we have to warp the mouse there. There is
	 * also some hardware which emits these events. */
	struct wlr_pointer_motion_absolute_event *event = data;
	double lx, ly, dx, dy;

	ipc_notify_device_event(&event->pointer->base);

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	if (!event->time_msec) /* this is 0 with virtual pointer */
		wlr_cursor_warp_absolute(server.cursor, &event->pointer->base, event->x,
								 event->y);

	wlr_cursor_absolute_to_layout_coords(server.cursor, &event->pointer->base,
										 event->x, event->y, &lx, &ly);
	dx = lx - server.cursor->x;
	dy = ly - server.cursor->y;
	pointer_process_motion(event->time_msec, &event->pointer->base, dx, dy, dx,
						   dy);
}

void pointer_resize_floating_window(Client *gc) {
	int cdx = (int)round(server.cursor->x) - server.grab_offset_x;
	int cdy = (int)round(server.cursor->y) - server.grab_offset_y;

	cdx = !(server.resize_corner & 1) &&
				  gc->geom.width - 2 * (int)gc->bw - cdx < 1
			  ? 0
			  : cdx;
	cdy = !(server.resize_corner & 2) &&
				  gc->geom.height - 2 * (int)gc->bw - cdy < 1
			  ? 0
			  : cdy;

	const struct wlr_box box = {
		.x = gc->geom.x + (server.resize_corner & 1 ? 0 : cdx),
		.y = gc->geom.y + (server.resize_corner & 2 ? 0 : cdy),
		.width = gc->geom.width + (server.resize_corner & 1 ? cdx : -cdx),
		.height = gc->geom.height + (server.resize_corner & 2 ? cdy : -cdy)};

	gc->float_geom = box;

	resize(gc, box, 1);
	server.grab_offset_x += cdx;
	server.grab_offset_y += cdy;
}
void pointer_process_motion(uint32_t time, struct wlr_input_device *device,
							double dx, double dy, double dx_unaccel,
							double dy_unaccel) {
	double sx = 0, sy = 0, sx_confined, sy_confined;
	Client *c = NULL, *w = NULL;
	Client *closet_drop_client = NULL;
	LayerSurface *l = NULL;
	struct wlr_surface *surface = NULL;
	bool should_lock = false;

	/* time is 0 in internal calls meant to restore pointer focus. */
	if (time) {
		wlr_relative_pointer_manager_v1_send_relative_motion(
			server.relative_pointer_manager, server.seat, (uint64_t)time * 1000,
			dx, dy, dx_unaccel, dy_unaccel);

		if (server.active_constraint && server.cursor_mode != CurResize &&
			server.cursor_mode != CurMove) {
			if (server.active_constraint->surface ==
				server.seat->pointer_state.focused_surface) {

				if (server.active_constraint->type ==
					WLR_POINTER_CONSTRAINT_V1_LOCKED)
					return;

				toplevel_from_wlr_surface(server.active_constraint->surface, &c,
										  NULL);
				if (c) {
					sx = server.cursor->x - c->geom.x - c->bw;
					sy = server.cursor->y - c->geom.y - c->bw;
					if (wlr_region_confine(&server.active_constraint->region,
										   sx, sy, sx + dx, sy + dy,
										   &sx_confined, &sy_confined)) {
						dx = sx_confined - sx;
						dy = sy_confined - sy;
					}
				}
			}
		}

		wlr_cursor_move(server.cursor, device, dx, dy);
		pointer_cursor_activity();
		wlr_idle_notifier_v1_notify_activity(server.idle_notifier, server.seat);

		/* Update selected_monitor (even while dragging a window) */
		if (config.sloppyfocus) {
			Monitor *oldmon = server.selected_monitor;
			server.selected_monitor =
				monitor_at_point(server.cursor->x, server.cursor->y);
			if (oldmon != server.selected_monitor)
				printstatus(IPC_WATCH_MONITOR | IPC_WATCH_ALL_MONITORS);
		}
	}

	/* Find the client under the pointer and send the event along. */
	node_at_point(server.cursor->x, server.cursor->y, &surface, &c, NULL, NULL,
				  &sx, &sy);

	if (server.cursor_mode == CurPressed && !server.seat->drag &&
		surface != server.seat->pointer_state.focused_surface &&
		toplevel_from_wlr_surface(server.seat->pointer_state.focused_surface,
								  &w, &l) >= 0) {
		c = w;
		surface = server.seat->pointer_state.focused_surface;
		sx = server.cursor->x - (l ? l->scene->node.x : w->geom.x);
		sy = server.cursor->y - (l ? l->scene->node.y : w->geom.y);
	}

	/* Update drag icon's position */
	wlr_scene_node_set_position(&server.drag_icon->node,
								(int32_t)round(server.cursor->x),
								(int32_t)round(server.cursor->y));

	/* If we are currently grabbing the mouse, handle and return */
	if (server.cursor_mode == CurMove) {
		/* Move the grabbed client to the new position. */
		server.grab_client->iscustomsize = 1;
		server.grab_client->float_geom = (struct wlr_box){
			.x = (int32_t)round(server.cursor->x) - server.grab_offset_x,
			.y = (int32_t)round(server.cursor->y) - server.grab_offset_y,
			.width = server.grab_client->geom.width,
			.height = server.grab_client->geom.height};
		if (config.drag_tile_to_tile && server.grab_client->drag_to_tile) {
			closet_drop_client = find_closest_tiled_client(server.grab_client);
			if (closet_drop_client && server.drop_client &&
				closet_drop_client != server.drop_client) {
				server.drop_client->enable_drop_area_draw = false;
				client_set_drop_area(server.drop_client);
				server.drop_client = closet_drop_client;
				server.drop_client->enable_drop_area_draw = true;
				client_set_drop_area(server.drop_client);
			} else if (closet_drop_client) {
				server.drop_client = closet_drop_client;
				server.drop_client->enable_drop_area_draw = true;
				client_set_drop_area(server.drop_client);
			} else if (server.drop_client) {
				server.drop_client->enable_drop_area_draw = false;
				client_set_drop_area(server.drop_client);
				server.drop_client = NULL;
			}
		}
		resize(server.grab_client, server.grab_client->float_geom, 1);
		return;
	} else if (server.cursor_mode == CurResize) {
		if (server.grab_client->isfloating) {
			server.grab_client->iscustomsize = 1;
			if (server.last_apply_drag_time == 0 ||
				time - server.last_apply_drag_time >
					config.drag_floating_refresh_interval) {
				pointer_resize_floating_window(server.grab_client);
				server.last_apply_drag_time = time;
			}
			return;
		} else {
			resize_tile_client(server.grab_client, true, 0, 0, time);
		}
	}

	/* If there's no client surface under the cursor, set the cursor image
	 * to a default. This is what makes the cursor image appear when you
	 * move it off of a client or over its border. */
	if (!surface && !server.seat->drag && !server.cursor_hidden)
		wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "default");

	if (c && c->mon && !c->animation.running &&
		(INSIDEMON(c) || !ISSCROLLTILED(c))) {
		server.scroller_focus_lock = 0;
	}

	should_lock = false;
	double speed = 0.0f;

	if (config.edge_scroller_pointer_focus) {
		speed = sqrt(dx * dx + dy * dy);
	}

	if (!server.scroller_focus_lock || !(c && c->mon && !INSIDEMON(c))) {
		if (c && c->mon && ISSCROLLTILED(c) && is_scroller_layout(c->mon) &&
			!INSIDEMON(c)) {
			should_lock = true;
		}

		if (!((!config.edge_scroller_pointer_focus ||
			   speed < config.edge_scroller_focus_allow_speed) &&
			  c && c->mon && ISSCROLLTILED(c) && is_scroller_layout(c->mon) &&
			  !INSIDEMON(c))) {
			pointer_focus(c, surface, sx, sy, time);
		}

		if (should_lock && c && c->mon && ISTILED(c) && c == c->mon->sel) {
			server.scroller_focus_lock = 1;
		}
	}
}

void handle_cursor_motion(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a
	 * _relative_ pointer motion event (i.e. a delta) */
	struct wlr_pointer_motion_event *event = data;
	ipc_notify_device_event(&event->pointer->base);
	/* The cursor doesn't move unless we tell it to. The cursor
	 * automatically handles constraining the motion to the output layout,
	 * as well as any special configuration applied for the specific input
	 * device which generated the event. You can pass NULL for the device if
	 * you want to move the cursor around without any input. */

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	pointer_process_motion(event->time_msec, &event->pointer->base,
						   event->delta_x, event->delta_y, event->unaccel_dx,
						   event->unaccel_dy);
	toggle_hotarea(server.cursor->x, server.cursor->y);
}
void pointer_focus(Client *c, struct wlr_surface *surface, double sx, double sy,
				   uint32_t time) {
	struct timespec now;

	if (config.sloppyfocus && !server.start_drag_window && c && time &&
		c->scene && c->scene->node.enabled &&
		(!c->mon || !c->mon->isoverview) && !c->animation.tagining &&
		(surface != server.seat->pointer_state.focused_surface ||
		 (server.selected_monitor && server.selected_monitor->isoverview &&
		  server.selected_monitor->sel != c)) &&
		!client_is_unmanaged(c) && VISIBLEON(c, c->mon))
		client_focus(c, 0);

	/* Pointer-driven layer constraints: deactivate as soon as the pointer
	 * leaves their surface. Toplevel constraints are managed by focusclient
	 * (keyboard focus driven), so they are left untouched here. */
	if (server.active_constraint &&
		surface != server.seat->pointer_state.focused_surface &&
		toplevel_from_wlr_surface(server.active_constraint->surface, NULL,
								  NULL) == LayerShell) {
		pointer_constrain_cursor(NULL);
	}

	/* If surface is NULL, clear pointer focus */
	if (!surface) {
		wlr_seat_pointer_notify_clear_focus(server.seat);
		return;
	}

	if (!time) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		time = now.tv_sec * 1000 + now.tv_nsec / 1000000;
	}

	/* Let the client know that the mouse cursor has entered one
	 * of its surfaces, and make keyboard focus follow if desired.
	 * wlroots makes this a no-op if surface is already focused */

	/* X11 windows use physical sizes, so surface-local coordinates are also
	 * multiplied by xwayland_scale. */
#ifdef XWAYLAND
	if (c && client_is_x11(c) && config.xwayland_ignore_scale &&
		c->xwayland_scale > 0.f) {
		sx *= c->xwayland_scale;
		sy *= c->xwayland_scale;
	}
#endif

	if (!c || !c->mon || !c->mon->isoverview) {
		// don't let window get pointer focus,
		// avoid game window force grab pointer in overview mode
		struct wlr_surface *old_focus =
			server.seat->pointer_state.focused_surface;
		wlr_seat_pointer_notify_enter(server.seat, surface, sx, sy);

		// toplevel constraints are handled by focusclient, this picks up the
		// ones focusclient can't see
		if (!c && surface != old_focus) {
			struct wlr_pointer_constraint_v1 *constraint;
			wl_list_for_each(constraint,
							 &server.pointer_constraints->constraints, link) {
				if (constraint->surface == surface) {
					pointer_constrain_cursor(constraint);
					break;
				}
			}
		}
	}

	wlr_seat_pointer_notify_motion(server.seat, time, sx, sy);
}

void handle_request_start_drag(struct wl_listener *listener, void *data) {
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(server.seat, event->origin,
											  event->serial))
		wlr_seat_start_pointer_drag(server.seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}

void handle_request_set_cursor(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client provides a cursor
	 * image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	/* If we're "grabbing" the cursor, don't use the client's image, we will
	 * restore it after "grabbing" sending a leave event, followed by a
	 * enter event, which will result in the client requesting set the
	 * cursor surface
	 */
	if (server.cursor_mode != CurNormal && server.cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided surface as the cursor image. It will set the
	 * hardware cursor on the output that it's currently on and continue to
	 * do so as the cursor moves between outputs. */
	if (event->seat_client == server.seat->pointer_state.focused_client) {
		/* Clear previous surface destroy listener if any */
		if (last_cursor.surface &&
			server.last_cursor_surface_destroy_listener.link.prev != NULL)
			wl_list_remove(&server.last_cursor_surface_destroy_listener.link);

		last_cursor.shape = 0;
		last_cursor.surface = event->surface;
		last_cursor.hotspot_x = event->hotspot_x;
		last_cursor.hotspot_y = event->hotspot_y;

		/* Track surface destruction to avoid dangling pointer */
		if (event->surface)
			wl_signal_add(&event->surface->events.destroy,
						  &server.last_cursor_surface_destroy_listener);

		if (!server.cursor_hidden)
			wlr_cursor_set_surface(server.cursor, event->surface,
								   event->hotspot_x, event->hotspot_y);
	}
}

void handle_start_drag(struct wl_listener *listener, void *data) {
	struct wlr_drag *drag = data;
	if (!drag->icon)
		return;

	drag->icon->data =
		&wlr_scene_drag_icon_create(server.drag_icon, drag->icon)->node;
	LISTEN_STATIC(&drag->icon->events.destroy, handle_drag_icon_destroy);
}

void pointer_cursor_activity(void) {
	wl_event_source_timer_update(server.hide_cursor_source,
								 config.cursor_hide_timeout * 1000);

	if (!server.cursor_hidden)
		return;

	server.cursor_hidden = false;

	if (last_cursor.shape)
		wlr_cursor_set_xcursor(server.cursor, server.cursor_manager,
							   wlr_cursor_shape_v1_name(last_cursor.shape));
	else if (last_cursor.surface)
		wlr_cursor_set_surface(server.cursor, last_cursor.surface,
							   last_cursor.hotspot_x, last_cursor.hotspot_y);
}

int32_t pointer_hide_cursor(void *data) {
	wlr_cursor_unset_image(server.cursor);
	server.cursor_hidden = true;
	return 1;
}

void pointer_warp_to_client(const Client *c) {
	if (INSIDEMON(c)) {
		wlr_cursor_warp_closest(server.cursor, NULL,
								c->geom.x + c->geom.width / 2.0,
								c->geom.y + c->geom.height / 2.0);
		pointer_process_motion(0, NULL, 0, 0, 0, 0);
	}
}

void pointer_warp_to_monitor(Monitor *m) {
	wlr_cursor_warp_closest(server.cursor, NULL, m->w.x + m->w.width / 2.0,
							m->w.y + m->w.height / 2.0);
	wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "default");
	pointer_cursor_activity();
}

void handle_new_virtual_pointer(struct wl_listener *listener, void *data) {
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	struct wlr_input_device *device = &event->new_pointer->pointer.base;
	wlr_seat_set_capabilities(server.seat, server.seat->capabilities |
											   WL_SEAT_CAPABILITY_POINTER);
	wlr_cursor_attach_input_device(server.cursor, device);
	if (event->suggested_output)
		wlr_cursor_map_input_to_output(server.cursor, device,
									   event->suggested_output);

	pointer_cursor_activity();
}
// New from here
int32_t pointer_process_swipe_end(struct wlr_pointer_swipe_end_event *event) {
	uint32_t mods;
	const GestureBinding *g;
	uint32_t motion;
	uint32_t adx = (int32_t)round(fabs(server.swipe_dx));
	uint32_t ady = (int32_t)round(fabs(server.swipe_dy));
	int32_t handled = 0;
	int32_t ji;

	if (event->cancelled) {
		return handled;
	}

	// Require absolute distance movement beyond a small thresh-hold
	if (adx * adx + ady * ady <
		config.swipe_min_threshold * config.swipe_min_threshold) {
		return handled;
	}

	if (adx > ady) {
		motion = server.swipe_dx < 0 ? SWIPE_LEFT : SWIPE_RIGHT;
	} else {
		motion = server.swipe_dy < 0 ? SWIPE_UP : SWIPE_DOWN;
	}

	mods = keyboard_hard_modifiers();

	for (ji = 0; ji < config.gesture_bindings_count; ji++) {
		g = &config.gesture_bindings[ji];
		if ((g->iscommonmode ||
			 (g->isdefaultmode && server.key_mode.isdefault) ||
			 (strcmp(server.key_mode.mode, g->mode) == 0)) &&
			CLEANMASK(mods) == CLEANMASK(g->mod) &&
			server.swipe_fingers == g->fingers_count && motion == g->motion &&
			g->func) {
			g->func(&g->arg);
			handled = 1;
		}
	}
	return handled;
}

Client *find_closest_tiled_client(Client *c) {
	Client *tc, *closest = NULL;
	long min_dist = LONG_MAX;
	Monitor *cursor_mon = monitor_at_point(server.cursor->x, server.cursor->y);

	wl_list_for_each(tc, &server.clients, link) {
		if (tc == c || !ISTILED(tc) || !VISIBLEON(tc, cursor_mon))
			continue;

		if (server.cursor->x >= tc->geom.x &&
			server.cursor->x < tc->geom.x + tc->geom.width &&
			server.cursor->y >= tc->geom.y &&
			server.cursor->y < tc->geom.y + tc->geom.height) {
			return tc;
		}

		int32_t dx =
			tc->geom.x + (int32_t)(tc->geom.width / 2) - server.cursor->x;
		int32_t dy =
			tc->geom.y + (int32_t)(tc->geom.height / 2) - server.cursor->y;
		long dist = (long)dx * dx + (long)dy * dy;

		if (dist < min_dist) {
			min_dist = dist;
			closest = tc;
		}
	}

	return closest;
}

void pointer_place_drag_tile(Client *c) {
	Client *closest = find_closest_tiled_client(c);

	if (closest && closest->mon) {
		const Layout *layout =
			closest->mon->pertag->ltidxs[get_client_tag_idx(closest)];

		if (closest->drop_direction == UNDIR) {
			client_set_floating(c, 0);
			wl_list_safe_reinsert_prev(&closest->link, &c->link);
			arrange(closest->mon, false, false);
			return;
		}

		if (layout->id == SCROLLER) {
			scroller_drop_tile(c, closest, 0);
			return;
		}
		if (layout->id == VERTICAL_SCROLLER) {
			scroller_drop_tile(c, closest, 1);
			return;
		}
		if (layout->id == DWINDLE) {
			uint32_t tag = get_client_tag_idx(c);
			bool insert_before = (closest->drop_direction == LEFT ||
								  closest->drop_direction == UP);
			bool split_h = (closest->drop_direction == LEFT ||
							closest->drop_direction == RIGHT);
			dwindle_insert(&c->mon->pertag->dwindle_root[tag], c, closest,
						   config.dwindle_split_ratio, insert_before, split_h,
						   !config.dwindle_drop_simple_split);
			client_set_floating(c, 0);
			return;
		}

		if (layout->id == RIGHT_TILE) {
			if (closest->drop_direction == LEFT) {
				wl_list_safe_reinsert_next(&closest->link, &c->link);
			} else if (closest->drop_direction == RIGHT) {
				wl_list_safe_reinsert_prev(&closest->link, &c->link);
			} else if (closest->drop_direction == UP) {
				wl_list_safe_reinsert_prev(&closest->link, &c->link);
			} else {
				wl_list_safe_reinsert_next(&closest->link, &c->link);
			}
			client_set_floating(c, 0);
			return;
		}

		if (closest->drop_direction == LEFT || closest->drop_direction == UP) {
			wl_list_safe_reinsert_prev(&closest->link, &c->link);
		} else {
			wl_list_safe_reinsert_next(&closest->link, &c->link);
		}
	}

	client_set_floating(c, 0);
}

bool pointer_process_button_press(struct wlr_pointer_button_event *event) {
	uint32_t mods;
	Client *c = NULL;
	LayerSurface *l = NULL;
	MangoGroupBar *gb = NULL;
	struct wlr_surface *surface;
	Client *tmpc = NULL;
	int32_t ji;
	const MouseBinding *m;
	struct wlr_surface *old_pointer_focus_surface =
		server.seat->pointer_state.focused_surface;

	pointer_cursor_activity();
	wlr_idle_notifier_v1_notify_activity(server.idle_notifier, server.seat);

	if (event->pointer && check_trackpad_disabled(event->pointer)) {
		return true;
	}

	switch (event->state) {
	case WL_POINTER_BUTTON_STATE_PRESSED:
		server.cursor_mode = CurPressed;
		server.selected_monitor =
			monitor_at_point(server.cursor->x, server.cursor->y);
		if (server.session_locked)
			break;

		if (switcher_is_active() &&
			(event->button == BTN_LEFT || event->button == BTN_RIGHT)) {
			Client *switcher_c =
				switcher_client_at(server.cursor->x, server.cursor->y);
			if (!switcher_c)
				switcher_close();
			else if (event->button == BTN_LEFT)
				switcher_commit_client(switcher_c);
			else
				pending_kill_client(switcher_c);
			wlr_seat_pointer_notify_clear_focus(server.seat);
			return true;
		}

		node_at_point(server.cursor->x, server.cursor->y, &surface, NULL, NULL,
					  &gb, NULL, NULL);
		if (toplevel_from_wlr_surface(surface, &c, &l) >= 0) {
			if (c && c->scene && c->scene->node.enabled &&
				VISIBLEON(c, c->mon) &&
				(!client_is_unmanaged(c) || client_wants_focus(c)))
				client_focus(c, 1);

			if (surface != old_pointer_focus_surface) {
				wlr_seat_pointer_notify_clear_focus(server.seat);
				pointer_process_motion(0, NULL, 0, 0, 0, 0);
			}

			// Focuses the layer that requests interactive focus, but must not
			// steal focus from an exclusive-focus layer.
			if (l && !server.exclusive_focus &&
				l->layer_surface->current.keyboard_interactive ==
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
				layer_focus(l);
			}
		}

		// In overview mode, left click jumps and right click closes windows.
		if (server.selected_monitor && server.selected_monitor->isoverview &&
			event->button == BTN_LEFT && c) {
			toggle_overview(&(Arg){.tc = c});
			return true;
		}

		if (server.selected_monitor && server.selected_monitor->isoverview &&
			event->button == BTN_RIGHT && c) {
			pending_kill_client(c);
			return true;
		}

		// handle click on tile node
		client_handle_decorate_click(gb);

		mods = keyboard_hard_modifiers();

		for (ji = 0; ji < config.mouse_bindings_count; ji++) {
			m = &config.mouse_bindings[ji];

			if ((m->iscommonmode ||
				 (m->isdefaultmode && server.key_mode.isdefault) ||
				 (strcmp(server.key_mode.mode, m->mode) == 0)) &&
				CLEANMASK(mods) == CLEANMASK(m->mod) &&
				event->button == m->button && m->func &&
				(CLEANMASK(m->mod) != 0 ||
				 (event->button != BTN_LEFT && event->button != BTN_RIGHT))) {
				m->func(&m->arg);
				return true;
			}
		}
		break;
	case WL_POINTER_BUTTON_STATE_RELEASED:
		/* If you released any buttons, we exit interactive move/resize mode. */
		if (!server.session_locked && server.cursor_mode != CurNormal &&
			server.cursor_mode != CurPressed) {
			server.cursor_mode = CurNormal;
			/* Clear the pointer focus, this way if the cursor is over a surface
			 * we will send an enter event after which the client will provide
			 * us a cursor surface */
			wlr_seat_pointer_clear_focus(server.seat);
			pointer_process_motion(0, NULL, 0, 0, 0, 0);
			/* Drop the window off on its new monitor */
			if (server.grab_client == server.selected_monitor->sel) {
				server.selected_monitor->sel = NULL;
			}
			server.selected_monitor =
				monitor_at_point(server.cursor->x, server.cursor->y);
			client_update_oldmonname_record(server.grab_client,
											server.selected_monitor);
			client_set_monitor(server.grab_client, server.selected_monitor, 0,
							   true);
			/* if the view changed mid-drag, drop onto the current tag
			 * instead of silently returning to the original one */
			if (!VISIBLEON(server.grab_client, server.selected_monitor))
				server.grab_client->tags =
					server.selected_monitor
						->tagset[server.selected_monitor->seltags];
			server.selected_monitor->prevsel =
				ISTILED(server.selected_monitor->sel)
					? server.selected_monitor->sel
					: NULL;
			server.selected_monitor->sel = server.grab_client;
			tmpc = server.grab_client;
			server.grab_client = NULL;
			server.start_drag_window = false;
			server.last_apply_drag_time = 0;
			if (tmpc->drag_to_tile && config.drag_tile_to_tile) {
				pointer_place_drag_tile(tmpc);
				tmpc->float_geom = tmpc->drag_tile_float_backup_geom;
			} else {
				apply_window_snap(tmpc);
			}
			tmpc->drag_to_tile = false;
			if (server.drop_client) {
				server.drop_client->enable_drop_area_draw = false;
				client_set_drop_area(server.drop_client);
				server.drop_client = NULL;
			}
			return true;
		} else {
			server.cursor_mode = CurNormal;
		}
		break;
	}
	/* If the event wasn't handled by the compositor, return false */
	return false;
}
