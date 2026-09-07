#include "mango/layout/group.h"
#include "mango/common/server.h"
#include "mango/config/parse_config.h"
#include "mango/draw/text-node.h"
#include "mango/layout/arrange.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"

void group_init(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	if (server.selected_monitor->isoverview)
		return;

	Client *target = arg->tc ? arg->tc : server.selected_monitor->sel;
	if (!target || !target->mon)
		return;
	if (target->group_prev || target->group_next || target->isgroupfocusing)
		return;

	target->isgroupfocusing = true;
	client_add_group_bar(target);
	client_focus(target, 1);
	arrange(target->mon, false, false);
}


void group_smart(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	if (server.selected_monitor->isoverview)
		return;

	Client *target = server.selected_monitor->sel;
	if (!target || !target->mon)
		return;
	if (!target->group_prev && !target->group_next) {
		group_join(arg);
	} else {
		group_merge(arg);
	}
	
}

void group_all(const Arg *arg) {
	(void)arg;
	if (!server.selected_monitor)
		return;
	if (server.selected_monitor->isoverview)
		return;

	Client *visible[256];
	int count = 0;

	Client *client;
	wl_list_for_each(client, &server.clients, link) {
		if (!VISIBLEON(client, server.selected_monitor))
			continue;
		if (!(client->group_prev || client->group_next ||
			  client->isgroupfocusing)) {
			if (count < 256)
				visible[count++] = client;
			continue;
		}
		Client *head = client;
		while (head->group_prev && head->group_prev != head)
			head = head->group_prev;
		Client *cur = head;
		while (cur && count < 256) {
			visible[count++] = cur;
			cur = cur->group_next;
		}
	}

	if (count < 2)
		return;

	for (int index = 0; index < count; index++) {
		if (visible[index]->group_next || visible[index]->group_prev)
			client_group_detach(visible[index]);
	}
	Client *target = visible[0];

	target->isgroupfocusing = true;

	for (int index = 1; index < count; index++) {
		visible[index]->group_next = target;
		if (target->group_prev)
			target->group_prev->group_next = visible[index];
		visible[index]->group_prev = target->group_prev;
		target->group_prev = visible[index];
		client_park(visible[index]);
		wlr_scene_node_set_enabled(&visible[index]->scene->node, false);
		if (visible[index]->group_bar)
			wlr_scene_node_set_enabled(
				&visible[index]->group_bar->scene_buffer->node, false);
	}
	client_focus(target, 1);
	arrange(target->mon, false, false);
}

void group_merge(const Arg *arg) {
	if (!server.selected_monitor)
		return;
	if (server.selected_monitor->isoverview)
		return;

	Client *group = server.selected_monitor->sel;
	if (!group || !group->mon)
		return;

	Client *target = direction_select(arg);
	if (!target || target == group)
		return;
	Monitor *oldmon = NULL;

	if (target->group_next || target->group_prev)
		group_leave(&(Arg){.tc = target});

	if (target->mon != group->mon) {
		oldmon = target->mon;
		target->mon = group->mon;
	}

	if (!group->group_prev && !group->group_next)
		group->isgroupfocusing = true;

	target->group_next = group;
	if (group->group_prev)
		group->group_prev->group_next = target;
	target->group_prev = group->group_prev;
	group->group_prev = target;

	client_focus_group_member(target);
	arrange(target->mon, false, false);

	if (oldmon)
		arrange(oldmon, false, false);
}

void group_disband(const Arg *arg) {
	if (!server.selected_monitor)
		return;

	Client *target = arg->tc ? arg->tc : server.selected_monitor->sel;

	if (!target || !target->mon)
		return;

	if (!target->group_prev && !target->group_next) {
		if (target->isgroupfocusing) {
			target->isgroupfocusing = false;
			client_focus(target, 1);
			arrange(target->mon, false, false);
		}
		return;
	}

	if (target->mon->isoverview)
		return;

	Client *head = target;
	while (head->group_prev && head->group_prev != head)
		head = head->group_prev;

	Client *members[256];
	int membercount = 0;

	Client *current = head;
	while (current && membercount < 256) {
		members[membercount++] = current;
		current = current->group_next;
	}
	for (int index = 0; index < membercount; index++) {
		members[index]->mon = target->mon;
		if (client_is_parked(members[index]))
			client_unpark(members[index], target);
		client_group_detach(members[index]);
		wlr_scene_node_set_enabled(&members[index]->scene->node, true);
	}
	client_focus(target, 1);
	arrange(target->mon, false, false);
}

Client *group_capture_get_parent(void) {
	if (!config.group_capture_spawn || !server.selected_monitor)
		return NULL;

	Client *sel = server.selected_monitor->sel;
	if (sel && (sel->group_prev || sel->group_next || sel->isgroupfocusing))
		return sel;

	return NULL;
}

