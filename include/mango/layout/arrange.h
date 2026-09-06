#ifndef ___ARRANGE_H__
#define ___ARRANGE_H__

#include "mango/mango.h"

void set_size_per(Monitor *m, Client *c);

void resize_tile_master_horizontal(Client *gc, bool isdrag, int32_t offsetx,
								   int32_t offsety, uint32_t time,
								   int32_t type);

void resize_tile_master_vertical(Client *gc, bool isdrag, int32_t offsetx,
								 int32_t offsety, uint32_t time, int32_t type);

void resize_tile_dwindle(Client *gc, bool isdrag, int32_t offsetx,
						 int32_t offsety, uint32_t time, bool isvertical);

void resize_tile_grid_fair(Client *gc, bool isdrag, int32_t offsetx,
						   int32_t offsety, uint32_t time);

void resize_tile_scroller(Client *gc, bool isdrag, int32_t offsetx,
						  int32_t offsety, uint32_t time, bool isvertical);

void resize_tile_client(Client *gc, bool isdrag, int32_t offsetx,
						int32_t offsety, uint32_t time);

void check_size_per_valid(Client *c);
/* If there are no calculation omissions,
these two functions will never be triggered.
Just in case to facilitate the final investigation*/

void reset_size_per_mon(Monitor *m, int32_t tile_cilent_num,
						double total_left_stack_hight_percent,
						double total_right_stack_hight_percent,
						double total_stack_hight_percent,
						double total_master_inner_percent, int32_t master_num,
						int32_t stack_num);

// normal-tag client kept visible as background under the special overlay
bool special_keep_bg_client(Monitor *m, Client *c);

void pre_calculate_before_arrange(Monitor *m, bool want_animation,
								  bool from_view, bool only_calculate);

// remap tags through map; unmapped tags stay as-is.
uint32_t tag_remap_mask(uint32_t tags, const uint32_t *map);

// reset a pertag slot to its tagrule state.

// Compact occupied tags on this monitor to 1..k (e.g. 1,3,9 -> 1,2,3).
void tag_gather_apply(Monitor *m);

void tag_gather_move_pertag(Monitor *m, uint32_t dst, uint32_t src);

void arrange(Monitor *m, bool want_animation, bool from_view);
#endif

// empty special view: keep it when entered via a view switch, exit otherwise.
// returns true when arrange should stop because the view was closed
bool special_handle_empty_view(Monitor *m, bool from_view);
