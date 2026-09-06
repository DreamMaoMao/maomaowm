#ifndef __DWINDLE_H__
#define __DWINDLE_H__

#include "mango/mango.h"

extern DwindleNode *dwindle_locked_h_node;
extern DwindleNode *dwindle_locked_v_node;

DwindleNode *dwindle_new_leaf(Client *c);

int count_block_items(DwindleNode *node, bool split_h);
int get_block_path_and_ratios(DwindleNode *target, bool split_h,
							  DwindleNode ***path, float **p);
DwindleNode *dwindle_find_leaf(DwindleNode *node, Client *c);
DwindleNode *dwindle_first_leaf(DwindleNode *node);
void dwindle_free_tree(DwindleNode *node);
void dwindle_remove(DwindleNode **root, Client *c);
void dwindle_insert(DwindleNode **root, Client *new_c, Client *focused,
					float ratio, bool as_first, bool split_h, bool lock);
void dwindle_assign(DwindleNode *node, int32_t ax, int32_t ay, int32_t aw,
					int32_t ah, int32_t gap_h, int32_t gap_v);
void dwindle_move_client(DwindleNode **root, Client *c, Client *target,
						 float ratio, int32_t dir);
void dwindle_swap_clients(Client *c1, Client *c2);
void dwindle_resize_client(Monitor *m, Client *c);
void dwindle_resize_client_step(Monitor *m, Client *c, int32_t dx, int32_t dy);
void dwindle_remove_client(Client *c);
void dwindle_insert_with_config(DwindleNode **root, Client *new_c,
								Client *focused, float ratio);
void dwindle(Monitor *m);
void cleanup_monitor_dwindle(Monitor *m);

// 统计同方向上的节点总和 (N_old)
#endif
