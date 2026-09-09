/* Trackpad gestures (swipe / pinch / hold). */
#include "mango/input/trackpad.h"
#include "mango/animation/client.h"
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/dispatch/bind.h"
#include "mango/input/device.h"
#include "mango/input/keyboard.h"
#include "mango/input/pointer.h"
#include "mango/ipc/ipc.h"
#include "mango/layout/arrange.h"
#include "mango/layout/dwindle.h"
#include "mango/layout/layout.h"
#include "mango/layout/scroll.h"
#include "mango/manage/client.h"
#include "mango/manage/layer.h"
#include "mango/manage/misc.h"
#include "mango/manage/monitor.h"
#include "mango/switcher/switcher.h"
#include <linux/input-event-codes.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/util/region.h>

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

bool check_trackpad_disabled(struct wlr_pointer *pointer) {
	if (!config.disable_trackpad)
		return false;

	return pointer_is_trackpad(pointer);
}

bool trackpad_enabled(void) { return !config.disable_trackpad; }

static bool fire_gesture_motion(uint32_t motion, uint32_t fingers) {
	bool handled = false;
	uint32_t mods = keyboard_hard_modifiers();

	for (int32_t ji = 0; ji < config.gesture_bindings_count; ji++) {
		const GestureBinding *g = &config.gesture_bindings[ji];
		if ((g->iscommonmode ||
			 (g->isdefaultmode && server.key_mode.isdefault) ||
			 (strcmp(server.key_mode.mode, g->mode) == 0)) &&
			CLEANMASK(mods) == CLEANMASK(g->mod) &&
			fingers == g->fingers_count && motion == g->motion && g->func) {
			g->func(&g->arg);
			handled = true;
		}
	}

	return handled;
}

static uint32_t swipe_opposite_motion(uint32_t motion) {
	if (motion == SWIPE_LEFT)
		return SWIPE_RIGHT;
	if (motion == SWIPE_RIGHT)
		return SWIPE_LEFT;
	if (motion == SWIPE_UP)
		return SWIPE_DOWN;
	return SWIPE_UP;
}

#define SWIPE_LOCK_DISTANCE 16

struct SwipeDrive {
	bool active;
	bool consumed;
	bool pan;
	Monitor *mon;
	uint32_t fingers;
	Client *start_sel;
	bool horizontal;
	double base;
	double dir;
	double prev_axis;
	double avg_speed;
	uint32_t speed_points;
	uint32_t motion;
	void (*func)(const Arg *);
	Arg arg;
};

static struct SwipeDrive swipe_drive;
static bool swipe_active;
static bool swipe_locked;
static bool swipe_horizontal;

static void (*view_opposite_func(void (*func)(const Arg *)))(const Arg *) {
	if (func == view_to_left)
		return view_to_right;
	if (func == view_to_right)
		return view_to_left;
	if (func == view_to_left_have_client)
		return view_to_right_have_client;
	if (func == view_to_right_have_client)
		return view_to_left_have_client;
	if (func == viewprev_have_client)
		return viewnext_have_client;
	if (func == viewnext_have_client)
		return viewprev_have_client;
	return NULL;
}

static bool swipe_func_is_view(void (*func)(const Arg *)) {
	return view_opposite_func(func) != NULL;
}

static bool swipe_layout_is_scroller(Monitor *m, bool *vertical) {
	if (!m || !m->pertag)
		return false;
	const Layout *l = m->pertag->ltidxs[get_mon_curtag(m)];
	if (!l)
		return false;
	if (l->id == VERTICAL_SCROLLER) {
		if (vertical)
			*vertical = true;
		return true;
	}
	if (l->id == SCROLLER) {
		if (vertical)
			*vertical = false;
		return true;
	}
	return false;
}

static bool swipe_func_drivable(void (*func)(const Arg *), const Arg *arg,
								uint32_t motion, Monitor *m) {
	if (swipe_func_is_view(func))
		return true;

	if (func == focus_direction) {
		bool vertical = false;
		if (!swipe_layout_is_scroller(m, &vertical))
			return false;
		if (vertical)
			return arg->i == UP || arg->i == DOWN;
		return arg->i == LEFT || arg->i == RIGHT;
	}

	if (func == toggle_overview) {
		if (motion != SWIPE_UP && motion != SWIPE_DOWN)
			return false;
		return m->isoverview ? motion == SWIPE_DOWN : motion == SWIPE_UP;
	}

	return false;
}

