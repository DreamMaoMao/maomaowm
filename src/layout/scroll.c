#include "mango/layout/scroll.h"
#include "mango/common/server.h"
#include "mango/manage/client.h"
#include "mango/layout/arrange.h"
#include "mango/common/util.h"
#include "mango/manage/monitor.h"

/* Gets or creates the scroller state for a given tag of the specified monitor.
 */
struct TagScrollerState *ensure_scroller_state(Monitor *m, uint32_t tag) {
	if (!m->pertag->scroller_state[tag]) {
		struct TagScrollerState *st =
			calloc(1, sizeof(struct TagScrollerState));
		m->pertag->scroller_state[tag] = st;
	}
	return m->pertag->scroller_state[tag];
}

/* Finds the node for a client in the tag state (returns NULL if absent). */
struct ScrollerStackNode *find_scroller_node(struct TagScrollerState *st,
											 Client *c) {
	if (!st)
		return NULL;
	for (struct ScrollerStackNode *n = st->all_first; n; n = n->all_next)
		if (n->client == c)
			return n;
	return NULL;
}

/* Creates a new node and inserts it into the tag state all list. */
struct ScrollerStackNode *scroller_node_create(struct TagScrollerState *st,
											   Client *c) {
	struct ScrollerStackNode *n = calloc(1, sizeof(*n));
	n->client = c;
	n->scroller_proportion = c->scroller_proportion;
	n->stack_proportion = c->stack_proportion;
	n->scroller_proportion_single = c->scroller_proportion_single;
	n->next_in_stack = NULL;
	n->prev_in_stack = NULL;
	n->all_next = st->all_first;
	st->all_first = n;
	st->count++;
	return n;
}

void scroller_node_remove(struct TagScrollerState *st,
						  struct ScrollerStackNode *target) {
	if (!st || !target)
		return;

	/* Saves the neighbours. */
	struct ScrollerStackNode *prev = target->prev_in_stack;
	struct ScrollerStackNode *next = target->next_in_stack;

	/* Unlinks from the stack list. */
	if (prev)
		prev->next_in_stack = next;
	if (next)
		next->prev_in_stack = prev;

	/* Unlinks from the all list. */
	struct ScrollerStackNode **indirect = &st->all_first;
	while (*indirect && *indirect != target)
		indirect = &(*indirect)->all_next;
	if (*indirect == target) {
		*indirect = target->all_next;
		st->count--;
	}
	free(target);
}

/* Clears all scroller state for a tag. */
void clear_scroller_state(struct TagScrollerState *st) {
	if (!st)
		return;
	struct ScrollerStackNode *n = st->all_first;
	while (n) {
		struct ScrollerStackNode *next = n->all_next;
		free(n);
		n = next;
	}
	free(st);
}

/* Cleans up scroller state of all tags when a Monitor is destroyed. */
void cleanup_monitor_scroller(Monitor *m) {
	for (int t = 0; t < PERTAG_SLOTS; t++) {
		if (m->pertag->scroller_state[t]) {
			clear_scroller_state(m->pertag->scroller_state[t]);
			m->pertag->scroller_state[t] = NULL;
		}
	}
}

/* Syncs a tag state back to the global fields of all clients. */
void sync_scroller_state_to_clients(Monitor *m, uint32_t tag) {
	struct TagScrollerState *st = m->pertag->scroller_state[tag];
	if (!st)
		return;
	for (struct ScrollerStackNode *n = st->all_first; n; n = n->all_next) {
		Client *c = n->client;
		c->scroller_proportion = n->scroller_proportion;
		c->stack_proportion = n->stack_proportion;
		c->scroller_proportion_single = n->scroller_proportion_single;
	}
}

void vertical_scroll_adjust_fullandmax(Client *c, struct wlr_box *target_geom) {
	Monitor *m = c->mon;
	int32_t cur_gappiv = server.enable_gaps ? m->gappiv : 0;
	int32_t cur_gappov = server.enable_gaps ? m->gappov : 0;
	int32_t cur_gappoh = server.enable_gaps ? m->gappoh : 0;

	cur_gappiv = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappiv;
	cur_gappov = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappov;
	cur_gappoh = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappoh;

	if (c->isfullscreen) {
		target_geom->width = m->m.width;
		target_geom->height = m->m.height;
		target_geom->x = m->m.x;
		return;
	}

	if (c->ismaximizescreen) {
		target_geom->width = m->w.width - 2 * cur_gappoh;
		target_geom->height = m->w.height - 2 * cur_gappov;
		target_geom->x = m->w.x + cur_gappoh;
		return;
	}

	target_geom->width = m->w.width - 2 * cur_gappoh;
	target_geom->x = m->w.x + (m->w.width - target_geom->width) / 2;
}

void vertical_check_scroller_root_inside_mon(Client *c,
											 struct wlr_box *geometry) {
	if (!c || !c->mon)
		return;
	if (!GEOMINSIDEMON(geometry, c->mon)) {
		geometry->y = c->mon->w.y + (c->mon->w.height - geometry->height) / 2;
	}
}

