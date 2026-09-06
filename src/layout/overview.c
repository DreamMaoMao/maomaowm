#include "mango/layout/overview.h"
#include "mango/common/server.h"
#include "mango/manage/client.h"
#include "mango/config/parse_config.h"
#include <stdbool.h>

int compare_layout_items(const void *a, const void *b) {
	float area_a = ((const OvLayoutItem *)a)->area;
	float area_b = ((const OvLayoutItem *)b)->area;
	if (area_a < area_b)
		return 1;
	if (area_a > area_b)
		return -1;
	return 0;
}

void begin_jump_mode(Monitor *m) { m->is_jump_mode = 1; }

bool try_place(OvPlacedRect *placed, int placed_cnt, float w, float h,
			   float gap, float avail_w, float avail_h, OvPlacedRect *out,
			   OvPoint *cands, OvPoint *feas) {
	int cand_cnt = 0;
	cands[cand_cnt++] = (OvPoint){0.0f, 0.0f};

	for (int i = 0; i < placed_cnt; i++) {
		OvPlacedRect p = placed[i];
		cands[cand_cnt++] = (OvPoint){p.x + p.w + gap, p.y};
		cands[cand_cnt++] = (OvPoint){p.x, p.y + p.h + gap};
		cands[cand_cnt++] = (OvPoint){p.x + p.w + gap, p.y + p.h + gap};
	}

	int unique_cnt = 0;
	for (int i = 0; i < cand_cnt; i++) {
		bool dup = false;
		for (int j = 0; j < unique_cnt; j++) {
			if (fabs(cands[i].x - cands[j].x) < 0.5f &&
				fabs(cands[i].y - cands[j].y) < 0.5f) {
				dup = true;
				break;
			}
		}
		if (!dup)
			cands[unique_cnt++] = cands[i];
	}
	cand_cnt = unique_cnt;

	int feas_cnt = 0;
	for (int i = 0; i < cand_cnt; i++) {
		float cx = cands[i].x;
		float cy = cands[i].y;

		if (cx < 0 || cy < 0 || cx + w > avail_w || cy + h > avail_h)
			continue;

		bool overlap = false;
		for (int j = 0; j < placed_cnt; j++) {
			OvPlacedRect p = placed[j];
			if (!(cx + w + gap <= p.x || cx >= p.x + p.w + gap ||
				  cy + h + gap <= p.y || cy >= p.y + p.h + gap)) {
				overlap = true;
				break;
			}
		}
		if (!overlap) {
			feas[feas_cnt++] = (OvPoint){cx, cy};
		}
	}

	if (feas_cnt == 0)
		return false;

	int best = 0;
	for (int i = 1; i < feas_cnt; i++) {
		if (feas[i].y < feas[best].y ||
			(fabs(feas[i].y - feas[best].y) < 0.5f &&
			 feas[i].x < feas[best].x)) {
			best = i;
		}
	}

	out->x = feas[best].x;
	out->y = feas[best].y;
	out->w = w;
	out->h = h;
	return true;
}