static bool swipe_find_binding(uint32_t motion, uint32_t fingers,
							   const GestureBinding **out) {
	uint32_t mods = keyboard_hard_modifiers();

	for (int32_t ji = 0; ji < config.gesture_bindings_count; ji++) {
		const GestureBinding *g = &config.gesture_bindings[ji];
		if ((g->iscommonmode ||
			 (g->isdefaultmode && server.key_mode.isdefault) ||
			 (strcmp(server.key_mode.mode, g->mode) == 0)) &&
			CLEANMASK(mods) == CLEANMASK(g->mod) &&
			fingers == g->fingers_count && motion == g->motion && g->func) {
			if (out)
				*out = g;
			return true;
		}
	}
	return false;
}

static bool swipe_has_running_transition(Monitor *m) {
	Client *c = NULL;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || !c->animation.running || !c->need_output_flush)
			continue;
		if (c->animation.action == TAG || c->animation.action == MOVE ||
			c->animation.action == OVERVIEW || c->animation.tagining ||
			c->animation.tagouting)
			return true;
	}
	return false;
}

static void swipe_drive_log_state(Monitor *m, const char *why, bool sel_changed,
								  Client *sel_before) {
	if (!m)
		return;

	int visible = 0;
	int tiled = 0;
	int running = 0;
	int geometry = 0;
	Client *c = NULL;

	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m)) {
			visible++;
			if (ISTILED(c))
				tiled++;
		}
		if (c->animation.running && c->need_output_flush) {
			running++;
			if (c->animation.action == TAG || c->animation.action == MOVE ||
				c->animation.action == OVERVIEW)
				geometry++;
		}
	}

	mango_error(true, WLR_DEBUG,
				"swipe drive: %s (vis=%d tiled=%d run=%d geom=%d "
				"sel_changed=%d sel_before=%p sel_now=%p)\n",
				why, visible, tiled, running, geometry, sel_changed,
				(void *)sel_before, (void *)m->sel);
}

static void swipe_drive_freeze(Monitor *m) {
	server.gesture_drive_mon = m;
	server.gesture_drive_active = true;
}

static void swipe_drive_unfreeze(void) {
	server.gesture_drive_active = false;
	server.gesture_drive_mon = NULL;
}

static double swipe_drive_axis(void) {
	return swipe_horizontal ? server.swipe_dx : server.swipe_dy;
}

static void swipe_drive_apply(Monitor *m, double p) {
	Client *c = NULL;

	if (swipe_func_is_view(swipe_drive.func)) {
		double extent = swipe_horizontal ? m->w.width : m->w.height;
		int dir = (swipe_drive.motion == SWIPE_RIGHT ||
				   swipe_drive.motion == SWIPE_DOWN)
					  ? 1
					  : -1;
		int32_t shift = (int32_t)llround(p * dir * extent);
		int32_t entry = (int32_t)llround((p - 1.0) * dir * extent);

		wl_list_for_each(c, &server.clients, link) {
			if (c->mon != m || !c->animation.running || !c->need_output_flush)
				continue;
			if (c->animation.action != TAG && c->animation.action != MOVE &&
				c->animation.action != OVERVIEW && !c->animation.tagining &&
				!c->animation.tagouting)
				continue;

			struct wlr_box box;
			if (c->animation.tagouting) {
				box = c->animation.initial;
				if (swipe_horizontal)
					box.x += shift;
				else
					box.y += shift;
			} else if (c->animation.tagining) {
				box = c->current;
				if (swipe_horizontal)
					box.x += entry;
				else
					box.y += entry;
			} else {
				client_animation_set_progress(c, p);
				continue;
			}

			wlr_scene_node_set_position(&c->scene->node, box.x, box.y);
			c->animation.current = box;
			client_apply_clip(c, 1.0f);
		}

		request_fresh_all_monitors();
		return;
	}

	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || !c->animation.running || !c->need_output_flush)
			continue;
		if (c->animation.action != TAG && c->animation.action != MOVE &&
			c->animation.action != OVERVIEW && !c->animation.tagining &&
			!c->animation.tagouting)
			continue;
		client_animation_set_progress(c, p);
	}
	request_fresh_all_monitors();
}