void horizontal_scroll_adjust_fullandmax(Client *c,
										 struct wlr_box *target_geom) {
	Monitor *m = c->mon;
	int32_t cur_gappih = server.enable_gaps ? m->gappih : 0;
	int32_t cur_gappoh = server.enable_gaps ? m->gappoh : 0;
	int32_t cur_gappov = server.enable_gaps ? m->gappov : 0;

	cur_gappih = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappih;
	cur_gappoh = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappoh;
	cur_gappov = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappov;

	if (c->isfullscreen) {
		target_geom->height = m->m.height;
		target_geom->width = m->m.width;
		target_geom->y = m->m.y;
		return;
	}

	if (c->ismaximizescreen) {
		target_geom->height = m->w.height - 2 * cur_gappov;
		target_geom->width = m->w.width - 2 * cur_gappoh;
		target_geom->y = m->w.y + cur_gappov;
		return;
	}

	target_geom->height = m->w.height - 2 * cur_gappov;
	target_geom->y = m->w.y + (m->w.height - target_geom->height) / 2;
}

void horizontal_check_scroller_root_inside_mon(Client *c,
											   struct wlr_box *geometry) {
	if (!c || !c->mon)
		return;
	if (!GEOMINSIDEMON(geometry, c->mon)) {
		geometry->x = c->mon->w.x + (c->mon->w.width - geometry->width) / 2;
	}
}

void arrange_stack_node(struct ScrollerStackNode *head, struct wlr_box geometry,
						int32_t gappiv) {
	int32_t stack_size = 0;
	struct ScrollerStackNode *iter = head;
	while (iter) {
		stack_size++;
		iter = iter->next_in_stack;
	}
	if (stack_size == 0)
		return;

	/* Normalized ratio. */
	float total_proportion = 0.0f;
	iter = head;
	while (iter) {
		if (iter->stack_proportion <= 0.0f || iter->stack_proportion >= 1.0f)
			iter->stack_proportion =
				stack_size == 1 ? 1.0f : 1.0f / (stack_size - 1);
		total_proportion += iter->stack_proportion;
		iter = iter->next_in_stack;
	}
	iter = head;
	while (iter) {
		iter->stack_proportion /= total_proportion;
		iter = iter->next_in_stack;
	}

	/* Vertical arrangement (horizontal stack). */
	int32_t client_height;
	int32_t current_y = geometry.y;
	int32_t remain_client_height = geometry.height - (stack_size - 1) * gappiv;
	float remain_proportion = 1.0f;

	iter = head;
	while (iter) {
		client_height =
			remain_client_height * (iter->stack_proportion / remain_proportion);
		struct wlr_box client_geom = {.x = geometry.x,
									  .y = current_y,
									  .width = geometry.width,
									  .height = client_height};
		client_tile_resize(iter->client, client_geom, 0);
		remain_proportion -= iter->stack_proportion;
		remain_client_height -= client_height;
		current_y += client_height + gappiv;
		iter = iter->next_in_stack;
	}
}

void arrange_stack_vertical_node(struct ScrollerStackNode *head,
								 struct wlr_box geometry, int32_t gappih) {
	int32_t stack_size = 0;
	struct ScrollerStackNode *iter = head;
	while (iter) {
		stack_size++;
		iter = iter->next_in_stack;
	}
	if (stack_size == 0)
		return;

	/* Normalized ratio. */
	float total_proportion = 0.0f;
	iter = head;
	while (iter) {
		if (iter->stack_proportion <= 0.0f || iter->stack_proportion >= 1.0f)
			iter->stack_proportion =
				stack_size == 1 ? 1.0f : 1.0f / (stack_size - 1);
		total_proportion += iter->stack_proportion;
		iter = iter->next_in_stack;
	}
	iter = head;
	while (iter) {
		iter->stack_proportion /= total_proportion;
		iter = iter->next_in_stack;
	}

	/* Horizontal arrangement (vertical stack). */
	int32_t client_width;
	int32_t current_x = geometry.x;
	int32_t remain_client_width = geometry.width - (stack_size - 1) * gappih;
	float remain_proportion = 1.0f;

	iter = head;
	while (iter) {
		client_width =
			remain_client_width * (iter->stack_proportion / remain_proportion);
		struct wlr_box client_geom = {.y = geometry.y,
									  .x = current_x,
									  .height = geometry.height,
									  .width = client_width};
		client_tile_resize(iter->client, client_geom, 0);
		remain_proportion -= iter->stack_proportion;
		remain_client_width -= client_width;
		current_x += client_width + gappih;
		iter = iter->next_in_stack;
	}
}