void overview_scale(Monitor *m) {
	int32_t target_gappo = config.overviewgappo;
	int32_t target_gappi = config.overviewgappi;

	int orig_n = m->visible_clients;
	if (orig_n == 0)
		return;

	OvLayoutItem *items = calloc(orig_n, sizeof(OvLayoutItem));
	if (!items)
		return;

	int n = 0;
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
			items[n].c = c;
			float w = c->overview_backup_geom.width;
			float h = c->overview_backup_geom.height;
			if (w <= 0 || h <= 0) {
				w = 100.0f;
				h = 100.0f;
			}
			items[n].orig_w = w;
			items[n].orig_h = h;
			items[n].area = w * h;
			n++;
		}
	}

	if (n == 0) {
		free(items);
		return;
	}

	qsort(items, n, sizeof(OvLayoutItem), compare_layout_items);

	float max_avail_w = fmaxf(1.0f, m->w.width - 2 * target_gappo);
	float max_avail_h = fmaxf(1.0f, m->w.height - 2 * target_gappo);

	int max_points = 1 + 3 * n;
	OvPlacedRect *placed = calloc(n, sizeof(OvPlacedRect));
	OvPoint *cands = calloc(max_points, sizeof(OvPoint));
	OvPoint *feas = calloc(max_points, sizeof(OvPoint));

	if (!placed || !cands || !feas) {
		free(items);
		free(placed);
		free(cands);
		free(feas);
		return;
	}

	float low = 0.0f, high = 1.0f, best_s = 0.0f;
	for (int iter = 0; iter < 50; iter++) {
		float mid = (low + high) / 2.0f;
		bool ok = true;
		int placed_cnt = 0;

		for (int k = 0; k < n; k++) {
			float w = items[k].orig_w * mid;
			float h = items[k].orig_h * mid;
			OvPlacedRect out;
			if (!try_place(placed, placed_cnt, w, h, (float)target_gappi,
						   max_avail_w, max_avail_h, &out, cands, feas)) {
				ok = false;
				break;
			}
			placed[placed_cnt++] = out;
		}

		if (ok) {
			best_s = mid;
			low = mid;
		} else {
			high = mid;
		}
	}

	if (best_s > 0.0f) {
		int placed_cnt = 0;

		for (int k = 0; k < n; k++) {
			float w = items[k].orig_w * best_s;
			float h = items[k].orig_h * best_s;
			OvPlacedRect out;
			try_place(placed, placed_cnt, w, h, (float)target_gappi,
					  max_avail_w, max_avail_h, &out, cands, feas);
			placed[placed_cnt++] = out;
		}

		if (n > 1) {
			float grid_box_w = 0;
			for (int k = 0; k < n - 1; k++) {
				float r = placed[k].x + placed[k].w;
				if (r > grid_box_w)
					grid_box_w = r;
			}

			OvPlacedRect *last = &placed[n - 1];
			float max_x = grid_box_w - last->w;

			if (max_x > last->x) {
				for (int k = 0; k < n - 1; k++) {
					OvPlacedRect p = placed[k];
					if (!(last->y + last->h + target_gappi <= p.y ||
						  last->y >= p.y + p.h + target_gappi)) {
						if (p.x > last->x) {
							float limit = p.x - target_gappi - last->w;
							if (limit < max_x) {
								max_x = limit;
							}
						}
					}
				}

				if (max_x > last->x) {
					last->x += (max_x - last->x) / 2.0f;
				}
			}
		}

		float box_w = 0, box_h = 0;
		for (int k = 0; k < n; k++) {
			float r = placed[k].x + placed[k].w;
			float b = placed[k].y + placed[k].h;
			if (r > box_w)
				box_w = r;
			if (b > box_h)
				box_h = b;
		}

		float dx = (max_avail_w - box_w) / 2.0f;
		float dy = (max_avail_h - box_h) / 2.0f;
		float base_x = m->w.x + target_gappo + dx;
		float base_y = m->w.y + target_gappo + dy;

		// Collects the target geometry of all clients and calls
		// client_tile_resize once at the end.
		struct wlr_box *overview_boxes = calloc(n, sizeof(*overview_boxes));
		if (!overview_boxes) {
			free(items);
			free(placed);
			free(cands);
			free(feas);
			return;
		}
		for (int k = 0; k < n; k++) {
			float w = items[k].orig_w * best_s;
			float h = items[k].orig_h * best_s;
			int ix = (int)(base_x + placed[k].x + 0.5f);
			int iy = (int)(base_y + placed[k].y + 0.5f);
			int iw = (int)(ix + w + 0.5f) - ix;
			int ih = (int)(iy + h + 0.5f) - iy;
			overview_boxes[k] = (struct wlr_box){ix, iy, iw, ih};
		}

		for (int k = 0; k < n; k++) {
			client_tile_resize(items[k].c, overview_boxes[k], 0);
		}
		free(overview_boxes);
	}

	free(items);
	free(placed);
	free(cands);
	free(feas);
}

// Overview layout: focused window centered (about half screen width), remaining
// windows split on both sides.
void overview_layout_column(Monitor *m, Client **items, int cnt, float x,
							float top, float col_w, float col_h, float gap) {
	if (cnt <= 0)
		return;

	float *ws = calloc(cnt, sizeof(float));
	float *hs = calloc(cnt, sizeof(float));
	if (!ws || !hs) {
		free(ws);
		free(hs);
		return;
	}

	// Width fills the column width; height is proportional.
	float total_h = 0.0f;
	for (int i = 0; i < cnt; i++) {
		float ow = items[i]->overview_backup_geom.width;
		float oh = items[i]->overview_backup_geom.height;
		if (ow <= 0 || oh <= 0) {
			ow = 100.0f;
			oh = 100.0f;
		}
		ws[i] = col_w;
		hs[i] = col_w * (oh / ow);
		total_h += hs[i];
	}

	// Scales the whole item down when it is too tall.
	float gap_total = gap * (cnt - 1);
	if (total_h + gap_total > col_h) {
		float s = (col_h - gap_total) / total_h;
		if (s < 0.0f)
			s = 0.01f;
		for (int i = 0; i < cnt; i++) {
			ws[i] *= s;
			hs[i] *= s;
		}
		total_h *= s;
	}

	// Vertically centers when there is extra room.
	float y = top;
	if (total_h + gap_total < col_h)
		y = top + (col_h - (total_h + gap_total)) / 2.0f;

	for (int i = 0; i < cnt; i++) {
		int ix = (int)(x + (col_w - ws[i]) / 2.0f + 0.5f);
		int iy = (int)(y + 0.5f);
		client_tile_resize(items[i],
						   (struct wlr_box){ix, iy, (int)ws[i], (int)hs[i]}, 0);
		y += hs[i] + gap;
	}

	free(ws);
	free(hs);
}

