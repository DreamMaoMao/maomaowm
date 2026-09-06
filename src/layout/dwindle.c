#include "mango/layout/dwindle.h"
#include "mango/common/globals.h"
#include "mango/manage/client.h"
#include "mango/common/util.h"
#include "mango/manage/monitor.h"

DwindleNode *dwindle_locked_h_node = NULL;
DwindleNode *dwindle_locked_v_node = NULL;

// 统计同方向上的节点总和 (N_old)
int count_block_items(DwindleNode *node, bool split_h) {
	if (!node)
		return 0;
	if (!node->is_split || node->split_h != split_h)
		return 1;
	return count_block_items(node->first, split_h) +
		   count_block_items(node->second, split_h);
}

DwindleNode *dwindle_find_leaf(DwindleNode *node, Client *c) {
	if (!node)
		return NULL;
	if (!node->is_split)
		return node->client == c ? node : NULL;
	DwindleNode *r = dwindle_find_leaf(node->first, c);
	return r ? r : dwindle_find_leaf(node->second, c);
}

DwindleNode *dwindle_first_leaf(DwindleNode *node) {
	if (!node)
		return NULL;
	while (node->is_split)
		node = node->first;
	return node;
}

void dwindle_free_tree(DwindleNode *node) {
	if (!node)
		return;
	dwindle_free_tree(node->first);
	dwindle_free_tree(node->second);
	free(node);
}

void dwindle_remove(DwindleNode **root, Client *c) {
	DwindleNode *leaf = dwindle_find_leaf(*root, c);
	if (!leaf)
		return;

	DwindleNode *parent = leaf->parent;

	if (dwindle_locked_h_node == leaf || dwindle_locked_h_node == parent)
		dwindle_locked_h_node = NULL;
	if (dwindle_locked_v_node == leaf || dwindle_locked_v_node == parent)
		dwindle_locked_v_node = NULL;

	if (!parent) {
		free(leaf);
		*root = NULL;
		return;
	}

	// 开始删除空间的比例回退逻辑

	// 查找连续的同方向块路径
	if (config.dwindle_manual_split) {
		bool split_h = parent->split_h;
		/* 第一遍统计同方向块路径长度 */
		int path_len = 1; /* parent */
		DwindleNode *cnt = parent->parent;
		while (cnt && cnt->split_h == split_h) {
			path_len++;
			cnt = cnt->parent;
		}

		DwindleNode **path = calloc(path_len, sizeof(*path));
		float *p = calloc(path_len, sizeof(*p));
		if (path && p) {
			path_len = 0;
			path[path_len++] = parent;
			DwindleNode *curr = parent->parent;
			while (curr && curr->split_h == split_h) {
				path[path_len++] = curr;
				curr = curr->parent;
			}

			// 计算各祖先的旧绝对占比
			p[path_len - 1] = 1.0f;
			for (int i = path_len - 1; i > 0; i--) {
				DwindleNode *S = path[i];
				DwindleNode *child = path[i - 1];
				if (S->first == child)
					p[i - 1] = p[i] * S->ratio;
				else
					p[i - 1] = p[i] * (1.0f - S->ratio);
			}

			// 计算即将被删除的叶子节点，在该方向块中所占的绝对面积比例 (P_del)
			float p_del =
				p[0] * (parent->first == leaf ? parent->ratio
											  : (1.0f - parent->ratio));
			if (p_del > 0.999f)
				p_del = 0.999f; // 兜底

			// 重算祖先比例：将 P_del 空出来的空间，按原定比例无缝分配给其他窗口
			for (int i = path_len - 1; i > 0; i--) {
				DwindleNode *S = path[i];
				DwindleNode *child = path[i - 1];
				float p_S = p[i];
				float p_first = p_S * S->ratio;

				float denom = p_S - p_del;
				if (denom < 0.0001f)
					denom = 0.0001f;

				if (S->first == child) {
					S->ratio = (p_first - p_del) / denom;
				} else {
					S->ratio = p_first / denom;
				}

				if (S->ratio < 0.001f)
					S->ratio = 0.001f;
				if (S->ratio > 0.999f)
					S->ratio = 0.999f;
			}
		}
		free(path);
		free(p);
	}

	// 比例重算结束

	// 基础的二叉树摘除节点逻辑
	DwindleNode *sibling =
		(parent->first == leaf) ? parent->second : parent->first;
	DwindleNode *grandparent = parent->parent;

	sibling->parent = grandparent;

	if (!sibling->is_split ||
		(!config.dwindle_preserve_split && !config.dwindle_smart_split)) {
		sibling->container_w = 0;
		sibling->container_h = 0;
	}

	if (!grandparent) {
		*root = sibling;
	} else {
		if (grandparent->first == parent)
			grandparent->first = sibling;
		else
			grandparent->second = sibling;
	}

	free(leaf);
	free(parent);
}