void scroller(Monitor *m) {
	uint32_t tag = get_mon_curtag(m);
	struct TagScrollerState *st = ensure_scroller_state(m, tag);
	Client *c = NULL;
	float scroller_default_proportion_single =
		m->pertag->scroller_default_proportion_single[tag];
	int32_t scroller_ignore_proportion_single =
		m->pertag->scroller_ignore_proportion_single[tag];

	/* Counts visible tiled stack heads. */
	int32_t n_heads = 0;
	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c)) {
			struct ScrollerStackNode *node = find_scroller_node(st, c);
			if (node && !node->prev_in_stack)
				n_heads++;
		}
	}

	if (n_heads == 0) {
		sync_scroller_state_to_clients(m, tag);
		return;
	}

	/* Collects all stack heads in global client list order to keep the visual
	 * order correct. */
	struct ScrollerStackNode **heads = calloc(n_heads, sizeof(*heads));
	if (!heads) {
		sync_scroller_state_to_clients(m, tag);
		return;
	}
	int32_t head_idx = 0;
	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c)) {
			struct ScrollerStackNode *node = find_scroller_node(st, c);
			if (node && !node->prev_in_stack) {
				bool already = false;
				for (int k = 0; k < head_idx; k++) {
					if (heads[k] == node) {
						already = true;
						break;
					}
				}
				if (!already)
					heads[head_idx++] = node;
			}
		}
	}
	n_heads = head_idx;

	m->visible_scroll_tiling_clients = n_heads;

	int32_t cur_gappih = server.enable_gaps ? m->gappih : 0;
	int32_t cur_gappoh = server.enable_gaps ? m->gappoh : 0;
	int32_t cur_gappov = server.enable_gaps ? m->gappov : 0;
	int32_t cur_gappiv = server.enable_gaps ? m->gappiv : 0;
	if (config.smartgaps && n_heads == 1) {
		cur_gappih = cur_gappoh = cur_gappov = 0;
	}
	int32_t max_client_width =
		m->w.width - 2 * config.scroller_structs - cur_gappih;

	/* Single-client special case. */
	if (n_heads == 1 && !scroller_ignore_proportion_single &&
		!heads[0]->client->isfullscreen &&
		!heads[0]->client->ismaximizescreen) {
		struct ScrollerStackNode *head = heads[0];
		float single_proportion = head->scroller_proportion_single > 0.0f
									  ? head->scroller_proportion_single
									  : scroller_default_proportion_single;
		struct wlr_box target_geom;
		target_geom.height = m->w.height - 2 * cur_gappov;
		target_geom.width = (m->w.width - 2 * cur_gappoh) * single_proportion;
		target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
		target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
		horizontal_check_scroller_root_inside_mon(head->client, &target_geom);
		arrange_stack_node(head, target_geom, cur_gappiv);
		sync_scroller_state_to_clients(m, tag);
		free(heads);
		return;
	}

	struct ScrollerStackNode *root_node = NULL;
	if (m->sel && ISSCROLLTILED(m->sel)) {
		root_node = find_scroller_node(st, m->sel);
		if (root_node) {
			while (root_node->prev_in_stack)
				root_node = root_node->prev_in_stack;
		}
	}
	if (!root_node && m->prevsel && ISSCROLLTILED(m->prevsel)) {
		root_node = find_scroller_node(st, m->prevsel);
		if (root_node) {
			while (root_node->prev_in_stack)
				root_node = root_node->prev_in_stack;
		}
	}
	if (!root_node)
		root_node = heads[n_heads / 2]; /* Simple fallback. */

	int32_t focus_index = -1;
	for (int i = 0; i < n_heads; i++) {
		if (heads[i] == root_node) {
			focus_index = i;
			break;
		}
	}
	if (focus_index < 0)
		focus_index = n_heads / 2;

	/* Decides whether scrolling, overspread, or centering is needed. */
	bool need_scroller = false;
	bool over_overspread_to_left = false;
	Client *root_client = root_node->client;

	if (root_client->geom.x >= m->w.x + config.scroller_structs &&
		root_client->geom.x + root_client->geom.width <=
			m->w.x + m->w.width - config.scroller_structs) {
		need_scroller = false;
	} else {
		need_scroller = true;
	}

	bool need_apply_overspread =
		config.scroller_prefer_overspread && n_heads > 1 &&
		(focus_index == 0 || focus_index == n_heads - 1) &&
		heads[focus_index]->scroller_proportion < 1.0f;

	if (need_apply_overspread) {
		if (focus_index == 0) {
			over_overspread_to_left = true;
		} else {
			over_overspread_to_left = false;
		}
		if (over_overspread_to_left &&
			(!INSIDEMON(heads[1]->client) ||
			 (heads[1]->scroller_proportion + heads[0]->scroller_proportion >=
			  1.0f))) {
			need_scroller = true;
		} else if (!over_overspread_to_left &&
				   (!INSIDEMON(heads[focus_index - 1]->client) ||
					(heads[focus_index - 1]->scroller_proportion +
						 heads[focus_index]->scroller_proportion >=
					 1.0f))) {
			need_scroller = true;
		} else {
			need_apply_overspread = false;
		}
	}

	bool need_apply_center =
		config.scroller_focus_center || n_heads == 1 ||
		(config.scroller_prefer_center && !need_apply_overspread &&
		 (!m->prevsel ||
		  (ISSCROLLTILED(m->prevsel) &&
		   (m->prevsel->scroller_proportion * max_client_width) +
				   (heads[focus_index]->scroller_proportion *
					max_client_width) >
			   m->w.width - 2 * config.scroller_structs - cur_gappih)));

	if (n_heads == 1 && scroller_ignore_proportion_single) {
		need_scroller = true;
	}
	if (server.start_drag_window)
		need_scroller = false;

	struct wlr_box target_geom;
	target_geom.height = m->w.height - 2 * cur_gappov;
	target_geom.width =
		max_client_width * heads[focus_index]->scroller_proportion;
	target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
	horizontal_scroll_adjust_fullandmax(heads[focus_index]->client,
										&target_geom);

	if (heads[focus_index]->client->isfullscreen) {
		target_geom.x = m->m.x;
		horizontal_check_scroller_root_inside_mon(heads[focus_index]->client,
												  &target_geom);
		arrange_stack_node(heads[focus_index], target_geom, cur_gappiv);
	} else if (heads[focus_index]->client->ismaximizescreen) {
		target_geom.x = m->w.x + cur_gappoh;
		horizontal_check_scroller_root_inside_mon(heads[focus_index]->client,
												  &target_geom);
		arrange_stack_node(heads[focus_index], target_geom, cur_gappiv);
	} else if (need_scroller) {
		if (need_apply_center) {
			target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
		} else if (need_apply_overspread) {
			if (over_overspread_to_left) {
				target_geom.x = m->w.x + config.scroller_structs;
			} else {
				target_geom.x =
					m->w.x + (m->w.width -
							  heads[focus_index]->scroller_proportion *
								  max_client_width -
							  config.scroller_structs);
			}
		} else {
			target_geom.x =
				root_client->geom.x > m->w.x + (m->w.width) / 2
					? m->w.x + (m->w.width -
								heads[focus_index]->scroller_proportion *
									max_client_width -
								config.scroller_structs)
					: m->w.x + config.scroller_structs;
		}
		horizontal_check_scroller_root_inside_mon(heads[focus_index]->client,
												  &target_geom);
		arrange_stack_node(heads[focus_index], target_geom, cur_gappiv);
	} else {
		target_geom.x = root_client->geom.x;
		horizontal_check_scroller_root_inside_mon(heads[focus_index]->client,
												  &target_geom);
		arrange_stack_node(heads[focus_index], target_geom, cur_gappiv);
	}

	/* Arranges the left stack. */
	for (int i = 1; i <= focus_index; i++) {
		struct ScrollerStackNode *cur = heads[focus_index - i];
		struct wlr_box left_geom;
		left_geom.height = m->w.height - 2 * cur_gappov;
		left_geom.width = max_client_width * cur->scroller_proportion;
		horizontal_scroll_adjust_fullandmax(cur->client, &left_geom);
		left_geom.x = heads[focus_index - i + 1]->client->geom.x - cur_gappih -
					  left_geom.width;
		arrange_stack_node(cur, left_geom, cur_gappiv);
	}

	/* Arranges the right stack. */
	for (int i = 1; i < n_heads - focus_index; i++) {
		struct ScrollerStackNode *cur = heads[focus_index + i];
		struct wlr_box right_geom;
		right_geom.height = m->w.height - 2 * cur_gappov;
		right_geom.width = max_client_width * cur->scroller_proportion;
		horizontal_scroll_adjust_fullandmax(cur->client, &right_geom);
		right_geom.x = heads[focus_index + i - 1]->client->geom.x + cur_gappih +
					   heads[focus_index + i - 1]->client->geom.width;
		arrange_stack_node(cur, right_geom, cur_gappiv);
	}

	sync_scroller_state_to_clients(m, tag);
	free(heads);
}

