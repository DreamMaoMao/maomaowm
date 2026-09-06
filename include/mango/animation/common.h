#ifndef __ANIMATION_COMMON_H__
#define __ANIMATION_COMMON_H__ 1

#include "mango/mango.h"
#include <stdbool.h>
#include <stdint.h>

struct dvec2 calculate_animation_curve_at(double t, int32_t type);
void handle_snapshot_meta_destroy(struct wl_listener *listener, void *data);
void init_baked_points(void);
double find_animation_curve_at(double t, int32_t type);

bool scene_node_snapshot(struct wlr_scene_node *node, int32_t lx, int32_t ly,
						 struct wlr_scene_tree *snapshot_tree);

struct wlr_scene_tree *wlr_scene_tree_snapshot(struct wlr_scene_node *node,
											   struct wlr_scene_tree *parent);
void request_fresh_all_monitors(void);
#endif