void dwindle_assign(DwindleNode *node, int32_t ax, int32_t ay, int32_t aw,
					int32_t ah, int32_t gap_h, int32_t gap_v) {
	if (!node)
		return;

	if (!node->is_split) {
		if (node->client) {
			if (!node->client->isfullscreen &&
				!node->client->ismaximizescreen) {
				struct wlr_box box = {ax, ay, MANGO_MAX(1, aw),
									  MANGO_MAX(1, ah)};
				client_tile_resize(node->client, box, 0);
			}
		}
		return;
	}

	if (!node->split_locked && node->container_w == 0 && node->container_h == 0)
		node->split_h = (aw >= ah);
	node->container_x = ax;
	node->container_y = ay;
	node->container_w = aw;
	node->container_h = ah;
	if (node->split_h) {
		int32_t w1 = MANGO_MAX(1, (int32_t)(aw * node->ratio) - gap_h / 2);
		dwindle_assign(node->first, ax, ay, w1, ah, gap_h, gap_v);
		dwindle_assign(node->second, ax + w1 + gap_h, ay, aw - w1 - gap_h, ah,
					   gap_h, gap_v);
	} else {
		int32_t h1 = MANGO_MAX(1, (int32_t)(ah * node->ratio) - gap_v / 2);
		dwindle_assign(node->first, ax, ay, aw, h1, gap_h, gap_v);
		dwindle_assign(node->second, ax, ay + h1 + gap_v, aw, ah - h1 - gap_v,
					   gap_h, gap_v);
	}
}

DwindleNode *dwindle_new_leaf(Client *c) {
	DwindleNode *n = calloc(1, sizeof(DwindleNode));
	n->client = c;
	return n;
}

// 向上查找方向块路径，并计算每个祖先节点的绝对占比
int get_block_path_and_ratios(DwindleNode *target, bool split_h,
							  DwindleNode ***path, float **p) {
	/* 第一遍统计同方向块深度，据此动态分配 */
	int depth = 1;
	DwindleNode *curr = target->parent;
	while (curr && curr->split_h == split_h) {
		depth++;
		curr = curr->parent;
	}

	*path = calloc(depth, sizeof(**path));
	*p = calloc(depth, sizeof(**p));
	if (!*path || !*p)
		return 0;

	int path_len = 0;
	(*path)[path_len++] = target;
	curr = target->parent;
	while (curr && curr->split_h == split_h) {
		(*path)[path_len++] = curr;
		curr = curr->parent;
	}

	(*p)[path_len - 1] = 1.0f; // 方向块根节点占比为 100%
	for (int i = path_len - 1; i > 0; i--) {
		DwindleNode *S = (*path)[i];
		DwindleNode *child = (*path)[i - 1];
		if (S->first == child)
			(*p)[i - 1] = (*p)[i] * S->ratio;
		else
			(*p)[i - 1] = (*p)[i] * (1.0f - S->ratio);
	}
	return path_len;
}