void vertical_scroller(Monitor *m) {
	uint32_t tag = get_mon_curtag(m);
	int32_t bar_height = 0;
	struct TagScrollerState *st = ensure_scroller_state(m, tag);
	Client *c = NULL;
	float scroller_default_proportion_single =
		m->pertag->scroller_default_proportion_single[tag];
	int32_t scroller_ignore_proportion_single =
		m->pertag->scroller_ignore_proportion_single[tag];

	/* Counts visible tiled stack heads. */
	int32_t n_heads = 0;
	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c)) {
			struct ScrollerStackNode *node = find_scroller_node(st, c);
			if (node && !node->prev_in_stack)
				n_heads++;
		}
	}

	if (n_heads == 0) {
		sync_scroller_state_to_clients(m, tag);
		return;
	}

	/* Collects stack heads in global order. */
	struct ScrollerStackNode **heads = calloc(n_heads, sizeof(*heads));
	if (!heads) {
		sync_scroller_state_to_clients(m, tag);
		return;
	}
	int32_t head_idx = 0;
	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c)) {
			struct ScrollerStackNode *node = find_scroller_node(st, c);
			if (node && !node->prev_in_stack) {
				bool already = false;
				for (int k = 0; k < head_idx; k++)
					if (heads[k] == node)
						already = true;
				if (!already)
					heads[head_idx++] = node;
			}
		}
	}
	n_heads = head_idx;

	m->visible_scroll_tiling_clients = n_heads;

	int32_t cur_gappiv = server.enable_gaps ? m->gappiv : 0;
	int32_t cur_gappov = server.enable_gaps ? m->gappov : 0;
	int32_t cur_gappoh = server.enable_gaps ? m->gappoh : 0;
	int32_t cur_gappih = server.enable_gaps ? m->gappih : 0;
	if (config.smartgaps && n_heads == 1) {
		cur_gappiv = cur_gappov = cur_gappoh = 0;
	}
	int32_t max_client_height =
		m->w.height - 2 * config.scroller_structs - cur_gappiv;

	if (n_heads == 1 && !scroller_ignore_proportion_single &&
		!heads[0]->client->isfullscreen &&
		!heads[0]->client->ismaximizescreen) {
		struct ScrollerStackNode *head = heads[0];
		float single_proportion = head->scroller_proportion_single > 0.0f
									  ? head->scroller_proportion_single
									  : scroller_default_proportion_single;
		struct wlr_box target_geom;
		target_geom.width = m->w.width - 2 * cur_gappoh;
		target_geom.height = (m->w.height - 2 * cur_gappov) * single_proportion;
		target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
		target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
		vertical_check_scroller_root_inside_mon(head->client, &target_geom);
		arrange_stack_vertical_node(head, target_geom, cur_gappih);
		sync_scroller_state_to_clients(m, tag);
		free(heads);
		return;
	}

	struct ScrollerStackNode *root_node = NULL;
	if (m->sel && ISSCROLLTILED(m->sel)) {
		root_node = find_scroller_node(st, m->sel);
		if (root_node) {
			while (root_node->prev_in_stack)
				root_node = root_node->prev_in_stack;
		}
	}
	if (!root_node && m->prevsel && ISSCROLLTILED(m->prevsel)) {
		root_node = find_scroller_node(st, m->prevsel);
		if (root_node) {
			while (root_node->prev_in_stack)
				root_node = root_node->prev_in_stack;
		}
	}
	if (!root_node)
		root_node = heads[n_heads / 2];

	int32_t focus_index = -1;
	for (int i = 0; i < n_heads; i++) {
		if (heads[i] == root_node) {
			focus_index = i;
			break;
		}
	}
	if (focus_index < 0)
		focus_index = n_heads / 2;

	bool need_scroller = false;
	bool over_overspread_to_up = false;
	Client *root_client = root_node->client;

	if (root_client->geom.y >= m->w.y + config.scroller_structs &&
		root_client->geom.y + root_client->geom.height <=
			m->w.y + m->w.height - config.scroller_structs) {
		need_scroller = false;
	} else {
		need_scroller = true;
	}

	bool need_apply_overspread =
		config.scroller_prefer_overspread && n_heads > 1 &&
		(focus_index == 0 || focus_index == n_heads - 1) &&
		heads[focus_index]->scroller_proportion < 1.0f;

	if (need_apply_overspread) {
		if (focus_index == 0) {
			over_overspread_to_up = true;
		} else {
			over_overspread_to_up = false;
		}
		if (over_overspread_to_up &&
			(!INSIDEMON(heads[1]->client) ||
			 (heads[1]->scroller_proportion + heads[0]->scroller_proportion >=
			  1.0f))) {
			need_scroller = true;
		} else if (!over_overspread_to_up &&
				   (!INSIDEMON(heads[focus_index - 1]->client) ||
					(heads[focus_index - 1]->scroller_proportion +
						 heads[focus_index]->scroller_proportion >=
					 1.0f))) {
			need_scroller = true;
		} else {
			need_apply_overspread = false;
		}
	}

	bool need_apply_center =
		config.scroller_focus_center || n_heads == 1 ||
		(config.scroller_prefer_center && !need_apply_overspread &&
		 (!m->prevsel ||
		  (ISSCROLLTILED(m->prevsel) &&
		   (m->prevsel->scroller_proportion * max_client_height) +
				   (heads[focus_index]->scroller_proportion *
					max_client_height) >
			   m->w.height - 2 * config.scroller_structs - cur_gappiv)));

	if (n_heads == 1 && scroller_ignore_proportion_single) {
		need_scroller = true;
	}
	if (server.start_drag_window)
		need_scroller = false;

	struct wlr_box target_geom;
	target_geom.width = m->w.width - 2 * cur_gappoh;
	target_geom.height =
		max_client_height * heads[focus_index]->scroller_proportion;
	target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
	vertical_scroll_adjust_fullandmax(heads[focus_index]->client, &target_geom);

	if (heads[focus_index]->client->isfullscreen) {
		target_geom.y = m->m.y;
		vertical_check_scroller_root_inside_mon(heads[focus_index]->client,
												&target_geom);
		arrange_stack_vertical_node(heads[focus_index], target_geom,
									cur_gappih);
	} else if (heads[focus_index]->client->ismaximizescreen) {
		target_geom.y = m->w.y + cur_gappov;
		vertical_check_scroller_root_inside_mon(heads[focus_index]->client,
												&target_geom);
		arrange_stack_vertical_node(heads[focus_index], target_geom,
									cur_gappih);
	} else if (need_scroller) {
		if (need_apply_center) {
			target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
		} else if (need_apply_overspread) {
			if (over_overspread_to_up) {
				target_geom.y = m->w.y + config.scroller_structs;
			} else {
				target_geom.y =
					m->w.y + (m->w.height -
							  heads[focus_index]->scroller_proportion *
								  max_client_height -
							  config.scroller_structs);
			}
		} else {
			target_geom.y =
				root_client->geom.y > m->w.y + (m->w.height) / 2
					? m->w.y + (m->w.height -
								heads[focus_index]->scroller_proportion *
									max_client_height -
								config.scroller_structs)
					: m->w.y + config.scroller_structs;
		}
		vertical_check_scroller_root_inside_mon(heads[focus_index]->client,
												&target_geom);
		arrange_stack_vertical_node(heads[focus_index], target_geom,
									cur_gappih);
	} else {
		bar_height = !root_client->isfullscreen && (root_client->group_prev ||
													root_client->group_next)
						 ? config.group_bar_height
						 : 0;

		target_geom.y = root_client->geom.y - bar_height;
		vertical_check_scroller_root_inside_mon(heads[focus_index]->client,
												&target_geom);
		arrange_stack_vertical_node(heads[focus_index], target_geom,
									cur_gappih);
	}

	for (int i = 1; i <= focus_index; i++) {
		struct ScrollerStackNode *cur = heads[focus_index - i];
		struct wlr_box up_geom;
		up_geom.width = m->w.width - 2 * cur_gappoh;
		up_geom.height = max_client_height * cur->scroller_proportion;
		vertical_scroll_adjust_fullandmax(cur->client, &up_geom);

		bar_height = !heads[focus_index - i + 1]->client->isfullscreen &&
							 (heads[focus_index - i + 1]->client->group_prev ||
							  heads[focus_index - i + 1]->client->group_next)
						 ? config.group_bar_height
						 : 0;

		up_geom.y = heads[focus_index - i + 1]->client->geom.y - cur_gappiv -
					up_geom.height - bar_height;
		arrange_stack_vertical_node(cur, up_geom, cur_gappih);
	}

	for (int i = 1; i < n_heads - focus_index; i++) {
		struct ScrollerStackNode *cur = heads[focus_index + i];
		struct wlr_box down_geom;
		down_geom.width = m->w.width - 2 * cur_gappoh;
		down_geom.height = max_client_height * cur->scroller_proportion;
		vertical_scroll_adjust_fullandmax(cur->client, &down_geom);
		down_geom.y = heads[focus_index + i - 1]->client->geom.y + cur_gappiv +
					  heads[focus_index + i - 1]->client->geom.height;
		arrange_stack_vertical_node(cur, down_geom, cur_gappih);
	}

	sync_scroller_state_to_clients(m, tag);
	free(heads);
}

