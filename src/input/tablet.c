#include "mango/input/tablet.h"
#include "mango/common/log.h"
#include "mango/mango.h"
#include "mango/common/globals.h"
#include <stdlib.h>
#include "mango/common/util.h"
#include "mango/ipc/ipc.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include "mango/manage/misc.h"
#include "mango/input/pointer.h"

void createtablet(struct wlr_input_device *device) {
	struct Tablet *tablet = calloc(1, sizeof(struct Tablet));
	if (!tablet) {
		mango_error(true, WLR_ERROR, "could not allocate tablet");
		return;
	}

	struct libinput_device *device_handle = NULL;
	if (!wlr_input_device_is_libinput(device) ||
		!(device_handle = wlr_libinput_get_device_handle(device))) {
		free(tablet);
		return;
	}

	tablet->device = device;
	tablet->tablet_v2 = wlr_tablet_create(tablet_mgr, seat, device);

	if (!tablet->tablet_v2) {
		free(tablet);
		return;
	}

	tablet->tablet_v2->wlr_tablet->data = tablet;
	tablet->destroy.notify = destroytablet;
	wl_signal_add(&tablet->device->events.destroy, &tablet->destroy);

	if (libinput_device_config_send_events_get_modes(device_handle)) {
		libinput_device_config_send_events_set_mode(device_handle,
													config.send_events_mode);
		wlr_cursor_attach_input_device(cursor, device);
	}

	wl_list_insert(&tablets, &tablet->link);

	/* Search for a sibling tablet pad */
	struct libinput_device_group *group = libinput_device_get_device_group(
		wlr_libinput_get_device_handle(device));
	struct TabletPad *tablet_pad;
	wl_list_for_each(tablet_pad, &tablet_pads, link) {
		struct wlr_input_device *pad_device = tablet_pad->device;
		if (!wlr_input_device_is_libinput(pad_device)) {
			continue;
		}

		struct libinput_device_group *pad_group =
			libinput_device_get_device_group(
				wlr_libinput_get_device_handle(pad_device));

		if (pad_group == group) {
			attach_tablet_pad(tablet_pad, tablet);
			break;
		}
	}
}

void destroytablet(struct wl_listener *listener, void *data) {
	struct Tablet *tablet = wl_container_of(listener, tablet, destroy);

	wl_list_remove(&listener->link);
	wl_list_remove(&tablet->link);
	free(tablet);
}

void tabletpadtabletdestroy(struct wl_listener *listener, void *data) {
	struct TabletPad *tablet_pad =
		wl_container_of(listener, tablet_pad, tablet_destroy);

	tablet_pad->tablet = NULL;

	wl_list_remove(&tablet_pad->tablet_destroy.link);
	wl_list_init(&tablet_pad->tablet_destroy.link);
}

void attach_tablet_pad(struct TabletPad *tablet_pad, struct Tablet *tablet) {
	tablet_pad->tablet = tablet;

	wl_list_remove(&tablet_pad->tablet_destroy.link);
	tablet_pad->tablet_destroy.notify = tabletpadtabletdestroy;
	wl_signal_add(&tablet->device->events.destroy, &tablet_pad->tablet_destroy);
}

void tabletpadattach(struct wl_listener *listener, void *data) {
	struct TabletPad *tablet_pad =
		wl_container_of(listener, tablet_pad, attach);
	struct wlr_tablet_tool *wlr_tool = data;
	struct TabletTool *tool = wlr_tool->data;

	if (!tool) {
		return;
	}

	attach_tablet_pad(tablet_pad, tool->tablet);
}

