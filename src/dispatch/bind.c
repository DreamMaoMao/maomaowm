#include "mango/dispatch/bind.h"
#include "mango/common/server.h"
#include "mango/manage/client.h"
#include "mango/common/log.h"
#include "mango/manage/monitor.h"
#include "mango/input/pointer.h"
#include "mango/layout/arrange.h"
#include "mango/layout/layout.h"
#include "mango/manage/misc.h"
#include "mango/animation/client.h"
#include "mango/config/parse_config.h"
#include "mango/ipc/ipc.h"
#include "mango/layout/scroll.h"
#include "mango/ext-protocol/xdg-activation.h"
#include "mango/ext-protocol/foreign-toplevel.h"
#include "mango/layout/overview.h"
#include "mango/ext-protocol/ext-workspace.h"
#include "mango/overview/overview.h"
#include "mango/layout/dwindle.h"
#include "mango/common/util.h"

void bind_to_view(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	uint32_t target = arg->ui;

	if (config.view_current_to_back &&
		server.selected_monitor->pertag->curtag &&
		(target & TAGMASK) ==
			(server.selected_monitor
				 ->tagset[server.selected_monitor->seltags])) {
		if (server.selected_monitor->pertag->prevtag)
			target = 1 << (server.selected_monitor->pertag->prevtag - 1);
		else
			// prevtag==0: previous view was all-tags or special,
			// decide by the other tagset
			target = (server.selected_monitor
						  ->tagset[server.selected_monitor->seltags ^ 1] &
					  TAG0_MASK)
						 ? TAG0_MASK
						 : 0;
	}

	if (!config.view_current_to_back &&
		(target & TAGMASK) ==
			(server.selected_monitor
				 ->tagset[server.selected_monitor->seltags])) {
		return;
	}

	if ((int32_t)target == INT_MIN &&
		server.selected_monitor->pertag->curtag == 0) {
		if (config.view_current_to_back &&
			server.selected_monitor->pertag->prevtag)
			target = 1 << (server.selected_monitor->pertag->prevtag - 1);
		else
			target = 0;
	}

	// TAG0_MASK is INT_MIN too; handle it before the all-tags branch
	if (target == TAG0_MASK) {
		client_switch_view(&(Arg){.ui = target, .i = arg->i}, true);
		return;
	}

	if (target == 0 || (int32_t)target == INT_MIN) {
		client_switch_view(&(Arg){.ui = ~0 & TAGMASK, .i = arg->i}, false);
	} else {
		client_switch_view(&(Arg){.ui = target, .i = arg->i}, true);
	}
	return;
}

void change_vt(const Arg *arg) {
	struct timespec ts;

	server.allow_frame_scheduling = false;

	if (server.selected_monitor) {
		server.chvt_backup_tag = server.selected_monitor->pertag->curtag;
		strncpy(server.chvt_backup_monitor_name,
				server.selected_monitor->wlr_output->name,
				sizeof(server.chvt_backup_monitor_name) - 1);
	}

	wlr_session_change_vt(server.session, arg->ui);

	ts.tv_sec = 0;
	ts.tv_nsec = 100000000;
	nanosleep(&ts, NULL);

	server.allow_frame_scheduling = true;
	return;
}

void create_virtual_output(const Arg *arg) {
	if (!wlr_backend_is_multi(server.backend)) {
		mango_error(true, WLR_ERROR, "Expected a multi backend");
		return;
	}

	bool done = false;
	wlr_multi_for_each_backend(server.backend, create_output, &done);

	if (!done) {
		mango_error(true, WLR_ERROR, "Failed to create virtual output");
		return;
	}

	mango_error(true, WLR_INFO, "Virtual output created");
	return;
}

void destroy_all_virtual_output(const Arg *arg) {
	if (!wlr_backend_is_multi(server.backend)) {
		mango_error(true, WLR_ERROR, "Expected a multi backend");
		return;
	}

	Monitor *m, *tmp;
	wl_list_for_each_safe(m, tmp, &server.monitors, link) {
		if (wlr_output_is_headless(m->wlr_output)) {
			wlr_output_destroy(m->wlr_output);
			mango_error(true, WLR_INFO, "Virtual output destroyed");
		}
	}
	return;
}

void reset_gaps(const Arg *arg) {
	if (server.selected_monitor && is_special_active(server.selected_monitor)) {
		setgaps(config.special_gappoh, config.special_gappov,
				config.special_gappih, config.special_gappiv);
	} else {
		setgaps(config.gappoh, config.gappov, config.gappih, config.gappiv);
	}
	return;
}

void exchange_client(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c || c->isfloating)
		return;

	if ((c->isfullscreen || c->ismaximizescreen) && !is_scroller_layout(c->mon))
		return;

	Client *tc = direction_select(arg);
	tc = get_focused_stack_client(tc, arg->tc);

	if (!tc)
		return;

	client_exchange(c, tc);
	return;
}

void exchange_stack_client(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	Client *tc = NULL;
	if (!c || c->isfloating || c->isfullscreen || c->ismaximizescreen)
		return;
	if (arg->i == NEXT) {
		tc = get_next_stack_client(c, false);
	} else {
		tc = get_next_stack_client(c, true);
	}
	if (tc)
		client_exchange(c, tc);
	return;
}

bool view_shift_tag(const Arg *arg, int dir);
bool view_shift_tag_have_client(const Arg *arg, int dir);

void focus_direction(const Arg *arg) {

	if (!server.selected_monitor)
		return;

	Client *c = NULL;
	c = direction_select(arg);

	if (!server.selected_monitor->isoverview)
		c = get_focused_stack_client(c, arg->tc);
	if (c) {
		client_focus(c, 1);
		if (config.warpcursor)
			pointer_warp_to_client(c);
	} else {
		// no cross-tag/monitor jumps inside the special workspace
		if (!is_special_active(server.selected_monitor)) {
			if (config.focus_cross_tag) {
				if (arg->i == LEFT || arg->i == UP)
					view_shift_tag_have_client(&(Arg){0}, -1);
				if (arg->i == RIGHT || arg->i == DOWN)
					view_shift_tag_have_client(&(Arg){0}, 1);
			} else if (config.focus_cross_monitor) {
				focus_monitor(arg);
			}
		}
	}
	return;
}

void focus_window_or_workspace(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	if (server.selected_monitor->isoverview)
		return;

	Client *c = NULL;

	c = direction_select(arg);
	if (!server.selected_monitor->isoverview)
		c = get_focused_stack_client(c, arg->tc);
	if (c && c->mon == server.selected_monitor) {
		client_focus(c, 1);
		if (config.warpcursor)
			pointer_warp_to_client(c);
		return;
	}

	if (!is_special_active(server.selected_monitor)) {
		int dir = arg->i;

		if (dir == LEFT || dir == UP) {
			if (!view_shift_tag_have_client(&(Arg){0}, -1))
				view_shift_tag(&(Arg){0}, -1);
		} else if (dir == RIGHT || dir == DOWN) {
			if (!view_shift_tag_have_client(&(Arg){0}, 1))
				view_shift_tag(&(Arg){0}, 1);
		}
	}

	return;
}

void group_join(const Arg *arg) {

	if (!server.selected_monitor)
		return;

	Monitor *oldmon = NULL;

	Client *need_join_client = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!need_join_client || !need_join_client->mon)
		return;

	if (need_join_client->mon->isoverview)
		return;

	Client *need_replace_client = NULL;
	need_replace_client = direction_select(arg);

	if (!need_replace_client || !need_replace_client->mon)
		return;

	if (need_join_client == need_replace_client)
		return;

	if (need_join_client->group_next || need_join_client->group_prev) {
		group_leave(&(Arg){.tc = need_join_client});
	}

	if (need_join_client->mon != need_replace_client->mon) {
		oldmon = need_join_client->mon;
		need_join_client->mon = need_replace_client->mon;
	}

	if (!need_replace_client->group_prev && !need_replace_client->group_next) {
		need_replace_client->isgroupfocusing = true;
	}

	need_join_client->group_next = need_replace_client;

	if (need_replace_client->group_prev) {
		need_replace_client->group_prev->group_next = need_join_client;
	}

	need_join_client->group_prev = need_replace_client->group_prev;

	need_replace_client->group_prev = need_join_client;

	client_focus_group_member(need_join_client);
	arrange(need_join_client->mon, false, false);

	// oldmon may already be destroyed.
	if (oldmon) {
		arrange(oldmon, false, false);
	}

	return;
}

void group_leave(const Arg *arg) {

	if (!server.selected_monitor)
		return;
	Client *tc = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!tc || !tc->mon || !tc->isgroupfocusing)
		return;
	if (!tc->group_next && !tc->group_prev) {
		return;
	}

	if (tc->mon->isoverview)
		return;

	Client *rc = tc->group_next ? tc->group_next : tc->group_prev;

	client_focus_group_member(rc);
	client_group_detach(tc);

	tc->isgroupfocusing = false;
	tc->is_logic_hide = false;

	if (!rc->group_prev && !rc->group_next) {
		rc->isgroupfocusing = false;
	}

	arrange(tc->mon, false, false);

	return;
}

