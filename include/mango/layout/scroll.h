#ifndef __LAYOUT_SCROLL_H__
#define __LAYOUT_SCROLL_H__ 1

#include "mango/mango.h"

/* 获取或创建指定 monitor 某个 tag 的 scroller 状态 */
struct TagScrollerState *ensure_scroller_state(Monitor *m, uint32_t tag);
/* 在 tag 状态中查找客户端对应的节点（无则返回 NULL） */
struct ScrollerStackNode *find_scroller_node(struct TagScrollerState *st,
											 Client *c);
void scroller_node_remove(struct TagScrollerState *st,
						  struct ScrollerStackNode *target);
/* 清空一个 tag 的全部 scroller 状态 */
void clear_scroller_state(struct TagScrollerState *st);
/* 在 Monitor 销毁时清理所有 tag 的 scroller 状态 */
void cleanup_monitor_scroller(Monitor *m);
/* 将某个 tag 的状态同步回所有客户端的全局字段 */
void sync_scroller_state_to_clients(Monitor *m, uint32_t tag);
void vertical_scroll_adjust_fullandmax(Client *c, struct wlr_box *target_geom);
void vertical_check_scroller_root_inside_mon(Client *c,
											 struct wlr_box *geometry);
void horizontal_scroll_adjust_fullandmax(Client *c,
										 struct wlr_box *target_geom);
void arrange_stack_node(struct ScrollerStackNode *head, struct wlr_box geometry,
						int32_t gappiv);
void arrange_stack_vertical_node(struct ScrollerStackNode *head,
								 struct wlr_box geometry, int32_t gappih);
void scroller(Monitor *m);
void vertical_scroller(Monitor *m);
void scroller_remove_client(Client *c);
void scroller_insert_stack(Client *c, Client *target_client,
						   bool insert_before);
void scroller_drop_tile(Client *c, Client *closest, int vertical);
Client *scroll_get_stack_head_client(Client *c);
Client *scroll_get_stack_tail_client(Client *c);
void update_scroller_state(Monitor *m);
void scroller_swap_nodes_in_same_stack(struct ScrollerStackNode *n1,
									   struct ScrollerStackNode *n2);
void scroller_swap_different_stacks(struct ScrollerStackNode *head1,
									struct ScrollerStackNode *head2);
void exchange_two_scroller_clients(Client *c1, Client *c2);

/* 创建一个新节点并插入到 tag 状态的 all 链表中 */
struct ScrollerStackNode *scroller_node_create(struct TagScrollerState *st,
											   Client *c);

#endif