void createtabletpad(struct wlr_input_device *device) {
	struct TabletPad *tablet_pad = calloc(1, sizeof(struct TabletPad));
	if (!tablet_pad) {
		mango_error(true, WLR_ERROR, "could not allocate tablet_pad");
		return;
	}

	tablet_pad->device = device;
	tablet_pad->pad_v2 = wlr_tablet_pad_create(tablet_mgr, seat, device);

	if (!tablet_pad->pad_v2) {
		mango_error(true, WLR_ERROR, "could not create tablet_pad_v2 wrapper");
		free(tablet_pad);
		return;
	}

	tablet_pad->destroy.notify = destroytabletpad;
	tablet_pad->attach.notify = tabletpadattach;
	wl_list_init(&tablet_pad->tablet_destroy.link);

	wl_signal_add(&device->events.destroy, &tablet_pad->destroy);

	wl_signal_add(&tablet_pad->pad_v2->wlr_pad->events.attach_tablet,
				  &tablet_pad->attach);
	wl_list_insert(&tablet_pads, &tablet_pad->link);

	/* Search for a sibling tablet */
	if (!wlr_input_device_is_libinput(device)) {
		/* We can only do this on libinput devices */
		return;
	}

	struct libinput_device_group *group = libinput_device_get_device_group(
		wlr_libinput_get_device_handle(device));

	struct Tablet *tablet;
	wl_list_for_each(tablet, &tablets, link) {
		struct wlr_input_device *tablet_device = tablet->device;
		if (!wlr_input_device_is_libinput(tablet_device)) {
			continue;
		}

		struct libinput_device_group *tablet_group =
			libinput_device_get_device_group(
				wlr_libinput_get_device_handle(tablet_device));

		if (tablet_group == group) {
			attach_tablet_pad(tablet_pad, tablet);
			break;
		}
	}
}

void destroytabletpad(struct wl_listener *listener, void *data) {
	struct TabletPad *tablet_pad =
		wl_container_of(listener, tablet_pad, destroy);

	wl_list_remove(&listener->link);
	wl_list_remove(&tablet_pad->link);
	wl_list_remove(&tablet_pad->tablet_destroy.link);
	wl_list_remove(&tablet_pad->attach.link);
	free(tablet_pad);
}

void destroytabletsurfacenotify(struct wl_listener *listener, void *data) {
	struct TabletTool *tool = wl_container_of(listener, tool, surface_destroy);
	wl_list_remove(&tool->surface_destroy.link);
	tool->curr_surface = NULL;
}

void destroytablettool(struct wl_listener *listener, void *data) {
	struct TabletTool *tool = wl_container_of(listener, tool, destroy);

	if (tool->curr_surface)
		wl_list_remove(&tool->surface_destroy.link);
	wl_list_remove(&tool->set_cursor.link);
	wl_list_remove(&listener->link);
	free(tool);
}

void tablettoolsetcursor(struct wl_listener *listener, void *data) {
	struct TabletTool *tool = wl_container_of(listener, tool, set_cursor);
	struct wlr_tablet_v2_event_cursor *event = data;

	struct wlr_seat_client *focused_client = NULL;
	if (tool->tool_v2->focused_surface) {
		focused_client = wlr_seat_client_for_wl_client(
			seat,
			wl_resource_get_client(tool->tool_v2->focused_surface->resource));
	}

	if (focused_client != event->seat_client)
		return;

	wlr_cursor_set_surface(cursor, event->surface, event->hotspot_x,
						   event->hotspot_y);
}