void focus_last(const Arg *arg) {
	Client *c = NULL;
	Client *tc = NULL;
	bool begin = false;
	uint32_t target = 0;

	wl_list_for_each(c, &server.focus_stack, flink) {
		if (c->iskilling || c->isminimized || c->isunglobal ||
			!client_surface(c)->mapped || client_is_unmanaged(c) ||
			client_is_x11_popup(c))
			continue;

		if (server.selected_monitor && !server.selected_monitor->sel) {
			tc = c;
			break;
		}

		if (server.selected_monitor && c == server.selected_monitor->sel &&
			!begin) {
			begin = true;
			continue;
		}

		if (begin) {
			tc = c;
			break;
		}
	}

	if (!tc || !client_surface(tc)->mapped)
		return;

	if ((int32_t)tc->tags > 0) {
		client_focus(tc, 1);
		target = get_tags_first_tag(tc->tags);
		client_switch_view(&(Arg){.ui = target}, true);
	}
	return;
}

void toggle_trackpad_enable(const Arg *arg) {
	config.disable_trackpad = !config.disable_trackpad;
	return;
}

void focus_monitor(const Arg *arg) {
	Client *c = NULL;
	Monitor *m = NULL;
	Monitor *tm = NULL;

	if (arg->i != UNDIR) {
		tm = monitor_from_direction(arg->i);
	} else if (arg->v) {
		wl_list_for_each(m, &server.monitors, link) {
			if (!m->wlr_output->enabled) {
				continue;
			}
			if (match_monitor_spec(arg->v, m)) {
				tm = m;
				break;
			}
		}
	} else {
		return;
	}

	if (!tm || !tm->wlr_output->enabled || tm == server.selected_monitor)
		return;

	server.selected_monitor = tm;
	if (config.warpcursor) {
		pointer_warp_to_monitor(server.selected_monitor);
	}
	c = arg->tc ? arg->tc : client_focus_top(server.selected_monitor);
	if (!c) {
		server.selected_monitor->sel = NULL;
		wlr_seat_pointer_notify_clear_focus(server.seat);
		wlr_seat_keyboard_notify_clear_focus(server.seat);
		client_focus(NULL, 0);
	} else
		client_focus(c, 1);

	return;
}

void focus_stack(const Arg *arg) {
	Client *sel = arg->tc ? arg->tc : client_focus_top(server.selected_monitor);
	Client *tc = NULL;

	if (!sel)
		return;
	if (arg->i == NEXT) {
		tc = get_next_stack_client(sel, false);
	} else {
		tc = get_next_stack_client(sel, true);
	}

	if (!tc)
		return;

	client_focus(tc, 1);
	if (config.warpcursor)
		pointer_warp_to_client(tc);
	return;
}

/*
 * overcircle: opens/cycles overview (centered tab layout)
 * - not in overview: enters overview
 * - already in overview: cycles windows of the current monitor in insertion
 * order and rearranges.
 */
void over_circle(const Arg *arg) {
	if (!server.selected_monitor || server.grab_client)
		return;

	Client *sel = arg->tc ? arg->tc : server.selected_monitor->sel;

	if (server.selected_monitor->isoverview &&
		!server.selected_monitor->is_jump_mode &&
		!server.selected_monitor->ov_normal_mode && sel) {
		server.selected_monitor->ov_tab_layout = 1;
		Client *tc = arg->i == NEXT ? get_next_stack_client(sel, false)
									: get_next_stack_client(sel, true);
		if (!tc)
			return;

		client_focus(tc, 1);

		/* Rearranges after focus change so the tab layout follows focus. */
		arrange(server.selected_monitor, true, false);
		return;
	}

	/* Entering overview: enables the centered tab layout; the rest is handled
	 * by toggle_overview. */
	server.selected_monitor->ov_tab_layout = 1;
	toggle_overview(arg);
	if (!server.selected_monitor->isoverview)
		server.selected_monitor->ov_tab_layout = 0;
}

void group_focus(const Arg *arg) {
	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c || !c->mon)
		return;

	if (!c->group_prev && !c->group_next) {
		return;
	}

	if (c->mon->isoverview)
		return;

	Client *tc = NULL;

	if (arg->i == NEXT) {
		tc = c->group_next;
	} else {
		tc = c->group_prev;
	}

	if (!tc)
		return;

	client_focus_group_member(tc);
	arrange(tc->mon, false, false);
	return;
}

void inc_nmaster(const Arg *arg) {
	if (!arg || !server.selected_monitor)
		return;
	uint32_t tag = get_mon_curtag(server.selected_monitor);
	server.selected_monitor->pertag->nmasters[tag] =
		MANGO_MAX(server.selected_monitor->pertag->nmasters[tag] + arg->i, 0);
	arrange(server.selected_monitor, false, false);
	return;
}

void increase_gaps(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	setgaps(server.selected_monitor->gappoh + arg->i,
			server.selected_monitor->gappov + arg->i,
			server.selected_monitor->gappih + arg->i,
			server.selected_monitor->gappiv + arg->i);
	return;
}

void increase_inner_gap(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	setgaps(server.selected_monitor->gappoh, server.selected_monitor->gappov,
			server.selected_monitor->gappih + arg->i,
			server.selected_monitor->gappiv + arg->i);
	return;
}

void increase_outer_gap(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	setgaps(server.selected_monitor->gappoh + arg->i,
			server.selected_monitor->gappov + arg->i,
			server.selected_monitor->gappih, server.selected_monitor->gappiv);
	return;
}

void increase_inner_horizontal_gap(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	setgaps(server.selected_monitor->gappoh, server.selected_monitor->gappov,
			server.selected_monitor->gappih + arg->i,
			server.selected_monitor->gappiv);
	return;
}

void increase_inner_vertical_gap(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	setgaps(server.selected_monitor->gappoh, server.selected_monitor->gappov,
			server.selected_monitor->gappih,
			server.selected_monitor->gappiv + arg->i);
	return;
}

void increase_outer_horizontal_gap(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	setgaps(server.selected_monitor->gappoh + arg->i,
			server.selected_monitor->gappov, server.selected_monitor->gappih,
			server.selected_monitor->gappiv);
	return;
}

void increase_outer_vertical_gap(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	setgaps(server.selected_monitor->gappoh,
			server.selected_monitor->gappov + arg->i,
			server.selected_monitor->gappih, server.selected_monitor->gappiv);
	return;
}

void set_master_factor(const Arg *arg) {
	float f;
	Client *c = NULL;

	if (!arg || !server.selected_monitor ||
		!server.selected_monitor->pertag
			 ->ltidxs[get_mon_curtag(server.selected_monitor)]
			 ->arrange)
		return;
	f = arg->f < 1.0
			? arg->f + server.selected_monitor->pertag
						   ->mfacts[get_mon_curtag(server.selected_monitor)]
			: arg->f - 1.0;
	if (f < 0.1 || f > 0.9)
		return;

	server.selected_monitor->pertag
		->mfacts[get_mon_curtag(server.selected_monitor)] = f;
	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, server.selected_monitor) && ISTILED(c)) {
			c->master_mfact_per = f;
		}
	}
	arrange(server.selected_monitor, false, false);
	return;
}

void kill_client(const Arg *arg) {
	Client *c =
		arg->tc
			? arg->tc
			: (server.selected_monitor ? server.selected_monitor->sel : NULL);
	if (c) {
		if (arg->i == FORCE) {
			client_pending_force_kill(c);
		} else {
			pending_kill_client(c);
		}
	}
	return;
}

void move_resize(const Arg *arg) {
	const char *cursors[] = {"nw-resize", "ne-resize", "sw-resize",
							 "se-resize"};

	if (server.cursor_mode != CurNormal && server.cursor_mode != CurPressed)
		return;
	node_at_point(server.cursor->x, server.cursor->y, NULL, &server.grab_client,
				  NULL, NULL, NULL, NULL);
	if (!server.grab_client || client_is_unmanaged(server.grab_client) ||
		server.grab_client->isfullscreen ||
		server.grab_client->ismaximizescreen) {
		server.grab_client = NULL;
		return;
	}
	if (server.grab_client->isfloating == 0 && arg->ui == CurMove) {
		server.grab_client->drag_to_tile = true;
		exit_scroller_stack(server.grab_client);
		client_set_floating(server.grab_client, 1);
		server.grab_client->drag_tile_float_backup_geom =
			server.grab_client->float_geom;
		server.grab_client->old_stack_inner_per = 0.0f;
		server.grab_client->old_master_inner_per = 0.0f;
		set_size_per(server.grab_client->mon, server.grab_client);
	}

	if (server.grab_client && server.grab_client->drag_to_tile &&
		config.drag_tile_to_tile && config.drag_tile_small) {
		server.grab_client->geom.x = server.cursor->x - 150;
		server.grab_client->geom.y = server.cursor->y - 150;
		server.grab_client->geom.width = 300;
		server.grab_client->geom.height = 300;
		resize(server.grab_client, server.grab_client->geom, 1);
	}

	switch (server.cursor_mode = arg->ui) {
	case CurMove:
		server.grab_offset_x = server.cursor->x - server.grab_client->geom.x;
		server.grab_offset_y = server.cursor->y - server.grab_client->geom.y;
		wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "grab");
		break;
	case CurResize:
		if (server.grab_client->isfloating) {
			server.resize_corner = config.drag_corner;
			server.grab_offset_x = (int)round(server.cursor->x);
			server.grab_offset_y = (int)round(server.cursor->y);
			if (server.resize_corner == 4)
				server.resize_corner =
					(server.grab_offset_x - server.grab_client->geom.x <
							 server.grab_client->geom.x +
								 server.grab_client->geom.width -
								 server.grab_offset_x
						 ? 0
						 : 1) +
					(server.grab_offset_y - server.grab_client->geom.y <
							 server.grab_client->geom.y +
								 server.grab_client->geom.height -
								 server.grab_offset_y
						 ? 0
						 : 2);

			if (config.drag_warp_cursor) {
				server.grab_offset_x = server.resize_corner & 1
										   ? server.grab_client->geom.x +
												 server.grab_client->geom.width
										   : server.grab_client->geom.x;
				server.grab_offset_y = server.resize_corner & 2
										   ? server.grab_client->geom.y +
												 server.grab_client->geom.height
										   : server.grab_client->geom.y;
				wlr_cursor_warp_closest(server.cursor, NULL,
										server.grab_offset_x,
										server.grab_offset_y);
			}

			wlr_cursor_set_xcursor(server.cursor, server.cursor_manager,
								   cursors[server.resize_corner]);
		} else {
			wlr_cursor_set_xcursor(server.cursor, server.cursor_manager,
								   "grab");
		}
		break;
	}
	return;
}

