#include "mango/layout/arrange.h"
#include "mango/common/server.h"
#include "mango/layout/layout.h"
#include "mango/manage/monitor.h"
#include "mango/layout/dwindle.h"
#include "mango/layout/scroll.h"
#include "mango/manage/client.h"
#include "mango/animation/tag.h"
#include "mango/input/pointer.h"
#include "mango/manage/misc.h"
#include "mango/dispatch/bind.h"
#include "mango/layout/vertical.h"
#include <assert.h>
#include "mango/ipc/ipc.h"
#include <wlr/types/wlr_cursor.h>

void set_size_per(Monitor *m, Client *c) {
	Client *fc = NULL;
	bool found = false;

	if (!m || !c)
		return;

	uint32_t tag = get_mon_curtag(m);
	const Layout *current_layout = m->pertag->ltidxs[tag];

	wl_list_for_each(fc, &server.clients, link) {
		if (VISIBLEON(fc, m) && ISTILED(fc) && fc != c) {
			if (current_layout->id == CENTER_TILE &&
				(fc->isleftstack ^ c->isleftstack))
				continue;
			c->master_mfact_per = fc->master_mfact_per;
			c->master_inner_per = fc->master_inner_per;
			c->stack_inner_per = fc->stack_inner_per;
			found = true;
			break;
		}
	}

	if (!found || c->isfloating) {
		c->master_mfact_per = m->pertag->mfacts[tag];
		c->master_inner_per = 1.0f;
		c->stack_inner_per = 1.0f;
	}

	if (!c->iscustom_scroller_proportion) {
		c->scroller_proportion = m->pertag->scroller_default_proportion[tag];
	}

	if (!c->iscustom_scroller_proportion_single) {
		c->scroller_proportion_single =
			m->pertag->scroller_default_proportion_single[tag];
	}
}

void resize_tile_master_horizontal(Client *gc, bool isdrag, int32_t offsetx,
								   int32_t offsety, uint32_t time,
								   int32_t type) {
	Client *tc = NULL;
	float delta_x, delta_y;
	Client *next = NULL;
	Client *prev = NULL;
	Client *nextnext = NULL;
	Client *prevprev = NULL;
	struct wl_list *node;
	bool begin_find_nextnext = false;
	bool begin_find_prevprev = false;

	/* Finds next / nextnext. */
	for (node = gc->link.next; node != &server.clients; node = node->next) {
		tc = wl_container_of(node, tc, link);
		if (begin_find_nextnext && VISIBLEON(tc, gc->mon) && ISTILED(tc)) {
			nextnext = tc;
			break;
		}
		if (!begin_find_nextnext && VISIBLEON(tc, gc->mon) && ISTILED(tc)) {
			next = tc;
			begin_find_nextnext = true;
			continue;
		}
	}

	/* Finds prev / prevprev. */
	for (node = gc->link.prev; node != &server.clients; node = node->prev) {
		tc = wl_container_of(node, tc, link);
		if (begin_find_prevprev && VISIBLEON(tc, gc->mon) && ISTILED(tc)) {
			prevprev = tc;
			break;
		}
		if (!begin_find_prevprev && VISIBLEON(tc, gc->mon) && ISTILED(tc)) {
			prev = tc;
			begin_find_prevprev = true;
			continue;
		}
	}

	if (!server.start_drag_window && isdrag) {
		server.drag_begin_cursor_x = server.cursor->x;
		server.drag_begin_cursor_y = server.cursor->y;
		server.start_drag_window = true;
		gc->old_master_mfact_per = gc->master_mfact_per;
		gc->old_master_inner_per = gc->master_inner_per;
		gc->old_stack_inner_per = gc->stack_inner_per;
		gc->cursor_in_upper_half =
			server.cursor->y < gc->geom.y + gc->geom.height / 2;
		gc->cursor_in_left_half =
			server.cursor->x < gc->geom.x + gc->geom.width / 2;
		gc->drag_begin_geom = gc->geom;
	} else {
		if (isdrag) {
			offsetx = server.cursor->x - server.drag_begin_cursor_x;
			offsety = server.cursor->y - server.drag_begin_cursor_y;
		} else {
			gc->old_master_mfact_per = gc->master_mfact_per;
			gc->old_master_inner_per = gc->master_inner_per;
			gc->old_stack_inner_per = gc->stack_inner_per;
			gc->drag_begin_geom = gc->geom;
			gc->cursor_in_upper_half = true;
			gc->cursor_in_left_half = false;
		}

		if (gc->ismaster) {
			delta_x = (float)(offsetx) * (gc->old_master_mfact_per) /
					  gc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (gc->old_master_inner_per) /
					  gc->drag_begin_geom.height;
		} else {
			delta_x = (float)(offsetx) * (1 - gc->old_master_mfact_per) /
					  gc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (gc->old_stack_inner_per) /
					  gc->drag_begin_geom.height;
		}

		bool moving_up, moving_down;
		if (!isdrag) {
			moving_up = offsety < 0;
			moving_down = offsety > 0;
		} else {
			moving_up = server.cursor->y < server.drag_begin_cursor_y;
			moving_down = server.cursor->y > server.drag_begin_cursor_y;
		}

		if (gc->ismaster && !prev) {
			if (moving_up)
				delta_y = -fabsf(delta_y);
			else
				delta_y = fabsf(delta_y);
		} else if (gc->ismaster && next && !next->ismaster) {
			if (moving_up)
				delta_y = fabsf(delta_y);
			else
				delta_y = -fabsf(delta_y);
		} else if (!gc->ismaster && prev && prev->ismaster) {
			if (moving_up)
				delta_y = -fabsf(delta_y);
			else
				delta_y = fabsf(delta_y);
		} else if (!gc->ismaster && !next) {
			if (moving_up)
				delta_y = fabsf(delta_y);
			else
				delta_y = -fabsf(delta_y);
		} else if (type == CENTER_TILE && !gc->ismaster && !nextnext) {
			if (moving_up)
				delta_y = fabsf(delta_y);
			else
				delta_y = -fabsf(delta_y);
		} else if (type == CENTER_TILE && !gc->ismaster && prevprev &&
				   prevprev->ismaster) {
			if (moving_up)
				delta_y = -fabsf(delta_y);
			else
				delta_y = fabsf(delta_y);
		} else if ((gc->cursor_in_upper_half && moving_up) ||
				   (!gc->cursor_in_upper_half && moving_down)) {
			delta_y = fabsf(delta_y) * 2;
		} else {
			delta_y = -fabsf(delta_y) * 2;
		}

		if (!gc->ismaster && gc->isleftstack && type == CENTER_TILE)
			delta_x = delta_x * -1.0f;
		if (gc->ismaster && type == CENTER_TILE && gc->cursor_in_left_half)
			delta_x = delta_x * -1.0f;
		if (gc->ismaster && type == CENTER_TILE)
			delta_x = delta_x * 2;
		if (type == RIGHT_TILE)
			delta_x = delta_x * -1.0f;

		float new_master_mfact_per = gc->old_master_mfact_per + delta_x;
		float new_master_inner_per = gc->old_master_inner_per + delta_y;
		float new_stack_inner_per = gc->old_stack_inner_per + delta_y;

		new_master_mfact_per = fmaxf(0.1f, fminf(0.9f, new_master_mfact_per));
		new_master_inner_per = fmaxf(0.1f, fminf(0.9f, new_master_inner_per));
		new_stack_inner_per = fmaxf(0.1f, fminf(0.9f, new_stack_inner_per));

		// Scales other windows in the same group in real time, keeping the
		// group sum at 1; otherwise the added ratio is not the ratio after
		// arrangement.
		if (isdrag) {
			if (gc->ismaster) {
				/* Master window group: adjusts master_inner_per of all master
				 * windows. */
				float cur_other_sum = 1.0f - gc->master_inner_per;
				float new_other_sum = 1.0f - new_master_inner_per;
				if (cur_other_sum > 0.001f) {
					float scale = new_other_sum / cur_other_sum;
					wl_list_for_each(tc, &server.clients, link) {
						if (VISIBLEON(tc, gc->mon) && ISTILED(tc) &&
							tc->ismaster && tc != gc)
							tc->master_inner_per *= scale;
					}
				}
			} else {
				/* Stack window group: handled separately per layout type. */
				if (type == CENTER_TILE) {
					/* Scales only the stack_inner_per of same-side stack
					 * windows. */
					float cur_other_sum = 1.0f - gc->stack_inner_per;
					float new_other_sum = 1.0f - new_stack_inner_per;
					if (cur_other_sum > 0.001f) {
						float scale = new_other_sum / cur_other_sum;
						wl_list_for_each(tc, &server.clients, link) {
							if (VISIBLEON(tc, gc->mon) && ISTILED(tc) &&
								!tc->ismaster && tc != gc &&
								tc->isleftstack == gc->isleftstack)
								tc->stack_inner_per *= scale;
						}
					}
				} else {
					/* TILE / RIGHT_TILE / DECK: all stack windows share one
					 * ratio group. */
					float cur_other_sum = 1.0f - gc->stack_inner_per;
					float new_other_sum = 1.0f - new_stack_inner_per;
					if (cur_other_sum > 0.001f) {
						float scale = new_other_sum / cur_other_sum;
						wl_list_for_each(tc, &server.clients, link) {
							if (VISIBLEON(tc, gc->mon) && ISTILED(tc) &&
								!tc->ismaster && tc != gc)
								tc->stack_inner_per *= scale;
						}
					}
				}
			}
		} else {
			/* Keyboard stepping. */
			wl_list_for_each(tc, &server.clients, link) {
				if (!VISIBLEON(tc, gc->mon) || !ISTILED(tc))
					continue;
				if (tc != gc) {
					if (!tc->ismaster && new_stack_inner_per != 1.0f &&
						gc->old_stack_inner_per != 1.0f &&
						(type != CENTER_TILE ||
						 !(gc->isleftstack ^ tc->isleftstack)))
						tc->stack_inner_per = (1 - new_stack_inner_per) /
											  (1 - gc->old_stack_inner_per) *
											  tc->stack_inner_per;
					if (tc->ismaster && new_master_inner_per != 1.0f &&
						gc->old_master_inner_per != 1.0f)
						tc->master_inner_per =
							(1.0f - new_master_inner_per) /
							(1.0f - gc->old_master_inner_per) *
							tc->master_inner_per;
				}
			}
		}

		/* Applies the new ratio to the grabbed window itself. */
		gc->master_inner_per = new_master_inner_per;
		gc->stack_inner_per = new_stack_inner_per;

		/* Broadcasts master_mfact_per to all tiled windows. */
		wl_list_for_each(tc, &server.clients, link) {
			if (VISIBLEON(tc, gc->mon) && ISTILED(tc))
				tc->master_mfact_per = new_master_mfact_per;
		}

		if (!isdrag) {
			arrange(gc->mon, false, false);
			return;
		}

		if (server.last_apply_drag_time == 0 ||
			time - server.last_apply_drag_time >
				config.drag_tile_refresh_interval) {
			arrange(gc->mon, false, false);
			server.last_apply_drag_time = time;
		}
	}
}