void tablettoolmotion(struct TabletTool *tool, bool change_x, bool change_y,
					  double x, double y, double dx, double dy) {
	struct wlr_surface *surface = NULL;
	Client *c = NULL, *w = NULL;
	LayerSurface *l = NULL;
	struct Tablet *tablet = tool->tablet;
	struct TabletPad *tablet_pad;
	double sx, sy;

	if (!change_x && !change_y)
		return;

	// TODO: apply constraints
	switch (tool->tool_v2->wlr_tool->type) {
	case WLR_TABLET_TOOL_TYPE_LENS:
	case WLR_TABLET_TOOL_TYPE_MOUSE:
		wlr_cursor_move(cursor, tablet->device, dx, dy);
		break;
	default:
		wlr_cursor_warp_absolute(cursor, tablet->device, change_x ? x : NAN,
								 change_y ? y : NAN);
		break;
	}

	motionnotify(0, NULL, 0, 0, 0, 0);

	if (config.sloppyfocus) {
		Monitor *oldmon = selmon;
		selmon = xytomon(cursor->x, cursor->y);
		if (oldmon != selmon)
			printstatus(IPC_WATCH_MONITOR | IPC_WATCH_ALL_MONITORS);
	}

	xytonode(cursor->x, cursor->y, &surface, &c, NULL, NULL, &sx, &sy);
	if (cursor_mode == CurPressed && !seat->drag &&
		surface != seat->pointer_state.focused_surface &&
		toplevel_from_wlr_surface(seat->pointer_state.focused_surface, &w,
								  &l) >= 0) {
		c = w;
		surface = seat->pointer_state.focused_surface;
		sx = cursor->x - (l ? l->scene->node.x : w->geom.x);
		sy = cursor->y - (l ? l->scene->node.y : w->geom.y);
	}

	if (config.sloppyfocus && c && c->scene && c->scene->node.enabled &&
		(surface != seat->pointer_state.focused_surface ||
		 (selmon && selmon->sel && c != selmon->sel)) &&
		!client_is_unmanaged(c))
		focusclient(c, 0);

	if (surface && !wlr_surface_accepts_tablet_v2(surface, tablet->tablet_v2))
		surface = NULL;

	if (surface != tool->curr_surface) {
		if (tool->curr_surface) {
			// TODO: wait until all buttons released before leaving
			wlr_tablet_v2_tablet_tool_notify_proximity_out(tool->tool_v2);
			wl_list_for_each(tablet_pad, &tablet_pads, link) {
				if (tablet_pad->tablet && tablet_pad->tablet == tablet)
					wlr_tablet_v2_tablet_pad_notify_leave(tablet_pad->pad_v2,
														  tool->curr_surface);
			}
			wl_list_remove(&tool->surface_destroy.link);
		}
		if (surface) {
			wl_list_for_each(tablet_pad, &tablet_pads, link) {
				if (tablet_pad->tablet && tablet_pad->tablet == tablet)
					wlr_tablet_v2_tablet_pad_notify_enter(
						tablet_pad->pad_v2, tablet->tablet_v2, surface);
			}
			wlr_tablet_v2_tablet_tool_notify_proximity_in(
				tool->tool_v2, tablet->tablet_v2, surface);
			wl_signal_add(&surface->events.destroy, &tool->surface_destroy);
		}
		tool->curr_surface = surface;
	}

	/* X11 窗口是物理尺寸，坐标乘 xwayland_scale */
#ifdef XWAYLAND
	if (c && client_is_x11(c) && config.xwayland_ignore_scale &&
		c->xwayland_scale > 0.f) {
		sx *= c->xwayland_scale;
		sy *= c->xwayland_scale;
	}
#endif

	if (surface)
		wlr_tablet_v2_tablet_tool_notify_motion(tool->tool_v2, sx, sy);

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
	handlecursoractivity();
}

void tablettoolproximity(struct wl_listener *listener, void *data) {
	struct wlr_tablet_tool_proximity_event *event = data;
	struct wlr_tablet_tool *wlr_tool = event->tool;
	struct TabletTool *tool = wlr_tool->data;
	Monitor *m_iter;

	if (!tool) {
		tool = calloc(1, sizeof(struct TabletTool));
		if (!tool) {
			mango_error(true, WLR_ERROR, "could not allocate tablet_tool");
			return;
		}
		tool->tool_v2 = wlr_tablet_tool_create(tablet_mgr, seat, wlr_tool);
		tool->surface_destroy.notify = destroytabletsurfacenotify;
		tool->destroy.notify = destroytablettool;
		tool->set_cursor.notify = tablettoolsetcursor;
		tool->tablet = event->tablet->data;
		wlr_tool->data = tool;
		wl_signal_add(&tool->tool_v2->wlr_tool->events.destroy, &tool->destroy);
		wl_signal_add(&tool->tool_v2->events.set_cursor, &tool->set_cursor);

		if (config.tablet_map_to_mon) {
			wl_list_for_each(m_iter, &mons, link) {
				if (match_monitor_spec(config.tablet_map_to_mon, m_iter)) {
					mango_error(
						true, WLR_DEBUG, "Mapping tablet %s to output %s",
						event->tablet->base.name, config.tablet_map_to_mon);
					wlr_cursor_map_input_to_output(cursor, &event->tablet->base,
												   m_iter->wlr_output);
					break;
				}
			}
		}
	}

	switch (event->state) {
	case WLR_TABLET_TOOL_PROXIMITY_OUT:
		wlr_tablet_v2_tablet_tool_notify_proximity_out(tool->tool_v2);
		if (tool->curr_surface)
			wl_list_remove(&tool->surface_destroy.link);
		tool->curr_surface = NULL;
		break;
	case WLR_TABLET_TOOL_PROXIMITY_IN:
		tablettoolmotion(tool, true, true, event->x, event->y, 0, 0);
		break;
	}
}