void move_window(const Arg *arg) {
	Client *c =
		arg->tc
			? arg->tc
			: (server.selected_monitor ? server.selected_monitor->sel : NULL);
	if (!c || c->isfullscreen)
		return;
	if (!c->isfloating)
		client_set_floating(c, 1);

	switch (arg->ui) {
	case NUM_TYPE_MINUS:
		c->geom.x -= arg->i;
		break;
	case NUM_TYPE_PLUS:
		c->geom.x += arg->i;
		break;
	default:
		c->geom.x = arg->i;
		break;
	}

	switch (arg->ui2) {
	case NUM_TYPE_MINUS:
		c->geom.y -= arg->i2;
		break;
	case NUM_TYPE_PLUS:
		c->geom.y += arg->i2;
		break;
	default:
		c->geom.y = arg->i2;
		break;
	}

	c->iscustomsize = 1;
	c->float_geom = c->geom;
	resize(c, c->geom, 0);
	return;
}

void quit(const Arg *arg) {
	wl_display_terminate(server.display);
	return;
}

void resize_window(const Arg *arg) {
	Client *c =
		arg->tc
			? arg->tc
			: (server.selected_monitor ? server.selected_monitor->sel : NULL);
	int32_t offsetx = 0, offsety = 0;

	if (!c || c->isfullscreen || c->ismaximizescreen)
		return;

	int32_t animations_state_backup = config.animations;
	if (!c->isfloating)
		config.animations = 0;

	if (ISTILED(c)) {
		switch (arg->ui) {
		case NUM_TYPE_MINUS:
			offsetx = -arg->i;
			break;
		case NUM_TYPE_PLUS:
			offsetx = arg->i;
			break;
		default:
			offsetx = arg->i;
			break;
		}

		switch (arg->ui2) {
		case NUM_TYPE_MINUS:
			offsety = -arg->i2;
			break;
		case NUM_TYPE_PLUS:
			offsety = arg->i2;
			break;
		default:
			offsety = arg->i2;
			break;
		}
		resize_tile_client(c, false, offsetx, offsety, 0);
		config.animations = animations_state_backup;
		return;
	}

	switch (arg->ui) {
	case NUM_TYPE_MINUS:
		c->geom.width -= arg->i;
		break;
	case NUM_TYPE_PLUS:
		c->geom.width += arg->i;
		break;
	default:
		c->geom.width = arg->i;
		break;
	}

	switch (arg->ui2) {
	case NUM_TYPE_MINUS:
		c->geom.height -= arg->i2;
		break;
	case NUM_TYPE_PLUS:
		c->geom.height += arg->i2;
		break;
	default:
		c->geom.height = arg->i2;
		break;
	}

	c->iscustomsize = 1;
	c->float_geom = c->geom;
	resize(c, c->geom, 0);
	config.animations = animations_state_backup;
	return;
}

void restore_minimized(const Arg *arg) {
	if (server.selected_monitor && server.selected_monitor->isoverview)
		return;

	Client *c = NULL;
	Client *focused =
		server.selected_monitor ? server.selected_monitor->sel : NULL;
	if (!focused && server.selected_monitor)
		focused = client_focus_top(server.selected_monitor);

	/* 1. If focused window or any shown scratchpad exists, evict it */
	if (focused && focused->is_in_scratchpad && focused->is_scratchpad_show) {
		c = focused;
	} else {
		Client *tc = NULL;
		wl_list_for_each(tc, &server.clients, link) {
			if ((tc->mon == server.selected_monitor ||
				 config.scratchpad_cross_monitor) &&
				tc->is_in_scratchpad && tc->is_scratchpad_show) {
				c = tc;
				break;
			}
		}
	}

	/* 2. Otherwise, find a minimized window to restore */
	if (!c) {
		wl_list_for_each(c, &server.clients, link) {
			if (c->isminimized && !c->isnamedscratchpad)
				break;
		}
		if (&c->link == &server.clients)
			c = NULL;
	}

	if (!c)
		return;

	/* Clear scratchpad & minimized state */
	c->is_scratchpad_show = 0;
	c->is_in_scratchpad = 0;
	c->isnamedscratchpad = 0;
	c->isminimized = 0;
	client_pending_minimized_state(c, 0);
	c->iscustomsize = 0;

	/* Restore to the tag where a window currently is focused */
	c->tags =
		(focused && focused != c && focused->tags)
			? focused->tags
			: server.selected_monitor->tagset[server.selected_monitor->seltags];
	c->oldtags = c->tags;
	c->mon = server.selected_monitor;

	client_set_floating(c, 0);
	client_update_border_color(c);
	arrange(c->mon, false, false);
	client_focus(c, 1);
	pointer_warp_to_client(c);
}

void set_layout(const Arg *arg) {
	int32_t jk;
	if (!server.selected_monitor)
		return;

	for (jk = 0; jk < LENGTH(layouts); jk++) {
		if (strcmp(layouts[jk].name, arg->v) == 0) {
			server.selected_monitor->pertag
				->ltidxs[get_mon_curtag(server.selected_monitor)] =
				&layouts[jk];
			clear_fullscreen_and_maximized_state(server.selected_monitor);
			arrange(server.selected_monitor, false, false);
			printstatus(IPC_WATCH_ARRANGGE);
			return;
		}
	}
	return;
}

void set_key_mode(const Arg *arg) {
	snprintf(server.key_mode.mode, sizeof(server.key_mode.mode), "%.27s",
			 arg->v);
	if (strcmp(server.key_mode.mode, "default") == 0) {
		server.key_mode.isdefault = true;
	} else {
		server.key_mode.isdefault = false;
	}
	printstatus(IPC_WATCH_KEYMODE);
	return;
}

void set_proportion(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	if (server.selected_monitor->isoverview ||
		!is_scroller_layout(server.selected_monitor))
		return;

	if (server.selected_monitor->visible_tiling_clients == 1 &&
		!config.scroller_ignore_proportion_single)
		return;

	Client *tc = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!tc)
		return;

	tc = scroll_get_stack_head_client(tc);
	if (!tc)
		return;

	Monitor *m = tc->mon;
	uint32_t tag = get_mon_curtag(m);
	struct TagScrollerState *st = m->pertag->scroller_state[tag];
	struct ScrollerStackNode *node = NULL;

	if (st)
		node = find_scroller_node(st, tc);

	if (node)
		node->scroller_proportion = arg->f;
	tc->scroller_proportion = arg->f;

	uint32_t max_client_width =
		m->w.width - 2 * config.scroller_structs - config.gappih;
	tc->geom.width = max_client_width * arg->f;

	arrange(m, false, false);
	return;
}

void switch_proportion_preset(const Arg *arg) {
	float target_proportion = 0;
	if (!server.selected_monitor)
		return;

	if (config.scroller_proportion_preset_count == 0)
		return;

	if (server.selected_monitor->isoverview ||
		!is_scroller_layout(server.selected_monitor))
		return;

	if (server.selected_monitor->visible_tiling_clients == 1 &&
		!config.scroller_ignore_proportion_single)
		return;

	Client *tc = arg->tc ? arg->tc : server.selected_monitor->sel;

	if (!tc)
		return;

	if (tc->isfloating)
		return; // Do not switch scroller proportions for floating windows

	tc = scroll_get_stack_head_client(tc);
	if (!tc)
		return;

	Monitor *m = tc->mon;
	uint32_t tag = get_mon_curtag(m);
	struct TagScrollerState *st = m->pertag->scroller_state[tag];
	struct ScrollerStackNode *node = NULL;

	if (st)
		node = find_scroller_node(st, tc);

	float current_proportion =
		node ? node->scroller_proportion : tc->scroller_proportion;

	for (int32_t i = 0; i < config.scroller_proportion_preset_count; i++) {
		if (config.scroller_proportion_preset[i] == current_proportion) {
			if (arg->i == NEXT) {
				if (i == config.scroller_proportion_preset_count - 1)
					target_proportion = config.scroller_proportion_preset[0];
				else
					target_proportion =
						config.scroller_proportion_preset[i + 1];
			} else {
				if (i == 0)
					target_proportion =
						config.scroller_proportion_preset
							[config.scroller_proportion_preset_count - 1];
				else
					target_proportion =
						config.scroller_proportion_preset[i - 1];
			}
			break;
		}
	}

	if (target_proportion == 0.0f)
		target_proportion = config.scroller_proportion_preset[0];

	if (node)
		node->scroller_proportion = target_proportion;
	tc->scroller_proportion = target_proportion;

	uint32_t max_client_width =
		m->w.width - 2 * config.scroller_structs - config.gappih;
	tc->geom.width = max_client_width * target_proportion;

	arrange(m, false, false);
	return;
}

