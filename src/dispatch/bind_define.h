void bind_to_view(const Arg *arg) {
	if (!selmon)
		return;
	uint32_t target = arg->ui;

	if (config.view_current_to_back && selmon->pertag->curtag &&
		(target & TAGMASK) == (selmon->tagset[selmon->seltags])) {
		if (selmon->pertag->prevtag)
			target = 1 << (selmon->pertag->prevtag - 1);
		else
			// prevtag==0: previous view was all-tags or special,
			// decide by the other tagset
			target = (selmon->tagset[selmon->seltags ^ 1] & TAG0_MASK)
						 ? TAG0_MASK
						 : 0;
	}

	if (!config.view_current_to_back &&
		(target & TAGMASK) == (selmon->tagset[selmon->seltags])) {
		return;
	}

	if ((int32_t)target == INT_MIN && selmon->pertag->curtag == 0) {
		if (config.view_current_to_back && selmon->pertag->prevtag)
			target = 1 << (selmon->pertag->prevtag - 1);
		else
			target = 0;
	}

	// TAG0_MASK is INT_MIN too; handle it before the all-tags branch
	if (target == TAG0_MASK) {
		view(&(Arg){.ui = target, .i = arg->i}, true);
		return;
	}

	if (target == 0 || (int32_t)target == INT_MIN) {
		view(&(Arg){.ui = ~0 & TAGMASK, .i = arg->i}, false);
	} else {
		view(&(Arg){.ui = target, .i = arg->i}, true);
	}
	return;
}

void chvt(const Arg *arg) {
	struct timespec ts;

	allow_frame_scheduling = false;

	if (selmon) {
		chvt_backup_tag = selmon->pertag->curtag;
		strncpy(chvt_backup_selmon, selmon->wlr_output->name,
				sizeof(chvt_backup_selmon) - 1);
	}

	wlr_session_change_vt(session, arg->ui);

	ts.tv_sec = 0;
	ts.tv_nsec = 100000000;
	nanosleep(&ts, NULL);

	allow_frame_scheduling = true;
	return;
}

void create_virtual_output(const Arg *arg) {
	if (!wlr_backend_is_multi(backend)) {
		mango_error(true, WLR_ERROR, "Expected a multi backend");
		return;
	}

	bool done = false;
	wlr_multi_for_each_backend(backend, create_output, &done);

	if (!done) {
		mango_error(true, WLR_ERROR, "Failed to create virtual output");
		return;
	}

	mango_error(true, WLR_INFO, "Virtual output created");
	return;
}

void destroy_all_virtual_output(const Arg *arg) {
	if (!wlr_backend_is_multi(backend)) {
		mango_error(true, WLR_ERROR, "Expected a multi backend");
		return;
	}

	Monitor *m, *tmp;
	wl_list_for_each_safe(m, tmp, &mons, link) {
		if (wlr_output_is_headless(m->wlr_output)) {
			wlr_output_destroy(m->wlr_output);
			mango_error(true, WLR_INFO, "Virtual output destroyed");
		}
	}
	return;
}

void defaultgaps(const Arg *arg) {
	if (selmon && is_special_active(selmon)) {
		setgaps(config.special_gappoh, config.special_gappov,
				config.special_gappih, config.special_gappiv);
	} else {
		setgaps(config.gappoh, config.gappov, config.gappih, config.gappiv);
	}
	return;
}

void exchange_client(const Arg *arg) {
	if (!selmon)
		return;
	Client *c = arg->tc ? arg->tc : selmon->sel;
	if (!c || c->isfloating)
		return;

	if ((c->isfullscreen || c->ismaximizescreen) && !is_scroller_layout(c->mon))
		return;

	Client *tc = direction_select(arg);
	tc = get_focused_stack_client(tc, arg->tc);

	if (!tc)
		return;

	exchange_two_client(c, tc);
	return;
}

void exchange_stack_client(const Arg *arg) {
	if (!selmon)
		return;

	Client *c = arg->tc ? arg->tc : selmon->sel;
	Client *tc = NULL;
	if (!c || c->isfloating || c->isfullscreen || c->ismaximizescreen)
		return;
	if (arg->i == NEXT) {
		tc = get_next_stack_client(c, false);
	} else {
		tc = get_next_stack_client(c, true);
	}
	if (tc)
		exchange_two_client(c, tc);
	return;
}

static bool view_shift_tag(const Arg *arg, int dir);
static bool view_shift_tag_have_client(const Arg *arg, int dir);

void focusdir(const Arg *arg) {

	if (!selmon)
		return;

	Client *c = NULL;
	c = direction_select(arg);

	if (!selmon->isoverview)
		c = get_focused_stack_client(c, arg->tc);
	if (c) {
		focusclient(c, 1);
		if (config.warpcursor)
			warp_cursor(c);
	} else {
		// no cross-tag/monitor jumps inside the special workspace
		if (!is_special_active(selmon)) {
			if (config.focus_cross_tag) {
				if (arg->i == LEFT || arg->i == UP)
					view_shift_tag_have_client(&(Arg){0}, -1);
				if (arg->i == RIGHT || arg->i == DOWN)
					view_shift_tag_have_client(&(Arg){0}, 1);
			} else if (config.focus_cross_monitor) {
				focusmon(arg);
			}
		}
	}
	return;
}