void tablettoolaxis(struct wl_listener *listener, void *data) {
	struct wlr_tablet_tool_axis_event *event = data;
	struct TabletTool *tool = event->tool->data;
	if (!tool)
		return;

	tablettoolmotion(tool, event->updated_axes & WLR_TABLET_TOOL_AXIS_X,
					 event->updated_axes & WLR_TABLET_TOOL_AXIS_Y, event->x,
					 event->y, event->dx, event->dy);

	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE)
		wlr_tablet_v2_tablet_tool_notify_pressure(tool->tool_v2,
												  event->pressure);
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE)
		wlr_tablet_v2_tablet_tool_notify_distance(tool->tool_v2,
												  event->distance);
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_X)
		tool->tilt_x = event->tilt_x;
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_Y)
		tool->tilt_y = event->tilt_y;
	if (event->updated_axes &
		(WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y)) {
		wlr_tablet_v2_tablet_tool_notify_tilt(tool->tool_v2, tool->tilt_x,
											  tool->tilt_y);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION)
		wlr_tablet_v2_tablet_tool_notify_rotation(tool->tool_v2,
												  event->rotation);
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER)
		wlr_tablet_v2_tablet_tool_notify_slider(tool->tool_v2, event->slider);
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL)
		wlr_tablet_v2_tablet_tool_notify_wheel(tool->tool_v2,
											   event->wheel_delta, 0);
}

void tablettoolbutton(struct wl_listener *listener, void *data) {
	struct wlr_tablet_tool_button_event *event = data;
	struct TabletTool *tool = event->tool->data;
	if (!tool)
		return;
	wlr_tablet_v2_tablet_tool_notify_button(
		tool->tool_v2, event->button,
		(enum zwp_tablet_pad_v2_button_state)event->state);
}

void tablettooltip(struct wl_listener *listener, void *data) {
	struct wlr_tablet_tool_tip_event *event = data;
	struct TabletTool *tool = event->tool->data;
	if (!tool)
		return;

	struct wlr_pointer_button_event fakeptrbtnevent = {
		.button = BTN_LEFT,
		.state = event->state == WLR_TABLET_TOOL_TIP_UP
					 ? WL_POINTER_BUTTON_STATE_RELEASED
					 : WL_POINTER_BUTTON_STATE_PRESSED,
		.time_msec = event->time_msec,
	};

	if (handle_buttonpress(&fakeptrbtnevent))
		return;

	if (!tool->curr_surface) {
		wlr_seat_pointer_notify_button(seat, fakeptrbtnevent.time_msec,
									   fakeptrbtnevent.button,
									   fakeptrbtnevent.state);
		return;
	}

	if (event->state == WLR_TABLET_TOOL_TIP_UP) {
		wlr_tablet_v2_tablet_tool_notify_up(tool->tool_v2);
		return;
	}

	wlr_tablet_v2_tablet_tool_notify_down(tool->tool_v2);
	wlr_tablet_tool_v2_start_implicit_grab(tool->tool_v2);
}

struct wlr_tablet_manager_v2 *tablet_mgr;
struct wl_list tablets;
struct wl_list tablet_pads;
struct wl_listener tablet_tool_axis = {.notify = tablettoolaxis};
struct wl_listener tablet_tool_button = {.notify = tablettoolbutton};
struct wl_listener tablet_tool_proximity = {.notify = tablettoolproximity};
struct wl_listener tablet_tool_tip = {.notify = tablettooltip};