void smart_move_window(const Arg *arg) {
	Client *c = NULL, *tc = NULL;
	int32_t nx, ny;
	int32_t buttom, top, left, right, tar;
	if (!server.selected_monitor)
		return;
	c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c || c->isfullscreen || !c->mon)
		return;
	if (!c->isfloating)
		client_set_floating(c, true);
	nx = c->geom.x;
	ny = c->geom.y;

	switch (arg->i) {
	case UP:
		tar = -99999;
		top = c->geom.y;
		ny -= c->mon->w.height / 4;

		wl_list_for_each(tc, &server.clients, link) {
			if (!VISIBLEON(tc, server.selected_monitor) || !tc->isfloating ||
				tc == c)
				continue;
			if (c->geom.x + c->geom.width < tc->geom.x ||
				c->geom.x > tc->geom.x + tc->geom.width)
				continue;
			buttom = tc->geom.y + tc->geom.height + config.gappiv;
			if (top > buttom && ny < buttom) {
				tar = MANGO_MAX(tar, buttom);
			};
		}

		ny = tar == -99999 ? ny : tar;
		ny = MANGO_MAX(ny, c->mon->w.y + c->mon->gappov);
		break;
	case DOWN:
		tar = 99999;
		buttom = c->geom.y + c->geom.height;
		ny += c->mon->w.height / 4;

		wl_list_for_each(tc, &server.clients, link) {
			if (!VISIBLEON(tc, server.selected_monitor) || !tc->isfloating ||
				tc == c)
				continue;
			if (c->geom.x + c->geom.width < tc->geom.x ||
				c->geom.x > tc->geom.x + tc->geom.width)
				continue;
			top = tc->geom.y - config.gappiv;
			if (buttom < top && (ny + c->geom.height) > top) {
				tar = MANGO_MIN(tar, top - c->geom.height);
			};
		}
		ny = tar == 99999 ? ny : tar;
		ny = MANGO_MIN(ny, c->mon->w.y + c->mon->w.height - c->geom.height -
							   c->mon->gappov);
		break;
	case LEFT:
		tar = -99999;
		left = c->geom.x;
		nx -= c->mon->w.width / 6;

		wl_list_for_each(tc, &server.clients, link) {
			if (!VISIBLEON(tc, server.selected_monitor) || !tc->isfloating ||
				tc == c)
				continue;
			if (c->geom.y + c->geom.height < tc->geom.y ||
				c->geom.y > tc->geom.y + tc->geom.height)
				continue;
			right = tc->geom.x + tc->geom.width + config.gappih;
			if (left > right && nx < right) {
				tar = MANGO_MAX(tar, right);
			};
		}

		nx = tar == -99999 ? nx : tar;
		nx = MANGO_MAX(nx, c->mon->w.x + c->mon->gappoh);
		break;
	case RIGHT:
		tar = 99999;
		right = c->geom.x + c->geom.width;
		nx += c->mon->w.width / 6;
		wl_list_for_each(tc, &server.clients, link) {
			if (!VISIBLEON(tc, server.selected_monitor) || !tc->isfloating ||
				tc == c)
				continue;
			if (c->geom.y + c->geom.height < tc->geom.y ||
				c->geom.y > tc->geom.y + tc->geom.height)
				continue;
			left = tc->geom.x - config.gappih;
			if (right < left && (nx + c->geom.width) > left) {
				tar = MANGO_MIN(tar, left - c->geom.width);
			};
		}
		nx = tar == 99999 ? nx : tar;
		nx = MANGO_MIN(nx, c->mon->w.x + c->mon->w.width - c->geom.width -
							   c->mon->gappoh);
		break;
	}

	c->float_geom = (struct wlr_box){
		.x = nx, .y = ny, .width = c->geom.width, .height = c->geom.height};
	c->iscustomsize = 1;
	resize(c, c->float_geom, 1);
	return;
}

void smart_resize_window(const Arg *arg) {
	Client *c = NULL, *tc = NULL;
	int32_t nw, nh;
	int32_t buttom, top, left, right, tar;
	if (!server.selected_monitor)
		return;
	c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c || c->isfullscreen)
		return;
	if (!c->isfloating)
		client_set_floating(c, true);
	nw = c->geom.width;
	nh = c->geom.height;

	switch (arg->i) {
	case UP:
		nh -= server.selected_monitor->w.height / 8;
		nh = MANGO_MAX(nh, server.selected_monitor->w.height / 10);
		break;
	case DOWN:
		tar = -99999;
		buttom = c->geom.y + c->geom.height;
		nh += server.selected_monitor->w.height / 8;

		wl_list_for_each(tc, &server.clients, link) {
			if (!VISIBLEON(tc, server.selected_monitor) || !tc->isfloating ||
				tc == c)
				continue;
			if (c->geom.x + c->geom.width < tc->geom.x ||
				c->geom.x > tc->geom.x + tc->geom.width)
				continue;
			top = tc->geom.y - config.gappiv;
			if (buttom < top && (nh + c->geom.y) > top) {
				tar = MANGO_MAX(tar, top - c->geom.y);
			};
		}
		nh = tar == -99999 ? nh : tar;
		if (c->geom.y + nh + config.gappov >
			server.selected_monitor->w.y + server.selected_monitor->w.height)
			nh = server.selected_monitor->w.y +
				 server.selected_monitor->w.height - c->geom.y - config.gappov;
		break;
	case LEFT:
		nw -= server.selected_monitor->w.width / 16;
		nw = MANGO_MAX(nw, server.selected_monitor->w.width / 10);
		break;
	case RIGHT:
		tar = 99999;
		right = c->geom.x + c->geom.width;
		nw += server.selected_monitor->w.width / 16;
		wl_list_for_each(tc, &server.clients, link) {
			if (!VISIBLEON(tc, server.selected_monitor) || !tc->isfloating ||
				tc == c)
				continue;
			if (c->geom.y + c->geom.height < tc->geom.y ||
				c->geom.y > tc->geom.y + tc->geom.height)
				continue;
			left = tc->geom.x - config.gappih;
			if (right < left && (nw + c->geom.x) > left) {
				tar = MANGO_MIN(tar, left - c->geom.x);
			};
		}

		nw = tar == 99999 ? nw : tar;
		if (c->geom.x + nw + config.gappoh >
			server.selected_monitor->w.x + server.selected_monitor->w.width)
			nw = server.selected_monitor->w.x +
				 server.selected_monitor->w.width - c->geom.x - config.gappoh;
		break;
	}

	c->float_geom = (struct wlr_box){
		.x = c->geom.x, .y = c->geom.y, .width = nw, .height = nh};
	c->iscustomsize = 1;
	resize(c, c->float_geom, 1);
	return;
}

void center_window(const Arg *arg) {
	Client *c =
		arg->tc
			? arg->tc
			: (server.selected_monitor ? server.selected_monitor->sel : NULL);

	if (!c || c->isfullscreen || c->ismaximizescreen)
		return;

	if (c->isfloating) {
		c->float_geom = client_center_geometry(c, c->mon, c->geom, 0, 0);
		c->iscustomsize = 1;
		resize(c, c->float_geom, 1);
		return;
	}

	if (!is_scroller_layout(server.selected_monitor))
		return;

	Client *stack_head = scroll_get_stack_head_client(c);
	if (server.selected_monitor->pertag
			->ltidxs[get_mon_curtag(server.selected_monitor)]
			->id == SCROLLER) {
		stack_head->geom.x =
			server.selected_monitor->w.x +
			(server.selected_monitor->w.width - stack_head->geom.width) / 2;
	} else {
		stack_head->geom.y =
			server.selected_monitor->w.y +
			(server.selected_monitor->w.height - stack_head->geom.height) / 2;
	}

	arrange(server.selected_monitor, false, false);
	return;
}

static void close_inherited_fds(void) {
#ifdef SYS_close_range
	extern long syscall(long number, ...);
	if (syscall(SYS_close_range, 3, ~0U, 0) == 0)
		return;
#endif
	int fd_max = sysconf(_SC_OPEN_MAX);
	for (int i = 3; i < fd_max; i++) {
		close(i);
	}
}

void spawn_shell(const Arg *arg) {
	if (!arg->v)
		return;

	// hand the child an activation token so it can request activation
	const char *activation_token = xdg_activation_v1_export_token();

	if (fork() == 0) {
		if (activation_token)
			setenv("XDG_ACTIVATION_TOKEN", activation_token, 1);

		close_inherited_fds();

		dup2(STDERR_FILENO, STDOUT_FILENO);
		setsid();

		execlp("sh", "sh", "-c", arg->v, (char *)NULL);
		execlp("bash", "bash", "-c", arg->v, (char *)NULL);

		mango_error(true, WLR_DEBUG,
					"mango: failed to execute command '%s' with shell: %s\n",
					(char *)arg->v, strerror(errno));
		_exit(EXIT_FAILURE);
	}
	return;
}

