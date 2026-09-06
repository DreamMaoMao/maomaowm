#include "mango/layout/vertical.h"
#include "mango/common/server.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"

void vertical_tile(Monitor *m) {
	int32_t i, n = 0, w, r, ie = server.enable_gaps, mh, mx, tx;
	Client *c = NULL;
	Client *fc = NULL;
	double mfact = 0;
	int32_t master_num = 0;
	int32_t stack_num = 0;

	n = m->visible_fake_tiling_clients;
	master_num = m->pertag->nmasters[get_mon_curtag(m)];
	master_num = n > master_num ? master_num : n;
	stack_num = n - master_num;

	if (n == 0)
		return;

	int32_t cur_gapih = server.enable_gaps ? m->gappih : 0;
	int32_t cur_gapiv = server.enable_gaps ? m->gappiv : 0;
	int32_t cur_gapoh = server.enable_gaps ? m->gappoh : 0;
	int32_t cur_gapov = server.enable_gaps ? m->gappov : 0;

	cur_gapih =
		config.smartgaps && m->visible_fake_tiling_clients == 1 ? 0 : cur_gapih;
	cur_gapiv =
		config.smartgaps && m->visible_fake_tiling_clients == 1 ? 0 : cur_gapiv;
	cur_gapoh =
		config.smartgaps && m->visible_fake_tiling_clients == 1 ? 0 : cur_gapoh;
	cur_gapov =
		config.smartgaps && m->visible_fake_tiling_clients == 1 ? 0 : cur_gapov;

	wl_list_for_each(fc, &server.clients, link) {
		if (VISIBLEON(fc, m) && ISFAKETILED(fc))
			break;
	}

	mfact = fc->master_mfact_per > 0.0f ? fc->master_mfact_per
										: m->pertag->mfacts[get_mon_curtag(m)];

	if (n > m->pertag->nmasters[get_mon_curtag(m)])
		mh = m->pertag->nmasters[get_mon_curtag(m)]
				 ? (m->w.height + cur_gapiv * ie) * mfact
				 : 0;
	else
		mh = m->w.height - 2 * cur_gapov + cur_gapiv * ie;

	i = 0;
	mx = tx = cur_gapoh;

	int32_t master_surplus_width =
		(m->w.width - 2 * cur_gapoh - cur_gapih * ie * (master_num - 1));
	float master_surplus_ratio = 1.0;

	int32_t slave_surplus_width =
		(m->w.width - 2 * cur_gapoh - cur_gapih * ie * (stack_num - 1));
	float slave_surplus_ratio = 1.0;

	wl_list_for_each(c, &server.clients, link) {
		if (!VISIBLEON(c, m) || !ISFAKETILED(c))
			continue;
		if (i < m->pertag->nmasters[get_mon_curtag(m)]) {
			r = MANGO_MIN(n, m->pertag->nmasters[get_mon_curtag(m)]) - i;
			if (c->master_inner_per > 0.0f) {
				w = master_surplus_width * c->master_inner_per /
					master_surplus_ratio;
				master_surplus_width = master_surplus_width - w;
				master_surplus_ratio =
					master_surplus_ratio - c->master_inner_per;
				c->master_mfact_per = mfact;
			} else {
				w = (m->w.width - mx - cur_gapih - cur_gapih * ie * (r - 1)) /
					r;
				c->master_inner_per = w / (m->w.width - mx - cur_gapih -
										   cur_gapih * ie * (r - 1));
				c->master_mfact_per = mfact;
			}
			client_tile_resize(c,
							   (struct wlr_box){.x = m->w.x + mx,
												.y = m->w.y + cur_gapov,
												.width = w,
												.height = mh - cur_gapiv * ie},
							   0);
			mx +=
				w + cur_gapih * ie; // Accumulates using the theoretical width.
		} else {
			r = n - i;
			if (c->stack_inner_per > 0.0f) {
				w = slave_surplus_width * c->stack_inner_per /
					slave_surplus_ratio;
				slave_surplus_width = slave_surplus_width - w;
				slave_surplus_ratio = slave_surplus_ratio - c->stack_inner_per;
				c->master_mfact_per = mfact;
			} else {
				w = (m->w.width - tx - cur_gapih - cur_gapih * ie * (r - 1)) /
					r;
				c->stack_inner_per = w / (m->w.width - tx - cur_gapih -
										  cur_gapih * ie * (r - 1));
				c->master_mfact_per = mfact;
			}

			client_tile_resize(
				c,
				(struct wlr_box){.x = m->w.x + tx,
								 .y = m->w.y + mh + cur_gapov,
								 .width = w,
								 .height = m->w.height - mh - 2 * cur_gapov},
				0);
			tx +=
				w + cur_gapih * ie; // Accumulates using the theoretical width.
		}
		i++;
	}
}