void resize_tile_master_vertical(Client *gc, bool isdrag, int32_t offsetx,
								 int32_t offsety, uint32_t time, int32_t type) {
	Client *tc = NULL;
	float delta_x, delta_y;
	Client *next = NULL;
	Client *prev = NULL;
	struct wl_list *node;

	/* Finds next. */
	for (node = gc->link.next; node != &server.clients; node = node->next) {
		tc = wl_container_of(node, tc, link);
		if (VISIBLEON(tc, gc->mon) && ISTILED(tc)) {
			next = tc;
			break;
		}
	}

	/* Finds prev. */
	for (node = gc->link.prev; node != &server.clients; node = node->prev) {
		tc = wl_container_of(node, tc, link);
		if (VISIBLEON(tc, gc->mon) && ISTILED(tc)) {
			prev = tc;
			break;
		}
	}

	if (!server.start_drag_window && isdrag) {
		server.drag_begin_cursor_x = server.cursor->x;
		server.drag_begin_cursor_y = server.cursor->y;
		server.start_drag_window = true;
		gc->old_master_mfact_per = gc->master_mfact_per;
		gc->old_master_inner_per = gc->master_inner_per;
		gc->old_stack_inner_per = gc->stack_inner_per;
		gc->cursor_in_upper_half =
			server.cursor->y < gc->geom.y + gc->geom.height / 2;
		gc->cursor_in_left_half =
			server.cursor->x < gc->geom.x + gc->geom.width / 2;
		gc->drag_begin_geom = gc->geom;
	} else {
		if (isdrag) {
			offsetx = server.cursor->x - server.drag_begin_cursor_x;
			offsety = server.cursor->y - server.drag_begin_cursor_y;
		} else {
			gc->old_master_mfact_per = gc->master_mfact_per;
			gc->old_master_inner_per = gc->master_inner_per;
			gc->old_stack_inner_per = gc->stack_inner_per;
			gc->drag_begin_geom = gc->geom;
			gc->cursor_in_upper_half = true;
			gc->cursor_in_left_half = false;
		}

		if (gc->ismaster) {
			delta_x = (float)(offsetx) * (gc->old_master_inner_per) /
					  gc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (gc->old_master_mfact_per) /
					  gc->drag_begin_geom.height;
		} else {
			delta_x = (float)(offsetx) * (gc->old_stack_inner_per) /
					  gc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (1 - gc->old_master_mfact_per) /
					  gc->drag_begin_geom.height;
		}

		bool moving_left, moving_right;
		if (!isdrag) {
			moving_left = offsetx < 0;
			moving_right = offsetx > 0;
		} else {
			moving_left = server.cursor->x < server.drag_begin_cursor_x;
			moving_right = server.cursor->x > server.drag_begin_cursor_x;
		}

		if (gc->ismaster && !prev) {
			if (moving_left)
				delta_x = -fabsf(delta_x);
			else
				delta_x = fabsf(delta_x);
		} else if (gc->ismaster && next && !next->ismaster) {
			if (moving_left)
				delta_x = fabsf(delta_x);
			else
				delta_x = -fabsf(delta_x);
		} else if (!gc->ismaster && prev && prev->ismaster) {
			if (moving_left)
				delta_x = -fabsf(delta_x);
			else
				delta_x = fabsf(delta_x);
		} else if (!gc->ismaster && !next) {
			if (moving_left)
				delta_x = fabsf(delta_x);
			else
				delta_x = -fabsf(delta_x);
		} else if ((gc->cursor_in_left_half && moving_left) ||
				   (!gc->cursor_in_left_half && moving_right)) {
			delta_x = fabsf(delta_x) * 2;
		} else {
			delta_x = -fabsf(delta_x) * 2;
		}

		float new_master_mfact_per = gc->old_master_mfact_per + delta_y;
		float new_master_inner_per = gc->old_master_inner_per + delta_x;
		float new_stack_inner_per = gc->old_stack_inner_per + delta_x;

		new_master_mfact_per = fmaxf(0.1f, fminf(0.9f, new_master_mfact_per));
		new_master_inner_per = fmaxf(0.1f, fminf(0.9f, new_master_inner_per));
		new_stack_inner_per = fmaxf(0.1f, fminf(0.9f, new_stack_inner_per));

		// Scales other windows in the same group in real time, keeping the
		// group sum at 1; otherwise the added ratio is not the ratio after
		// arrangement.

		if (isdrag) {
			if (gc->ismaster) {
				float cur_other_sum = 1.0f - gc->master_inner_per;
				float new_other_sum = 1.0f - new_master_inner_per;
				if (cur_other_sum > 0.001f) {
					float scale = new_other_sum / cur_other_sum;
					wl_list_for_each(tc, &server.clients, link) {
						if (VISIBLEON(tc, gc->mon) && ISTILED(tc) &&
							tc->ismaster && tc != gc)
							tc->master_inner_per *= scale;
					}
				}
			} else {
				/* All stack windows (vertical layouts have no left/right
				 * distinction). */
				float cur_other_sum = 1.0f - gc->stack_inner_per;
				float new_other_sum = 1.0f - new_stack_inner_per;
				if (cur_other_sum > 0.001f) {
					float scale = new_other_sum / cur_other_sum;
					wl_list_for_each(tc, &server.clients, link) {
						if (VISIBLEON(tc, gc->mon) && ISTILED(tc) &&
							!tc->ismaster && tc != gc)
							tc->stack_inner_per *= scale;
					}
				}
			}
		} else {
			/* Keyboard stepping. */
			wl_list_for_each(tc, &server.clients, link) {
				if (!VISIBLEON(tc, gc->mon) || !ISTILED(tc))
					continue;
				if (tc != gc) {
					if (!tc->ismaster && new_stack_inner_per != 1.0f &&
						gc->old_stack_inner_per != 1.0f)
						tc->stack_inner_per = (1 - new_stack_inner_per) /
											  (1 - gc->old_stack_inner_per) *
											  tc->stack_inner_per;
					if (tc->ismaster && new_master_inner_per != 1.0f &&
						gc->old_master_inner_per != 1.0f)
						tc->master_inner_per =
							(1.0f - new_master_inner_per) /
							(1.0f - gc->old_master_inner_per) *
							tc->master_inner_per;
				}
			}
		}

		gc->master_inner_per = new_master_inner_per;
		gc->stack_inner_per = new_stack_inner_per;

		/* Broadcasts master_mfact_per. */
		wl_list_for_each(tc, &server.clients, link) {
			if (VISIBLEON(tc, gc->mon) && ISTILED(tc))
				tc->master_mfact_per = new_master_mfact_per;
		}

		if (!isdrag) {
			arrange(gc->mon, false, false);
			return;
		}

		if (server.last_apply_drag_time == 0 ||
			time - server.last_apply_drag_time >
				config.drag_tile_refresh_interval) {
			arrange(gc->mon, false, false);
			server.last_apply_drag_time = time;
		}
	}
}