static bool swipe_drive_pan_target(Monitor *m, double offset, Client **out) {
	if (!m || !m->sel)
		return false;

	double mon_cx = m->w.x + m->w.width / 2.0;
	double mon_cy = m->w.y + m->w.height / 2.0;
	Client *best = NULL;
	double best_dist = 0;
	bool horizontal = swipe_horizontal;

	Client *c = NULL;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
			continue;

		double cx = c->geom.x + c->geom.width / 2.0;
		double cy = c->geom.y + c->geom.height / 2.0;
		if (horizontal)
			cx += offset;
		else
			cy += offset;

		double dx = cx - mon_cx;
		double dy = cy - mon_cy;
		double dist = dx * dx + dy * dy;
		if (!best || dist < best_dist) {
			best = c;
			best_dist = dist;
		}
	}

	if (out)
		*out = best;
	return best != NULL;
}

static void swipe_drive_apply_pan(Monitor *m, double offset) {
	Client *c = NULL;
	bool horizontal = swipe_horizontal;

	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
			continue;

		struct wlr_box box = c->geom;
		if (horizontal)
			box.x += (int32_t)llround(offset);
		else
			box.y += (int32_t)llround(offset);

		wlr_scene_node_set_position(&c->scene->node, box.x, box.y);
		c->animation.current = box;
		client_apply_clip(c, 1.0f);
	}

	request_fresh_all_monitors();
}

/* Computes the pan range that keeps at least one window centrable, then
 * applies a soft rubber band outside of it so the strip never hard-stops
 * mid-gesture. */
static double swipe_drive_pan_offset(Monitor *m, double raw) {
	double lo = 0, hi = 0;
	bool first = true;
	bool horizontal = swipe_horizontal;
	double mon_center = (horizontal ? m->w.width : m->w.height) / 2.0;

	Client *c = NULL;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
			continue;

		double center = horizontal ? c->geom.x + c->geom.width / 2.0
								   : c->geom.y + c->geom.height / 2.0;
		double center_rel = center - (horizontal ? m->w.x : m->w.y);
		double o = mon_center - center_rel;
		if (first) {
			lo = hi = o;
			first = false;
		} else {
			if (o < lo)
				lo = o;
			if (o > hi)
				hi = o;
		}
	}

	if (first)
		return raw;

	/* Soft rubber band outside the valid range; never a hard stop. */
	if (raw < lo)
		return lo + (raw - lo) * 0.25;
	if (raw > hi)
		return hi + (raw - hi) * 0.25;
	return raw;
}

static bool swipe_drive_begin(uint32_t fingers) {
	Monitor *m = server.selected_monitor;
	if (!m || !config.gesture_live)
		return false;
	Client *sel_before = m->sel;

	double axis = swipe_drive_axis();
	uint32_t motion = swipe_horizontal ? (axis < 0 ? SWIPE_LEFT : SWIPE_RIGHT)
									   : (axis < 0 ? SWIPE_UP : SWIPE_DOWN);

	const GestureBinding *binding = NULL;
	if (!swipe_find_binding(motion, fingers, &binding) ||
		!swipe_func_drivable(binding->func, &binding->arg, motion, m))
		return false;

	void (*exec_func)(const Arg *) = binding->func;
	Arg exec_arg = binding->arg;

	swipe_drive.consumed = true;
	swipe_drive.mon = m;
	swipe_drive.fingers = fingers;
	swipe_drive.horizontal = swipe_horizontal;
	swipe_drive.dir = axis < 0 ? -1.0 : 1.0;
	swipe_drive.base = axis - swipe_drive.dir * SWIPE_LOCK_DISTANCE;
	swipe_drive.motion = motion;
	swipe_drive.func = exec_func;
	swipe_drive.arg = exec_arg;
	swipe_drive.active = false;
	swipe_drive.pan = false;
	swipe_drive.start_sel = sel_before;

	if (exec_func == focus_direction) {
		swipe_drive.base = axis;
		swipe_drive.pan = true;
		swipe_drive.active = true;
		swipe_drive_freeze(m);
		swipe_drive_apply_pan(m, 0.0);
		swipe_drive_log_state(m, "panning scroller", false, sel_before);
		return true;
	}

	swipe_drive_freeze(m);
	exec_func(&exec_arg);

	if (!swipe_has_running_transition(m)) {
		swipe_drive_unfreeze();
		swipe_drive_log_state(m, "fired but no transition",
							  m->sel != sel_before, sel_before);
		mango_error(true, WLR_DEBUG,
					"swipe drive: %s fired but produced no transition\n",
					binding->func == toggle_overview   ? "toggle_overview"
					: binding->func == focus_direction ? "focus_direction"
													   : "view switch");
		return true; /* command had no effect (edge, empty tag, ...) */
	}

	swipe_drive.active = true;
	swipe_drive_apply(m, 0.0);
	swipe_drive_log_state(m, "driving transition", m->sel != sel_before,
						  sel_before);
	mango_error(true, WLR_DEBUG,
				"swipe drive: driving transition, motion=%u fingers=%u\n",
				swipe_drive.motion, swipe_drive.fingers);
	return true;
}