void scroller_remove_client(Client *c) {
	Monitor *m;
	wl_list_for_each(m, &server.monitors, link) {
		for (uint32_t t = 0; t < PERTAG_SLOTS; t++) {
			struct TagScrollerState *st = m->pertag->scroller_state[t];
			if (!st)
				continue;
			struct ScrollerStackNode *node = find_scroller_node(st, c);
			if (node) {
				scroller_node_remove(st, node);
			}
		}
	}
}

void scroller_insert_stack(Client *c, Client *target_client,
						   bool insert_before) {
	if (!target_client || target_client->mon != c->mon)
		return;

	if (c->isfullscreen)
		client_apply_fullscreen(c, 0, true);
	if (c->ismaximizescreen)
		client_set_maximize_screen(c, 0, true);

	Monitor *m = c->mon;
	uint32_t tag = get_mon_curtag(m);
	struct TagScrollerState *st = ensure_scroller_state(m, tag);

	struct ScrollerStackNode *cnode = find_scroller_node(st, c);
	if (cnode)
		scroller_node_remove(st, cnode);

	struct ScrollerStackNode *tnode = find_scroller_node(st, target_client);
	if (!tnode)
		tnode = scroller_node_create(st, target_client);

	struct ScrollerStackNode *newnode = scroller_node_create(st, c);
	/* Inserts the new node before or after tnode. */
	if (insert_before) {
		newnode->next_in_stack = tnode;
		newnode->prev_in_stack = tnode->prev_in_stack;
		if (tnode->prev_in_stack)
			tnode->prev_in_stack->next_in_stack = newnode;
		tnode->prev_in_stack = newnode;
		wl_list_safe_reinsert_prev(&tnode->client->link, &c->link);
	} else {
		newnode->prev_in_stack = tnode;
		newnode->next_in_stack = tnode->next_in_stack;
		if (tnode->next_in_stack)
			tnode->next_in_stack->prev_in_stack = newnode;
		tnode->next_in_stack = newnode;
		wl_list_safe_reinsert_next(&tnode->client->link, &c->link);
	}

	/* Handles fullscreen/maximized state of the stack head. */
	struct ScrollerStackNode *head = tnode;
	while (head->prev_in_stack)
		head = head->prev_in_stack;
	Client *stack_head = head->client;
	if (stack_head->ismaximizescreen)
		client_set_maximize_screen(stack_head, 0, true);
	if (stack_head->isfullscreen)
		client_apply_fullscreen(stack_head, 0, true);

	/* Syncs to the Client fields. */
	sync_scroller_state_to_clients(m, tag);

	arrange(m, false, false);
}