void vertical_deck(Monitor *m) {
	int32_t mh, mx;
	int32_t i, n = 0;
	Client *c = NULL;
	Client *fc = NULL;
	float mfact;
	uint32_t nmasters = m->pertag->nmasters[get_mon_curtag(m)];

	int32_t cur_gappiv = server.enable_gaps ? m->gappiv : 0;
	int32_t cur_gappoh = server.enable_gaps ? m->gappoh : 0;
	int32_t cur_gappov = server.enable_gaps ? m->gappov : 0;

	cur_gappiv = config.smartgaps && m->visible_fake_tiling_clients == 1
					 ? 0
					 : cur_gappiv;
	cur_gappoh = config.smartgaps && m->visible_fake_tiling_clients == 1
					 ? 0
					 : cur_gappoh;
	cur_gappov = config.smartgaps && m->visible_fake_tiling_clients == 1
					 ? 0
					 : cur_gappov;

	n = m->visible_fake_tiling_clients;

	if (n == 0)
		return;

	wl_list_for_each(fc, &server.clients, link) {
		if (VISIBLEON(fc, m) && ISFAKETILED(fc))
			break;
	}

	mfact = fc->master_mfact_per > 0.0f ? fc->master_mfact_per
										: m->pertag->mfacts[get_mon_curtag(m)];

	if (n > nmasters)
		mh = nmasters ? round((m->w.height - 2 * cur_gappov) * mfact) : 0;
	else
		mh = m->w.height - 2 * cur_gappov;

	i = mx = 0;
	wl_list_for_each(c, &server.clients, link) {
		if (!VISIBLEON(c, m) || !ISFAKETILED(c))
			continue;
		if (i < nmasters) {
			c->master_mfact_per = mfact;
			int32_t w = (m->w.width - 2 * cur_gappoh - mx) /
						(MANGO_MIN(n, nmasters) - i);
			client_tile_resize(c,
							   (struct wlr_box){.x = m->w.x + cur_gappoh + mx,
												.y = m->w.y + cur_gappov,
												.width = w,
												.height = mh},
							   0);
			mx += w;
		} else {
			c->master_mfact_per = mfact;
			client_tile_resize(
				c,
				(struct wlr_box){.x = m->w.x + cur_gappoh,
								 .y = m->w.y + mh + cur_gappov + cur_gappiv,
								 .width = m->w.width - 2 * cur_gappoh,
								 .height = m->w.height - mh - 2 * cur_gappov -
										   cur_gappiv},
				0);
			if (c == client_focus_top(m))
				wlr_scene_node_raise_to_top(&c->scene->node);
		}
		i++;
	}
}