void spawn(const Arg *arg) {
	if (!arg->v)
		return;

	// hand the child an activation token so it can request activation
	const char *activation_token = xdg_activation_v1_export_token();

	if (fork() == 0) {
		if (activation_token)
			setenv("XDG_ACTIVATION_TOKEN", activation_token, 1);

		close_inherited_fds();

		dup2(STDERR_FILENO, STDOUT_FILENO);
		setsid();

		wordexp_t p;
		if (wordexp(arg->v, &p, 0) != 0) {
			mango_error(true, WLR_DEBUG, "mango: wordexp failed for '%s'\n",
						(char *)arg->v);
			_exit(EXIT_FAILURE);
		}

		execvp(p.we_wordv[0], p.we_wordv);

		mango_error(true, WLR_DEBUG, "mango: execvp '%s' failed: %s\n",
					p.we_wordv[0], strerror(errno));
		wordfree(&p);
		_exit(EXIT_FAILURE);
	}
	return;
}

void spawn_on_empty(const Arg *arg) {
	bool is_empty = true;
	Client *c = NULL;

	wl_list_for_each(c, &server.clients, link) {
		if (arg->ui & c->tags && c->mon == server.selected_monitor) {
			is_empty = false;
			break;
		}
	}
	if (!is_empty) {
		client_switch_view(arg, true);
		return;
	} else {
		client_switch_view(arg, true);
		spawn_shell(arg);
	}
	return;
}

void switch_keyboard_layout(const Arg *arg) {
	if (!server.keyboard_group || !server.keyboard_group->wlr_group ||
		!server.seat) {
		mango_error(true, WLR_ERROR, "Invalid keyboard group or seat");
		return;
	}

	struct wlr_keyboard *keyboard = &server.keyboard_group->wlr_group->keyboard;
	if (!keyboard || !keyboard->keymap) {
		mango_error(true, WLR_ERROR, "Invalid keyboard or keymap");
		return;
	}

	xkb_layout_index_t current = xkb_state_serialize_layout(
		keyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	const int32_t num_layouts = xkb_keymap_num_layouts(keyboard->keymap);
	if (num_layouts < 2) {
		mango_error(true, WLR_INFO, "Only one layout available");
		return;
	}

	xkb_layout_index_t next = 0;
	if (arg->i > 0 && arg->i <= num_layouts) {
		next = arg->i - 1;
	} else {
		next = (current + 1) % num_layouts;
	}

	uint32_t depressed = keyboard->modifiers.depressed;
	uint32_t latched = keyboard->modifiers.latched;
	uint32_t locked_mods = keyboard->modifiers.locked;

	wlr_keyboard_notify_modifiers(keyboard, depressed, latched, locked_mods,
								  next);

	wlr_seat_set_keyboard(server.seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(server.seat, &keyboard->modifiers);

	InputDevice *id;
	wl_list_for_each(id, &server.input_devices, link) {
		if (id->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD ||
			id->standalone) {
			/* Standalone keyboards keep their own keymap/layout and are not
			 * synced on layout switches. */
			continue;
		}

		struct wlr_keyboard *tkb = (struct wlr_keyboard *)id->device_data;

		wlr_keyboard_notify_modifiers(tkb, depressed, latched, locked_mods,
									  next);
		wlr_seat_set_keyboard(server.seat, tkb);
		wlr_seat_keyboard_notify_modifiers(server.seat, &tkb->modifiers);
	}

	printstatus(IPC_WATCH_KB_LAYOUT);
	return;
}

void switch_layout(const Arg *arg) {

	int32_t jk, ji;
	char *target_layout_name = NULL;
	uint32_t len;

	if (!server.selected_monitor)
		return;

	uint32_t tag = get_mon_curtag(server.selected_monitor);

	if (config.circle_layout_count != 0) {
		for (jk = 0; jk < config.circle_layout_count; jk++) {

			len = MANGO_MAX(
				strlen(config.circle_layout[jk]),
				strlen(server.selected_monitor->pertag->ltidxs[tag]->name));

			if (strncmp(config.circle_layout[jk],
						server.selected_monitor->pertag->ltidxs[tag]->name,
						len) == 0) {
				target_layout_name = jk == config.circle_layout_count - 1
										 ? config.circle_layout[0]
										 : config.circle_layout[jk + 1];
				break;
			}
		}

		if (!target_layout_name) {
			target_layout_name = config.circle_layout[0];
		}

		for (ji = 0; ji < LENGTH(layouts); ji++) {
			len =
				MANGO_MAX(strlen(layouts[ji].name), strlen(target_layout_name));
			if (strncmp(layouts[ji].name, target_layout_name, len) == 0) {
				server.selected_monitor->pertag->ltidxs[tag] = &layouts[ji];

				break;
			}
		}
		clear_fullscreen_and_maximized_state(server.selected_monitor);
		arrange(server.selected_monitor, false, false);
		printstatus(IPC_WATCH_ARRANGGE);
		return;
	}

	for (jk = 0; jk < LENGTH(layouts); jk++) {
		if (strcmp(layouts[jk].name,
				   server.selected_monitor->pertag->ltidxs[tag]->name) == 0) {
			server.selected_monitor->pertag->ltidxs[tag] =
				jk == LENGTH(layouts) - 1 ? &layouts[0] : &layouts[jk + 1];
			clear_fullscreen_and_maximized_state(server.selected_monitor);
			arrange(server.selected_monitor, false, false);
			printstatus(IPC_WATCH_ARRANGGE);
			return;
		}
	}
	return;
}

void tag(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	Client *target_client = arg->tc ? arg->tc : server.selected_monitor->sel;
	tag_client(arg, target_client);
	return;
}

void tag_monitor(const Arg *arg) {
	Monitor *m = NULL, *cm = NULL, *oldmon = NULL;
	if (!server.selected_monitor)
		return;
	Client *c = arg->tc ? arg->tc : client_focus_top(server.selected_monitor);

	if (!c)
		return;

	oldmon = c->mon;

	if (arg->i != UNDIR) {
		m = monitor_from_direction(arg->i);
	} else if (arg->v) {
		wl_list_for_each(cm, &server.monitors, link) {
			if (!cm->wlr_output->enabled) {
				continue;
			}
			if (match_monitor_spec(arg->v, cm)) {
				m = cm;
				break;
			}
		}
	} else {
		return;
	}

	if (!m || !m->wlr_output->enabled)
		return;

	uint32_t newtags = arg->ui ? arg->ui : arg->i2 ? c->tags : 0;
	uint32_t target;

	if (c->mon == m) {
		client_switch_view(&(Arg){.ui = newtags}, true);
		return;
	}

	if (c == oldmon->sel) {
		oldmon->sel = NULL;
	}

	client_set_monitor(c, m, newtags, true);
	client_update_oldmonname_record(c, m);

	reset_foreign_tolevel(c, oldmon, c->mon);

	c->float_geom.width =
		(int32_t)(c->float_geom.width * c->mon->w.width / oldmon->w.width);
	c->float_geom.height =
		(int32_t)(c->float_geom.height * c->mon->w.height / oldmon->w.height);
	server.selected_monitor = c->mon;
	c->float_geom = client_center_geometry(c, c->mon, c->float_geom, 0, 0);

	if (c->isfloating) {
		c->geom = c->float_geom;
		target = get_tags_first_tag(c->tags);
		client_switch_view(&(Arg){.ui = target}, true);
		client_focus(c, 1);
		resize(c, c->geom, 1);
	} else {
		server.selected_monitor = c->mon;
		target = get_tags_first_tag(c->tags);
		client_switch_view(&(Arg){.ui = target}, true);
		client_focus(c, 1);
		arrange(server.selected_monitor, false, false);
	}
	if (config.warpcursor) {
		pointer_warp_to_monitor(c->mon);
	}
	return;
}

void tag_silent(const Arg *arg) {
	Client *fc = NULL;
	Client *target_client =
		arg->tc
			? arg->tc
			: (server.selected_monitor ? server.selected_monitor->sel : NULL);

	if (!target_client)
		return;

	target_client->tags =
		(arg->ui & TAG0_MASK) ? TAG0_MASK : (arg->ui & TAGMASK);
	client_reparent_group(target_client);
	wl_list_for_each(fc, &server.clients, link) {
		if (fc && fc != target_client && target_client->tags & fc->tags &&
			ISFULLSCREEN(fc) && !target_client->isfloating) {
			clear_fullscreen_flag(fc);
		}
	}
	client_focus(client_focus_top(server.selected_monitor), 1);
	arrange(target_client->mon, false, false);
	return;
}

void tag_to_left(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *sel = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (sel != NULL &&
		__builtin_popcount(
			server.selected_monitor->tagset[server.selected_monitor->seltags] &
			TAGMASK) == 1) {
		uint32_t target =
			server.selected_monitor->tagset[server.selected_monitor->seltags] >>
			1;

		if (target == 0) {
			if (!config.tag_carousel)
				return;
			target = (1 << (config.tag_num - 1)) & TAGMASK;
			server.selected_monitor->carousel_anim_dir = -1;
		}

		Arg a = {.ui = target & TAGMASK, .i = arg->i, .tc = sel};
		tag(&a);
		server.selected_monitor->carousel_anim_dir = 0;
	}
	return;
}

void tag_to_right(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *sel = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (sel != NULL &&
		__builtin_popcount(
			server.selected_monitor->tagset[server.selected_monitor->seltags] &
			TAGMASK) == 1) {
		uint32_t target =
			server.selected_monitor->tagset[server.selected_monitor->seltags]
			<< 1;

		if (!(target & TAGMASK)) {
			if (!config.tag_carousel)
				return;
			target = 1;
			server.selected_monitor->carousel_anim_dir = 1;
		}

		Arg a = {.ui = target & TAGMASK, .i = arg->i, .tc = sel};
		tag(&a);
		server.selected_monitor->carousel_anim_dir = 0;
	}
	return;
}

void toggle_named_scratchpad(const Arg *arg) {
	Client *target_client = NULL;
	char *arg_id = arg->v;
	char *arg_title = arg->v2;

	if (server.selected_monitor && server.selected_monitor->isoverview)
		return;

	target_client = get_client_by_id_or_title(arg_id, arg_title);

	if (!target_client && arg->v3) {
		Arg arg_spawn = {.v = arg->v3};
		spawn_shell(&arg_spawn);
		return;
	}

	target_client->isnamedscratchpad = 1;
	apply_named_scratchpad(target_client);
	return;
}

void toggle_render_border(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	server.render_border = !server.render_border;
	arrange(server.selected_monitor, false, false);
	return;
}

void toggle_scratchpad(const Arg *arg) {
	Client *c = NULL;
	bool hit = false;
	Client *tmp = NULL;

	if (server.selected_monitor && server.selected_monitor->isoverview)
		return;

	wl_list_for_each_safe(c, tmp, &server.clients, link) {
		if (!config.scratchpad_cross_monitor &&
			c->mon != server.selected_monitor) {
			continue;
		}

		if (config.single_scratchpad && c->isnamedscratchpad &&
			!c->isminimized) {
			set_minimized(c);
			continue;
		}

		if (c->isnamedscratchpad)
			continue;

		if (hit)
			continue;

		hit = switch_scratchpad_client_state(c);
	}
	return;
}

// toggle the special workspace view on a given monitor
void toggle_special_tag_mon(Monitor *m) {
	if (!m || m->isoverview)
		return;

	server.selected_monitor = m;
	if (is_special_active(m)) {
		/* Toggle back to previous tagset (supports multi-tag views) */
		uint32_t prev_set = m->tagset[m->seltags ^ 1] & TAGMASK;
		uint32_t target =
			prev_set
				? prev_set
				: (m->pertag->prevtag ? (1 << (m->pertag->prevtag - 1)) : 1);
		client_switch_view(&(Arg){.ui = target}, true);
	} else {
		/* Switch to tag 0 */
		client_switch_view(&(Arg){.ui = TAG0_MASK}, true);
	}
}

void toggle_special_tag(const Arg *arg) {
	Monitor *m = (arg && arg->tc && arg->tc->mon) ? arg->tc->mon
												  : server.selected_monitor;
	toggle_special_tag_mon(m);
}

static void tag_special_tag_internal(const Arg *arg, bool silent) {
	if (!server.selected_monitor)
		return;
	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c)
		return;

	Monitor *m = c->mon ? c->mon : server.selected_monitor;
	if (c->tags & TAG0_MASK) {
		/* Return window from tag 0 to normal tag */
		uint32_t target =
			(m->pertag->prevtag > 0)
				? (1 << (m->pertag->prevtag - 1))
				: ((m->pertag->curtag > 0) ? (1 << (m->pertag->curtag - 1))
										   : 1);
		if (silent)
			tag_silent(&(Arg){.ui = target, .tc = c});
		else
			tag(&(Arg){.ui = target, .tc = c});

	} else {
		/* Send window to tag 0 */
		if (silent)
			tag_silent(&(Arg){.ui = TAG0_MASK, .tc = c});
		else {
			tag(&(Arg){.ui = TAG0_MASK, .tc = c});
			/* Switch to tag 0 so user sees the window */
			if (!is_special_active(m)) {
				toggle_special_tag(&(Arg){0});
			}
		}
	}
}

void tag_special_tag(const Arg *arg) { tag_special_tag_internal(arg, false); }

void tag_special_silent(const Arg *arg) { tag_special_tag_internal(arg, true); }

void toggle_fake_fullscreen(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	Client *sel = arg->tc ? arg->tc : client_focus_top(server.selected_monitor);
	if (sel)
		client_set_fake_fullscreen(sel, !sel->isfakefullscreen);
	return;
}

void toggle_floating(const Arg *arg) {
	if (!server.selected_monitor || server.grab_client)
		return;

	Client *sel = arg->tc ? arg->tc : client_focus_top(server.selected_monitor);

	if (server.selected_monitor && server.selected_monitor->isoverview)
		return;

	if (!sel)
		return;

	bool isfloating = sel->isfloating;

	if ((sel->isfullscreen || sel->ismaximizescreen)) {
		isfloating = 1;
	} else {
		isfloating = !sel->isfloating;
	}

	client_set_floating(sel, isfloating);
	return;
}

void toggle_fullscreen(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *sel = arg->tc ? arg->tc : client_focus_top(server.selected_monitor);
	if (!sel)
		return;

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
	sel->isnamedscratchpad = 0;

	if (sel->isfullscreen)
		client_apply_fullscreen(sel, 0, true);
	else
		client_apply_fullscreen(sel, 1, true);
	return;
}

void toggle_global(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c)
		return;

	if (c->is_in_scratchpad) {
		c->is_in_scratchpad = 0;
		c->is_scratchpad_show = 0;
		c->isnamedscratchpad = 0;
	}
	c->isglobal ^= 1;
	client_update_border_color(c);
	return;
}