void dwindle_insert(DwindleNode **root, Client *new_c, Client *focused,
					float ratio, bool as_first, bool split_h, bool lock) {
	DwindleNode *new_leaf = dwindle_new_leaf(new_c);

	if (!*root) {
		new_leaf->custom_leaf_split_h = true;
		*root = new_leaf;
		return;
	}

	DwindleNode *target = focused ? dwindle_find_leaf(*root, focused) : NULL;
	if (!target)
		target = dwindle_first_leaf(*root);

	// ================= 保持其他窗口比例缩减逻辑 =================
	if (config.dwindle_manual_split) {
		DwindleNode **path = NULL;
		float *p = NULL;
		int path_len = get_block_path_and_ratios(target, split_h, &path, &p);

		if (path && p) {
			int n_old = 1;
			if (path_len > 1) {
				n_old = count_block_items(path[path_len - 1], split_h);
			}
			float N = (float)(n_old + 1);

			for (int i = path_len - 1; i > 0; i--) {
				DwindleNode *S = path[i];
				DwindleNode *child = path[i - 1];
				float p_S = p[i];
				float p_first = p_S * S->ratio;

				if (S->first == child) {
					float p_first_new = p_first * (N - 1.0f) / N + 1.0f / N;
					float p_S_new = p_S * (N - 1.0f) / N + 1.0f / N;
					S->ratio = p_first_new / p_S_new;
				} else {
					float p_first_new = p_first * (N - 1.0f) / N;
					float p_S_new = p_S * (N - 1.0f) / N + 1.0f / N;
					S->ratio = p_first_new / p_S_new;
				}
				if (S->ratio < 0.001f)
					S->ratio = 0.001f;
				if (S->ratio > 0.999f)
					S->ratio = 0.999f;
			}
		}
		free(path);
		free(p);
	}
	// ============================================================

	DwindleNode *split = calloc(1, sizeof(DwindleNode));
	split->is_split = true;
	split->split_h = split_h;
	split->split_locked = lock;
	split->custom_leaf_split_h = target->custom_leaf_split_h;
	new_leaf->custom_leaf_split_h = target->custom_leaf_split_h;

	if (as_first) {
		split->first = new_leaf;
		split->second = target;
	} else {
		split->first = target;
		split->second = new_leaf;
	}

	// 通用逻辑
	split->ratio = ratio;

	split->parent = target->parent;
	target->parent = split;
	new_leaf->parent = split;

	if (!split->parent) {
		*root = split;
	} else {
		if (split->parent->first == target)
			split->parent->first = split;
		else
			split->parent->second = split;
	}
}

void dwindle_move_client(DwindleNode **root, Client *c, Client *target,
						 float ratio, int32_t dir) {
	if (!c || !target || c == target)
		return;
	if (!dwindle_find_leaf(*root, c) || !dwindle_find_leaf(*root, target))
		return;
	dwindle_remove(root, c);
	bool as_first = (dir == UP || dir == LEFT);
	bool split_h = (dir == LEFT || dir == RIGHT);
	dwindle_insert(root, c, target, ratio, as_first, split_h, true);
}

void dwindle_swap_clients(Client *c1, Client *c2) {

	if (!c1 || !c2 || !c1->mon || !c2->mon || c1 == c2)
		return;

	Monitor *m1 = c1->mon;
	Monitor *m2 = c2->mon;

	DwindleNode **c1_root = &m1->pertag->dwindle_root[get_mon_curtag(m1)];
	DwindleNode *c1node = dwindle_find_leaf(*c1_root, c1);
	DwindleNode **c2_root = &m2->pertag->dwindle_root[get_mon_curtag(m2)];
	DwindleNode *c2node = dwindle_find_leaf(*c2_root, c2);

	client_swap_layout_properties(c1, c2);

	if (c1node)
		c1node->client = c2;
	if (c2node)
		c2node->client = c1;

	if (m1 != m2) {
		client_swap_monitors_and_tags(c1, c2);
	}

	wl_list_swap(&c1->link, &c2->link);
	finish_exchange_arrange_and_focus(c1, c2, m1, m2);
}