void scroller_drop_tile(Client *c, Client *closest, int vertical) {

	// Must update first; otherwise nodes inside still hold cnode info and
	// stack_head/stack_tail would point at the wrong clients.
	update_scroller_state(c->mon);

	Client *stack_head = scroll_get_stack_head_client(closest);
	Client *stack_tail = scroll_get_stack_tail_client(closest);

	if (vertical) {
		if (closest->drop_direction == LEFT) {
			client_set_floating(c, 0);
			scroller_insert_stack(c, closest, true);
			return;
		} else if (closest->drop_direction == RIGHT) {
			client_set_floating(c, 0);
			scroller_insert_stack(c, closest, false);
			return;
		} else if (closest->drop_direction == UP) {
			if (c != stack_head) {
				wl_list_safe_reinsert_prev(&stack_head->link, &c->link);
			}
		} else if (closest->drop_direction == DOWN) {
			if (c != stack_tail) {
				wl_list_safe_reinsert_next(&stack_head->link, &c->link);
			}
		}
	} else {
		if (closest->drop_direction == UP) {
			client_set_floating(c, 0);
			scroller_insert_stack(c, closest, true);
			return;
		} else if (closest->drop_direction == DOWN) {
			client_set_floating(c, 0);
			scroller_insert_stack(c, closest, false);
			return;
		} else if (closest->drop_direction == LEFT) {
			if (c != stack_head) {
				wl_list_safe_reinsert_prev(&stack_head->link, &c->link);
			}
		} else if (closest->drop_direction == RIGHT) {
			if (c != stack_tail) {
				wl_list_safe_reinsert_next(&stack_head->link, &c->link);
			}
		}
	}

	client_set_floating(c, 0);
}