bool group_capture_spawn(Client *c, Client *group_parent) {
	if (!config.group_capture_spawn || !group_parent || !c->mon ||
		c->mon != group_parent->mon ||
		!(group_parent->group_prev || group_parent->group_next ||
		  group_parent->isgroupfocusing))
		return false;

	c->group_prev = group_parent->group_prev;
	if (group_parent->group_prev)
		group_parent->group_prev->group_next = c;
	c->group_next = group_parent;
	group_parent->group_prev = c;
	c->is_pending_open_animation = false;
	client_focus_group_member(c);
	return true;
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
	tc->mon = rc->mon;
	client_unpark(tc, rc);
	/* rc stays focused: put tc right behind it in the focus stack. */
	wl_list_remove(&tc->flink);
	wl_list_insert(rc->flink.next, &tc->flink);

	if (!rc->group_prev && !rc->group_next) {
		rc->isgroupfocusing = false;
	}

	arrange(tc->mon, false, false);

	return;
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

void client_add_group_bar(Client *c) {

	if (config.group_bar_height <= 0) {
		return;
	}

	uint32_t layer = client_target_layer(c);

	c->group_bar = mango_group_bar_create(c, GroupBar, server.layers[layer],
										  config.groupbardata, 0, 0);
	wlr_scene_node_lower_to_bottom(&c->group_bar->scene_buffer->node);
	wlr_scene_node_set_enabled(&c->group_bar->scene_buffer->node, false);
	mango_group_bar_update(c->group_bar, client_get_display_title(c),
						   c->mon ? c->mon->wlr_output->scale
						   : server.selected_monitor
							   ? server.selected_monitor->wlr_output->scale
							   : 1.0f);
}

void client_focus_group_member(Client *c) {
	if (!c->group_prev && !c->group_next)
		return;

	if (c->isgroupfocusing)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur_focusing = NULL;
	while (head) {
		if (head->isgroupfocusing) {
			cur_focusing = head;
			break;
		}
		head = head->group_next;
	}

	if (!cur_focusing || !cur_focusing->mon)
		return;

	if (cur_focusing && cur_focusing->mon->isoverview)
		return;

	cur_focusing->isgroupfocusing = false;
	c->mon = cur_focusing->mon;
	client_replace(c, cur_focusing, true, false);
	mango_group_bar_set_focus(cur_focusing->group_bar, false);

	c->isgroupfocusing = true;
	mango_group_bar_set_focus(c->group_bar, true);

	client_reparent_group(c);

	client_focus(c, 1);

	arrange(c->mon, false, false);
}

void client_check_tab_node_visible(Client *c) {

	if (!c || !c->mon)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (!c->mon->isoverview && cur->group_bar &&
			(cur->group_next || cur->group_prev || cur->isgroupfocusing) &&
			TAGMATCH(c, c->mon) && ISNORMAL(c) && !c->isfullscreen) {
			wlr_scene_node_set_enabled(&cur->group_bar->scene_buffer->node,
									   true);
		} else {
			wlr_scene_node_set_enabled(&cur->group_bar->scene_buffer->node,
									   false);
		}
		cur = cur->group_next;
	}
}

void client_raise_group(Client *c) {
	if (!c || !c->mon)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->group_bar) {
			wlr_scene_node_raise_to_top(&cur->group_bar->scene_buffer->node);
		}
		wlr_scene_node_raise_to_top(&cur->scene->node);
		cur = cur->group_next;
	}
}

void client_reparent_group(Client *c) {
	if (!c || !c->mon)
		return;

	int32_t layer = client_target_layer(c);

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->group_bar) {
			wlr_scene_node_reparent(&cur->group_bar->scene_buffer->node,
									server.layers[layer]);
		}
		wlr_scene_node_reparent(&cur->scene->node, server.layers[layer]);
		cur = cur->group_next;
	}
}

void client_handle_decorate_click(MangoGroupBar *gb) {

	if (!gb)
		return;

	if (gb->node_data) {
		Client *c = gb->node_data;
		client_focus_group_member(c);
	}
}

void client_set_group_mon(Client *c, Monitor *m) {
	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		client_change_mon(cur, m);
		cur = cur->group_next;
	}
}

void client_set_group_config(Client *c) {
	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->jump_label_node)
			mango_jump_label_node_apply_config(cur->jump_label_node,
											   &config.jumplabeldata);
		wlr_scene_rect_set_color(cur->droparea, config.dropcolor);
		wlr_scene_rect_set_color(cur->splitindicator[0], config.splitcolor);
		wlr_scene_rect_set_color(cur->splitindicator[1], config.splitcolor);
		mango_group_bar_apply_config(cur->group_bar, &config.groupbardata);
		cur = cur->group_next;
	}
}

void client_group_detach(Client *c) {
	if (c->group_prev)
		c->group_prev->group_next = c->group_next;
	if (c->group_next)
		c->group_next->group_prev = c->group_prev;
	c->group_prev = NULL;
	c->group_next = NULL;
	c->isgroupfocusing = false;
}

void client_group_replace(Client *old, Client *new) {
	client_group_detach(new);

	new->group_prev = old->group_prev;
	new->group_next = old->group_next;
	if (old->group_prev)
		old->group_prev->group_next = new;
	if (old->group_next)
		old->group_next->group_prev = new;
	old->group_prev = NULL;
	old->group_next = NULL;

	if (client_is_parked(old) || (!new->group_prev && !new->group_next)) {
		new->isgroupfocusing = false;
	} else {
		new->isgroupfocusing = old->isgroupfocusing;
	}
}