static bool swipe_drive_fire_opposite(void) {
	if (!swipe_drive.func)
		return false;

	if (swipe_func_is_view(swipe_drive.func)) {
		void (*opposite)(const Arg *) = view_opposite_func(swipe_drive.func);
		if (!opposite)
			return false;
		opposite(&swipe_drive.arg);
		swipe_drive.func = opposite;
	} else if (swipe_drive.func == focus_direction) {
		Arg a = swipe_drive.arg;
		if (swipe_drive.motion == SWIPE_LEFT)
			a.i = RIGHT;
		else if (swipe_drive.motion == SWIPE_RIGHT)
			a.i = LEFT;
		else if (swipe_drive.motion == SWIPE_UP)
			a.i = DOWN;
		else
			a.i = UP;
		focus_direction(&a);
		swipe_drive.arg = a;
	} else if (swipe_drive.func == toggle_overview) {
		toggle_overview(&swipe_drive.arg);
	} else {
		return false;
	}

	swipe_drive.motion = swipe_opposite_motion(swipe_drive.motion);
	swipe_drive.dir = -swipe_drive.dir;
	return true;
}

/* Advance the driven transition on every swipe update.  Returns true when the
 * compositor took over the gesture. */
static bool swipe_drive_update(uint32_t fingers) {
	if (!swipe_active || !config.gesture_live)
		return false;

	double adx = fabs(server.swipe_dx);
	double ady = fabs(server.swipe_dy);
	if (!swipe_locked) {
		if (adx < SWIPE_LOCK_DISTANCE && ady < SWIPE_LOCK_DISTANCE)
			return false;
		swipe_locked = true;
		swipe_horizontal = adx >= ady;
		swipe_drive.prev_axis = swipe_drive_axis();
	}

	double axis = swipe_drive_axis();
	double step = fabs(axis - swipe_drive.prev_axis);
	swipe_drive.prev_axis = axis;
	if (swipe_drive.speed_points < 1000) {
		swipe_drive.avg_speed =
			(swipe_drive.avg_speed * swipe_drive.speed_points + step) /
			(swipe_drive.speed_points + 1);
		swipe_drive.speed_points++;
	}

	if (!swipe_drive.active) {
		if (swipe_drive.consumed)
			return true;
		return swipe_drive_begin(fingers);
	}

	Monitor *m = swipe_drive.mon;
	double distance = config.gesture_swipe_distance;
	double delta = (axis - swipe_drive.base) * swipe_drive.dir;
	double p = delta / distance;

	if (swipe_drive.pan) {
		double extent = swipe_horizontal ? m->w.width : m->w.height;
		double raw = ((axis - swipe_drive.base) / distance) * extent;
		double offset = swipe_drive_pan_offset(m, raw);
		swipe_drive_apply_pan(m, offset);
		return true;
	}

	if (p >= 1.0) {
		swipe_drive_apply(m, 1.0);
		return true;
	}

	if (delta <= -SWIPE_LOCK_DISTANCE) {
		swipe_drive.base = axis;
		if (!swipe_drive_fire_opposite())
			return true;
		if (!swipe_has_running_transition(m)) {
			swipe_drive.active = false;
			swipe_drive_unfreeze();
			return true;
		}
		swipe_drive_freeze(m);
		swipe_drive_apply(m, 0.0);
		return true;
	}

	if (p < 0.0)
		p = 0.0;
	if (p > 1.0)
		p = 1.0;
	swipe_drive_apply(m, p);
	return true;
}