Client *scroll_get_stack_head_client(Client *c) {
	if (!c || !c->mon)
		return c;
	uint32_t tag = get_client_tag_idx(c);
	struct TagScrollerState *st = c->mon->pertag->scroller_state[tag];
	if (st) {
		struct ScrollerStackNode *n = find_scroller_node(st, c);
		if (n) {
			while (n->prev_in_stack)
				n = n->prev_in_stack;
			return n->client;
		}
	}
	return c;
}

Client *scroll_get_stack_tail_client(Client *c) {
	if (!c || !c->mon)
		return c;
	uint32_t tag = get_client_tag_idx(c);
	struct TagScrollerState *st = c->mon->pertag->scroller_state[tag];
	if (st) {
		struct ScrollerStackNode *n = find_scroller_node(st, c);
		if (n) {
			while (n->next_in_stack)
				n = n->next_in_stack;
			return n->client;
		}
	}
	return c;
}

void update_scroller_state(Monitor *m) {
	uint32_t tag = get_mon_curtag(m);
	struct TagScrollerState *st = ensure_scroller_state(m, tag);

	/* Collects all currently visible scroller tiled windows. */
	int32_t count = 0;
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c))
			count++;
	}

	Client **vis = calloc(count ? count : 1, sizeof(*vis));
	if (!vis)
		return;
	int32_t vi = 0;
	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c))
			vis[vi++] = c;
	}

	struct ScrollerStackNode *n = st->all_first;
	while (n) {
		bool found = false;
		for (int i = 0; i < count; i++) {
			if (vis[i] == n->client) {
				found = true;
				break;
			}
		}
		struct ScrollerStackNode *next = n->all_next;
		if (!found)
			scroller_node_remove(st, n);
		n = next;
	}

	/* Creates nodes for newly visible windows. */
	for (int i = 0; i < count; i++) {
		if (!find_scroller_node(st, vis[i])) {
			scroller_node_create(st, vis[i]);
		}
	}

	free(vis);
}