void dwindle_resize_client(Monitor *m, Client *c) {
	uint32_t tag = get_mon_curtag(m);
	DwindleNode *leaf = dwindle_find_leaf(m->pertag->dwindle_root[tag], c);
	if (!leaf)
		return;

	if (!start_drag_window) {
		start_drag_window = true;
		dwindle_locked_h_node = NULL;
		dwindle_locked_v_node = NULL;
		drag_begin_cursorx = cursor->x;
		drag_begin_cursory = cursor->y;
		DwindleNode *node = leaf->parent;
		while (node) {
			if (node->split_h && !dwindle_locked_h_node) {
				dwindle_locked_h_node = node;
				node->drag_init_ratio = node->ratio;
			}
			if (!node->split_h && !dwindle_locked_v_node) {
				dwindle_locked_v_node = node;
				node->drag_init_ratio = node->ratio;
			}
			if (dwindle_locked_h_node && dwindle_locked_v_node)
				break;
			node = node->parent;
		}
	}

	if (!dwindle_locked_h_node && !dwindle_locked_v_node)
		return;

	if (dwindle_locked_h_node) {
		float cw = (float)MANGO_MAX(1, dwindle_locked_h_node->container_w);
		float ox = (float)(cursor->x - drag_begin_cursorx);
		if (config.dwindle_smart_resize) {
			/* Move the boundary toward the cursor: invert direction when
			 * the drag started on the right side of the split line. */
			float split_x = dwindle_locked_h_node->container_x +
							cw * dwindle_locked_h_node->drag_init_ratio;
			if (drag_begin_cursorx >= split_x)
				ox = -ox;
		}
		dwindle_locked_h_node->ratio =
			dwindle_locked_h_node->drag_init_ratio + ox / cw;
		dwindle_locked_h_node->ratio =
			CLAMP_FLOAT(dwindle_locked_h_node->ratio, 0.05f, 0.95f);
	}

	if (dwindle_locked_v_node) {
		float ch = (float)MANGO_MAX(1, dwindle_locked_v_node->container_h);
		float oy = (float)(cursor->y - drag_begin_cursory);
		if (config.dwindle_smart_resize) {
			/* Same logic for the vertical split line. */
			float split_y = dwindle_locked_v_node->container_y +
							ch * dwindle_locked_v_node->drag_init_ratio;
			if (drag_begin_cursory >= split_y)
				oy = -oy;
		}
		dwindle_locked_v_node->ratio =
			dwindle_locked_v_node->drag_init_ratio + oy / ch;
		dwindle_locked_v_node->ratio =
			CLAMP_FLOAT(dwindle_locked_v_node->ratio, 0.05f, 0.95f);
	}

	int32_t n = m->visible_tiling_clients;
	int32_t gap_ih = enablegaps ? m->gappih : 0;
	int32_t gap_iv = enablegaps ? m->gappiv : 0;
	int32_t gap_oh = enablegaps ? m->gappoh : 0;
	int32_t gap_ov = enablegaps ? m->gappov : 0;
	if (config.smartgaps && n == 1)
		gap_ih = gap_iv = gap_oh = gap_ov = 0;

	dwindle_assign(m->pertag->dwindle_root[tag], m->w.x + gap_oh,
				   m->w.y + gap_ov, m->w.width - 2 * gap_oh,
				   m->w.height - 2 * gap_ov, gap_ih, gap_iv);
}