void vertical_grid(Monitor *m) {
	int32_t i, n;
	int32_t cw, ch;
	int32_t rows, cols, overrows;
	Client *c = NULL;
	int32_t target_gappo = server.enable_gaps ? config.gappov : 0;
	int32_t target_gappi = server.enable_gaps ? config.gappiv : 0;
	float single_width_ratio = 0.9;
	float single_height_ratio = 0.9;
	struct wlr_box target_geom;

	n = m->visible_fake_tiling_clients;
	if (n == 0)
		return;

	if (n == 1) {
		wl_list_for_each(c, &server.clients, link) {
			if (c->mon != m)
				continue;
			if (VISIBLEON(c, m) && !c->isunglobal &&
				(!client_is_x11_popup(c) || ISFAKETILED(c))) {
				ch = (m->w.height - 2 * target_gappo) * single_height_ratio;
				cw = (m->w.width - 2 * target_gappo) * single_width_ratio;
				target_geom.x = m->w.x + (m->w.width - cw) / 2;
				target_geom.y = m->w.y + (m->w.height - ch) / 2;
				target_geom.width = cw;
				target_geom.height = ch;
				client_tile_resize(c, target_geom, 0);
				return;
			}
		}
	}

	if (n == 2) {
		float row_pers[2] = {1.0f, 1.0f};
		// First extracts the existing row ratios of these two windows.
		i = 0;
		wl_list_for_each(c, &server.clients, link) {
			if (c->mon != m)
				continue;
			if (VISIBLEON(c, m) && !c->isunglobal &&
				(!client_is_x11_popup(c) || ISFAKETILED(c))) {
				if (i < 2)
					row_pers[i] =
						(c->grid_row_per > 0.0f) ? c->grid_row_per : 1.0f;
				i++;
			}
		}

		float sum_row = row_pers[0] + row_pers[1];
		float avail_h = m->w.height - 2 * target_gappo - target_gappi;
		cw = (m->w.width - 2 * target_gappo) *
			 0.65; // Keeps the pleasing width of 0.65.

		i = 0;
		wl_list_for_each(c, &server.clients, link) {
			if (c->mon != m)
				continue;
			if (VISIBLEON(c, m) && !c->isunglobal &&
				(!client_is_x11_popup(c) || ISFAKETILED(c))) {
				c->grid_col_idx = 0;
				c->grid_row_idx = i;
				c->grid_col_per = 1.0f;
				c->grid_row_per = row_pers[i];

				// Computes the current window height dynamically from the
				// assigned weight.
				ch = avail_h * (row_pers[i] / sum_row);

				target_geom.x = m->w.x + (m->w.width - cw) / 2 + target_gappo;
				if (i == 0) {
					target_geom.y = m->w.y + target_gappo;
				} else if (i == 1) {
					// The second window Y coordinate follows right after the
					// first window.
					float ch0 = avail_h * (row_pers[0] / sum_row);
					target_geom.y = m->w.y + target_gappo + ch0 + target_gappi;
				}
				target_geom.width = cw;
				target_geom.height = ch;
				client_tile_resize(c, target_geom, 0);
				i++;
			}
		}
		return;
	}
	for (rows = 0; rows <= n / 2; rows++) {
		if (rows * rows >= n)
			break;
	}
	cols = (rows && (rows - 1) * rows >= n) ? rows - 1 : rows;
	overrows = n % rows;

	float *col_pers = calloc(cols, sizeof(*col_pers));
	float *row_pers = calloc(rows, sizeof(*row_pers));
	if (!col_pers || !row_pers) {
		free(col_pers);
		free(row_pers);
		return;
	}
	for (i = 0; i < cols; i++)
		col_pers[i] = 1.0f;
	for (i = 0; i < rows; i++)
		row_pers[i] = 1.0f;

	i = 0;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal &&
			(!client_is_x11_popup(c) || ISFAKETILED(c))) {
			int32_t c_idx = i / rows;
			int32_t r_idx = i % rows;
			if (r_idx == 0)
				col_pers[c_idx] =
					(c->grid_col_per > 0.0f) ? c->grid_col_per : 1.0f;
			if (c_idx == 0)
				row_pers[r_idx] =
					(c->grid_row_per > 0.0f) ? c->grid_row_per : 1.0f;
			i++;
		}
	}

	float sum_col = 0.0f, sum_row = 0.0f;
	for (i = 0; i < cols; i++)
		sum_col += col_pers[i];
	for (i = 0; i < rows; i++)
		sum_row += row_pers[i];

	float avail_w = m->w.width - 2 * target_gappo - (cols - 1) * target_gappi;
	float avail_h = m->w.height - 2 * target_gappo - (rows - 1) * target_gappi;

	i = 0;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal &&
			(!client_is_x11_popup(c) || ISFAKETILED(c))) {
			int32_t c_idx = i / rows;
			int32_t r_idx = i % rows;

			c->grid_col_per = col_pers[c_idx];
			c->grid_row_per = row_pers[r_idx];
			c->grid_col_idx = c_idx;
			c->grid_row_idx = r_idx;

			float fl_cy = m->w.y + target_gappo;
			float fl_ch = 0.0f;

			if (overrows && i >= n - overrows) {
				float over_h = 0.0f;
				for (int j = 0; j < overrows; j++)
					over_h += avail_h * (row_pers[j] / sum_row);
				over_h += (overrows - 1) * target_gappi;
				float dy = (m->w.height - over_h) / 2.0f - target_gappo;

				fl_cy += dy;
				for (int j = 0; j < r_idx; j++)
					fl_cy += avail_h * (row_pers[j] / sum_row) + target_gappi;
				fl_ch = avail_h * (row_pers[r_idx] / sum_row);
			} else {
				for (int j = 0; j < r_idx; j++)
					fl_cy += avail_h * (row_pers[j] / sum_row) + target_gappi;
				fl_ch = (r_idx == rows - 1)
							? (m->w.y + m->w.height - target_gappo - fl_cy)
							: avail_h * (row_pers[r_idx] / sum_row);
			}

			float fl_cx = m->w.x + target_gappo;
			for (int j = 0; j < c_idx; j++)
				fl_cx += avail_w * (col_pers[j] / sum_col) + target_gappi;
			float fl_cw = (c_idx == cols - 1)
							  ? (m->w.x + m->w.width - target_gappo - fl_cx)
							  : avail_w * (col_pers[c_idx] / sum_col);

			target_geom.x = (int32_t)fl_cx;
			target_geom.y = (int32_t)fl_cy;
			target_geom.width = (int32_t)fl_cw;
			target_geom.height = (int32_t)fl_ch;
			client_tile_resize(c, target_geom, 0);
			i++;
		}
	}

	free(col_pers);
	free(row_pers);
}