void resize_tile_dwindle(Client *gc, bool isdrag, int32_t offsetx,
						 int32_t offsety, uint32_t time, bool isvertical) {

	if (!isdrag) {
		dwindle_resize_client_step(gc->mon, gc, offsetx, offsety);
		return;
	}

	if (server.last_apply_drag_time == 0 ||
		time - server.last_apply_drag_time >
			config.drag_tile_refresh_interval) {
		dwindle_resize_client(gc->mon, gc);
		server.last_apply_drag_time = time;
	}
}

void resize_tile_grid_fair(Client *gc, bool isdrag, int32_t offsetx,
						   int32_t offsety, uint32_t time) {
	if (!gc || gc->isfullscreen || gc->ismaximizescreen)
		return;
	Monitor *m = gc->mon;
	if (m->isoverview)
		return;

	if (m->visible_tiling_clients <= 1)
		return;

	// Gets the current layout ID.
	const Layout *current_layout = m->pertag->ltidxs[get_mon_curtag(m)];

	if (!server.start_drag_window && isdrag) {
		server.drag_begin_cursor_x = server.cursor->x;
		server.drag_begin_cursor_y = server.cursor->y;
		server.start_drag_window = true;

		Client *c;
		wl_list_for_each(c, &server.clients, link) {
			c->old_grid_col_per =
				(c->grid_col_per > 0.0f) ? c->grid_col_per : 1.0f;
			c->old_grid_row_per =
				(c->grid_row_per > 0.0f) ? c->grid_row_per : 1.0f;
		}

		gc->old_grid_col_per = gc->grid_col_per;
		gc->old_grid_row_per = gc->grid_row_per;

		gc->cursor_in_left_half =
			server.cursor->x < gc->geom.x + gc->geom.width / 2;
		gc->cursor_in_upper_half =
			server.cursor->y < gc->geom.y + gc->geom.height / 2;
		gc->drag_begin_geom = gc->geom;
	} else {
		if (isdrag) {
			offsetx = server.cursor->x - server.drag_begin_cursor_x;
			offsety = server.cursor->y - server.drag_begin_cursor_y;
		} else {
			gc->drag_begin_geom = gc->geom;
			Client *c;
			wl_list_for_each(c, &server.clients, link) {
				c->old_grid_col_per =
					(c->grid_col_per > 0.0f) ? c->grid_col_per : 1.0f;
				c->old_grid_row_per =
					(c->grid_row_per > 0.0f) ? c->grid_row_per : 1.0f;
			}
			gc->cursor_in_upper_half = false;
			gc->cursor_in_left_half = false;
		}

		// Computes the ratio delta based on screen resolution.
		float delta_x =
			(float)offsetx * gc->old_grid_col_per / gc->drag_begin_geom.width;
		float delta_y =
			(float)offsety * gc->old_grid_row_per / gc->drag_begin_geom.height;

		int adj_c_idx = gc->grid_col_idx;
		int adj_r_idx = gc->grid_row_idx;
		float sign_x = 1.0f, sign_y = 1.0f;

		if (isdrag) {
			if (gc->cursor_in_left_half) {
				adj_c_idx -= 1;
				sign_x = -1.0f;
			} else {
				adj_c_idx += 1;
				sign_x = 1.0f;
			}

			if (gc->cursor_in_upper_half) {
				adj_r_idx -= 1;
				sign_y = -1.0f;
			} else {
				adj_r_idx += 1;
				sign_y = 1.0f;
			}
		}
		// Keyboard hotkey logic unchanged.
		int max_col = -1, max_row = -1, min_col = INT32_MAX,
			min_row = INT32_MAX;
		Client *tmp;
		wl_list_for_each(tmp, &server.clients, link) {
			if (tmp->mon != m || !VISIBLEON(tmp, m) || !ISTILED(tmp))
				continue;
			if (tmp->grid_col_idx > max_col)
				max_col = tmp->grid_col_idx;
			if (tmp->grid_row_idx > max_row)
				max_row = tmp->grid_row_idx;
			if (tmp->grid_col_idx < min_col)
				min_col = tmp->grid_col_idx;
			if (tmp->grid_row_idx < min_row)
				min_row = tmp->grid_row_idx;
		}

		adj_c_idx = gc->grid_col_idx + 1;
		adj_r_idx = gc->grid_row_idx + 1;
		sign_x = 1.0f;
		sign_y = 1.0f;

		if (gc->grid_col_idx == max_col) {
			adj_c_idx = gc->grid_col_idx - 1;
			sign_x = -1.0f;
		}
		if (gc->grid_row_idx == max_row) {
			adj_r_idx = gc->grid_row_idx - 1;
			sign_y = -1.0f;
		}
		if (gc->grid_col_idx == min_col) {
			adj_c_idx = gc->grid_col_idx + 1;
			sign_x = 1.0f;
		}
		if (gc->grid_row_idx == min_row) {
			adj_r_idx = gc->grid_row_idx + 1;
			sign_y = 1.0f;
		}

		float dx = delta_x * sign_x;
		float dy = delta_y * sign_y;

		float my_old_col = gc->old_grid_col_per;
		float my_old_row = gc->old_grid_row_per;
		float adj_old_col = -1.0f, adj_old_row = -1.0f;

		Client *c;
		wl_list_for_each(c, &server.clients, link) {
			if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
				continue;
			if (c->grid_col_idx == adj_c_idx && adj_old_col < 0)
				adj_old_col = c->old_grid_col_per;
			if (c->grid_row_idx == adj_r_idx && adj_old_row < 0)
				adj_old_row = c->old_grid_row_per;
		}

		// Applies the column-width adjustment.
		if (adj_old_col > 0.0f) {
			float dx_clamped = dx;
			if (my_old_col + dx_clamped < 0.1f)
				dx_clamped = 0.1f - my_old_col;
			if (adj_old_col - dx_clamped < 0.1f)
				dx_clamped = adj_old_col - 0.1f;

			float new_my_col = my_old_col + dx_clamped;
			float new_adj_col = adj_old_col - dx_clamped;

			// Handles column boundaries forced to stay locked at 1.0f where the
			// head is a misplaced window.
			if (current_layout && current_layout->id == VERTICAL_FAIR) {
				int32_t n_tiling = m->visible_tiling_clients;
				int32_t l_rows;
				for (l_rows = 0; l_rows <= n_tiling; l_rows++) {
					if (l_rows * l_rows >= n_tiling)
						break;
				}
				int32_t base_cols = n_tiling / l_rows;
				// When the adjusted boundary is at an asymmetric locked column
				// (e.g. between col 0 and col 1 with 3 windows).
				if ((gc->grid_col_idx == base_cols - 1 &&
					 adj_c_idx == base_cols) ||
					(gc->grid_col_idx == base_cols &&
					 adj_c_idx == base_cols - 1)) {

					float p_col =
						(gc->grid_col_idx == base_cols - 1)
							? (my_old_col + dx) / (my_old_col + adj_old_col)
							: (adj_old_col - dx) / (my_old_col + adj_old_col);
					if (p_col < 0.01f)
						p_col = 0.01f;
					if (p_col > 0.99f)
						p_col = 0.99f;

					// Backs out the non-linear real weight value.
					float new_r_var_per = p_col / (1.0f - p_col);
					if (new_r_var_per < 0.1f)
						new_r_var_per = 0.1f;
					if (new_r_var_per > 10.0f)
						new_r_var_per = 10.0f;

					if (gc->grid_col_idx == base_cols - 1) {
						new_my_col = new_r_var_per;
						new_adj_col = 1.0f;
					} else {
						new_my_col = 1.0f;
						new_adj_col = new_r_var_per;
					}
				}
			}

			wl_list_for_each(c, &server.clients, link) {
				if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
					continue;
				if (c->grid_col_idx == gc->grid_col_idx)
					c->grid_col_per = new_my_col;
				if (c->grid_col_idx == adj_c_idx)
					c->grid_col_per = new_adj_col;
			}

			wl_list_for_each(c, &server.clients, link) {
				if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
					continue;
				if (c->grid_row_idx == 0) {
					if (c->grid_col_idx == gc->grid_col_idx)
						c->grid_col_per = new_my_col;
					else if (c->grid_col_idx == adj_c_idx)
						c->grid_col_per = new_adj_col;
				}
			}
		}

		// Applies the row-height adjustment.
		if (adj_old_row > 0.0f) {
			float dy_clamped = dy;
			if (my_old_row + dy_clamped < 0.1f)
				dy_clamped = 0.1f - my_old_row;
			if (adj_old_row - dy_clamped < 0.1f)
				dy_clamped = adj_old_row - 0.1f;

			float new_my_row = my_old_row + dy_clamped;
			float new_adj_row = adj_old_row - dy_clamped;

			// Handles row boundaries forced to stay locked at 1.0f where the
			// head is a misplaced window.
			if (current_layout && current_layout->id == FAIR) {
				int32_t n_tiling = m->visible_tiling_clients;
				int32_t l_cols;
				for (l_cols = 0; l_cols <= n_tiling; l_cols++) {
					if (l_cols * l_cols >= n_tiling)
						break;
				}
				int32_t base_rows = n_tiling / l_cols;
				// When the adjusted boundary is at an asymmetric locked row
				// (e.g. between row 0 and row 1 with 3 windows).
				if ((gc->grid_row_idx == base_rows - 1 &&
					 adj_r_idx == base_rows) ||
					(gc->grid_row_idx == base_rows &&
					 adj_r_idx == base_rows - 1)) {

					float p_row =
						(gc->grid_row_idx == base_rows - 1)
							? (my_old_row + dy) / (my_old_row + adj_old_row)
							: (adj_old_row - dy) / (my_old_row + adj_old_row);
					if (p_row < 0.01f)
						p_row = 0.01f;
					if (p_row > 0.99f)
						p_row = 0.99f;

					// Backs out the non-linear real weight value.
					float new_r_var_per = p_row / (1.0f - p_row);
					if (new_r_var_per < 0.1f)
						new_r_var_per = 0.1f;
					if (new_r_var_per > 10.0f)
						new_r_var_per = 10.0f;

					if (gc->grid_row_idx == base_rows - 1) {
						new_my_row = new_r_var_per;
						new_adj_row = 1.0f;
					} else {
						new_my_row = 1.0f;
						new_adj_row = new_r_var_per;
					}
				}
			}

			wl_list_for_each(c, &server.clients, link) {
				if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
					continue;
				if (c->grid_row_idx == gc->grid_row_idx)
					c->grid_row_per = new_my_row;
				if (c->grid_row_idx == adj_r_idx)
					c->grid_row_per = new_adj_row;
			}

			wl_list_for_each(c, &server.clients, link) {
				if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
					continue;
				if (c->grid_col_idx == 0) {
					if (c->grid_row_idx == gc->grid_row_idx)
						c->grid_row_per = new_my_row;
					else if (c->grid_row_idx == adj_r_idx)
						c->grid_row_per = new_adj_row;
				}
			}
		}

		if (!isdrag) {
			arrange(m, false, false);
			return;
		}

		if (server.last_apply_drag_time == 0 ||
			time - server.last_apply_drag_time >
				config.drag_tile_refresh_interval) {
			arrange(m, false, false);
			server.last_apply_drag_time = time;
		}
	}
}