void dwindle_resize_client_step(Monitor *m, Client *c, int32_t dx, int32_t dy) {
	uint32_t tag = get_mon_curtag(m);
	DwindleNode *leaf = dwindle_find_leaf(m->pertag->dwindle_root[tag], c);
	if (!leaf)
		return;

	DwindleNode *h_node = NULL;
	DwindleNode *v_node = NULL;
	DwindleNode *node = leaf->parent;

	while (node) {
		if (node->split_h && !h_node)
			h_node = node;
		if (!node->split_h && !v_node)
			v_node = node;
		if (h_node && v_node)
			break;
		node = node->parent;
	}

	if (!h_node && !v_node)
		return;

	if (h_node && dx) {
		float cw = (float)MANGO_MAX(1, h_node->container_w);
		float delta = (float)dx / cw;
		h_node->ratio = CLAMP_FLOAT(h_node->ratio + delta, 0.05f, 0.95f);
	}

	if (v_node && dy) {
		float ch = (float)MANGO_MAX(1, v_node->container_h);
		float delta = (float)dy / ch;
		v_node->ratio = CLAMP_FLOAT(v_node->ratio + delta, 0.05f, 0.95f);
	}

	int32_t n_clients = m->visible_tiling_clients;
	int32_t gap_ih = enablegaps ? m->gappih : 0;
	int32_t gap_iv = enablegaps ? m->gappiv : 0;
	int32_t gap_oh = enablegaps ? m->gappoh : 0;
	int32_t gap_ov = enablegaps ? m->gappov : 0;
	if (config.smartgaps && n_clients == 1)
		gap_ih = gap_iv = gap_oh = gap_ov = 0;

	dwindle_assign(m->pertag->dwindle_root[tag], m->w.x + gap_oh,
				   m->w.y + gap_ov, m->w.width - 2 * gap_oh,
				   m->w.height - 2 * gap_ov, gap_ih, gap_iv);
}

void dwindle_remove_client(Client *c) {
	Monitor *m;
	wl_list_for_each(m, &mons, link) {
		for (uint32_t t = 0; t < PERTAG_SLOTS; t++)
			dwindle_remove(&m->pertag->dwindle_root[t], c);
	}
}

/* Insert a new client respecting dwindle_vsplit, dwindle_hsplit, and
 * dwindle_smart_split config options. */
void dwindle_insert_with_config(DwindleNode **root, Client *new_c,
								Client *focused, float ratio) {
	if (!new_c || !focused)
		return;

	bool as_first = false;
	bool split_h = false;
	bool lock = false;

	struct wlr_box *fg = &focused->geom;
	double fcx = fg->x + fg->width * 0.5;
	double fcy = fg->y + fg->height * 0.5;

	if (config.dwindle_smart_split) {
		double nx = (cursor->x - fcx) / (fg->width * 0.5);
		double ny = (cursor->y - fcy) / (fg->height * 0.5);

		if (fabs(ny) > fabs(nx)) {
			split_h = false;	 // vertical split
			as_first = (ny < 0); // top → new window on top
		} else {
			split_h = true;		 // horizontal split
			as_first = (nx < 0); // left → new window on left
		}
		lock = true; // lock split direction
	} else {
		// normal mode, auto split
		bool likely_h = (fg->width >= fg->height);
		split_h = likely_h;

		if (likely_h) {
			if (config.dwindle_hsplit == 0)
				as_first = (cursor->x < fcx);
			else
				as_first = (config.dwindle_hsplit == 2);
		} else {
			if (config.dwindle_vsplit == 0)
				as_first = (cursor->y < fcy);
			else
				as_first = (config.dwindle_vsplit == 2);
		}
	}

	DwindleNode *target = focused ? dwindle_find_leaf(*root, focused) : NULL;
	if (!target && *root)
		target = dwindle_first_leaf(*root);

	// 当且仅当 manual_split=1 时，计算精确的 1/N 新节点比例
	if (config.dwindle_manual_split && target) {
		split_h = target->custom_leaf_split_h;
		lock = true;
		as_first = false;

		// ================= 计算新节点的 1/N 比例 =================
		DwindleNode **path = NULL;
		float *p = NULL;
		int path_len = get_block_path_and_ratios(target, split_h, &path, &p);

		if (path && p) {
			int n_old = 1;
			if (path_len > 1) {
				n_old = count_block_items(path[path_len - 1], split_h);
			}
			float N = (float)(n_old + 1);

			float p_target_old = p[0];
			float p_split_new = p_target_old * (N - 1.0f) / N + 1.0f / N;

			if (as_first) {
				ratio = (1.0f / N) / p_split_new;
			} else {
				ratio = (p_target_old * (N - 1.0f) / N) / p_split_new;
			}

			if (ratio < 0.001f)
				ratio = 0.001f;
			if (ratio > 0.999f)
				ratio = 0.999f;
		}
		free(path);
		free(p);
		// =========================================================
	}

	// 调用通用插入函数
	dwindle_insert(root, new_c, focused, ratio, as_first, split_h, lock);
}