void toggle_gaps(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	server.enable_gaps ^= 1;
	arrange(server.selected_monitor, false, false);
	return;
}

void toggle_maximize_screen(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *sel = arg->tc ? arg->tc : client_focus_top(server.selected_monitor);
	if (!sel)
		return;

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
	sel->isnamedscratchpad = 0;

	if (sel->ismaximizescreen)
		client_set_maximize_screen(sel, 0, true);
	else
		client_set_maximize_screen(sel, 1, true);

	client_update_border_color(sel);
	return;
}

void toggle_overlay(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c || !c->mon || c->isfullscreen) {
		return;
	}

	c->isoverlay ^= 1;

	client_reparent_group(c);
	client_update_border_color(c);
	return;
}

void toggle_tag(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	uint32_t newtags;
	Client *sel = arg->tc ? arg->tc : client_focus_top(server.selected_monitor);
	if (!sel)
		return;
	// special workspace windows only belong to tag0; use tag_special_tag to
	// move them back to a normal tag
	if (sel->tags & TAG0_MASK)
		return;

	if ((int32_t)arg->ui == INT_MIN && sel->tags != (~0 & TAGMASK)) {
		newtags = ~0 & TAGMASK;
	} else if ((int32_t)arg->ui == INT_MIN && sel->tags == (~0 & TAGMASK)) {
		uint32_t tag = sel->mon->pertag->curtag;
		if (!tag)
			tag = get_tags_first_tag_num(sel->mon->tagset[sel->mon->seltags]);
		if (!tag)
			tag = 1;
		newtags = 1u << (tag - 1);
	} else {
		newtags = sel->tags ^ (arg->ui & TAGMASK);
	}

	if (newtags) {
		sel->tags = newtags;
		client_reparent_group(sel);
		client_focus(client_focus_top(server.selected_monitor), 1);
		arrange(server.selected_monitor, false, false);
	}
	printstatus(IPC_WATCH_ARRANGGE);
	return;
}

void toggle_view(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	uint32_t newtagset;
	uint32_t target;
	Client *c = NULL;

	target = arg->ui == 0 ? ~0 & TAGMASK : arg->ui;

	newtagset =
		server.selected_monitor->tagset[server.selected_monitor->seltags] ^
		(target & TAGMASK);

	if (newtagset) {
		server.selected_monitor->tagset[server.selected_monitor->seltags] =
			newtagset;
		client_focus(client_focus_top(server.selected_monitor), 1);
		wl_list_for_each(c, &server.clients, link) {
			if (VISIBLEON(c, server.selected_monitor) && ISTILED(c)) {
				set_size_per(server.selected_monitor, c);
			}
		}
		arrange(server.selected_monitor, false, false);
	}
	printstatus(IPC_WATCH_ARRANGGE);
	return;
}

bool view_shift_tag(const Arg *arg, int dir) {
	if (!server.selected_monitor)
		return false;

	if (server.selected_monitor->isoverview ||
		server.selected_monitor->pertag->curtag == 0)
		return false;

	uint32_t target =
		server.selected_monitor->tagset[server.selected_monitor->seltags];
	if (dir < 0) {
		target >>= 1;

		if (target == 0) {
			if (!config.tag_carousel)
				return false;
			target = (1 << (config.tag_num - 1)) & TAGMASK;
			server.selected_monitor->carousel_anim_dir = -1;
		}
	} else {
		target <<= 1;

		if (!(target & TAGMASK)) {
			if (!config.tag_carousel)
				return false;
			target = 1;
			server.selected_monitor->carousel_anim_dir = 1;
		}
	}

	if (target ==
		server.selected_monitor->tagset[server.selected_monitor->seltags])
		return false;

	client_switch_view(&(Arg){.ui = target & TAGMASK, .i = arg->i}, true);
	server.selected_monitor->carousel_anim_dir = 0;
	return true;
}