void resize_tile_scroller(Client *gc, bool isdrag, int32_t offsetx,
						  int32_t offsety, uint32_t time, bool isvertical) {

	if (!gc || gc->isfullscreen || gc->ismaximizescreen || !gc->mon)
		return;
	if (gc->mon->isoverview)
		return;

	Monitor *m = gc->mon;
	uint32_t tag = get_client_tag_idx(gc);
	struct TagScrollerState *st = m->pertag->scroller_state[tag];
	if (!st)
		return;

	struct ScrollerStackNode *curnode = find_scroller_node(st, gc);
	if (!curnode)
		return;

	struct ScrollerStackNode *headnode = curnode;
	while (headnode->prev_in_stack)
		headnode = headnode->prev_in_stack;

	Client *stack_head_client = headnode->client;

	float delta_x, delta_y;
	float new_scroller_proportion;
	float new_stack_proportion;

	if (!server.start_drag_window && isdrag) {
		server.drag_begin_cursor_x = server.cursor->x;
		server.drag_begin_cursor_y = server.cursor->y;
		server.start_drag_window = true;

		headnode->client->old_scroller_pproportion =
			headnode->scroller_proportion;
		gc->old_stack_proportion = curnode->stack_proportion;

		gc->cursor_in_left_half =
			server.cursor->x < gc->geom.x + gc->geom.width / 2;
		gc->cursor_in_upper_half =
			server.cursor->y < gc->geom.y + gc->geom.height / 2;
		gc->drag_begin_geom = gc->geom;
	} else {
		if (isdrag) {
			offsetx = server.cursor->x - server.drag_begin_cursor_x;
			offsety = server.cursor->y - server.drag_begin_cursor_y;
		} else {
			gc->old_master_mfact_per = gc->master_mfact_per;
			gc->old_master_inner_per = gc->master_inner_per;
			gc->old_stack_inner_per = gc->stack_inner_per;
			gc->drag_begin_geom = gc->geom;
			stack_head_client->old_scroller_pproportion =
				headnode->scroller_proportion;
			gc->old_stack_proportion = curnode->stack_proportion;
			gc->cursor_in_upper_half = false;
			gc->cursor_in_left_half = false;
		}

		if (isvertical) {
			delta_y = (float)(offsety) *
					  (headnode->client->old_scroller_pproportion) /
					  gc->drag_begin_geom.height;
			delta_x = (float)(offsetx) * (gc->old_stack_proportion) /
					  gc->drag_begin_geom.width;
		} else {
			delta_x = (float)(offsetx) *
					  (headnode->client->old_scroller_pproportion) /
					  gc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (gc->old_stack_proportion) /
					  gc->drag_begin_geom.height;
		}

		bool moving_up, moving_down, moving_left, moving_right;
		if (!isdrag) {
			moving_up = offsety < 0;
			moving_down = offsety > 0;
			moving_left = offsetx < 0;
			moving_right = offsetx > 0;
		} else {
			moving_up = server.cursor->y < server.drag_begin_cursor_y;
			moving_down = server.cursor->y > server.drag_begin_cursor_y;
			moving_left = server.cursor->x < server.drag_begin_cursor_x;
			moving_right = server.cursor->x > server.drag_begin_cursor_x;
		}

		if ((gc->cursor_in_upper_half && moving_up) ||
			(!gc->cursor_in_upper_half && moving_down)) {
			delta_y = fabsf(delta_y);
		} else {
			delta_y = -fabsf(delta_y);
		}

		if ((gc->cursor_in_left_half && moving_left) ||
			(!gc->cursor_in_left_half && moving_right)) {
			delta_x = fabsf(delta_x);
		} else {
			delta_x = -fabsf(delta_x);
		}

		if (isvertical) {
			if (!curnode->next_in_stack && curnode->prev_in_stack && !isdrag) {
				delta_x = delta_x * -1.0f;
			}
			if (!curnode->next_in_stack && curnode->prev_in_stack && isdrag) {
				if (moving_right)
					delta_x = -fabsf(delta_x);
				else
					delta_x = fabsf(delta_x);
			}
			if (!curnode->prev_in_stack && curnode->next_in_stack && isdrag) {
				if (moving_left)
					delta_x = -fabsf(delta_x);
				else
					delta_x = fabsf(delta_x);
			}
			if (isdrag) {
				if (moving_up)
					delta_y = -fabsf(delta_y);
				else
					delta_y = fabsf(delta_y);
			}
		} else {
			if (!curnode->next_in_stack && curnode->prev_in_stack && !isdrag) {
				delta_y = delta_y * -1.0f;
			}
			if (!curnode->next_in_stack && curnode->prev_in_stack && isdrag) {
				if (moving_down)
					delta_y = -fabsf(delta_y);
				else
					delta_y = fabsf(delta_y);
			}
			if (!curnode->prev_in_stack && curnode->next_in_stack && isdrag) {
				if (moving_up)
					delta_y = -fabsf(delta_y);
				else
					delta_y = fabsf(delta_y);
			}
			if (isdrag) {
				if (moving_left)
					delta_x = -fabsf(delta_x);
				else
					delta_x = fabsf(delta_x);
			}
		}

		if (isvertical) {
			new_scroller_proportion =
				headnode->client->old_scroller_pproportion + delta_y;
			new_stack_proportion = gc->old_stack_proportion + delta_x;
		} else {
			new_scroller_proportion =
				headnode->client->old_scroller_pproportion + delta_x;
			new_stack_proportion = gc->old_stack_proportion + delta_y;
		}

		new_scroller_proportion =
			fmaxf(0.1f, fminf(1.0f, new_scroller_proportion));
		new_stack_proportion = fmaxf(0.1f, fminf(0.9f, new_stack_proportion));

		// Keeps the sum at 1 so later arrange normalization does not swallow
		// the offset.
		if (isdrag) {
			float current_other_sum = 1.0f - curnode->stack_proportion;
			float new_other_sum = 1.0f - new_stack_proportion;
			if (current_other_sum > 0.001f) {
				float scale = new_other_sum / current_other_sum;
				for (struct ScrollerStackNode *tc = headnode; tc;
					 tc = tc->next_in_stack) {
					if (tc != curnode) {
						tc->stack_proportion *= scale;
					}
				}
			}
		} else {
			// Keyboard stepping.
			if (gc->old_stack_proportion != 1.0f) {
				for (struct ScrollerStackNode *tc = headnode; tc;
					 tc = tc->next_in_stack) {
					if (tc != curnode) {
						tc->stack_proportion =
							(1.0f - new_stack_proportion) /
							(1.0f - gc->old_stack_proportion) *
							tc->stack_proportion;
					}
				}
			}
		}

		curnode->stack_proportion = new_stack_proportion;

		if (m->visible_scroll_tiling_clients > 1 ||
			config.scroller_ignore_proportion_single) {
			headnode->scroller_proportion = new_scroller_proportion;
		}

		/* Syncs back to the global fields. */
		sync_scroller_state_to_clients(m, tag);

		if (!isdrag) {
			arrange(m, false, false);
			return;
		}

		if (server.last_apply_drag_time == 0 ||
			time - server.last_apply_drag_time >
				config.drag_tile_refresh_interval) {
			arrange(m, false, false);
			server.last_apply_drag_time = time;
		}
	}
}