void overview_scale_tab(Monitor *m) {
	int32_t target_gappo = config.overviewgappo;
	int32_t target_gappi = config.overviewgappi;

	if (m->visible_clients <= 0)
		return;

	Client **items = calloc(m->visible_clients, sizeof(Client *));
	if (!items)
		return;

	int n = 0;
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
			items[n++] = c;
		}
	}

	if (n == 0) {
		free(items);
		return;
	}

	// Index of the focused window (or the first one if none).
	Client *sel = m->sel;
	int focus_idx = 0;
	for (int i = 0; i < n; i++) {
		if (items[i] == sel) {
			focus_idx = i;
			break;
		}
	}

	// Uses the larger gap between columns and the smaller gap at the edges.
	float gap_mid = (float)target_gappo;
	float gap_edge = (float)target_gappi;
	if (gap_mid < gap_edge) {
		float tmp = gap_mid;
		gap_mid = gap_edge;
		gap_edge = tmp;
	}
	gap_mid *= 0.5f; /* Halves the gap between columns. */

	float avail_w = fmaxf(1.0f, m->w.width - 2 * gap_edge);
	float avail_h = fmaxf(1.0f, m->w.height - 2 * gap_edge);

	// Center column ratio is configurable; both sides split the remaining space
	// evenly.
	float center_w = avail_w * config.overcircle_center_ratio;
	float side_w = (avail_w - center_w - 2.0f * gap_mid) * 0.5f;
	if (side_w < 1.0f)
		side_w = 1.0f;

	float base_x = m->w.x + gap_edge;
	float base_y = m->w.y + gap_edge;

	float left_x = base_x;
	float center_x = base_x + side_w + gap_mid;
	float right_x = center_x + center_w + gap_mid;

	// The focused window is centered.
	Client *focus = items[focus_idx];
	{
		float ow = focus->overview_backup_geom.width;
		float oh = focus->overview_backup_geom.height;
		if (ow <= 0 || oh <= 0) {
			ow = 100.0f;
			oh = 100.0f;
		}
		float w = center_w;
		float h = w * (oh / ow);
		if (h > avail_h) {
			float s = avail_h / h;
			w *= s;
			h *= s;
		}
		int ix = (int)(center_x + (center_w - w) / 2.0f + 0.5f);
		int iy = (int)(base_y + (avail_h - h) / 2.0f + 0.5f);
		client_tile_resize(focus, (struct wlr_box){ix, iy, (int)w, (int)h}, 0);
	}

	// The rest split into the left/right columns; on focus change they
	// circulate (rotate).
	Client **left = calloc(n, sizeof(Client *));
	Client **right = calloc(n, sizeof(Client *));
	if (!left || !right) {
		free(items);
		free(left);
		free(right);
		return;
	}
	int rest = n - 1;
	int right_cnt = (rest + 1) / 2;
	int left_cnt = rest - right_cnt;

	int nr = 0;
	for (int k = 0; k < right_cnt; k++) {
		int idx = (focus_idx + 1 + k) % n;
		right[nr++] = items[idx];
	}
	int nl = 0;
	for (int k = 0; k < left_cnt; k++) {
		int idx = (focus_idx - 1 - k + n) % n;
		left[nl++] = items[idx];
	}

	overview_layout_column(m, left, nl, left_x, base_y, side_w, avail_h,
						   gap_edge);
	overview_layout_column(m, right, nr, right_x, base_y, side_w, avail_h,
						   gap_edge);

	free(items);
	free(left);
	free(right);
}

void create_jump_hints(Monitor *m) {
	// Uses the static default sequence when jump_labels is not configured.
	const char *jump_labels =
		config.jump_labels ? config.jump_labels : default_jump_labels;
	if (!jump_labels || !jump_labels[0])
		return;
	size_t jump_labels_len = strlen(jump_labels);
	int label_idx = 0;
	Client *c;

	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
			if (label_idx >= (int)jump_labels_len)
				break;
			char c_char = jump_labels[label_idx];
			c->jump_char = c_char;

			char label_text[2] = {c_char, '\0'};
			if (!c->jump_label_node)
				continue;
			mango_jump_label_node_update(c->jump_label_node, label_text,
										 m->wlr_output->scale);
			wlr_scene_node_set_enabled(&c->jump_label_node->scene_buffer->node,
									   true);
			wlr_scene_node_raise_to_top(
				&c->jump_label_node->scene_buffer->node);
			wlr_scene_node_set_position(
				&c->jump_label_node->scene_buffer->node,
				c->geom.width / 2 - c->jump_label_node->logical_width / 2,
				c->geom.height / 2 - c->jump_label_node->logical_height / 2);
			label_idx++;
		}
	}
}

void finish_jump_mode(Monitor *m) {
	if (!m->is_jump_mode)
		return;

	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon == m) {
			if (c->jump_label_node &&
				c->jump_label_node->scene_buffer->node.enabled) {
				c->jump_char = '\0';
				wlr_scene_node_set_enabled(
					&c->jump_label_node->scene_buffer->node, false);
			}
		}
	}
	m->is_jump_mode = 0;
}

void overview(Monitor *m) {
	if (m->ov_tab_layout && !m->is_jump_mode && !m->ov_normal_mode) {
		overview_scale_tab(m);
	} else {
		overview_scale(m);
	}

	if (m->is_jump_mode) {
		create_jump_hints(m);
	}
}