bool view_shift_tag_have_client(const Arg *arg, int dir) {
	if (!server.selected_monitor)
		return false;

	if (server.selected_monitor->isoverview ||
		is_special_active(server.selected_monitor))
		return false;

	uint32_t n;
	uint32_t current = get_tags_first_tag_num(
		server.selected_monitor->tagset[server.selected_monitor->seltags]);
	bool found = false;
	bool wrapped = false;

	if (dir < 0) {
		for (n = current - 1; n >= 1; n--) {
			if (get_tag_status(n, server.selected_monitor)) {
				found = true;
				break;
			}
		}

		if (!found && config.tag_carousel) {
			for (n = (uint32_t)config.tag_num; n > current; n--) {
				if (get_tag_status(n, server.selected_monitor)) {
					found = true;
					wrapped = true;
					break;
				}
			}
		}
	} else {
		for (n = current + 1; n <= (uint32_t)config.tag_num; n++) {
			if (get_tag_status(n, server.selected_monitor)) {
				found = true;
				break;
			}
		}

		if (!found && config.tag_carousel) {
			for (n = 1; n < current; n++) {
				if (get_tag_status(n, server.selected_monitor)) {
					found = true;
					wrapped = true;
					break;
				}
			}
		}
	}

	if (found) {
		if (wrapped)
			server.selected_monitor->carousel_anim_dir = (dir < 0) ? -1 : 1;
		client_switch_view(&(Arg){.ui = (1 << (n - 1)) & TAGMASK, .i = arg->i},
						   true);
		server.selected_monitor->carousel_anim_dir = 0;
		return true;
	}
	return false;
}

void view_to_left(const Arg *arg) { view_shift_tag(arg, -1); }

void view_to_right(const Arg *arg) { view_shift_tag(arg, 1); }

void view_insert(const Arg *arg) {
	uint32_t cur, curmask, target;

	if (!server.selected_monitor || server.selected_monitor->isoverview)
		return;

	curmask =
		server.selected_monitor->tagset[server.selected_monitor->seltags] &
		TAGMASK;
	if (!curmask || (curmask & (curmask - 1)))
		return;
	cur = get_tags_first_tag_num(curmask);
	if (!cur)
		return;

	if (arg->i == NEXT) {
		if (cur >= (uint32_t)config.tag_num)
			return;
		target = cur + 1;
	} else if (cur == 1) {
		target = cur;
	} else if (get_tag_status(cur - 1, server.selected_monitor) == 0) {
		target = cur - 1;
	} else {
		target = cur;
	}

	if (get_tag_status(target, server.selected_monitor) == 0) {
		client_switch_view(&(Arg){.ui = (1u << (target - 1)) & TAGMASK}, true);
		return;
	}

	if (target >= (uint32_t)config.tag_num ||
		get_tag_status((uint32_t)config.tag_num, server.selected_monitor))
		return;

	view_insert_shift_tags(server.selected_monitor, target);
	int32_t tag_gather_bak = config.tag_gather;
	config.tag_gather = 0;
	client_switch_view(&(Arg){.ui = (1u << (target - 1)) & TAGMASK}, true);
	config.tag_gather = tag_gather_bak;
}

void view_to_left_have_client(const Arg *arg) {
	view_shift_tag_have_client(arg, -1);
}

void view_to_right_have_client(const Arg *arg) {
	view_shift_tag_have_client(arg, 1);
}

void view_cross_monitor(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	focus_monitor(&(Arg){.v = arg->v, .i = UNDIR});
	client_view_on_monitor(arg, true, server.selected_monitor, true);
	return;
}

void tag_cross_monitor(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c)
		return;

	if (match_monitor_spec(arg->v, server.selected_monitor)) {
		tag_client(arg, c);
		return;
	}

	Arg a = {.ui = arg->ui, .i = UNDIR, .v = arg->v, .tc = c};
	tag_monitor(&a);
	return;
}

void combo_view(const Arg *arg) {
	uint32_t newtags = arg->ui & TAGMASK;

	if (!newtags || !server.selected_monitor)
		return;

	if (server.tag_combo) {
		server.selected_monitor->tagset[server.selected_monitor->seltags] |=
			newtags;
		client_focus(client_focus_top(server.selected_monitor), 1);
		arrange(server.selected_monitor, false, false);
	} else {
		server.tag_combo = true;
		client_switch_view(&(Arg){.ui = newtags}, true);
	}

	printstatus(IPC_WATCH_ARRANGGE);
	return;
}

void zoom(const Arg *arg) {
	Client *c = NULL,
		   *sel = arg->tc ? arg->tc : client_focus_top(server.selected_monitor);

	if (!sel || !server.selected_monitor ||
		!server.selected_monitor->pertag
			 ->ltidxs[get_mon_curtag(server.selected_monitor)]
			 ->arrange ||
		sel->isfloating)
		return;

	wl_list_for_each(c, &server.clients,
					 link) if (VISIBLEON(c, server.selected_monitor) &&
							   !c->isfloating) {
		if (c != sel)
			break;
		sel = NULL;
	}

	if (&c->link == &server.clients)
		return;

	if (!sel)
		sel = c;
	wl_list_remove(&sel->link);
	wl_list_insert(&server.clients, &sel->link);

	client_focus(sel, 1);
	arrange(server.selected_monitor, false, false);
	return;
}

void setoption(const Arg *arg) {
	parse_option(&config, arg->v, arg->v2, 0);
	override_config();
	reset_option();
	return;
}

void minimize_window(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	if (server.selected_monitor && server.selected_monitor->isoverview)
		return;

	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (c && !c->isminimized) {
		set_minimized(c);
	}
	return;
}

void fix_mon_tagset_from_overview(Monitor *m) {
	if (m->tagset[m->seltags] == (m->ovbk_prev_tagset & TAGMASK)) {
		m->tagset[m->seltags ^ 1] = m->ovbk_current_tagset;
		m->pertag->prevtag = get_tags_first_tag_num(m->ovbk_current_tagset);
	} else {
		m->tagset[m->seltags ^ 1] = m->ovbk_prev_tagset;
		m->pertag->prevtag = get_tags_first_tag_num(m->ovbk_prev_tagset);
	}
}

void toggle_overview(const Arg *arg) {
	Client *c = NULL;
	if (!server.selected_monitor || server.grab_client)
		return;

	Client *sel = arg->tc ? arg->tc : server.selected_monitor->sel;

	server.selected_monitor->isoverview ^= 1;
	uint32_t target = 0;
	uint32_t visible_client_number = 0;

	if (!server.selected_monitor->isoverview) {
		server.selected_monitor->ov_tab_layout = 0;
		if (server.selected_monitor->is_jump_mode)
			finish_jump_mode(server.selected_monitor);
	}

	if (server.selected_monitor->isoverview) {
		wl_list_for_each(c, &server.clients,
						 link) if (c && c->mon == server.selected_monitor &&
								   !client_is_unmanaged(c) &&
								   !client_is_x11_popup(c) && !c->isminimized &&
								   !c->isunglobal && !(c->tags & TAG0_MASK)) {
			visible_client_number++;
		}
		if (visible_client_number > 0) {
			server.selected_monitor->ovbk_current_tagset =
				server.selected_monitor
					->tagset[server.selected_monitor->seltags];
			server.selected_monitor->ovbk_prev_tagset =
				server.selected_monitor
					->tagset[server.selected_monitor->seltags ^ 1];
			target = ~0 & TAGMASK;
		} else {
			server.selected_monitor->isoverview ^= 1;
			server.selected_monitor->ov_tab_layout = 0;
			return;
		}
	} else if (!server.selected_monitor->isoverview && sel &&
			   (sel->tags & TAGMASK) != 0) {
		target = get_tags_first_tag(sel->tags);
	} else {
		target =
			server.selected_monitor->ovbk_current_tagset
				? (server.selected_monitor->ovbk_current_tagset & TAGMASK)
				: (server.selected_monitor->pertag->prevtag
					   ? (1 << (server.selected_monitor->pertag->prevtag - 1))
					   : 1);
		if (!target)
			target = 1;
	}

	if (server.selected_monitor->isoverview) {
		wlr_seat_pointer_clear_focus(server.seat);

		if (server.cursor_hidden) {
			pointer_cursor_activity();
		} else {
			wlr_cursor_set_xcursor(server.cursor, server.cursor_manager,
								   "default");
		}

		wl_list_for_each(c, &server.clients, link) {
			if (c && c->mon == server.selected_monitor &&
				!client_is_unmanaged(c) && !client_is_x11_popup(c) &&
				!c->isunglobal && !c->isminimized && !(c->tags & TAG0_MASK) &&
				client_surface(c)->mapped) {
				c->animation.overining = true;
				if (!server.selected_monitor->is_jump_mode &&
					!server.selected_monitor->ov_normal_mode)
					/* Tab layout: skip view arrangement first; set it when the
					 * unified rearrange runs after entering. */
					c->animation.overview_enter_anim_set = true;
				else
					/* Other modes: set zoom during view arrangement. */
					c->animation.overview_enter_anim_set = false;
				overview_backup(c);
			}
		}
	} else {
		server.selected_monitor->ov_normal_mode =
			0; /* Clears hot-area normal mode when exiting overview. */

		server.selected_monitor->tagset[server.selected_monitor->seltags] =
			target;
		wl_list_for_each(c, &server.clients, link) {
			if (c && c->mon == server.selected_monitor && !c->iskilling &&
				!client_is_unmanaged(c) && !c->isunglobal && !c->isminimized &&
				!client_is_x11_popup(c) && client_surface(c)->mapped &&
				!(c->tags & TAG0_MASK)) {
				overview_restore(c, &(Arg){.ui = target});
			}
		}
	}

	client_switch_view(&(Arg){.ui = target}, false);

	/* Tab layout: rearrange after entering. */
	if (server.selected_monitor->isoverview &&
		!server.selected_monitor->is_jump_mode &&
		!server.selected_monitor->ov_normal_mode) {

		Client *cc = NULL;
		wl_list_for_each(cc, &server.clients, link) {
			if (cc && cc->mon == server.selected_monitor &&
				!client_is_unmanaged(cc) && !client_is_x11_popup(cc) &&
				!(cc->tags & TAG0_MASK))
				cc->animation.overview_enter_anim_set = false;
		}
		arrange(server.selected_monitor, true, false);
	}

	fix_mon_tagset_from_overview(server.selected_monitor);
	refresh_monitors_workspaces_status(server.selected_monitor);

	if (!server.selected_monitor->isoverview && sel && (sel->tags & target)) {
		client_focus(sel, 1);
	}

	return;
}