void resize_tile_client(Client *gc, bool isdrag, int32_t offsetx,
						int32_t offsety, uint32_t time) {

	if (!gc || gc->isfullscreen || gc->ismaximizescreen || !gc->mon)
		return;

	if (gc->mon->isoverview)
		return;

	const Layout *current_layout =
		gc->mon->pertag->ltidxs[get_client_tag_idx(gc)];
	if (current_layout->id == TILE || current_layout->id == DECK ||
		current_layout->id == CENTER_TILE || current_layout->id == RIGHT_TILE

	) {
		resize_tile_master_horizontal(gc, isdrag, offsetx, offsety, time,
									  current_layout->id);
	} else if (current_layout->id == VERTICAL_TILE ||
			   current_layout->id == VERTICAL_DECK) {
		resize_tile_master_vertical(gc, isdrag, offsetx, offsety, time,
									current_layout->id);
	} else if (current_layout->id == SCROLLER) {
		resize_tile_scroller(gc, isdrag, offsetx, offsety, time, false);
	} else if (current_layout->id == VERTICAL_SCROLLER) {
		resize_tile_scroller(gc, isdrag, offsetx, offsety, time, true);
	} else if (current_layout->id == DWINDLE) {
		resize_tile_dwindle(gc, isdrag, offsetx, offsety, time, true);
	} else if (current_layout->id == GRID ||
			   current_layout->id == VERTICAL_GRID ||
			   current_layout->id == FAIR ||
			   current_layout->id == VERTICAL_FAIR) {
		resize_tile_grid_fair(gc, isdrag, offsetx, offsety, time);
	}
}