void dwindle(Monitor *m) {
	int32_t n = m->visible_tiling_clients;
	if (n == 0)
		return;

	uint32_t tag = get_mon_curtag(m);
	DwindleNode **root = &m->pertag->dwindle_root[tag];
	float ratio = config.dwindle_split_ratio;

	/* 统计全局客户端总数，作为动态数组的安全容量上界 */
	int32_t total_clients = 0;
	Client *c;
	wl_list_for_each(c, &clients, link) total_clients++;

	Client **vis = calloc(total_clients ? total_clients : 1, sizeof(*vis));
	DwindleNode **leaves =
		calloc(total_clients ? total_clients : 1, sizeof(*leaves));
	DwindleNode **stack = calloc((size_t)total_clients * 2 + 2, sizeof(*stack));
	if (!vis || !leaves || !stack) {
		free(vis);
		free(leaves);
		free(stack);
		return;
	}

	int32_t count = 0;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISTILED(c))
			vis[count++] = c;
	}

	// 清理树中已不存在的客户端
	int32_t lc = 0;
	int32_t sp = 0;
	if (*root)
		stack[sp++] = *root;
	while (sp > 0) {
		DwindleNode *nd = stack[--sp];
		if (!nd->is_split) {
			leaves[lc++] = nd;
		} else {
			if (nd->second)
				stack[sp++] = nd->second;
			if (nd->first)
				stack[sp++] = nd->first;
		}
	}

	for (int32_t i = 0; i < lc; i++) {
		bool found = false;
		for (int32_t j = 0; j < count; j++)
			if (vis[j] == leaves[i]->client) {
				found = true;
				break;
			}
		if (!found) {
			if (VISIBLEON(leaves[i]->client, m) &&
				(leaves[i]->client->isfullscreen ||
				 leaves[i]->client->ismaximizescreen))
				continue;
			dwindle_remove(root, leaves[i]->client);
		}
	}

	// 获得焦点客户端，若为空则用第一个可见平铺客户端兜底
	Client *focused = focustop(m);
	if (focused && !dwindle_find_leaf(*root, focused))
		focused = m->sel;

	if (!focused && count > 0)
		focused = vis[0];

	for (int32_t i = 0; i < count; i++) {
		if (!dwindle_find_leaf(*root, vis[i]))
			dwindle_insert_with_config(root, vis[i], focused, ratio);
	}

	int32_t gap_ih = enablegaps ? m->gappih : 0;
	int32_t gap_iv = enablegaps ? m->gappiv : 0;
	int32_t gap_oh = enablegaps ? m->gappoh : 0;
	int32_t gap_ov = enablegaps ? m->gappov : 0;
	if (config.smartgaps && n == 1)
		gap_ih = gap_iv = gap_oh = gap_ov = 0;

	dwindle_assign(*root, m->w.x + gap_oh, m->w.y + gap_ov,
				   m->w.width - 2 * gap_oh, m->w.height - 2 * gap_ov, gap_ih,
				   gap_iv);

	free(vis);
	free(leaves);
	free(stack);
}

void cleanup_monitor_dwindle(Monitor *m) {
	for (uint32_t t = 0; t < PERTAG_SLOTS; t++)
		dwindle_free_tree(m->pertag->dwindle_root[t]);
}