static void swipe_drive_end(void) {
	Monitor *m = swipe_drive.mon;

	if (swipe_drive.active && m) {
		if (swipe_drive.pan) {
			double axis = swipe_drive_axis();
			double distance = config.gesture_swipe_distance;
			double extent = swipe_horizontal ? m->w.width : m->w.height;
			double raw = ((axis - swipe_drive.base) / distance) * extent;
			double offset = swipe_drive_pan_offset(m, raw);

			swipe_drive_unfreeze();

			Client *target = NULL;
			swipe_drive_pan_target(m, offset, &target);
			if (target && target != swipe_drive.start_sel) {
				mango_error(true, WLR_DEBUG,
							"swipe drive: pan commit, offset=%.0f target=%p\n",
							offset, (void *)target);

				int32_t shift = (int32_t)llround(offset);
				if (shift) {
					Client *c = NULL;
					wl_list_for_each(c, &server.clients, link) {
						if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
							continue;
						if (swipe_horizontal)
							c->geom.x += shift;
						else
							c->geom.y += shift;
					}
				}

				client_focus(target, 1);
				arrange(m, false, false);
			} else {
				mango_error(true, WLR_DEBUG,
							"swipe drive: pan revert, offset=%.0f\n", offset);
				arrange(m, false, false);
			}

			swipe_drive.active = false;
			swipe_drive.consumed = false;
			swipe_drive.speed_points = 0;
			swipe_drive.avg_speed = 0;
			return;
		}

		double axis = swipe_drive_axis();
		double distance = config.gesture_swipe_distance;
		double delta = (axis - swipe_drive.base) * swipe_drive.dir;
		double p = delta / distance;
		if (p < 0.0)
			p = 0.0;
		if (p > 1.0)
			p = 1.0;

		bool commit =
			delta >= distance * config.gesture_swipe_cancel_ratio ||
			(swipe_drive.speed_points > 0 &&
			 swipe_drive.avg_speed >= config.gesture_swipe_min_speed_to_force);

		swipe_drive_unfreeze();

		if (commit) {
			Client *c = NULL;
			bool mirror_view = swipe_func_is_view(swipe_drive.func);
			mango_error(true, WLR_DEBUG,
						"swipe drive: commit, p=%.2f speed=%.1f\n", p,
						swipe_drive.avg_speed);
			wl_list_for_each(c, &server.clients, link) {
				if (c->mon != m || !c->animation.running ||
					!c->need_output_flush)
					continue;
				if (c->animation.action != TAG && c->animation.action != MOVE &&
					c->animation.action != OVERVIEW && !c->animation.tagining &&
					!c->animation.tagouting)
					continue;

				if (mirror_view &&
					(c->animation.tagouting || c->animation.tagining)) {
					double extent = swipe_horizontal ? m->w.width : m->w.height;
					int dir = (swipe_drive.motion == SWIPE_RIGHT ||
							   swipe_drive.motion == SWIPE_DOWN)
								  ? 1
								  : -1;
					int32_t shift = (int32_t)llround(dir * extent);
					double remain = 1.0 - p;
					if (remain < 0.05)
						remain = 0.05;

					struct wlr_box target;
					if (c->animation.tagouting) {
						target = c->animation.initial;
						if (swipe_horizontal)
							target.x += shift;
						else
							target.y += shift;
					} else {
						target = c->geom;
					}

					c->animation.initial = c->animation.current;
					c->current = target;
					c->pending = target;
					c->animation.duration = (uint32_t)MANGO_MAX(
						1, (int32_t)(c->animation.duration * remain));
					c->animation.time_started = get_now_in_ms();
					c->animation.action = TAG;
					c->animation.running = true;
					c->need_output_flush = true;
				} else {
					client_animation_resume(c, 1.0 - p);
				}
			}
			request_fresh_all_monitors();
			if (!config.animations)
				pointer_process_motion(0, NULL, 0, 0, 0, 0);
		} else {
			mango_error(true, WLR_DEBUG,
						"swipe drive: revert, p=%.2f speed=%.1f\n", p,
						swipe_drive.avg_speed);
			swipe_drive_fire_opposite();
		}

		swipe_drive.active = false;
	}

	swipe_drive.consumed = false;
	swipe_drive.speed_points = 0;
	swipe_drive.avg_speed = 0;
}