/* If there are no calculation omissions,
these two functions will never be triggered.
Just in case to facilitate the final investigation*/

void check_size_per_valid(Client *c) {
	if (c->ismaster) {
		assert(c->master_inner_per > 0.0f && c->master_inner_per <= 1.0f);
	} else {
		assert(c->stack_inner_per > 0.0f && c->stack_inner_per <= 1.0f);
	}
}

bool special_keep_bg_client(Monitor *m, Client *c) {
	return is_special_active(m) && !c->is_logic_hide && !c->isminimized &&
		   ((m->pertag->prevtag > 0 &&
			 (c->tags & (1 << (m->pertag->prevtag - 1)))) ||
			(c->tags & (m->tagset[m->seltags ^ 1] & ~TAG0_MASK)));
}
void reset_size_per_mon(Monitor *m, int32_t tile_cilent_num,
						double total_left_stack_hight_percent,
						double total_right_stack_hight_percent,
						double total_stack_hight_percent,
						double total_master_inner_percent, int32_t master_num,
						int32_t stack_num) {
	Client *c = NULL;
	int32_t i = 0;
	uint32_t stack_index = 0;
	uint32_t tag = get_mon_curtag(m);
	uint32_t nmasters = m->pertag->nmasters[tag];

	if (m->pertag->ltidxs[tag]->id != CENTER_TILE) {

		wl_list_for_each(c, &server.clients, link) {
			if (VISIBLEON(c, m) && ISFAKETILED(c)) {

				if (total_master_inner_percent > 0.0 && i < nmasters) {
					c->ismaster = true;
					c->stack_inner_per = stack_num ? 1.0f / stack_num : 1.0f;
					c->master_inner_per =
						c->master_inner_per / total_master_inner_percent;
				} else {
					c->ismaster = false;
					c->master_inner_per =
						master_num > 0 ? 1.0f / master_num : 1.0f;
					c->stack_inner_per =
						total_stack_hight_percent
							? c->stack_inner_per / total_stack_hight_percent
							: 1.0f;
				}
				i++;

				check_size_per_valid(c);
			}
		}
	} else {
		wl_list_for_each(c, &server.clients, link) {
			if (VISIBLEON(c, m) && ISFAKETILED(c)) {

				if (total_master_inner_percent > 0.0 && i < nmasters) {
					c->ismaster = true;
					if ((stack_index % 2) ^ (tile_cilent_num % 2 == 0)) {
						c->stack_inner_per =
							stack_num > 1 ? 1.0f / ((stack_num - 1) / 2.0f)
										  : 1.0f;
					} else {
						c->stack_inner_per =
							stack_num > 1 ? 2.0f / stack_num : 1.0f;
					}

					c->master_inner_per =
						c->master_inner_per / total_master_inner_percent;
				} else {
					stack_index = i - nmasters;

					c->ismaster = false;
					c->master_inner_per =
						master_num > 0 ? 1.0f / master_num : 1.0f;
					if ((stack_index % 2) ^ (tile_cilent_num % 2 == 0)) {
						c->stack_inner_per =
							total_right_stack_hight_percent
								? c->stack_inner_per /
									  total_right_stack_hight_percent
								: 1.0f;
					} else {
						c->stack_inner_per =
							total_left_stack_hight_percent
								? c->stack_inner_per /
									  total_left_stack_hight_percent
								: 1.0f;
					}
				}
				i++;

				check_size_per_valid(c);
			}
		}
	}
}

