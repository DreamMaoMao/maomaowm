#include "mango/input/pointer.h"
#include "mango/animation/client.h"
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/dispatch/bind.h"
#include "mango/input/device.h"
#include "mango/input/keyboard.h"
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
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/util/region.h>

static struct LastCursor last_cursor;

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

	if (config.disable_trackpad) {
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

	if (config.disable_trackpad) {
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