void scroller_swap_nodes_in_same_stack(struct ScrollerStackNode *n1,
									   struct ScrollerStackNode *n2) {
	float tmp_sc = n1->scroller_proportion;
	float tmp_st = n1->stack_proportion;
	n1->scroller_proportion = n2->scroller_proportion;
	n1->stack_proportion = n2->stack_proportion;
	n2->scroller_proportion = tmp_sc;
	n2->stack_proportion = tmp_st;

	struct ScrollerStackNode *p1 = n1->prev_in_stack;
	struct ScrollerStackNode *next1 = n1->next_in_stack;
	struct ScrollerStackNode *p2 = n2->prev_in_stack;
	struct ScrollerStackNode *next2 = n2->next_in_stack;

	if (n1->next_in_stack == n2) {
		n1->next_in_stack = next2;
		n2->prev_in_stack = p1;
		n1->prev_in_stack = n2;
		n2->next_in_stack = n1;
		if (p1)
			p1->next_in_stack = n2;
		if (next2)
			next2->prev_in_stack = n1;
	} else if (n2->next_in_stack == n1) {
		n2->next_in_stack = next1;
		n1->prev_in_stack = p2;
		n2->prev_in_stack = n1;
		n1->next_in_stack = n2;
		if (p2)
			p2->next_in_stack = n1;
		if (next1)
			next1->prev_in_stack = n2;
	} else {
		if (p1)
			p1->next_in_stack = n2;
		if (next1)
			next1->prev_in_stack = n2;
		if (p2)
			p2->next_in_stack = n1;
		if (next2)
			next2->prev_in_stack = n1;
		n1->prev_in_stack = p2;
		n1->next_in_stack = next2;
		n2->prev_in_stack = p1;
		n2->next_in_stack = next1;
	}
}

void scroller_swap_different_stacks(struct ScrollerStackNode *head1,
									struct ScrollerStackNode *head2) {
	Client *head1_c = head1->client;
	Client *head2_c = head2->client;
	Client *tail1_c = scroll_get_stack_tail_client(head1_c);
	Client *tail2_c = scroll_get_stack_tail_client(head2_c);

	struct wl_list *p1 = head1_c->link.prev;
	struct wl_list *n1_next = tail1_c->link.next;
	struct wl_list *p2 = head2_c->link.prev;
	struct wl_list *n2_next = tail2_c->link.next;

	if (n1_next == &head2_c->link) {
		p2->next = n2_next;
		n2_next->prev = p2;
		p1->next = &head2_c->link;
		head2_c->link.prev = p1;
		tail2_c->link.next = &head1_c->link;
		head1_c->link.prev = &tail2_c->link;
	} else if (n2_next == &head1_c->link) {
		p1->next = n1_next;
		n1_next->prev = p1;
		p2->next = &head1_c->link;
		head1_c->link.prev = p2;
		tail1_c->link.next = &head2_c->link;
		head2_c->link.prev = &tail1_c->link;
	} else {
		p1->next = &head2_c->link;
		head2_c->link.prev = p1;
		tail2_c->link.next = n1_next;
		n1_next->prev = &tail2_c->link;

		p2->next = &head1_c->link;
		head1_c->link.prev = p2;
		tail1_c->link.next = n2_next;
		n2_next->prev = &tail1_c->link;
	}
}

void exchange_two_scroller_clients(Client *c1, Client *c2) {

	if (!c1 || !c2 || !c1->mon || !c2->mon)
		return;

	struct ScrollerStackNode *n1 = NULL;
	struct ScrollerStackNode *n2 = NULL;
	Monitor *m1 = c1->mon;
	Monitor *m2 = c2->mon;
	uint32_t tag1 = get_mon_curtag(m1);
	uint32_t tag2 = get_mon_curtag(m2);

	struct TagScrollerState *st1 = ensure_scroller_state(m1, tag1);
	n1 = find_scroller_node(st1, c1);

	struct TagScrollerState *st2 = ensure_scroller_state(m2, tag2);
	n2 = find_scroller_node(st2, c2);

	if (!n1 && !n2)
		return;

	if (m1 != m2 && ((n1 && n1->prev_in_stack) || (n2 && n2->prev_in_stack) ||
					 (n1 && n1->next_in_stack) || (n2 && n2->next_in_stack))) {
		return;
	}

	client_swap_layout_properties(c1, c2);

	if (n1 && n2) {
		struct ScrollerStackNode *head1 = n1;
		while (head1->prev_in_stack)
			head1 = head1->prev_in_stack;
		struct ScrollerStackNode *head2 = n2;
		while (head2->prev_in_stack)
			head2 = head2->prev_in_stack;

		if (head1 == head2) {
			scroller_swap_nodes_in_same_stack(n1, n2);
			sync_scroller_state_to_clients(m1, tag1);
			wl_list_swap(&c1->link, &c2->link);
		} else {
			scroller_swap_different_stacks(head1, head2);
		}
	} else {
		wl_list_swap(&c1->link, &c2->link);
	}

	if (m1 != m2) {
		client_swap_monitors_and_tags(c1, c2);
	}
	finish_exchange_arrange_and_focus(c1, c2, m1, m2);

	return;
}