void pre_calculate_before_arrange(Monitor *m, bool want_animation,
								  bool from_view, bool only_calculate) {
	Client *c = NULL;
	double total_stack_inner_percent = 0;
	double total_master_inner_percent = 0;
	double total_right_stack_hight_percent = 0;
	double total_left_stack_hight_percent = 0;
	int32_t i = 0;
	int32_t nmasters = 0;
	int32_t stack_index = 0;
	int32_t master_num = 0;
	int32_t stack_num = 0;

	m->visible_clients = 0;
	m->visible_tiling_clients = 0;
	m->visible_scroll_tiling_clients = 0;
	m->visible_fake_tiling_clients = 0;
	m->hide_clients = 0;

	uint32_t tag = get_mon_curtag(m);
	struct TagScrollerState *st = m->pertag->scroller_state[tag];

	const Layout *cur_layout = m->pertag->ltidxs[tag];
	if (cur_layout->id == SCROLLER || cur_layout->id == VERTICAL_SCROLLER) {
		update_scroller_state(m);
	}

	wl_list_for_each(c, &server.clients, link) {

		if (from_view && (c->isglobal || c->isunglobal)) {
			set_size_per(m, c);
		}

		if (m->is_jump_mode && !c->jump_label_node) {
			client_add_jump_label_node(c);
		}

		if (c->group_bar->scene_buffer->node.enabled) {
			client_check_tab_node_visible(c);
		}

		if (c->mon == m && (c->isglobal || c->isunglobal)) {
			c->tags = m->tagset[m->seltags];
		}

		if (from_view && m->sel == NULL && c->isglobal && VISIBLEON(c, m)) {
			client_focus(c, 1);
		}

		if (c->isminimized) {
			m->hide_clients++;
		}

		if (VISIBLEON(c, m)) {
			if (from_view && !client_only_in_one_tag(c)) {
				set_size_per(m, c);
			}

			if (!c->isunglobal)
				m->visible_clients++;

			if (ISTILED(c)) {
				m->visible_tiling_clients++;

				/* Updates the visible scroll-client count. */
				if (st) {
					struct ScrollerStackNode *n = find_scroller_node(st, c);
					if (n && !n->prev_in_stack) /* Is a stack head. */
						m->visible_scroll_tiling_clients++;
				} else if (ISSCROLLTILED(c)) {
					m->visible_scroll_tiling_clients++;
				}
			}

			if (ISFAKETILED(c)) {
				m->visible_fake_tiling_clients++;
			}
		}
	}

	nmasters = m->pertag->nmasters[get_mon_curtag(m)];

	wl_list_for_each(c, &server.clients, link) {
		if (c->iskilling)
			continue;

		if (c->mon == m) {
			if (VISIBLEON(c, m)) {
				if (ISFAKETILED(c)) {
					if (i < nmasters) {
						master_num++;
						total_master_inner_percent += c->master_inner_per;
					} else {
						stack_num++;
						total_stack_inner_percent += c->stack_inner_per;
						stack_index = i - nmasters;
						if ((stack_index % 2) ^
							(m->visible_tiling_clients % 2 == 0)) {
							c->isleftstack = false;
							total_right_stack_hight_percent +=
								c->stack_inner_per;
						} else {
							c->isleftstack = true;
							total_left_stack_hight_percent +=
								c->stack_inner_per;
						}
					}
					i++;
				}

				if (!only_calculate)
					set_arrange_visible(m, c, want_animation);
				if (!only_calculate)
					client_sync_layer(c);
			} else if (special_keep_bg_client(m, c)) {
				wlr_scene_node_set_enabled(&c->scene->node, true);
				c->animation.running = false;
			} else if (!only_calculate && c != server.grab_client) {
				set_arrange_hidden(m, c, want_animation);
			}
		}

		if (!only_calculate && c->mon == m && c->ismaximizescreen &&
			!c->animation.tagouted && !c->animation.tagouting &&
			VISIBLEON(c, m)) {
			reset_maximizescreen_size(c);
		}
	}

	reset_size_per_mon(
		m, m->visible_tiling_clients, total_left_stack_hight_percent,
		total_right_stack_hight_percent, total_stack_inner_percent,
		total_master_inner_percent, master_num, stack_num);

	special_update_dim(m);
}

// remap tags through map; unmapped tags stay as-is.
uint32_t tag_remap_mask(uint32_t tags, const uint32_t *map) {
	uint32_t out = tags & ~server.tagmask;
	uint32_t i;

	for (i = 1; i <= (uint32_t)config.tag_num; i++) {
		if (tags & (1u << (i - 1)))
			out |= map[i] ? (1u << (map[i] - 1)) : (1u << (i - 1));
	}
	return out;
}

// reset a pertag slot to its tagrule state.
void tag_gather_reset_slot(Monitor *m, uint32_t tag) {
	int32_t i;

	tag_slot_set_defaults(m, tag);
	m->pertag->no_hide[tag] = 0;
	m->pertag->no_render_border[tag] = 0;
	m->pertag->open_as_floating[tag] = 0;
	m->pertag->dwindle_root[tag] = NULL;
	m->pertag->scroller_state[tag] = NULL;

	for (i = 0; i < config.tag_rules_count; i++) {
		const ConfigTagRule *tr = &config.tag_rules[i];

		if (tag_rule_matches_monitor(tr, m) &&
			(tr->id_wildcard || tr->id == (int32_t)tag))
			tag_rule_apply_to_slot(m, tr, tag);
	}
}