void toggle_jump(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	if (!server.selected_monitor->isoverview) {
		begin_jump_mode(server.selected_monitor);
		toggle_overview(arg);
		return;
	}

	if (server.selected_monitor->isoverview) {
		toggle_overview(arg);
	}

	return;
}

void disable_monitor(const Arg *arg) {
	Monitor *m = NULL;

	wl_list_for_each(m, &server.monitors, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, false);
			mango_output_commit(m);
			m->only_sleep = 0;
			handle_output_layout_change(NULL, NULL);
			break;
		}
	}
	return;
}

void enable_monitor(const Arg *arg) {
	Monitor *m = NULL;
	wl_list_for_each(m, &server.monitors, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, true);
			mango_output_commit(m);
			m->only_sleep = 0;
			handle_output_layout_change(NULL, NULL);
			break;
		}
	}
	return;
}

void toggle_monitor(const Arg *arg) {
	Monitor *m = NULL;
	wl_list_for_each(m, &server.monitors, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, !m->wlr_output->enabled);
			mango_output_commit(m);
			m->only_sleep = 0;
			handle_output_layout_change(NULL, NULL);
			break;
		}
	}
	return;
}

void sleep_monitor(const Arg *arg) {
	Monitor *m = NULL;

	wl_list_for_each(m, &server.monitors, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, false);
			mango_output_commit(m);
			m->only_sleep = 1;
			handle_output_layout_change(NULL, NULL);
			break;
		}
	}
	return;
}

void wakeup_monitor(const Arg *arg) {
	Monitor *m = NULL;
	wl_list_for_each(m, &server.monitors, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, true);
			mango_output_commit(m);
			m->only_sleep = 0;
			handle_output_layout_change(NULL, NULL);
			break;
		}
	}
	return;
}

void sleep_toggle_monitor(const Arg *arg) {
	Monitor *m = NULL;
	wl_list_for_each(m, &server.monitors, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, !m->wlr_output->enabled);
			mango_output_commit(m);
			m->only_sleep = !m->wlr_output->enabled;
			handle_output_layout_change(NULL, NULL);
			break;
		}
	}
	return;
}

void scroller_apply_stack(Client *c, Client *target_client, int32_t direction) {
	if (!c || !c->mon || c->isfloating || !is_scroller_layout(c->mon))
		return;

	Monitor *m = c->mon;
	uint32_t tag = get_client_tag_idx(c);

	bool is_horizontal = (m->pertag->ltidxs[tag]->id == SCROLLER);

	if (is_horizontal && (direction == UP || direction == DOWN))
		return;
	if (!is_horizontal && (direction == LEFT || direction == RIGHT))
		return;

	struct TagScrollerState *st = ensure_scroller_state(m, tag);

	struct ScrollerStackNode *cnode = find_scroller_node(st, c);

	if (!cnode)
		return;

	struct ScrollerStackNode *tnode =
		target_client ? find_scroller_node(st, target_client) : NULL;

	if (direction == UNDIR && target_client && target_client->mon == c->mon) {
		scroller_insert_stack(c, target_client, false);
		return;
	}

	if (cnode->prev_in_stack || cnode->next_in_stack) {
		struct ScrollerStackNode *move_out_refer_node =
			cnode->prev_in_stack ? cnode->prev_in_stack : cnode->next_in_stack;
		scroller_node_remove(st, cnode);

		update_scroller_state(c->mon);

		Client *stack_head =
			scroll_get_stack_head_client(move_out_refer_node->client);
		Client *stack_tail =
			scroll_get_stack_tail_client(move_out_refer_node->client);

		if (direction == LEFT || direction == UP) {
			if (c != stack_head) {
				wl_list_safe_reinsert_prev(&stack_head->link, &c->link);
			}
		} else if (direction == RIGHT || direction == DOWN) {
			if (c != stack_tail) {
				wl_list_safe_reinsert_next(&stack_head->link, &c->link);
			}
		}
		sync_scroller_state_to_clients(m, tag);
		arrange(m, false, false);
		return;
	}

	if (!tnode || target_client->mon != c->mon)
		return;

	struct ScrollerStackNode *tail = tnode;
	while (tail->next_in_stack)
		tail = tail->next_in_stack;

	scroller_insert_stack(c, tail->client, false);

	if (c != tail->client) {
		wl_list_remove(&c->link);
		wl_list_insert(&tail->client->link, &c->link);
	}
	return;
}

void scroller_stack(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c || !c->mon || c->isfloating ||
		!is_scroller_layout(server.selected_monitor))
		return;

	Client *target_client = find_client_by_direction(c, arg, false);

	scroller_apply_stack(c, target_client, arg->i);
}

void toggle_all_floating(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *ref = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!ref)
		return;

	bool should_floating = !ref->isfloating;

	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, server.selected_monitor)) {
			if (c->isfloating && !should_floating) {
				c->old_master_inner_per = 0.0f;
				c->old_stack_inner_per = 0.0f;
				set_size_per(server.selected_monitor, c);
			}

			if (c->isfloating != should_floating) {
				client_set_floating(c, should_floating);
			}
		}
	}
	return;
}

void dwindle_set_split_direction(Client *c, bool istoggle, bool horizontal) {
	uint32_t tag = get_client_tag_idx(c);
	const Layout *layout = c->mon->pertag->ltidxs[tag];

	if (layout->id != DWINDLE)
		return;

	DwindleNode **root = &c->mon->pertag->dwindle_root[tag];
	DwindleNode *leaf = dwindle_find_leaf(*root, c);

	if (!leaf)
		return;

	if (istoggle) {
		leaf->custom_leaf_split_h = !leaf->custom_leaf_split_h;
	} else if (horizontal) {
		leaf->custom_leaf_split_h = true;
	} else {
		leaf->custom_leaf_split_h = false;
	}
	bool hit_no_border = check_hit_no_border(c);
	struct ivec2 offsets = compute_edge_offsets(c);
	client_draw_split_border(c, hit_no_border, offsets);
	return;
}

void dwindle_toggle_split_direction(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c || !c->mon || c->isfloating)
		return;
	dwindle_set_split_direction(c, true, false);
}

void dwindle_split_horizontal(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c || !c->mon || c->isfloating)
		return;
	dwindle_set_split_direction(c, false, true);
}

void dwindle_split_vertical(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c || !c->mon || c->isfloating)
		return;
	dwindle_set_split_direction(c, false, false);
}

void dwindle_toggle_current_split(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	Client *c = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!c || !c->mon || c->isfloating)
		return;

	uint32_t tag = get_client_tag_idx(c);
	const Layout *layout = c->mon->pertag->ltidxs[tag];
	if (layout->id != DWINDLE)
		return;

	DwindleNode *root = c->mon->pertag->dwindle_root[tag];
	DwindleNode *leaf = dwindle_find_leaf(root, c);
	if (!leaf || !leaf->parent)
		return;

	DwindleNode *parent = leaf->parent;
	parent->split_h = !parent->split_h;
	parent->split_locked = true;

	arrange(c->mon, false, false);
}

void focus_by_id(const Arg *arg) {
	if (!server.selected_monitor || !arg->tc)
		return;

	Client *c = arg->tc;

	if (c->swallowdby)
		return;

	if (c->group_next || c->group_prev)
		client_focus_group_member(c);

	client_active(c);
	return;
}

void load_config_file(const Arg *arg) {
	snprintf(server.cli_config_path, sizeof(server.cli_config_path), "%s",
			 arg->v);
	reload_config(arg);
	return;
}
