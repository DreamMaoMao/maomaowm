#include "mango/layout/arrange.h"
#include "mango/common/globals.h"
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

void set_size_per(Monitor *m, Client *c) {
	Client *fc = NULL;
	bool found = false;

	if (!m || !c)
		return;

	uint32_t tag = get_mon_curtag(m);
	const Layout *current_layout = m->pertag->ltidxs[tag];

	wl_list_for_each(fc, &clients, link) {
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

void resize_tile_master_horizontal(Client *grabc, bool isdrag, int32_t offsetx,
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

	/* 寻找 next / nextnext */
	for (node = grabc->link.next; node != &clients; node = node->next) {
		tc = wl_container_of(node, tc, link);
		if (begin_find_nextnext && VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			nextnext = tc;
			break;
		}
		if (!begin_find_nextnext && VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			next = tc;
			begin_find_nextnext = true;
			continue;
		}
	}

	/* 寻找 prev / prevprev */
	for (node = grabc->link.prev; node != &clients; node = node->prev) {
		tc = wl_container_of(node, tc, link);
		if (begin_find_prevprev && VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			prevprev = tc;
			break;
		}
		if (!begin_find_prevprev && VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			prev = tc;
			begin_find_prevprev = true;
			continue;
		}
	}

	if (!start_drag_window && isdrag) {
		drag_begin_cursorx = cursor->x;
		drag_begin_cursory = cursor->y;
		start_drag_window = true;
		grabc->old_master_mfact_per = grabc->master_mfact_per;
		grabc->old_master_inner_per = grabc->master_inner_per;
		grabc->old_stack_inner_per = grabc->stack_inner_per;
		grabc->cursor_in_upper_half =
			cursor->y < grabc->geom.y + grabc->geom.height / 2;
		grabc->cursor_in_left_half =
			cursor->x < grabc->geom.x + grabc->geom.width / 2;
		grabc->drag_begin_geom = grabc->geom;
	} else {
		if (isdrag) {
			offsetx = cursor->x - drag_begin_cursorx;
			offsety = cursor->y - drag_begin_cursory;
		} else {
			grabc->old_master_mfact_per = grabc->master_mfact_per;
			grabc->old_master_inner_per = grabc->master_inner_per;
			grabc->old_stack_inner_per = grabc->stack_inner_per;
			grabc->drag_begin_geom = grabc->geom;
			grabc->cursor_in_upper_half = true;
			grabc->cursor_in_left_half = false;
		}

		if (grabc->ismaster) {
			delta_x = (float)(offsetx) * (grabc->old_master_mfact_per) /
					  grabc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (grabc->old_master_inner_per) /
					  grabc->drag_begin_geom.height;
		} else {
			delta_x = (float)(offsetx) * (1 - grabc->old_master_mfact_per) /
					  grabc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (grabc->old_stack_inner_per) /
					  grabc->drag_begin_geom.height;
		}

		bool moving_up, moving_down;
		if (!isdrag) {
			moving_up = offsety < 0;
			moving_down = offsety > 0;
		} else {
			moving_up = cursor->y < drag_begin_cursory;
			moving_down = cursor->y > drag_begin_cursory;
		}

		if (grabc->ismaster && !prev) {
			if (moving_up)
				delta_y = -fabsf(delta_y);
			else
				delta_y = fabsf(delta_y);
		} else if (grabc->ismaster && next && !next->ismaster) {
			if (moving_up)
				delta_y = fabsf(delta_y);
			else
				delta_y = -fabsf(delta_y);
		} else if (!grabc->ismaster && prev && prev->ismaster) {
			if (moving_up)
				delta_y = -fabsf(delta_y);
			else
				delta_y = fabsf(delta_y);
		} else if (!grabc->ismaster && !next) {
			if (moving_up)
				delta_y = fabsf(delta_y);
			else
				delta_y = -fabsf(delta_y);
		} else if (type == CENTER_TILE && !grabc->ismaster && !nextnext) {
			if (moving_up)
				delta_y = fabsf(delta_y);
			else
				delta_y = -fabsf(delta_y);
		} else if (type == CENTER_TILE && !grabc->ismaster && prevprev &&
				   prevprev->ismaster) {
			if (moving_up)
				delta_y = -fabsf(delta_y);
			else
				delta_y = fabsf(delta_y);
		} else if ((grabc->cursor_in_upper_half && moving_up) ||
				   (!grabc->cursor_in_upper_half && moving_down)) {
			delta_y = fabsf(delta_y) * 2;
		} else {
			delta_y = -fabsf(delta_y) * 2;
		}

		if (!grabc->ismaster && grabc->isleftstack && type == CENTER_TILE)
			delta_x = delta_x * -1.0f;
		if (grabc->ismaster && type == CENTER_TILE &&
			grabc->cursor_in_left_half)
			delta_x = delta_x * -1.0f;
		if (grabc->ismaster && type == CENTER_TILE)
			delta_x = delta_x * 2;
		if (type == RIGHT_TILE)
			delta_x = delta_x * -1.0f;

		float new_master_mfact_per = grabc->old_master_mfact_per + delta_x;
		float new_master_inner_per = grabc->old_master_inner_per + delta_y;
		float new_stack_inner_per = grabc->old_stack_inner_per + delta_y;

		new_master_mfact_per = fmaxf(0.1f, fminf(0.9f, new_master_mfact_per));
		new_master_inner_per = fmaxf(0.1f, fminf(0.9f, new_master_inner_per));
		new_stack_inner_per = fmaxf(0.1f, fminf(0.9f, new_stack_inner_per));

		// 实时缩放同组其他窗口的比例，保持组内总和为 1,
		// 不然增加的比例并不是排布后的比例
		if (isdrag) {
			if (grabc->ismaster) {
				/* 主窗口组：调整所有主窗口的 master_inner_per */
				float cur_other_sum = 1.0f - grabc->master_inner_per;
				float new_other_sum = 1.0f - new_master_inner_per;
				if (cur_other_sum > 0.001f) {
					float scale = new_other_sum / cur_other_sum;
					wl_list_for_each(tc, &clients, link) {
						if (VISIBLEON(tc, grabc->mon) && ISTILED(tc) &&
							tc->ismaster && tc != grabc)
							tc->master_inner_per *= scale;
					}
				}
			} else {
				/* 栈窗口组：根据布局类型分开处理 */
				if (type == CENTER_TILE) {
					/* 仅缩放同侧栈窗口的 stack_inner_per */
					float cur_other_sum = 1.0f - grabc->stack_inner_per;
					float new_other_sum = 1.0f - new_stack_inner_per;
					if (cur_other_sum > 0.001f) {
						float scale = new_other_sum / cur_other_sum;
						wl_list_for_each(tc, &clients, link) {
							if (VISIBLEON(tc, grabc->mon) && ISTILED(tc) &&
								!tc->ismaster && tc != grabc &&
								tc->isleftstack == grabc->isleftstack)
								tc->stack_inner_per *= scale;
						}
					}
				} else {
					/* TILE / RIGHT_TILE / DECK：所有栈窗口共用一个比例组 */
					float cur_other_sum = 1.0f - grabc->stack_inner_per;
					float new_other_sum = 1.0f - new_stack_inner_per;
					if (cur_other_sum > 0.001f) {
						float scale = new_other_sum / cur_other_sum;
						wl_list_for_each(tc, &clients, link) {
							if (VISIBLEON(tc, grabc->mon) && ISTILED(tc) &&
								!tc->ismaster && tc != grabc)
								tc->stack_inner_per *= scale;
						}
					}
				}
			}
		} else {
			/* 键盘步进 */
			wl_list_for_each(tc, &clients, link) {
				if (!VISIBLEON(tc, grabc->mon) || !ISTILED(tc))
					continue;
				if (tc != grabc) {
					if (!tc->ismaster && new_stack_inner_per != 1.0f &&
						grabc->old_stack_inner_per != 1.0f &&
						(type != CENTER_TILE ||
						 !(grabc->isleftstack ^ tc->isleftstack)))
						tc->stack_inner_per = (1 - new_stack_inner_per) /
											  (1 - grabc->old_stack_inner_per) *
											  tc->stack_inner_per;
					if (tc->ismaster && new_master_inner_per != 1.0f &&
						grabc->old_master_inner_per != 1.0f)
						tc->master_inner_per =
							(1.0f - new_master_inner_per) /
							(1.0f - grabc->old_master_inner_per) *
							tc->master_inner_per;
				}
			}
		}

		/* 将新比例应用到抓取窗口本身 */
		grabc->master_inner_per = new_master_inner_per;
		grabc->stack_inner_per = new_stack_inner_per;

		/* 广播 master_mfact_per 到所有平铺窗口 */
		wl_list_for_each(tc, &clients, link) {
			if (VISIBLEON(tc, grabc->mon) && ISTILED(tc))
				tc->master_mfact_per = new_master_mfact_per;
		}

		if (!isdrag) {
			arrange(grabc->mon, false, false);
			return;
		}

		if (last_apply_drap_time == 0 ||
			time - last_apply_drap_time > config.drag_tile_refresh_interval) {
			arrange(grabc->mon, false, false);
			last_apply_drap_time = time;
		}
	}
}

void resize_tile_master_vertical(Client *grabc, bool isdrag, int32_t offsetx,
								 int32_t offsety, uint32_t time, int32_t type) {
	Client *tc = NULL;
	float delta_x, delta_y;
	Client *next = NULL;
	Client *prev = NULL;
	struct wl_list *node;

	/* 寻找 next */
	for (node = grabc->link.next; node != &clients; node = node->next) {
		tc = wl_container_of(node, tc, link);
		if (VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			next = tc;
			break;
		}
	}

	/* 寻找 prev */
	for (node = grabc->link.prev; node != &clients; node = node->prev) {
		tc = wl_container_of(node, tc, link);
		if (VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			prev = tc;
			break;
		}
	}

	if (!start_drag_window && isdrag) {
		drag_begin_cursorx = cursor->x;
		drag_begin_cursory = cursor->y;
		start_drag_window = true;
		grabc->old_master_mfact_per = grabc->master_mfact_per;
		grabc->old_master_inner_per = grabc->master_inner_per;
		grabc->old_stack_inner_per = grabc->stack_inner_per;
		grabc->cursor_in_upper_half =
			cursor->y < grabc->geom.y + grabc->geom.height / 2;
		grabc->cursor_in_left_half =
			cursor->x < grabc->geom.x + grabc->geom.width / 2;
		grabc->drag_begin_geom = grabc->geom;
	} else {
		if (isdrag) {
			offsetx = cursor->x - drag_begin_cursorx;
			offsety = cursor->y - drag_begin_cursory;
		} else {
			grabc->old_master_mfact_per = grabc->master_mfact_per;
			grabc->old_master_inner_per = grabc->master_inner_per;
			grabc->old_stack_inner_per = grabc->stack_inner_per;
			grabc->drag_begin_geom = grabc->geom;
			grabc->cursor_in_upper_half = true;
			grabc->cursor_in_left_half = false;
		}

		if (grabc->ismaster) {
			delta_x = (float)(offsetx) * (grabc->old_master_inner_per) /
					  grabc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (grabc->old_master_mfact_per) /
					  grabc->drag_begin_geom.height;
		} else {
			delta_x = (float)(offsetx) * (grabc->old_stack_inner_per) /
					  grabc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (1 - grabc->old_master_mfact_per) /
					  grabc->drag_begin_geom.height;
		}

		bool moving_left, moving_right;
		if (!isdrag) {
			moving_left = offsetx < 0;
			moving_right = offsetx > 0;
		} else {
			moving_left = cursor->x < drag_begin_cursorx;
			moving_right = cursor->x > drag_begin_cursorx;
		}

		if (grabc->ismaster && !prev) {
			if (moving_left)
				delta_x = -fabsf(delta_x);
			else
				delta_x = fabsf(delta_x);
		} else if (grabc->ismaster && next && !next->ismaster) {
			if (moving_left)
				delta_x = fabsf(delta_x);
			else
				delta_x = -fabsf(delta_x);
		} else if (!grabc->ismaster && prev && prev->ismaster) {
			if (moving_left)
				delta_x = -fabsf(delta_x);
			else
				delta_x = fabsf(delta_x);
		} else if (!grabc->ismaster && !next) {
			if (moving_left)
				delta_x = fabsf(delta_x);
			else
				delta_x = -fabsf(delta_x);
		} else if ((grabc->cursor_in_left_half && moving_left) ||
				   (!grabc->cursor_in_left_half && moving_right)) {
			delta_x = fabsf(delta_x) * 2;
		} else {
			delta_x = -fabsf(delta_x) * 2;
		}

		float new_master_mfact_per = grabc->old_master_mfact_per + delta_y;
		float new_master_inner_per = grabc->old_master_inner_per + delta_x;
		float new_stack_inner_per = grabc->old_stack_inner_per + delta_x;

		new_master_mfact_per = fmaxf(0.1f, fminf(0.9f, new_master_mfact_per));
		new_master_inner_per = fmaxf(0.1f, fminf(0.9f, new_master_inner_per));
		new_stack_inner_per = fmaxf(0.1f, fminf(0.9f, new_stack_inner_per));

		// 实时缩放同组其他窗口的比例，保持组内总和为 1,
		// 不然增加的比例并不是排布后的比例

		if (isdrag) {
			if (grabc->ismaster) {
				float cur_other_sum = 1.0f - grabc->master_inner_per;
				float new_other_sum = 1.0f - new_master_inner_per;
				if (cur_other_sum > 0.001f) {
					float scale = new_other_sum / cur_other_sum;
					wl_list_for_each(tc, &clients, link) {
						if (VISIBLEON(tc, grabc->mon) && ISTILED(tc) &&
							tc->ismaster && tc != grabc)
							tc->master_inner_per *= scale;
					}
				}
			} else {
				/* 所有栈窗口（垂直布局没有左侧/右侧区分） */
				float cur_other_sum = 1.0f - grabc->stack_inner_per;
				float new_other_sum = 1.0f - new_stack_inner_per;
				if (cur_other_sum > 0.001f) {
					float scale = new_other_sum / cur_other_sum;
					wl_list_for_each(tc, &clients, link) {
						if (VISIBLEON(tc, grabc->mon) && ISTILED(tc) &&
							!tc->ismaster && tc != grabc)
							tc->stack_inner_per *= scale;
					}
				}
			}
		} else {
			/* 键盘步进 */
			wl_list_for_each(tc, &clients, link) {
				if (!VISIBLEON(tc, grabc->mon) || !ISTILED(tc))
					continue;
				if (tc != grabc) {
					if (!tc->ismaster && new_stack_inner_per != 1.0f &&
						grabc->old_stack_inner_per != 1.0f)
						tc->stack_inner_per = (1 - new_stack_inner_per) /
											  (1 - grabc->old_stack_inner_per) *
											  tc->stack_inner_per;
					if (tc->ismaster && new_master_inner_per != 1.0f &&
						grabc->old_master_inner_per != 1.0f)
						tc->master_inner_per =
							(1.0f - new_master_inner_per) /
							(1.0f - grabc->old_master_inner_per) *
							tc->master_inner_per;
				}
			}
		}

		grabc->master_inner_per = new_master_inner_per;
		grabc->stack_inner_per = new_stack_inner_per;

		/* 广播 master_mfact_per */
		wl_list_for_each(tc, &clients, link) {
			if (VISIBLEON(tc, grabc->mon) && ISTILED(tc))
				tc->master_mfact_per = new_master_mfact_per;
		}

		if (!isdrag) {
			arrange(grabc->mon, false, false);
			return;
		}

		if (last_apply_drap_time == 0 ||
			time - last_apply_drap_time > config.drag_tile_refresh_interval) {
			arrange(grabc->mon, false, false);
			last_apply_drap_time = time;
		}
	}
}

void resize_tile_dwindle(Client *grabc, bool isdrag, int32_t offsetx,
						 int32_t offsety, uint32_t time, bool isvertical) {

	if (!isdrag) {
		dwindle_resize_client_step(grabc->mon, grabc, offsetx, offsety);
		return;
	}

	if (last_apply_drap_time == 0 ||
		time - last_apply_drap_time > config.drag_tile_refresh_interval) {
		dwindle_resize_client(grabc->mon, grabc);
		last_apply_drap_time = time;
	}
}

void resize_tile_grid_fair(Client *grabc, bool isdrag, int32_t offsetx,
						   int32_t offsety, uint32_t time) {
	if (!grabc || grabc->isfullscreen || grabc->ismaximizescreen)
		return;
	Monitor *m = grabc->mon;
	if (m->isoverview)
		return;

	if (m->visible_tiling_clients <= 1)
		return;

	// 获取当前布局 ID
	const Layout *current_layout = m->pertag->ltidxs[get_mon_curtag(m)];

	if (!start_drag_window && isdrag) {
		drag_begin_cursorx = cursor->x;
		drag_begin_cursory = cursor->y;
		start_drag_window = true;

		Client *c;
		wl_list_for_each(c, &clients, link) {
			c->old_grid_col_per =
				(c->grid_col_per > 0.0f) ? c->grid_col_per : 1.0f;
			c->old_grid_row_per =
				(c->grid_row_per > 0.0f) ? c->grid_row_per : 1.0f;
		}

		grabc->old_grid_col_per = grabc->grid_col_per;
		grabc->old_grid_row_per = grabc->grid_row_per;

		grabc->cursor_in_left_half =
			cursor->x < grabc->geom.x + grabc->geom.width / 2;
		grabc->cursor_in_upper_half =
			cursor->y < grabc->geom.y + grabc->geom.height / 2;
		grabc->drag_begin_geom = grabc->geom;
	} else {
		if (isdrag) {
			offsetx = cursor->x - drag_begin_cursorx;
			offsety = cursor->y - drag_begin_cursory;
		} else {
			grabc->drag_begin_geom = grabc->geom;
			Client *c;
			wl_list_for_each(c, &clients, link) {
				c->old_grid_col_per =
					(c->grid_col_per > 0.0f) ? c->grid_col_per : 1.0f;
				c->old_grid_row_per =
					(c->grid_row_per > 0.0f) ? c->grid_row_per : 1.0f;
			}
			grabc->cursor_in_upper_half = false;
			grabc->cursor_in_left_half = false;
		}

		// 以屏幕分辨率为基准算出缩放比变化的量
		float delta_x = (float)offsetx * grabc->old_grid_col_per /
						grabc->drag_begin_geom.width;
		float delta_y = (float)offsety * grabc->old_grid_row_per /
						grabc->drag_begin_geom.height;

		int adj_c_idx = grabc->grid_col_idx;
		int adj_r_idx = grabc->grid_row_idx;
		float sign_x = 1.0f, sign_y = 1.0f;

		if (isdrag) {
			if (grabc->cursor_in_left_half) {
				adj_c_idx -= 1;
				sign_x = -1.0f;
			} else {
				adj_c_idx += 1;
				sign_x = 1.0f;
			}

			if (grabc->cursor_in_upper_half) {
				adj_r_idx -= 1;
				sign_y = -1.0f;
			} else {
				adj_r_idx += 1;
				sign_y = 1.0f;
			}
		}
		// 键盘热键逻辑不变
		int max_col = -1, max_row = -1, min_col = INT32_MAX,
			min_row = INT32_MAX;
		Client *tmp;
		wl_list_for_each(tmp, &clients, link) {
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

		adj_c_idx = grabc->grid_col_idx + 1;
		adj_r_idx = grabc->grid_row_idx + 1;
		sign_x = 1.0f;
		sign_y = 1.0f;

		if (grabc->grid_col_idx == max_col) {
			adj_c_idx = grabc->grid_col_idx - 1;
			sign_x = -1.0f;
		}
		if (grabc->grid_row_idx == max_row) {
			adj_r_idx = grabc->grid_row_idx - 1;
			sign_y = -1.0f;
		}
		if (grabc->grid_col_idx == min_col) {
			adj_c_idx = grabc->grid_col_idx + 1;
			sign_x = 1.0f;
		}
		if (grabc->grid_row_idx == min_row) {
			adj_r_idx = grabc->grid_row_idx + 1;
			sign_y = 1.0f;
		}

		float dx = delta_x * sign_x;
		float dy = delta_y * sign_y;

		float my_old_col = grabc->old_grid_col_per;
		float my_old_row = grabc->old_grid_row_per;
		float adj_old_col = -1.0f, adj_old_row = -1.0f;

		Client *c;
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
				continue;
			if (c->grid_col_idx == adj_c_idx && adj_old_col < 0)
				adj_old_col = c->old_grid_col_per;
			if (c->grid_row_idx == adj_r_idx && adj_old_row < 0)
				adj_old_row = c->old_grid_row_per;
		}

		// 应用列宽调节
		if (adj_old_col > 0.0f) {
			float dx_clamped = dx;
			if (my_old_col + dx_clamped < 0.1f)
				dx_clamped = 0.1f - my_old_col;
			if (adj_old_col - dx_clamped < 0.1f)
				dx_clamped = adj_old_col - 0.1f;

			float new_my_col = my_old_col + dx_clamped;
			float new_adj_col = adj_old_col - dx_clamped;

			// 处理被强行锁死在 1.0f 的列边界，头部是个错位窗口
			if (current_layout && current_layout->id == VERTICAL_FAIR) {
				int32_t n_tiling = m->visible_tiling_clients;
				int32_t l_rows;
				for (l_rows = 0; l_rows <= n_tiling; l_rows++) {
					if (l_rows * l_rows >= n_tiling)
						break;
				}
				int32_t base_cols = n_tiling / l_rows;
				// 当调节边界恰好处于非对称的锁死列（如 3 窗口下的 col 0 与 col
				// 1 之间）
				if ((grabc->grid_col_idx == base_cols - 1 &&
					 adj_c_idx == base_cols) ||
					(grabc->grid_col_idx == base_cols &&
					 adj_c_idx == base_cols - 1)) {

					float p_col =
						(grabc->grid_col_idx == base_cols - 1)
							? (my_old_col + dx) / (my_old_col + adj_old_col)
							: (adj_old_col - dx) / (my_old_col + adj_old_col);
					if (p_col < 0.01f)
						p_col = 0.01f;
					if (p_col > 0.99f)
						p_col = 0.99f;

					// 反推非线性真实权重值
					float new_r_var_per = p_col / (1.0f - p_col);
					if (new_r_var_per < 0.1f)
						new_r_var_per = 0.1f;
					if (new_r_var_per > 10.0f)
						new_r_var_per = 10.0f;

					if (grabc->grid_col_idx == base_cols - 1) {
						new_my_col = new_r_var_per;
						new_adj_col = 1.0f;
					} else {
						new_my_col = 1.0f;
						new_adj_col = new_r_var_per;
					}
				}
			}

			wl_list_for_each(c, &clients, link) {
				if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
					continue;
				if (c->grid_col_idx == grabc->grid_col_idx)
					c->grid_col_per = new_my_col;
				if (c->grid_col_idx == adj_c_idx)
					c->grid_col_per = new_adj_col;
			}

			wl_list_for_each(c, &clients, link) {
				if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
					continue;
				if (c->grid_row_idx == 0) {
					if (c->grid_col_idx == grabc->grid_col_idx)
						c->grid_col_per = new_my_col;
					else if (c->grid_col_idx == adj_c_idx)
						c->grid_col_per = new_adj_col;
				}
			}
		}

		// 应用行高调节
		if (adj_old_row > 0.0f) {
			float dy_clamped = dy;
			if (my_old_row + dy_clamped < 0.1f)
				dy_clamped = 0.1f - my_old_row;
			if (adj_old_row - dy_clamped < 0.1f)
				dy_clamped = adj_old_row - 0.1f;

			float new_my_row = my_old_row + dy_clamped;
			float new_adj_row = adj_old_row - dy_clamped;

			// 处理被强行锁死在 1.0f 的行边界，头部是个错位窗口
			if (current_layout && current_layout->id == FAIR) {
				int32_t n_tiling = m->visible_tiling_clients;
				int32_t l_cols;
				for (l_cols = 0; l_cols <= n_tiling; l_cols++) {
					if (l_cols * l_cols >= n_tiling)
						break;
				}
				int32_t base_rows = n_tiling / l_cols;
				// 当调节边界恰好处于非对称的锁死行（如 3 窗口下的 row 0 与 row
				// 1 之间）
				if ((grabc->grid_row_idx == base_rows - 1 &&
					 adj_r_idx == base_rows) ||
					(grabc->grid_row_idx == base_rows &&
					 adj_r_idx == base_rows - 1)) {

					float p_row =
						(grabc->grid_row_idx == base_rows - 1)
							? (my_old_row + dy) / (my_old_row + adj_old_row)
							: (adj_old_row - dy) / (my_old_row + adj_old_row);
					if (p_row < 0.01f)
						p_row = 0.01f;
					if (p_row > 0.99f)
						p_row = 0.99f;

					// 反推非线性真实权重值
					float new_r_var_per = p_row / (1.0f - p_row);
					if (new_r_var_per < 0.1f)
						new_r_var_per = 0.1f;
					if (new_r_var_per > 10.0f)
						new_r_var_per = 10.0f;

					if (grabc->grid_row_idx == base_rows - 1) {
						new_my_row = new_r_var_per;
						new_adj_row = 1.0f;
					} else {
						new_my_row = 1.0f;
						new_adj_row = new_r_var_per;
					}
				}
			}

			wl_list_for_each(c, &clients, link) {
				if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
					continue;
				if (c->grid_row_idx == grabc->grid_row_idx)
					c->grid_row_per = new_my_row;
				if (c->grid_row_idx == adj_r_idx)
					c->grid_row_per = new_adj_row;
			}

			wl_list_for_each(c, &clients, link) {
				if (c->mon != m || !VISIBLEON(c, m) || !ISTILED(c))
					continue;
				if (c->grid_col_idx == 0) {
					if (c->grid_row_idx == grabc->grid_row_idx)
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

		if (last_apply_drap_time == 0 ||
			time - last_apply_drap_time > config.drag_tile_refresh_interval) {
			arrange(m, false, false);
			last_apply_drap_time = time;
		}
	}
}

void resize_tile_scroller(Client *grabc, bool isdrag, int32_t offsetx,
						  int32_t offsety, uint32_t time, bool isvertical) {

	if (!grabc || grabc->isfullscreen || grabc->ismaximizescreen || !grabc->mon)
		return;
	if (grabc->mon->isoverview)
		return;

	Monitor *m = grabc->mon;
	uint32_t tag = get_client_tag_idx(grabc);
	struct TagScrollerState *st = m->pertag->scroller_state[tag];
	if (!st)
		return;

	struct ScrollerStackNode *curnode = find_scroller_node(st, grabc);
	if (!curnode)
		return;

	struct ScrollerStackNode *headnode = curnode;
	while (headnode->prev_in_stack)
		headnode = headnode->prev_in_stack;

	Client *stack_head_client = headnode->client;

	float delta_x, delta_y;
	float new_scroller_proportion;
	float new_stack_proportion;

	if (!start_drag_window && isdrag) {
		drag_begin_cursorx = cursor->x;
		drag_begin_cursory = cursor->y;
		start_drag_window = true;

		headnode->client->old_scroller_pproportion =
			headnode->scroller_proportion;
		grabc->old_stack_proportion = curnode->stack_proportion;

		grabc->cursor_in_left_half =
			cursor->x < grabc->geom.x + grabc->geom.width / 2;
		grabc->cursor_in_upper_half =
			cursor->y < grabc->geom.y + grabc->geom.height / 2;
		grabc->drag_begin_geom = grabc->geom;
	} else {
		if (isdrag) {
			offsetx = cursor->x - drag_begin_cursorx;
			offsety = cursor->y - drag_begin_cursory;
		} else {
			grabc->old_master_mfact_per = grabc->master_mfact_per;
			grabc->old_master_inner_per = grabc->master_inner_per;
			grabc->old_stack_inner_per = grabc->stack_inner_per;
			grabc->drag_begin_geom = grabc->geom;
			stack_head_client->old_scroller_pproportion =
				headnode->scroller_proportion;
			grabc->old_stack_proportion = curnode->stack_proportion;
			grabc->cursor_in_upper_half = false;
			grabc->cursor_in_left_half = false;
		}

		if (isvertical) {
			delta_y = (float)(offsety) *
					  (headnode->client->old_scroller_pproportion) /
					  grabc->drag_begin_geom.height;
			delta_x = (float)(offsetx) * (grabc->old_stack_proportion) /
					  grabc->drag_begin_geom.width;
		} else {
			delta_x = (float)(offsetx) *
					  (headnode->client->old_scroller_pproportion) /
					  grabc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (grabc->old_stack_proportion) /
					  grabc->drag_begin_geom.height;
		}

		bool moving_up, moving_down, moving_left, moving_right;
		if (!isdrag) {
			moving_up = offsety < 0;
			moving_down = offsety > 0;
			moving_left = offsetx < 0;
			moving_right = offsetx > 0;
		} else {
			moving_up = cursor->y < drag_begin_cursory;
			moving_down = cursor->y > drag_begin_cursory;
			moving_left = cursor->x < drag_begin_cursorx;
			moving_right = cursor->x > drag_begin_cursorx;
		}

		if ((grabc->cursor_in_upper_half && moving_up) ||
			(!grabc->cursor_in_upper_half && moving_down)) {
			delta_y = fabsf(delta_y);
		} else {
			delta_y = -fabsf(delta_y);
		}

		if ((grabc->cursor_in_left_half && moving_left) ||
			(!grabc->cursor_in_left_half && moving_right)) {
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
			new_stack_proportion = grabc->old_stack_proportion + delta_x;
		} else {
			new_scroller_proportion =
				headnode->client->old_scroller_pproportion + delta_x;
			new_stack_proportion = grabc->old_stack_proportion + delta_y;
		}

		new_scroller_proportion =
			fmaxf(0.1f, fminf(1.0f, new_scroller_proportion));
		new_stack_proportion = fmaxf(0.1f, fminf(0.9f, new_stack_proportion));

		// 保持总和为 1，避免后续 arrange 归一化吞掉位移
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
			// 键盘步进
			if (grabc->old_stack_proportion != 1.0f) {
				for (struct ScrollerStackNode *tc = headnode; tc;
					 tc = tc->next_in_stack) {
					if (tc != curnode) {
						tc->stack_proportion =
							(1.0f - new_stack_proportion) /
							(1.0f - grabc->old_stack_proportion) *
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

		/* 同步回全局字段 */
		sync_scroller_state_to_clients(m, tag);

		if (!isdrag) {
			arrange(m, false, false);
			return;
		}

		if (last_apply_drap_time == 0 ||
			time - last_apply_drap_time > config.drag_tile_refresh_interval) {
			arrange(m, false, false);
			last_apply_drap_time = time;
		}
	}
}

void resize_tile_client(Client *grabc, bool isdrag, int32_t offsetx,
						int32_t offsety, uint32_t time) {

	if (!grabc || grabc->isfullscreen || grabc->ismaximizescreen || !grabc->mon)
		return;

	if (grabc->mon->isoverview)
		return;

	const Layout *current_layout =
		grabc->mon->pertag->ltidxs[get_client_tag_idx(grabc)];
	if (current_layout->id == TILE || current_layout->id == DECK ||
		current_layout->id == CENTER_TILE || current_layout->id == RIGHT_TILE

	) {
		resize_tile_master_horizontal(grabc, isdrag, offsetx, offsety, time,
									  current_layout->id);
	} else if (current_layout->id == VERTICAL_TILE ||
			   current_layout->id == VERTICAL_DECK) {
		resize_tile_master_vertical(grabc, isdrag, offsetx, offsety, time,
									current_layout->id);
	} else if (current_layout->id == SCROLLER) {
		resize_tile_scroller(grabc, isdrag, offsetx, offsety, time, false);
	} else if (current_layout->id == VERTICAL_SCROLLER) {
		resize_tile_scroller(grabc, isdrag, offsetx, offsety, time, true);
	} else if (current_layout->id == DWINDLE) {
		resize_tile_dwindle(grabc, isdrag, offsetx, offsety, time, true);
	} else if (current_layout->id == GRID ||
			   current_layout->id == VERTICAL_GRID ||
			   current_layout->id == FAIR ||
			   current_layout->id == VERTICAL_FAIR) {
		resize_tile_grid_fair(grabc, isdrag, offsetx, offsety, time);
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

		wl_list_for_each(c, &clients, link) {
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
		wl_list_for_each(c, &clients, link) {
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

	wl_list_for_each(c, &clients, link) {

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
			focusclient(c, 1);
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

				/* 更新可见滚动客户端计数 */
				if (st) {
					struct ScrollerStackNode *n = find_scroller_node(st, c);
					if (n && !n->prev_in_stack) /* 是堆叠头部 */
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

	wl_list_for_each(c, &clients, link) {
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
			} else if (!only_calculate && c != grabc) {
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
	uint32_t out = tags & ~tagmask;
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
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m && !c->iskilling && !c->is_logic_hide &&
			!(c->tags & TAG0_MASK))
			occupied |= c->tags & tagmask;
	}
	// the current view counts as occupied even when empty.
	occupied |= m->tagset[m->seltags] & tagmask;

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
	wl_list_for_each(c, &clients, link) {
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
	// 最少两个,不能删除少于两个
	/* symbol     arrange function   name */
	{"T", tile, "tile", TILE},						 // 平铺布局
	{"S", scroller, "scroller", SCROLLER},			 // 滚动布局
	{"G", grid, "grid", GRID},						 // 格子布局
	{"M", monocle, "monocle", MONOCLE},				 // 单屏布局
	{"K", deck, "deck", DECK},						 // 卡片布局
	{"CT", center_tile, "center_tile", CENTER_TILE}, // 居中布局
	{"RT", right_tile, "right_tile", RIGHT_TILE},	 // 右布局
	{"VS", vertical_scroller, "vertical_scroller",
	 VERTICAL_SCROLLER},								   // 垂直滚动布局
	{"VT", vertical_tile, "vertical_tile", VERTICAL_TILE}, // 垂直平铺布局
	{"VG", vertical_grid, "vertical_grid", VERTICAL_GRID}, // 垂直格子布局
	{"VK", vertical_deck, "vertical_deck", VERTICAL_DECK}, // 垂直卡片布局
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
		m->sel = focustop(m);
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

	if (!start_drag_window) {
		motionnotify(0, NULL, 0, 0, 0, 0);
		checkidleinhibitor(NULL);
	}

	printstatus(IPC_WATCH_ARRANGGE);
}