// move pertag state from src to dst, then reset src.
void tag_gather_move_pertag(Monitor *m, uint32_t dst, uint32_t src) {
	m->pertag->nmasters[dst] = m->pertag->nmasters[src];
	m->pertag->mfacts[dst] = m->pertag->mfacts[src];
	m->pertag->no_hide[dst] = m->pertag->no_hide[src];
	m->pertag->no_render_border[dst] = m->pertag->no_render_border[src];
	m->pertag->open_as_floating[dst] = m->pertag->open_as_floating[src];
	m->pertag->scroller_default_proportion[dst] =
		m->pertag->scroller_default_proportion[src];
	m->pertag->scroller_default_proportion_single[dst] =
		m->pertag->scroller_default_proportion_single[src];
	m->pertag->scroller_ignore_proportion_single[dst] =
		m->pertag->scroller_ignore_proportion_single[src];
	m->pertag->dwindle_root[dst] = m->pertag->dwindle_root[src];
	m->pertag->ltidxs[dst] = m->pertag->ltidxs[src];
	m->pertag->scroller_state[dst] = m->pertag->scroller_state[src];
	tag_gather_reset_slot(m, src);
}

// Compact occupied tags on this monitor to 1..k (e.g. 1,3,9 -> 1,2,3).
void tag_gather_apply(Monitor *m) {
	Client *c;
	uint32_t occupied = 0;
	uint32_t map[tag_num_MAX + 1] = {0};
	uint32_t i, next = 1, k;
	bool changed = false;

	if (!m || m->iscleanuping)
		return;

	// collect occupied tags on this monitor.
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon == m && !c->iskilling && !c->is_logic_hide &&
			!(c->tags & TAG0_MASK))
			occupied |= c->tags & server.tagmask;
	}
	// the current view counts as occupied even when empty.
	occupied |= m->tagset[m->seltags] & server.tagmask;

	// old->new mapping and detect gaps.
	for (i = 1; i <= (uint32_t)config.tag_num; i++) {
		if (occupied & (1u << (i - 1))) {
			map[i] = next++;
			if (map[i] != i)
				changed = true;
		} else {
			map[i] = 0;
		}
	}

	if (!changed)
		return;

	// remap client tags.
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || c->iskilling || c->is_logic_hide ||
			(c->tags & TAG0_MASK))
			continue;
		c->tags = tag_remap_mask(c->tags, map);
	}

	// remap current/previous views (and overview backups).
	m->tagset[m->seltags] = tag_remap_mask(m->tagset[m->seltags], map);
	m->tagset[m->seltags ^ 1] = tag_remap_mask(m->tagset[m->seltags ^ 1], map);
	m->ovbk_current_tagset = tag_remap_mask(m->ovbk_current_tagset, map);
	m->ovbk_prev_tagset = tag_remap_mask(m->ovbk_prev_tagset, map);

	// keep view indices; empty tags stay unchanged.
	if (m->pertag->curtag <= (uint32_t)config.tag_num && map[m->pertag->curtag])
		m->pertag->curtag = map[m->pertag->curtag];
	if (m->pertag->prevtag <= (uint32_t)config.tag_num &&
		map[m->pertag->prevtag])
		m->pertag->prevtag = map[m->pertag->prevtag];

	// move per-tag state in ascending order so later moves don't clobber
	// earlier sources.
	for (i = 1; i <= (uint32_t)config.tag_num; i++) {
		if (map[i] && map[i] != i)
			tag_gather_move_pertag(m, map[i], i);
	}

	// reset trailing empty slots.
	k = next - 1;
	for (i = k + 1; i <= (uint32_t)config.tag_num; i++)
		tag_gather_reset_slot(m, i);
}

Layout overviewlayout = {"󰃇", overview, "overview"};

Layout layouts[] = {
	// At least two are required; fewer than two cannot be removed.
	/* symbol     arrange function   name */
	{"T", tile, "tile", TILE},						 // Tiled layout
	{"S", scroller, "scroller", SCROLLER},			 // Scroll layout
	{"G", grid, "grid", GRID},						 // Grid layout
	{"M", monocle, "monocle", MONOCLE},				 // Single layout
	{"K", deck, "deck", DECK},						 // Card layout
	{"CT", center_tile, "center_tile", CENTER_TILE}, // Centered layout
	{"RT", right_tile, "right_tile", RIGHT_TILE},	 // Right layout
	{"VS", vertical_scroller, "vertical_scroller",
	 VERTICAL_SCROLLER}, // Vertical scroll layout
	{"VT", vertical_tile, "vertical_tile",
	 VERTICAL_TILE}, // Vertical tiled layout
	{"VG", vertical_grid, "vertical_grid",
	 VERTICAL_GRID}, // Vertical grid layout
	{"VK", vertical_deck, "vertical_deck",
	 VERTICAL_DECK}, // Vertical card layout
	{"DW", dwindle, "dwindle", DWINDLE},
	{"F", fair, "fair", FAIR},
	{"VF", vertical_fair, "vertical_fair", VERTICAL_FAIR},
};

bool special_handle_empty_view(Monitor *m, bool from_view) {
	if (!is_special_active(m)) {
		m->special_empty_view = false;
		return false;
	}
	if (special_has_clients(m)) {
		m->special_empty_view = false;
		return false;
	}
	if (from_view) {
		m->special_empty_view = true;
		return false;
	}
	if (!m->special_empty_view) {
		toggle_special_tag_mon(m);
		return true;
	}
	return false;
}
void // 17
arrange(Monitor *m, bool want_animation, bool from_view) {

	if (!m || m->iscleanuping)
		return;

	if (!m->wlr_output->enabled)
		return;

	if (!m->sel) {
		m->sel = client_focus_top(m);
	}

	if (special_handle_empty_view(m, from_view))
		return;

	pre_calculate_before_arrange(m, want_animation, from_view, false);

	bool is_tag0 = is_special_active(m);
	int32_t saved_oh = m->gappoh, saved_ov = m->gappov;
	int32_t saved_ih = m->gappih, saved_iv = m->gappiv;

	if (is_tag0) {
		m->gappoh = m->special_gappoh;
		m->gappov = m->special_gappov;
		m->gappih = m->special_gappih;
		m->gappiv = m->special_gappiv;
	}

	if (m->isoverview) {
		overviewlayout.arrange(m);
	} else {
		m->pertag->ltidxs[get_mon_curtag(m)]->arrange(m);
	}

	if (is_tag0) {
		m->gappoh = saved_oh;
		m->gappov = saved_ov;
		m->gappih = saved_ih;
		m->gappiv = saved_iv;
	}

	// gather after layout/animation setup so tag-switch animations still play.
	if (config.tag_gather) {
		tag_gather_apply(m);
	}

	if (!server.start_drag_window) {
		pointer_process_motion(0, NULL, 0, 0, 0, 0);
		check_idle_inhibitor(NULL);
	}

	printstatus(IPC_WATCH_ARRANGGE);
}