void focus_window_or_workspace(const Arg *arg) {
	if (!selmon)
		return;

	if (selmon->isoverview)
		return;

	Client *c = NULL;

	c = direction_select(arg);
	if (!selmon->isoverview)
		c = get_focused_stack_client(c, arg->tc);
	if (c && c->mon == selmon) {
		focusclient(c, 1);
		if (config.warpcursor)
			warp_cursor(c);
		return;
	}

	if (!is_special_active(selmon)) {
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

void groupjoin(const Arg *arg) {

	if (!selmon)
		return;

	Monitor *oldmon = NULL;

	Client *need_join_client = arg->tc ? arg->tc : selmon->sel;
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
		groupleave(&(Arg){.tc = need_join_client});
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

	// oldmon可能已经死掉了
	if (oldmon) {
		arrange(oldmon, false, false);
	}

	return;
}

void groupleave(const Arg *arg) {

	if (!selmon)
		return;
	Client *tc = arg->tc ? arg->tc : selmon->sel;
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

void focuslast(const Arg *arg) {
	Client *c = NULL;
	Client *tc = NULL;
	bool begin = false;
	uint32_t target = 0;

	wl_list_for_each(c, &fstack, flink) {
		if (c->iskilling || c->isminimized || c->isunglobal ||
			!client_surface(c)->mapped || client_is_unmanaged(c) ||
			client_is_x11_popup(c))
			continue;

		if (selmon && !selmon->sel) {
			tc = c;
			break;
		}

		if (selmon && c == selmon->sel && !begin) {
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
		focusclient(tc, 1);
		target = get_tags_first_tag(tc->tags);
		view(&(Arg){.ui = target}, true);
	}
	return;
}

void toggle_trackpad_enable(const Arg *arg) {
	config.disable_trackpad = !config.disable_trackpad;
	return;
}

void focusmon(const Arg *arg) {
	Client *c = NULL;
	Monitor *m = NULL;
	Monitor *tm = NULL;

	if (arg->i != UNDIR) {
		tm = dirtomon(arg->i);
	} else if (arg->v) {
		wl_list_for_each(m, &mons, link) {
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

	if (!tm || !tm->wlr_output->enabled || tm == selmon)
		return;

	selmon = tm;
	if (config.warpcursor) {
		warp_cursor_to_selmon(selmon);
	}
	c = arg->tc ? arg->tc : focustop(selmon);
	if (!c) {
		selmon->sel = NULL;
		wlr_seat_pointer_notify_clear_focus(seat);
		wlr_seat_keyboard_notify_clear_focus(seat);
		focusclient(NULL, 0);
	} else
		focusclient(c, 1);

	return;
}

void focusstack(const Arg *arg) {
	Client *sel = arg->tc ? arg->tc : focustop(selmon);
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

	focusclient(tc, 1);
	if (config.warpcursor)
		warp_cursor(tc);
	return;
}

/* overcircle: 打开/循环 overview（居中 tab 布局）
 * - 未进入 overview：进入 overview
 * - 已在 overview 中：按插入顺序循环当前显示器的窗口并重排 */
void overcircle(const Arg *arg) {
	if (!selmon || grabc)
		return;

	Client *sel = arg->tc ? arg->tc : selmon->sel;

	if (selmon->isoverview && !selmon->is_jump_mode &&
		!selmon->ov_normal_mode && sel) {
		selmon->ov_tab_layout = 1;
		Client *tc = arg->i == NEXT ? get_next_stack_client(sel, false)
									: get_next_stack_client(sel, true);
		if (!tc)
			return;

		focusclient(tc, 1);

		/* 切换焦点后重排，让 tab 布局跟随焦点 */
		arrange(selmon, true, false);
		return;
	}

	/* 进入 overview：启用居中 tab 布局，其余交给 toggleoverview */
	selmon->ov_tab_layout = 1;
	toggleoverview(arg);
	if (!selmon->isoverview)
		selmon->ov_tab_layout = 0;
}

void groupfocus(const Arg *arg) {
	Client *c = arg->tc ? arg->tc : selmon->sel;
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

void incnmaster(const Arg *arg) {
	if (!arg || !selmon)
		return;
	uint32_t tag = get_mon_curtag(selmon);
	selmon->pertag->nmasters[tag] =
		MANGO_MAX(selmon->pertag->nmasters[tag] + arg->i, 0);
	arrange(selmon, false, false);
	return;
}

void incgaps(const Arg *arg) {
	if (!selmon)
		return;
	setgaps(selmon->gappoh + arg->i, selmon->gappov + arg->i,
			selmon->gappih + arg->i, selmon->gappiv + arg->i);
	return;
}

void incigaps(const Arg *arg) {
	if (!selmon)
		return;
	setgaps(selmon->gappoh, selmon->gappov, selmon->gappih + arg->i,
			selmon->gappiv + arg->i);
	return;
}

void incogaps(const Arg *arg) {
	if (!selmon)
		return;
	setgaps(selmon->gappoh + arg->i, selmon->gappov + arg->i, selmon->gappih,
			selmon->gappiv);
	return;
}

void incihgaps(const Arg *arg) {
	if (!selmon)
		return;
	setgaps(selmon->gappoh, selmon->gappov, selmon->gappih + arg->i,
			selmon->gappiv);
	return;
}

void incivgaps(const Arg *arg) {
	if (!selmon)
		return;
	setgaps(selmon->gappoh, selmon->gappov, selmon->gappih,
			selmon->gappiv + arg->i);
	return;
}

void incohgaps(const Arg *arg) {
	if (!selmon)
		return;
	setgaps(selmon->gappoh + arg->i, selmon->gappov, selmon->gappih,
			selmon->gappiv);
	return;
}

void incovgaps(const Arg *arg) {
	if (!selmon)
		return;
	setgaps(selmon->gappoh, selmon->gappov + arg->i, selmon->gappih,
			selmon->gappiv);
	return;
}

void setmfact(const Arg *arg) {
	float f;
	Client *c = NULL;

	if (!arg || !selmon ||
		!selmon->pertag->ltidxs[get_mon_curtag(selmon)]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->pertag->mfacts[get_mon_curtag(selmon)]
					 : arg->f - 1.0;
	if (f < 0.1 || f > 0.9)
		return;

	selmon->pertag->mfacts[get_mon_curtag(selmon)] = f;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, selmon) && ISTILED(c)) {
			c->master_mfact_per = f;
		}
	}
	arrange(selmon, false, false);
	return;
}

void killclient(const Arg *arg) {
	Client *c = arg->tc ? arg->tc : (selmon ? selmon->sel : NULL);
	if (c) {
		if (arg->i == FORCE) {
			client_pending_force_kill(c);
		} else {
			if (config.smartkill && c->mon && !c->isglobal && !c->isunglobal &&
				!c->is_in_scratchpad) {
				uint32_t newtags =
					c->tags & ~(c->mon->tagset[c->mon->seltags] & TAGMASK);
				if (newtags && newtags != c->tags) {
					c->tags = newtags;
					focusclient(focustop(c->mon), 1);
					arrange(c->mon, false, false);
					printstatus(IPC_WATCH_ARRANGGE);
					return;
				}
			}
			pending_kill_client(c);
		}
	}
	return;
}

void moveresize(const Arg *arg) {
	const char *cursors[] = {"nw-resize", "ne-resize", "sw-resize",
							 "se-resize"};

	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	xytonode(cursor->x, cursor->y, NULL, &grabc, NULL, NULL, NULL, NULL);
	if (!grabc || client_is_unmanaged(grabc) || grabc->isfullscreen ||
		grabc->ismaximizescreen) {
		grabc = NULL;
		return;
	}
	if (grabc->isfloating == 0 && arg->ui == CurMove) {
		grabc->drag_to_tile = true;
		exit_scroller_stack(grabc);
		setfloating(grabc, 1);
		grabc->drag_tile_float_backup_geom = grabc->float_geom;
		grabc->old_stack_inner_per = 0.0f;
		grabc->old_master_inner_per = 0.0f;
		set_size_per(grabc->mon, grabc);
	}

	if (grabc && grabc->drag_to_tile && config.drag_tile_to_tile &&
		config.drag_tile_small) {
		grabc->geom.x = cursor->x - 150;
		grabc->geom.y = cursor->y - 150;
		grabc->geom.width = 300;
		grabc->geom.height = 300;
		resize(grabc, grabc->geom, 1);
	}

	switch (cursor_mode = arg->ui) {
	case CurMove:
		grabcx = cursor->x - grabc->geom.x;
		grabcy = cursor->y - grabc->geom.y;
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "grab");
		break;
	case CurResize:
		if (grabc->isfloating) {
			rzcorner = config.drag_corner;
			grabcx = (int)round(cursor->x);
			grabcy = (int)round(cursor->y);
			if (rzcorner == 4)
				rzcorner = (grabcx - grabc->geom.x <
									grabc->geom.x + grabc->geom.width - grabcx
								? 0
								: 1) +
						   (grabcy - grabc->geom.y <
									grabc->geom.y + grabc->geom.height - grabcy
								? 0
								: 2);

			if (config.drag_warp_cursor) {
				grabcx = rzcorner & 1 ? grabc->geom.x + grabc->geom.width
									  : grabc->geom.x;
				grabcy = rzcorner & 2 ? grabc->geom.y + grabc->geom.height
									  : grabc->geom.y;
				wlr_cursor_warp_closest(cursor, NULL, grabcx, grabcy);
			}

			wlr_cursor_set_xcursor(cursor, cursor_mgr, cursors[rzcorner]);
		} else {
			wlr_cursor_set_xcursor(cursor, cursor_mgr, "grab");
		}
		break;
	}
	return;
}

void movewin(const Arg *arg) {
	Client *c = arg->tc ? arg->tc : (selmon ? selmon->sel : NULL);
	if (!c || c->isfullscreen)
		return;
	if (!c->isfloating)
		setfloating(c, 1);

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
	wl_display_terminate(dpy);
	return;
}

void resizewin(const Arg *arg) {
	Client *c = arg->tc ? arg->tc : (selmon ? selmon->sel : NULL);
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
	if (selmon && selmon->isoverview)
		return;

	Client *c = NULL;
	Client *focused = selmon ? selmon->sel : NULL;
	if (!focused && selmon)
		focused = focustop(selmon);

	/* 1. If focused window or any shown scratchpad exists, evict it */
	if (focused && focused->is_in_scratchpad && focused->is_scratchpad_show) {
		c = focused;
	} else {
		Client *tc = NULL;
		wl_list_for_each(tc, &clients, link) {
			if ((tc->mon == selmon || config.scratchpad_cross_monitor) &&
				tc->is_in_scratchpad && tc->is_scratchpad_show) {
				c = tc;
				break;
			}
		}
	}

	/* 2. Otherwise, find a minimized window to restore */
	if (!c) {
		wl_list_for_each(c, &clients, link) {
			if (c->isminimized && !c->isnamedscratchpad)
				break;
		}
		if (&c->link == &clients)
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
	c->tags = (focused && focused != c && focused->tags)
				  ? focused->tags
				  : selmon->tagset[selmon->seltags];
	c->oldtags = c->tags;
	c->mon = selmon;

	setfloating(c, 0);
	setborder_color(c);
	arrange(c->mon, false, false);
	focusclient(c, 1);
	warp_cursor(c);
}

void setlayout(const Arg *arg) {
	int32_t jk;
	if (!selmon)
		return;

	for (jk = 0; jk < LENGTH(layouts); jk++) {
		if (strcmp(layouts[jk].name, arg->v) == 0) {
			selmon->pertag->ltidxs[get_mon_curtag(selmon)] = &layouts[jk];
			clear_fullscreen_and_maximized_state(selmon);
			arrange(selmon, false, false);
			printstatus(IPC_WATCH_ARRANGGE);
			return;
		}
	}
	return;
}

void setkeymode(const Arg *arg) {
	snprintf(keymode.mode, sizeof(keymode.mode), "%.27s", arg->v);
	if (strcmp(keymode.mode, "default") == 0) {
		keymode.isdefault = true;
	} else {
		keymode.isdefault = false;
	}
	printstatus(IPC_WATCH_KEYMODE);
	return;
}

void set_proportion(const Arg *arg) {
	if (!selmon)
		return;

	if (selmon->isoverview || !is_scroller_layout(selmon))
		return;

	if (selmon->visible_tiling_clients == 1 &&
		!config.scroller_ignore_proportion_single)
		return;

	Client *tc = arg->tc ? arg->tc : selmon->sel;
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
	if (!selmon)
		return;

	if (config.scroller_proportion_preset_count == 0)
		return;

	if (selmon->isoverview || !is_scroller_layout(selmon))
		return;

	if (selmon->visible_tiling_clients == 1 &&
		!config.scroller_ignore_proportion_single)
		return;

	Client *tc = arg->tc ? arg->tc : selmon->sel;

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

void smartmovewin(const Arg *arg) {
	Client *c = NULL, *tc = NULL;
	int32_t nx, ny;
	int32_t buttom, top, left, right, tar;
	if (!selmon)
		return;
	c = arg->tc ? arg->tc : selmon->sel;
	if (!c || c->isfullscreen || !c->mon)
		return;
	if (!c->isfloating)
		setfloating(c, true);
	nx = c->geom.x;
	ny = c->geom.y;

	switch (arg->i) {
	case UP:
		tar = -99999;
		top = c->geom.y;
		ny -= c->mon->w.height / 4;

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
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

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
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

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
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
		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
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

void smartresizewin(const Arg *arg) {
	Client *c = NULL, *tc = NULL;
	int32_t nw, nh;
	int32_t buttom, top, left, right, tar;
	if (!selmon)
		return;
	c = arg->tc ? arg->tc : selmon->sel;
	if (!c || c->isfullscreen)
		return;
	if (!c->isfloating)
		setfloating(c, true);
	nw = c->geom.width;
	nh = c->geom.height;

	switch (arg->i) {
	case UP:
		nh -= selmon->w.height / 8;
		nh = MANGO_MAX(nh, selmon->w.height / 10);
		break;
	case DOWN:
		tar = -99999;
		buttom = c->geom.y + c->geom.height;
		nh += selmon->w.height / 8;

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
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
		if (c->geom.y + nh + config.gappov > selmon->w.y + selmon->w.height)
			nh = selmon->w.y + selmon->w.height - c->geom.y - config.gappov;
		break;
	case LEFT:
		nw -= selmon->w.width / 16;
		nw = MANGO_MAX(nw, selmon->w.width / 10);
		break;
	case RIGHT:
		tar = 99999;
		right = c->geom.x + c->geom.width;
		nw += selmon->w.width / 16;
		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
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
		if (c->geom.x + nw + config.gappoh > selmon->w.x + selmon->w.width)
			nw = selmon->w.x + selmon->w.width - c->geom.x - config.gappoh;
		break;
	}

	c->float_geom = (struct wlr_box){
		.x = c->geom.x, .y = c->geom.y, .width = nw, .height = nh};
	c->iscustomsize = 1;
	resize(c, c->float_geom, 1);
	return;
}

void centerwin(const Arg *arg) {
	Client *c = arg->tc ? arg->tc : (selmon ? selmon->sel : NULL);

	if (!c || c->isfullscreen || c->ismaximizescreen)
		return;

	if (c->isfloating) {
		c->float_geom = setclient_coordinate_center(c, c->mon, c->geom, 0, 0);
		c->iscustomsize = 1;
		resize(c, c->float_geom, 1);
		return;
	}

	if (!is_scroller_layout(selmon))
		return;

	Client *stack_head = scroll_get_stack_head_client(c);
	if (selmon->pertag->ltidxs[get_mon_curtag(selmon)]->id == SCROLLER) {
		stack_head->geom.x =
			selmon->w.x + (selmon->w.width - stack_head->geom.width) / 2;
	} else {
		stack_head->geom.y =
			selmon->w.y + (selmon->w.height - stack_head->geom.height) / 2;
	}

	arrange(selmon, false, false);
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

	wl_list_for_each(c, &clients, link) {
		if (arg->ui & c->tags && c->mon == selmon) {
			is_empty = false;
			break;
		}
	}
	if (!is_empty) {
		view(arg, true);
		return;
	} else {
		view(arg, true);
		spawn_shell(arg);
	}
	return;
}

void switch_keyboard_layout(const Arg *arg) {
	if (!kb_group || !kb_group->wlr_group || !seat) {
		mango_error(true, WLR_ERROR, "Invalid keyboard group or seat");
		return;
	}

	struct wlr_keyboard *keyboard = &kb_group->wlr_group->keyboard;
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
	uint32_t locked = keyboard->modifiers.locked;

	wlr_keyboard_notify_modifiers(keyboard, depressed, latched, locked, next);

	wlr_seat_set_keyboard(seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(seat, &keyboard->modifiers);

	InputDevice *id;
	wl_list_for_each(id, &inputdevices, link) {
		if (id->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD ||
			id->standalone) {
			/* 独立键盘保留自己的 keymap/layout，不同步布局切换 */
			continue;
		}

		struct wlr_keyboard *tkb = (struct wlr_keyboard *)id->device_data;

		wlr_keyboard_notify_modifiers(tkb, depressed, latched, locked, next);
		wlr_seat_set_keyboard(seat, tkb);
		wlr_seat_keyboard_notify_modifiers(seat, &tkb->modifiers);
	}

	printstatus(IPC_WATCH_KB_LAYOUT);
	return;
}

void switch_layout(const Arg *arg) {

	int32_t jk, ji;
	char *target_layout_name = NULL;
	uint32_t len;

	if (!selmon)
		return;

	uint32_t tag = get_mon_curtag(selmon);

	if (config.circle_layout_count != 0) {
		for (jk = 0; jk < config.circle_layout_count; jk++) {

			len = MANGO_MAX(strlen(config.circle_layout[jk]),
							strlen(selmon->pertag->ltidxs[tag]->name));

			if (strncmp(config.circle_layout[jk],
						selmon->pertag->ltidxs[tag]->name, len) == 0) {
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
				selmon->pertag->ltidxs[tag] = &layouts[ji];

				break;
			}
		}
		clear_fullscreen_and_maximized_state(selmon);
		arrange(selmon, false, false);
		printstatus(IPC_WATCH_ARRANGGE);
		return;
	}

	for (jk = 0; jk < LENGTH(layouts); jk++) {
		if (strcmp(layouts[jk].name, selmon->pertag->ltidxs[tag]->name) == 0) {
			selmon->pertag->ltidxs[tag] =
				jk == LENGTH(layouts) - 1 ? &layouts[0] : &layouts[jk + 1];
			clear_fullscreen_and_maximized_state(selmon);
			arrange(selmon, false, false);
			printstatus(IPC_WATCH_ARRANGGE);
			return;
		}
	}
	return;
}

void tag(const Arg *arg) {
	if (!selmon)
		return;
	Client *target_client = arg->tc ? arg->tc : selmon->sel;
	tag_client(arg, target_client);
	return;
}

void tagmon(const Arg *arg) {
	Monitor *m = NULL, *cm = NULL, *oldmon = NULL;
	if (!selmon)
		return;
	Client *c = arg->tc ? arg->tc : focustop(selmon);

	if (!c)
		return;

	oldmon = c->mon;

	if (arg->i != UNDIR) {
		m = dirtomon(arg->i);
	} else if (arg->v) {
		wl_list_for_each(cm, &mons, link) {
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
		view(&(Arg){.ui = newtags}, true);
		return;
	}

	if (c == oldmon->sel) {
		oldmon->sel = NULL;
	}

	setmon(c, m, newtags, true);
	client_update_oldmonname_record(c, m);

	reset_foreign_tolevel(c, oldmon, c->mon);

	c->float_geom.width =
		(int32_t)(c->float_geom.width * c->mon->w.width / oldmon->w.width);
	c->float_geom.height =
		(int32_t)(c->float_geom.height * c->mon->w.height / oldmon->w.height);
	selmon = c->mon;
	c->float_geom = setclient_coordinate_center(c, c->mon, c->float_geom, 0, 0);

	if (c->isfloating) {
		c->geom = c->float_geom;
		target = get_tags_first_tag(c->tags);
		view(&(Arg){.ui = target}, true);
		focusclient(c, 1);
		resize(c, c->geom, 1);
	} else {
		selmon = c->mon;
		target = get_tags_first_tag(c->tags);
		view(&(Arg){.ui = target}, true);
		focusclient(c, 1);
		arrange(selmon, false, false);
	}
	if (config.warpcursor) {
		warp_cursor_to_selmon(c->mon);
	}
	return;
}

void tagsilent(const Arg *arg) {
	Client *fc = NULL;
	Client *target_client = arg->tc ? arg->tc : (selmon ? selmon->sel : NULL);

	if (!target_client)
		return;

	target_client->tags =
		(arg->ui & TAG0_MASK) ? TAG0_MASK : (arg->ui & TAGMASK);
	client_reparent_group(target_client);
	wl_list_for_each(fc, &clients, link) {
		if (fc && fc != target_client && target_client->tags & fc->tags &&
			ISFULLSCREEN(fc) && !target_client->isfloating) {
			clear_fullscreen_flag(fc);
		}
	}
	focusclient(focustop(selmon), 1);
	arrange(target_client->mon, false, false);
	return;
}

void tagtoleft(const Arg *arg) {
	if (!selmon)
		return;

	Client *sel = arg->tc ? arg->tc : selmon->sel;
	if (sel != NULL &&
		__builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1) {
		uint32_t target = selmon->tagset[selmon->seltags] >> 1;

		if (target == 0) {
			if (!config.tag_carousel)
				return;
			target = (1 << (config.tag_num - 1)) & TAGMASK;
			selmon->carousel_anim_dir = -1;
		}

		Arg a = {.ui = target & TAGMASK, .i = arg->i, .tc = sel};
		tag(&a);
		selmon->carousel_anim_dir = 0;
	}
	return;
}

void tagtoright(const Arg *arg) {
	if (!selmon)
		return;

	Client *sel = arg->tc ? arg->tc : selmon->sel;
	if (sel != NULL &&
		__builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1) {
		uint32_t target = selmon->tagset[selmon->seltags] << 1;

		if (!(target & TAGMASK)) {
			if (!config.tag_carousel)
				return;
			target = 1;
			selmon->carousel_anim_dir = 1;
		}

		Arg a = {.ui = target & TAGMASK, .i = arg->i, .tc = sel};
		tag(&a);
		selmon->carousel_anim_dir = 0;
	}
	return;
}

void toggle_named_scratchpad(const Arg *arg) {
	Client *target_client = NULL;
	char *arg_id = arg->v;
	char *arg_title = arg->v2;

	if (selmon && selmon->isoverview)
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
	if (!selmon)
		return;
	render_border = !render_border;
	arrange(selmon, false, false);
	return;
}

void toggle_scratchpad(const Arg *arg) {
	Client *c = NULL;
	bool hit = false;
	Client *tmp = NULL;

	if (selmon && selmon->isoverview)
		return;

	wl_list_for_each_safe(c, tmp, &clients, link) {
		if (!config.scratchpad_cross_monitor && c->mon != selmon) {
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
static void toggle_special_tag_mon(Monitor *m) {
	if (!m || m->isoverview)
		return;

	selmon = m;
	if (is_special_active(m)) {
		/* Toggle back to previous tagset (supports multi-tag views) */
		uint32_t prev_set = m->tagset[m->seltags ^ 1] & TAGMASK;
		uint32_t target =
			prev_set
				? prev_set
				: (m->pertag->prevtag ? (1 << (m->pertag->prevtag - 1)) : 1);
		view(&(Arg){.ui = target}, true);
	} else {
		/* Switch to tag 0 */
		view(&(Arg){.ui = TAG0_MASK}, true);
	}
}

void toggle_special_tag(const Arg *arg) {
	Monitor *m = (arg && arg->tc && arg->tc->mon) ? arg->tc->mon : selmon;
	toggle_special_tag_mon(m);
}

static void tag_special_tag_internal(const Arg *arg, bool silent) {
	if (!selmon)
		return;
	Client *c = arg->tc ? arg->tc : selmon->sel;
	if (!c)
		return;

	Monitor *m = c->mon ? c->mon : selmon;
	if (c->tags & TAG0_MASK) {
		/* Return window from tag 0 to normal tag */
		uint32_t target =
			(m->pertag->prevtag > 0)
				? (1 << (m->pertag->prevtag - 1))
				: ((m->pertag->curtag > 0) ? (1 << (m->pertag->curtag - 1))
										   : 1);
		if (silent)
			tagsilent(&(Arg){.ui = target, .tc = c});
		else
			tag(&(Arg){.ui = target, .tc = c});

	} else {
		/* Send window to tag 0 */
		if (silent)
			tagsilent(&(Arg){.ui = TAG0_MASK, .tc = c});
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

void togglefakefullscreen(const Arg *arg) {
	if (!selmon)
		return;
	Client *sel = arg->tc ? arg->tc : focustop(selmon);
	if (sel)
		setfakefullscreen(sel, !sel->isfakefullscreen);
	return;
}

void togglefloating(const Arg *arg) {
	if (!selmon || grabc)
		return;

	Client *sel = arg->tc ? arg->tc : focustop(selmon);

	if (selmon && selmon->isoverview)
		return;

	if (!sel)
		return;

	bool isfloating = sel->isfloating;

	if ((sel->isfullscreen || sel->ismaximizescreen)) {
		isfloating = 1;
	} else {
		isfloating = !sel->isfloating;
	}

	setfloating(sel, isfloating);
	return;
}

void togglefullscreen(const Arg *arg) {
	if (!selmon)
		return;

	Client *sel = arg->tc ? arg->tc : focustop(selmon);
	if (!sel)
		return;

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
	sel->isnamedscratchpad = 0;

	if (sel->isfullscreen)
		setfullscreen(sel, 0, true);
	else
		setfullscreen(sel, 1, true);
	return;
}

void toggleglobal(const Arg *arg) {
	if (!selmon)
		return;

	Client *c = arg->tc ? arg->tc : selmon->sel;
	if (!c)
		return;

	if (c->is_in_scratchpad) {
		c->is_in_scratchpad = 0;
		c->is_scratchpad_show = 0;
		c->isnamedscratchpad = 0;
	}
	c->isglobal ^= 1;
	setborder_color(c);
	return;
}

void togglegaps(const Arg *arg) {
	if (!selmon)
		return;

	enablegaps ^= 1;
	arrange(selmon, false, false);
	return;
}

void togglemaximizescreen(const Arg *arg) {
	if (!selmon)
		return;

	Client *sel = arg->tc ? arg->tc : focustop(selmon);
	if (!sel)
		return;

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
	sel->isnamedscratchpad = 0;

	if (sel->ismaximizescreen)
		setmaximizescreen(sel, 0, true);
	else
		setmaximizescreen(sel, 1, true);

	setborder_color(sel);
	return;
}

void toggleoverlay(const Arg *arg) {
	if (!selmon)
		return;

	Client *c = arg->tc ? arg->tc : selmon->sel;
	if (!c || !c->mon || c->isfullscreen) {
		return;
	}

	c->isoverlay ^= 1;

	client_reparent_group(c);
	setborder_color(c);
	return;
}

void toggletag(const Arg *arg) {
	if (!selmon)
		return;

	uint32_t newtags;
	Client *sel = arg->tc ? arg->tc : focustop(selmon);
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
		focusclient(focustop(selmon), 1);
		arrange(selmon, false, false);
	}
	printstatus(IPC_WATCH_ARRANGGE);
	return;
}

void toggleview(const Arg *arg) {
	if (!selmon)
		return;

	uint32_t newtagset;
	uint32_t target;
	Client *c = NULL;

	target = arg->ui == 0 ? ~0 & TAGMASK : arg->ui;

	newtagset = selmon->tagset[selmon->seltags] ^ (target & TAGMASK);

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focusclient(focustop(selmon), 1);
		wl_list_for_each(c, &clients, link) {
			if (VISIBLEON(c, selmon) && ISTILED(c)) {
				set_size_per(selmon, c);
			}
		}
		arrange(selmon, false, false);
	}
	printstatus(IPC_WATCH_ARRANGGE);
	return;
}

static bool view_shift_tag(const Arg *arg, int dir) {
	if (!selmon)
		return false;

	if (selmon->isoverview || selmon->pertag->curtag == 0)
		return false;

	uint32_t target = selmon->tagset[selmon->seltags];
	if (dir < 0) {
		target >>= 1;

		if (target == 0) {
			if (!config.tag_carousel)
				return false;
			target = (1 << (config.tag_num - 1)) & TAGMASK;
			selmon->carousel_anim_dir = -1;
		}
	} else {
		target <<= 1;

		if (!(target & TAGMASK)) {
			if (!config.tag_carousel)
				return false;
			target = 1;
			selmon->carousel_anim_dir = 1;
		}
	}

	if (target == selmon->tagset[selmon->seltags])
		return false;

	view(&(Arg){.ui = target & TAGMASK, .i = arg->i}, true);
	selmon->carousel_anim_dir = 0;
	return true;
}

static bool view_shift_tag_have_client(const Arg *arg, int dir) {
	if (!selmon)
		return false;

	if (selmon->isoverview || is_special_active(selmon))
		return false;

	uint32_t n;
	uint32_t current = get_tags_first_tag_num(selmon->tagset[selmon->seltags]);
	bool found = false;
	bool wrapped = false;

	if (dir < 0) {
		for (n = current - 1; n >= 1; n--) {
			if (get_tag_status(n, selmon)) {
				found = true;
				break;
			}
		}

		if (!found && config.tag_carousel) {
			for (n = (uint32_t)config.tag_num; n > current; n--) {
				if (get_tag_status(n, selmon)) {
					found = true;
					wrapped = true;
					break;
				}
			}
		}
	} else {
		for (n = current + 1; n <= (uint32_t)config.tag_num; n++) {
			if (get_tag_status(n, selmon)) {
				found = true;
				break;
			}
		}

		if (!found && config.tag_carousel) {
			for (n = 1; n < current; n++) {
				if (get_tag_status(n, selmon)) {
					found = true;
					wrapped = true;
					break;
				}
			}
		}
	}

	if (found) {
		if (wrapped)
			selmon->carousel_anim_dir = (dir < 0) ? -1 : 1;
		view(&(Arg){.ui = (1 << (n - 1)) & TAGMASK, .i = arg->i}, true);
		selmon->carousel_anim_dir = 0;
		return true;
	}
	return false;
}

void viewtoleft(const Arg *arg) { view_shift_tag(arg, -1); }

void viewtoright(const Arg *arg) { view_shift_tag(arg, 1); }

void view_insert(const Arg *arg) {
	uint32_t cur, curmask, target;

	if (!selmon || selmon->isoverview)
		return;

	curmask = selmon->tagset[selmon->seltags] & TAGMASK;
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
	} else if (get_tag_status(cur - 1, selmon) == 0) {
		target = cur - 1;
	} else {
		target = cur;
	}

	if (get_tag_status(target, selmon) == 0) {
		view(&(Arg){.ui = (1u << (target - 1)) & TAGMASK}, true);
		return;
	}

	if (target >= (uint32_t)config.tag_num ||
		get_tag_status((uint32_t)config.tag_num, selmon))
		return;

	view_insert_shift_tags(selmon, target);
	int32_t tag_gather_bak = config.tag_gather;
	config.tag_gather = 0;
	view(&(Arg){.ui = (1u << (target - 1)) & TAGMASK}, true);
	config.tag_gather = tag_gather_bak;
}

void viewtoleft_have_client(const Arg *arg) {
	view_shift_tag_have_client(arg, -1);
}

void viewtoright_have_client(const Arg *arg) {
	view_shift_tag_have_client(arg, 1);
}

void viewcrossmon(const Arg *arg) {
	if (!selmon)
		return;

	focusmon(&(Arg){.v = arg->v, .i = UNDIR});
	view_in_mon(arg, true, selmon, true);
	return;
}

void tagcrossmon(const Arg *arg) {
	if (!selmon)
		return;

	Client *c = arg->tc ? arg->tc : selmon->sel;
	if (!c)
		return;

	if (match_monitor_spec(arg->v, selmon)) {
		tag_client(arg, c);
		return;
	}

	Arg a = {.ui = arg->ui, .i = UNDIR, .v = arg->v, .tc = c};
	tagmon(&a);
	return;
}

void comboview(const Arg *arg) {
	uint32_t newtags = arg->ui & TAGMASK;

	if (!newtags || !selmon)
		return;

	if (tag_combo) {
		selmon->tagset[selmon->seltags] |= newtags;
		focusclient(focustop(selmon), 1);
		arrange(selmon, false, false);
	} else {
		tag_combo = true;
		view(&(Arg){.ui = newtags}, true);
	}

	printstatus(IPC_WATCH_ARRANGGE);
	return;
}

void zoom(const Arg *arg) {
	Client *c = NULL, *sel = arg->tc ? arg->tc : focustop(selmon);

	if (!sel || !selmon ||
		!selmon->pertag->ltidxs[get_mon_curtag(selmon)]->arrange ||
		sel->isfloating)
		return;

	wl_list_for_each(c, &clients,
					 link) if (VISIBLEON(c, selmon) && !c->isfloating) {
		if (c != sel)
			break;
		sel = NULL;
	}

	if (&c->link == &clients)
		return;

	if (!sel)
		sel = c;
	wl_list_remove(&sel->link);
	wl_list_insert(&clients, &sel->link);

	focusclient(sel, 1);
	arrange(selmon, false, false);
	return;
}

void setoption(const Arg *arg) {
	parse_option(&config, arg->v, arg->v2, 0);
	override_config();
	reset_option();
	return;
}

void minimized(const Arg *arg) {
	if (!selmon)
		return;

	if (selmon && selmon->isoverview)
		return;

	Client *c = arg->tc ? arg->tc : selmon->sel;
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

void toggleoverview(const Arg *arg) {
	Client *c = NULL;
	if (!selmon || grabc)
		return;

	Client *sel = arg->tc ? arg->tc : selmon->sel;

	selmon->isoverview ^= 1;
	uint32_t target = 0;
	uint32_t visible_client_number = 0;

	if (!selmon->isoverview) {
		selmon->ov_tab_layout = 0;
		if (selmon->is_jump_mode)
			finish_jump_mode(selmon);
	}

	if (selmon->isoverview) {
		wl_list_for_each(c, &clients,
						 link) if (c && c->mon == selmon &&
								   !client_is_unmanaged(c) &&
								   !client_is_x11_popup(c) && !c->isminimized &&
								   !c->isunglobal && !(c->tags & TAG0_MASK)) {
			visible_client_number++;
		}
		if (visible_client_number > 0) {
			selmon->ovbk_current_tagset = selmon->tagset[selmon->seltags];
			selmon->ovbk_prev_tagset = selmon->tagset[selmon->seltags ^ 1];
			target = ~0 & TAGMASK;
		} else {
			selmon->isoverview ^= 1;
			selmon->ov_tab_layout = 0;
			return;
		}
	} else if (!selmon->isoverview && sel && (sel->tags & TAGMASK) != 0) {
		target = get_tags_first_tag(sel->tags);
	} else {
		target = selmon->ovbk_current_tagset
					 ? (selmon->ovbk_current_tagset & TAGMASK)
					 : (selmon->pertag->prevtag
							? (1 << (selmon->pertag->prevtag - 1))
							: 1);
		if (!target)
			target = 1;
	}

	if (selmon->isoverview) {
		wlr_seat_pointer_clear_focus(seat);

		if (cursor_hidden) {
			handlecursoractivity();
		} else {
			wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
		}

		wl_list_for_each(c, &clients, link) {
			if (c && c->mon == selmon && !client_is_unmanaged(c) &&
				!client_is_x11_popup(c) && !c->isunglobal && !c->isminimized &&
				!(c->tags & TAG0_MASK) && client_surface(c)->mapped) {
				c->animation.overining = true;
				if (!selmon->is_jump_mode && !selmon->ov_normal_mode)
					/* tab 布局：先跳过 view 排布，进入后统一重排时再设 */
					c->animation.overview_enter_anim_set = true;
				else
					/* 其余模式：view 排布时设置放大 */
					c->animation.overview_enter_anim_set = false;
				overview_backup(c);
			}
		}
	} else {
		selmon->ov_normal_mode = 0; /* 退出 overview 清除热区普通模式 */

		selmon->tagset[selmon->seltags] = target;
		wl_list_for_each(c, &clients, link) {
			if (c && c->mon == selmon && !c->iskilling &&
				!client_is_unmanaged(c) && !c->isunglobal && !c->isminimized &&
				!client_is_x11_popup(c) && client_surface(c)->mapped &&
				!(c->tags & TAG0_MASK)) {
				overview_restore(c, &(Arg){.ui = target});
			}
		}
	}

	view(&(Arg){.ui = target}, false);

	/* tab 布局：进入后重排 */
	if (selmon->isoverview && !selmon->is_jump_mode &&
		!selmon->ov_normal_mode) {

		Client *cc = NULL;
		wl_list_for_each(cc, &clients, link) {
			if (cc && cc->mon == selmon && !client_is_unmanaged(cc) &&
				!client_is_x11_popup(cc) && !(cc->tags & TAG0_MASK))
				cc->animation.overview_enter_anim_set = false;
		}
		arrange(selmon, true, false);
	}

	fix_mon_tagset_from_overview(selmon);
	refresh_monitors_workspaces_status(selmon);

	if (!selmon->isoverview && sel && (sel->tags & target)) {
		focusclient(sel, 1);
	}

	return;
}

void togglejump(const Arg *arg) {
	if (!selmon)
		return;

	if (!selmon->isoverview) {
		begin_jump_mode(selmon);
		toggleoverview(arg);
		return;
	}

	if (selmon->isoverview) {
		toggleoverview(arg);
	}

	return;
}

void disable_monitor(const Arg *arg) {
	Monitor *m = NULL;

	wl_list_for_each(m, &mons, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, false);
			mango_output_commit(m);
			m->only_sleep = 0;
			updatemons(NULL, NULL);
			break;
		}
	}
	return;
}

void enable_monitor(const Arg *arg) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, true);
			mango_output_commit(m);
			m->only_sleep = 0;
			updatemons(NULL, NULL);
			break;
		}
	}
	return;
}

void toggle_monitor(const Arg *arg) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, !m->wlr_output->enabled);
			mango_output_commit(m);
			m->only_sleep = 0;
			updatemons(NULL, NULL);
			break;
		}
	}
	return;
}

void sleep_monitor(const Arg *arg) {
	Monitor *m = NULL;

	wl_list_for_each(m, &mons, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, false);
			mango_output_commit(m);
			m->only_sleep = 1;
			updatemons(NULL, NULL);
			break;
		}
	}
	return;
}

void wakeup_monitor(const Arg *arg) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, true);
			mango_output_commit(m);
			m->only_sleep = 0;
			updatemons(NULL, NULL);
			break;
		}
	}
	return;
}