void handle_cursor_swipe_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_begin_event *event = data;

	if (!trackpad_enabled()) {
		return;
	}

	swipe_drive_unfreeze();
	swipe_active = true;
	swipe_locked = false;
	swipe_horizontal = false;
	memset(&swipe_drive, 0, sizeof(swipe_drive));
	server.swipe_fingers = event->fingers;
	server.swipe_dx = 0;
	server.swipe_dy = 0;

	// Forward swipe begin event to client
	wlr_pointer_gestures_v1_send_swipe_begin(
		server.pointer_gestures, server.seat, event->time_msec, event->fingers);
}

void handle_cursor_swipe_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_update_event *event = data;

	if (!trackpad_enabled()) {
		return;
	}

	server.swipe_fingers = event->fingers;
	// Accumulate swipe distance
	server.swipe_dx += event->dx;
	server.swipe_dy += event->dy;

	swipe_drive_update(event->fingers);

	// Forward swipe update event to client
	wlr_pointer_gestures_v1_send_swipe_update(server.pointer_gestures,
											  server.seat, event->time_msec,
											  event->dx, event->dy);
}

void handle_cursor_swipe_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_end_event *event = data;

	if (!trackpad_enabled()) {
		return;
	}

	bool consumed = swipe_drive.consumed;
	swipe_drive_end();

	if (!consumed && !event->cancelled) {
		/* No finger-driven transition: keep the previous release-based
		 * behavior so short flicks and non-drivable bindings still work. */
		pointer_process_swipe_end(event);
	}

	// Forward swipe end event to client
	wlr_pointer_gestures_v1_send_swipe_end(server.pointer_gestures, server.seat,
										   event->time_msec, event->cancelled);

	swipe_active = false;
	server.swipe_dx = 0;
	server.swipe_dy = 0;
}

void handle_cursor_pinch_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_begin_event *event = data;

	if (!trackpad_enabled()) {
		return;
	}

	// Forward pinch begin event to client
	wlr_pointer_gestures_v1_send_pinch_begin(
		server.pointer_gestures, server.seat, event->time_msec, event->fingers);
}

void handle_cursor_pinch_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_update_event *event = data;

	if (!trackpad_enabled()) {
		return;
	}

	// Forward pinch update event to client
	wlr_pointer_gestures_v1_send_pinch_update(
		server.pointer_gestures, server.seat, event->time_msec, event->dx,
		event->dy, event->scale, event->rotation);
}

void handle_cursor_pinch_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_end_event *event = data;

	if (!trackpad_enabled()) {
		return;
	}

	// Forward pinch end event to client
	wlr_pointer_gestures_v1_send_pinch_end(server.pointer_gestures, server.seat,
										   event->time_msec, event->cancelled);
}

void handle_cursor_hold_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_begin_event *event = data;

	if (!trackpad_enabled()) {
		return;
	}

	// Forward hold begin event to client
	wlr_pointer_gestures_v1_send_hold_begin(
		server.pointer_gestures, server.seat, event->time_msec, event->fingers);
}

void handle_cursor_hold_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_end_event *event = data;

	if (!trackpad_enabled()) {
		return;
	}

	// Forward hold end event to client
	wlr_pointer_gestures_v1_send_hold_end(server.pointer_gestures, server.seat,
										  event->time_msec, event->cancelled);
}

// New from here
int32_t pointer_process_swipe_end(struct wlr_pointer_swipe_end_event *event) {
	uint32_t motion;
	uint32_t adx = (int32_t)round(fabs(server.swipe_dx));
	uint32_t ady = (int32_t)round(fabs(server.swipe_dy));

	if (event->cancelled) {
		return 0;
	}

	// Require absolute distance movement beyond a small thresh-hold
	if (adx * adx + ady * ady <
		config.swipe_min_threshold * config.swipe_min_threshold) {
		return 0;
	}

	if (adx > ady) {
		motion = server.swipe_dx < 0 ? SWIPE_LEFT : SWIPE_RIGHT;
	} else {
		motion = server.swipe_dy < 0 ? SWIPE_UP : SWIPE_DOWN;
	}

	return fire_gesture_motion(motion, server.swipe_fingers) ? 1 : 0;
}