void vertical_fair(Monitor *m) {
	int32_t i, n = 0;
	Client *c = NULL;

	n = m->visible_fake_tiling_clients;
	if (n == 0)
		return;

	int32_t cur_gappiv = server.enable_gaps ? m->gappiv : 0;
	int32_t cur_gappih = server.enable_gaps ? m->gappih : 0;
	int32_t cur_gappov = server.enable_gaps ? m->gappov : 0;
	int32_t cur_gappoh = server.enable_gaps ? m->gappoh : 0;

	if (config.smartgaps && n == 1) {
		cur_gappiv = cur_gappih = cur_gappov = cur_gappoh = 0;
	}

	int32_t rows;
	for (rows = 0; rows <= n; rows++) {
		if (rows * rows >= n)
			break;
	}

	int32_t base_cols = n / rows;
	int32_t remainder = n % rows;
	int32_t first_group_rows = rows - remainder;
	int32_t first_group_count = first_group_rows * base_cols;
	int32_t max_cols = base_cols + (remainder > 0 ? 1 : 0);

	Client **arr = calloc(n, sizeof(*arr));
	float *row_pers = calloc(rows, sizeof(*row_pers));
	float *col_pers = calloc(max_cols, sizeof(*col_pers));
	float *row_y = calloc(rows, sizeof(*row_y));
	float *row_h = calloc(rows, sizeof(*row_h));
	float *col_x_base = calloc(base_cols, sizeof(*col_x_base));
	float *col_w_base = calloc(base_cols, sizeof(*col_w_base));
	float *col_x_max = calloc(max_cols, sizeof(*col_x_max));
	float *col_w_max = calloc(max_cols, sizeof(*col_w_max));
	if (!arr || !row_pers || !col_pers || !row_y || !row_h || !col_x_base ||
		!col_w_base || !col_x_max || !col_w_max) {
		free(arr);
		free(row_pers);
		free(col_pers);
		free(row_y);
		free(row_h);
		free(col_x_base);
		free(col_w_base);
		free(col_x_max);
		free(col_w_max);
		return;
	}
	int32_t arr_idx = 0;
	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, m) && ISFAKETILED(c)) {
			arr[arr_idx++] = c;
			if (arr_idx >= n)
				break;
		}
	}

	for (i = 0; i < rows; i++)
		row_pers[i] = 0.0f;
	for (i = 0; i < max_cols; i++)
		col_pers[i] = 0.0f;

	for (i = 0; i < n; i++) {
		c = arr[i];
		int32_t row_idx =
			(i < first_group_count)
				? (i / base_cols)
				: (first_group_rows + (i - first_group_count) / max_cols);
		int32_t col_idx = (i < first_group_count)
							  ? (i % base_cols)
							  : ((i - first_group_count) % max_cols);

		if (c->grid_row_idx == row_idx && c->grid_row_per > 0.0f)
			row_pers[row_idx] = c->grid_row_per;
		if (c->grid_col_idx == col_idx && c->grid_col_per > 0.0f)
			col_pers[col_idx] = c->grid_col_per;
	}
	for (i = 0; i < n; i++) {
		c = arr[i];
		int32_t row_idx =
			(i < first_group_count)
				? (i / base_cols)
				: (first_group_rows + (i - first_group_count) / max_cols);
		int32_t col_idx = (i < first_group_count)
							  ? (i % base_cols)
							  : ((i - first_group_count) % max_cols);

		if (row_pers[row_idx] == 0.0f && c->grid_row_per > 0.0f)
			row_pers[row_idx] = c->grid_row_per;
		if (col_pers[col_idx] == 0.0f && c->grid_col_per > 0.0f)
			col_pers[col_idx] = c->grid_col_per;
	}

	float sum_row = 0.0f;
	for (i = 0; i < rows; i++) {
		if (row_pers[i] == 0.0f)
			row_pers[i] = 1.0f;
		sum_row += row_pers[i];
	}
	for (i = 0; i < max_cols; i++) {
		if (col_pers[i] == 0.0f)
			col_pers[i] = 1.0f;
	}

	float avail_h = m->w.height - 2 * cur_gappov - (rows - 1) * cur_gappiv;
	float next_y = m->w.y + cur_gappov;
	for (i = 0; i < rows; i++) {
		row_y[i] = next_y;
		row_h[i] = (i == rows - 1)
					   ? (m->w.y + m->w.height - cur_gappov - next_y)
					   : (avail_h * (row_pers[i] / sum_row));
		next_y += row_h[i] + cur_gappiv;
	}

	float sum_col_base = 0.0f;
	for (i = 0; i < base_cols; i++)
		sum_col_base += col_pers[i];
	float avail_w_base =
		m->w.width - 2 * cur_gappoh - (base_cols - 1) * cur_gappih;
	float next_x = m->w.x + cur_gappoh;
	for (i = 0; i < base_cols; i++) {
		col_x_base[i] = next_x;
		col_w_base[i] = (i == base_cols - 1)
							? (m->w.x + m->w.width - cur_gappoh - next_x)
							: (avail_w_base * (col_pers[i] / sum_col_base));
		next_x += col_w_base[i] + cur_gappih;
	}

	if (remainder > 0) {
		float sum_col_max = 0.0f;
		for (i = 0; i < max_cols; i++)
			sum_col_max += col_pers[i];
		float avail_w_max =
			m->w.width - 2 * cur_gappoh - (max_cols - 1) * cur_gappih;
		next_x = m->w.x + cur_gappoh;
		for (i = 0; i < max_cols; i++) {
			col_x_max[i] = next_x;
			col_w_max[i] = (i == max_cols - 1)
							   ? (m->w.x + m->w.width - cur_gappoh - next_x)
							   : (avail_w_max * (col_pers[i] / sum_col_max));
			next_x += col_w_max[i] + cur_gappih;
		}
	}

	for (i = 0; i < n; i++) {
		c = arr[i];
		int32_t row_idx, col_idx;
		float fl_cx, fl_cy, fl_cw, fl_ch;

		if (i < first_group_count) {
			row_idx = i / base_cols;
			col_idx = i % base_cols;
			fl_cx = col_x_base[col_idx];
			fl_cw = col_w_base[col_idx];
		} else {
			int32_t offset = i - first_group_count;
			row_idx = first_group_rows + (offset / max_cols);
			col_idx = offset % max_cols;
			fl_cx = col_x_max[col_idx];
			fl_cw = col_w_max[col_idx];
		}

		c->grid_row_per = row_pers[row_idx];
		c->grid_col_per = col_pers[col_idx];
		c->grid_row_idx = row_idx;
		c->grid_col_idx = col_idx;

		fl_cy = row_y[row_idx];
		fl_ch = row_h[row_idx];

		client_tile_resize(c,
						   (struct wlr_box){.x = (int32_t)fl_cx,
											.y = (int32_t)fl_cy,
											.width = (int32_t)fl_cw,
											.height = (int32_t)fl_ch},
						   0);
	}

	free(arr);
	free(row_pers);
	free(col_pers);
	free(row_y);
	free(row_h);
	free(col_x_base);
	free(col_w_base);
	free(col_x_max);
	free(col_w_max);
}