void sleep_toggle_monitor(const Arg *arg) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (match_monitor_spec(arg->v, m)) {
			wlr_output_state_set_enabled(&m->pending, !m->wlr_output->enabled);
			mango_output_commit(m);
			m->only_sleep = !m->wlr_output->enabled;
			updatemons(NULL, NULL);
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
	if (!selmon)
		return;
	Client *c = arg->tc ? arg->tc : selmon->sel;
	if (!c || !c->mon || c->isfloating || !is_scroller_layout(selmon))
		return;

	Client *target_client = find_client_by_direction(c, arg, false);

	scroller_apply_stack(c, target_client, arg->i);
}

void toggle_all_floating(const Arg *arg) {
	if (!selmon)
		return;

	Client *ref = arg->tc ? arg->tc : selmon->sel;
	if (!ref)
		return;

	bool should_floating = !ref->isfloating;

	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, selmon)) {
			if (c->isfloating && !should_floating) {
				c->old_master_inner_per = 0.0f;
				c->old_stack_inner_per = 0.0f;
				set_size_per(selmon, c);
			}

			if (c->isfloating != should_floating) {
				setfloating(c, should_floating);
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
	if (!selmon)
		return;

	Client *c = arg->tc ? arg->tc : selmon->sel;
	if (!c || !c->mon || c->isfloating)
		return;
	dwindle_set_split_direction(c, true, false);
}

void dwindle_split_horizontal(const Arg *arg) {
	if (!selmon)
		return;

	Client *c = arg->tc ? arg->tc : selmon->sel;
	if (!c || !c->mon || c->isfloating)
		return;
	dwindle_set_split_direction(c, false, true);
}

void dwindle_split_vertical(const Arg *arg) {
	if (!selmon)
		return;

	Client *c = arg->tc ? arg->tc : selmon->sel;
	if (!c || !c->mon || c->isfloating)
		return;
	dwindle_set_split_direction(c, false, false);
}

void dwindle_toggle_current_split(const Arg *arg) {
	if (!selmon)
		return;
	Client *c = arg->tc ? arg->tc : selmon->sel;
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

void focusid(const Arg *arg) {
	if (!selmon || !arg->tc)
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
	snprintf(cli_config_path, sizeof(cli_config_path), "%s", arg->v);
	reload_config(arg);
	return;
}
