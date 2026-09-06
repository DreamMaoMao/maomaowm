#ifndef __LAYOUT_SCROLL_H__
#define __LAYOUT_SCROLL_H__ 1

#include "mango/common/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <wlr/util/box.h>

struct ScrollerStackNode {
	Client *client;
	float scroller_proportion;
	float stack_proportion;
	float scroller_proportion_single;

	struct ScrollerStackNode *next_in_stack;
	struct ScrollerStackNode *prev_in_stack;
	struct ScrollerStackNode *all_next;
};

struct TagScrollerState {
	struct ScrollerStackNode
		*all_first; /* Singly linked list head for all nodes. */
	int count;
};

/* Gets or creates the scroller state for a given tag of the specified monitor.
 */
struct TagScrollerState *ensure_scroller_state(Monitor *m, uint32_t tag);
/* Finds the node for a client in the tag state (returns NULL if absent). */
struct ScrollerStackNode *find_scroller_node(struct TagScrollerState *st,
											 Client *c);
void scroller_node_remove(struct TagScrollerState *st,
						  struct ScrollerStackNode *target);
/* Clears all scroller state for a tag. */
void clear_scroller_state(struct TagScrollerState *st);
/* Cleans up scroller state of all tags when a Monitor is destroyed. */
void cleanup_monitor_scroller(Monitor *m);
/* Syncs a tag state back to the global fields of all clients. */
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

/* Creates a new node and inserts it into the tag state all list. */
struct ScrollerStackNode *scroller_node_create(struct TagScrollerState *st,
											   Client *c);

#endif
