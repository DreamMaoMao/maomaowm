#include "mango/animation/client.h"
#include <stdint.h>
#include "mango/common/server.h"
#include "mango/layout/layout.h"
#include "mango/animation/common.h"
#include "mango/overview/overview.h"
#include "mango/layout/dwindle.h"
#include "mango/common/util.h"
#include "mango/manage/client.h"
#include "mango/manage/misc.h"
#include "mango/manage/monitor.h"

bool client_is_ignore_output_clip(Client *c) {
	return c == server.grab_client ||
		   (!ISSCROLLTILED(c) && !c->animation.tagining &&
			!c->animation.tagouting);
}

struct ivec2 compute_edge_offsets(Client *c) {
	struct ivec2 offsets = {0};
	if (client_is_ignore_output_clip(c))
		return offsets;

	struct wlr_box cur = c->animation.current;
	offsets.width =
		GEZERO(cur.x + cur.width - c->mon->m.x - c->mon->m.width); // right
	offsets.height =
		GEZERO(cur.y + cur.height - c->mon->m.y - c->mon->m.height); // bottom
	offsets.x = GEZERO(c->mon->m.x - cur.x);						 // left
	offsets.y = GEZERO(c->mon->m.y - cur.y);						 // top
	return offsets;
}

void client_actual_size(Client *c, int32_t *width, int32_t *height) {
	*width = c->animation.current.width - 2 * (int32_t)c->bw;
	*height = c->animation.current.height - 2 * (int32_t)c->bw;
}
void set_rect_size(struct wlr_scene_rect *rect, int32_t width, int32_t height) {
	wlr_scene_rect_set_size(rect, GEZERO(width), GEZERO(height));
}
struct fx_corner_radii set_client_corner_location(Client *c) {
	struct fx_corner_radii current_corner_location =
		corner_radii_all(config.border_radius);

	if (client_is_ignore_output_clip(c))
		return current_corner_location;

	struct wlr_box target_geom =
		config.animations ? c->animation.current : c->geom;
	if (target_geom.x + config.border_radius <= c->mon->m.x) {
		current_corner_location.top_left = 0;
		current_corner_location.bottom_left = 0;
	}
	if (target_geom.x + target_geom.width - config.border_radius >=
		c->mon->m.x + c->mon->m.width) {
		current_corner_location.top_right = 0;
		current_corner_location.bottom_right = 0;
	}
	if (target_geom.y + config.border_radius <= c->mon->m.y) {
		current_corner_location.top_left = 0;
		current_corner_location.top_right = 0;
	}
	if (target_geom.y + target_geom.height - config.border_radius >=
		c->mon->m.y + c->mon->m.height) {
		current_corner_location.bottom_left = 0;
		current_corner_location.bottom_right = 0;
	}
	return current_corner_location;
}

bool is_horizontal_stack_layout(Monitor *m) {
	uint32_t tag = get_mon_curtag(m);
	return m->pertag->ltidxs[tag]->id == TILE ||
		   m->pertag->ltidxs[tag]->id == DECK;
}

bool is_horizontal_right_stack_layout(Monitor *m) {
	uint32_t tag = get_mon_curtag(m);
	return m->pertag->ltidxs[tag]->id == RIGHT_TILE;
}

int32_t is_special_animation_rule(Client *c) {
	if (is_scroller_layout(c->mon) && !c->isfloating) {
		return DOWN;
	} else if (c->mon->visible_tiling_clients == 1 && !c->isfloating) {
		return DOWN;
	} else if (c->mon->visible_tiling_clients == 2 && !c->isfloating &&
			   !config.new_is_master && is_horizontal_stack_layout(c->mon)) {
		return RIGHT;
	} else if (!c->isfloating && config.new_is_master &&
			   is_horizontal_stack_layout(c->mon)) {
		return LEFT;
	} else if (c->mon->visible_tiling_clients == 2 && !c->isfloating &&
			   !config.new_is_master &&
			   is_horizontal_right_stack_layout(c->mon)) {
		return LEFT;
	} else if (!c->isfloating && config.new_is_master &&
			   is_horizontal_right_stack_layout(c->mon)) {
		return RIGHT;
	} else {
		return UNDIR;
	}
}

void set_overview_enter_animation(Client *c) {
	struct wlr_box geo = c->geom;
	c->animainit_geom.width = geo.width * 1.2;
	c->animainit_geom.height = geo.height * 1.2;
	c->animainit_geom.x = geo.x + (geo.width - c->animainit_geom.width) / 2;
	c->animainit_geom.y = geo.y + (geo.height - c->animainit_geom.height) / 2;
}

void set_client_open_animation(Client *c, struct wlr_box geo) {
	int32_t slide_direction;
	int32_t horizontal, horizontal_value;
	int32_t vertical, vertical_value;
	int32_t special_direction;
	int32_t center_x, center_y;

	if ((!c->animation_type_open &&
		 strcmp(config.animation_type_open, "fade") == 0) ||
		(c->animation_type_open &&
		 strcmp(c->animation_type_open, "fade") == 0)) {
		c->animainit_geom.width = geo.width;
		c->animainit_geom.height = geo.height;
		c->animainit_geom.x = geo.x;
		c->animainit_geom.y = geo.y;
		return;
	} else if ((!c->animation_type_open &&
				strcmp(config.animation_type_open, "zoom") == 0) ||
			   (c->animation_type_open &&
				strcmp(c->animation_type_open, "zoom") == 0)) {
		c->animainit_geom.width = geo.width * config.zoom_initial_ratio;
		c->animainit_geom.height = geo.height * config.zoom_initial_ratio;
		c->animainit_geom.x = geo.x + (geo.width - c->animainit_geom.width) / 2;
		c->animainit_geom.y =
			geo.y + (geo.height - c->animainit_geom.height) / 2;
		return;
	} else {
		special_direction = is_special_animation_rule(c);
		center_x = c->geom.x + c->geom.width / 2;
		center_y = c->geom.y + c->geom.height / 2;
		if (special_direction == UNDIR) {
			horizontal = c->mon->w.x + c->mon->w.width - center_x <
								 center_x - c->mon->w.x
							 ? RIGHT
							 : LEFT;
			horizontal_value = horizontal == LEFT
								   ? center_x - c->mon->w.x
								   : c->mon->w.x + c->mon->w.width - center_x;
			vertical = c->mon->w.y + c->mon->w.height - center_y <
							   center_y - c->mon->w.y
						   ? DOWN
						   : UP;
			vertical_value = vertical == UP
								 ? center_y - c->mon->w.y
								 : c->mon->w.y + c->mon->w.height - center_y;
			slide_direction =
				horizontal_value < vertical_value ? horizontal : vertical;
		} else {
			slide_direction = special_direction;
		}
		c->animainit_geom.width = c->geom.width;
		c->animainit_geom.height = c->geom.height;
		switch (slide_direction) {
		case UP:
			c->animainit_geom.x = c->geom.x;
			c->animainit_geom.y = c->mon->m.y - c->geom.height;
			break;
		case DOWN:
			c->animainit_geom.x = c->geom.x;
			c->animainit_geom.y =
				c->geom.y + c->mon->m.height - (c->geom.y - c->mon->m.y);
			break;
		case LEFT:
			c->animainit_geom.x = c->mon->m.x - c->geom.width;
			c->animainit_geom.y = c->geom.y;
			break;
		case RIGHT:
			c->animainit_geom.x =
				c->geom.x + c->mon->m.width - (c->geom.x - c->mon->m.x);
			c->animainit_geom.y = c->geom.y;
			break;
		default:
			c->animainit_geom.x = c->geom.x;
			c->animainit_geom.y = 0 - c->geom.height;
		}
	}
}

void snap_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int32_t sx,
									int32_t sy, void *data) {
	BufferData *buffer_data = (BufferData *)data;
	wlr_scene_buffer_set_dest_size(buffer, buffer_data->width,
								   buffer_data->height);
}
void scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int32_t sx,
							   int32_t sy, void *data) {
	BufferData *buffer_data = (BufferData *)data;

	if (buffer_data->should_scale && buffer_data->height_scale < 1 &&
		buffer_data->width_scale < 1)
		buffer_data->should_scale = false;

	if (buffer_data->should_scale && buffer_data->height_scale == 1 &&
		buffer_data->width_scale < 1)
		buffer_data->should_scale = false;

	if (buffer_data->should_scale && buffer_data->height_scale < 1 &&
		buffer_data->width_scale == 1)
		buffer_data->should_scale = false;

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);
	if (scene_surface == NULL)
		return;

	struct wlr_surface *surface = scene_surface->surface;

	if (buffer_data->should_scale
#ifdef XWAYLAND
		&& !wlr_xwayland_surface_try_from_wlr_surface(surface)
#endif
	) {
		int32_t surface_width = surface->current.width;
		int32_t surface_height = surface->current.height;

		surface_width = buffer_data->width_scale < 1
							? surface_width
							: buffer_data->width_scale * surface_width;
		surface_height = buffer_data->height_scale < 1
							 ? surface_height
							 : buffer_data->height_scale * surface_height;

		if (surface_width > buffer_data->width &&
			wlr_subsurface_try_from_wlr_surface(surface) == NULL)
			surface_width = buffer_data->width;

		if (surface_height > buffer_data->height &&
			wlr_subsurface_try_from_wlr_surface(surface) == NULL)
			surface_height = buffer_data->height;

		if (surface_width > buffer_data->width &&
			wlr_subsurface_try_from_wlr_surface(surface) != NULL)
			return;

		if (surface_height > buffer_data->height &&
			wlr_subsurface_try_from_wlr_surface(surface) != NULL)
			return;

		if (surface_height > 0 && surface_width > 0)
			wlr_scene_buffer_set_dest_size(buffer, surface_width,
										   surface_height);
	}

	if (wlr_xdg_popup_try_from_wlr_surface(surface) != NULL)
		return;

	wlr_scene_buffer_set_corner_radii(buffer, buffer_data->corner_location);
}

void buffer_set_effect(Client *c, BufferData data) {
	if (!c || c->iskilling)
		return;

	if (c->animation.tagouting || c->animation.tagouted ||
		c->animation.tagining)
		data.should_scale = false;

	if (c == server.grab_client)
		data.should_scale = false;

	if (c->isfullscreen || (config.no_radius_when_single && c->mon &&
							c->mon->visible_tiling_clients == 1))
		data.corner_location = corner_radii_none();

	if (config.blur && !c->noblur)
		wlr_scene_blur_set_corner_radii(c->blur, data.corner_location);

	/* Overview cards get rounded corners directly. */
	if (c->ov_card_tree) {
		overview_card_set_corner_radii(c, data.corner_location);
		return;
	}

	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   scene_buffer_apply_effect, &data);
}

void client_draw_shadow(Client *c, struct ivec2 offsets) {
	if (c->iskilling || !client_surface(c)->mapped || c->isnoshadow)
		return;

	if (!config.shadows || c->isfullscreen ||
		(!c->isfloating && config.shadow_only_floating)) {
		if (c->shadow->node.enabled)
			wlr_scene_node_set_enabled(&c->shadow->node, false);
		return;
	} else {
		if (c->scene_surface->node.enabled && !c->shadow->node.enabled)
			wlr_scene_node_set_enabled(&c->shadow->node, true);
	}

	bool hit_no_border = check_hit_no_border(c);
	struct fx_corner_radii current_corner_location =
		c->isfullscreen || (config.no_radius_when_single && c->mon &&
							c->mon->visible_tiling_clients == 1)
			? corner_radii_none()
			: set_client_corner_location(c);

	int32_t bw = (int32_t)c->bw;
	int32_t bwoffset = bw != 0 && hit_no_border ? bw : 0;

	int32_t width, height;
	client_actual_size(c, &width, &height);

	int32_t delta = config.shadows_size + bw - bwoffset;

	struct wlr_box client_box = {
		.x = bwoffset,
		.y = bwoffset,
		.width = width + 2 * bw - 2 * bwoffset,
		.height = height + 2 * bw - 2 * bwoffset,
	};

	struct wlr_box shadow_box = {
		.x = config.shadows_position_x + bwoffset,
		.y = config.shadows_position_y + bwoffset,
		.width = width + 2 * delta,
		.height = height + 2 * delta,
	};

	struct wlr_box intersection_box;
	wlr_box_intersection(&intersection_box, &client_box, &shadow_box);
	intersection_box.x -= config.shadows_position_x + bwoffset;
	intersection_box.y -= config.shadows_position_y + bwoffset;

	struct clipped_region clipped_region = {
		.area = intersection_box,
		.corners = current_corner_location,
	};

	struct wlr_box cur = c->animation.current;
	struct wlr_box absolute_shadow_box = {
		.x = shadow_box.x + cur.x,
		.y = shadow_box.y + cur.y,
		.width = shadow_box.width,
		.height = shadow_box.height,
	};

	int32_t left, right, top, bottom;
	if (client_is_ignore_output_clip(c)) {
		left = right = top = bottom = 0;
	} else {
		right = GEZERO(absolute_shadow_box.x + absolute_shadow_box.width -
					   c->mon->m.x - c->mon->m.width);
		bottom = GEZERO(absolute_shadow_box.y + absolute_shadow_box.height -
						c->mon->m.y - c->mon->m.height);
		left = GEZERO(c->mon->m.x - absolute_shadow_box.x);
		top = GEZERO(c->mon->m.y - absolute_shadow_box.y);
	}

	left = MANGO_MIN(left, shadow_box.width);
	right = MANGO_MIN(right, shadow_box.width);
	top = MANGO_MIN(top, shadow_box.height);
	bottom = MANGO_MIN(bottom, shadow_box.height);

	wlr_scene_node_set_position(&c->shadow->node, shadow_box.x + left,
								shadow_box.y + top);

	wlr_scene_shadow_set_size(c->shadow,
							  GEZERO(shadow_box.width - left - right),
							  GEZERO(shadow_box.height - top - bottom));

	clipped_region.area.x = clipped_region.area.x - left;
	clipped_region.area.y = clipped_region.area.y - top;

	wlr_scene_shadow_set_clipped_region(c->shadow, clipped_region);
}
void client_draw_groupbar(Client *c, struct ivec2 offsets) {
	if (!c || !c->group_bar)
		return;

	if (!c->group_next && !c->group_prev &&
		c->group_bar->scene_buffer->node.enabled) {
		wlr_scene_node_set_enabled(&c->group_bar->scene_buffer->node, false);
		return;
	}

	if (c->is_logic_hide)
		return;

	if (!c->group_next && !c->group_prev)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	int count = 0;
	Client *cur = head;
	while (cur) {
		count++;
		cur = cur->group_next;
	}

	int32_t tab_x = c->animation.current.x;
	int32_t tab_y = c->animation.current.y - config.group_bar_height;
	int32_t tw = c->animation.current.width;
	int32_t th = config.group_bar_height;

	int32_t top_over = offsets.y;		  // top
	int32_t bottom_over = offsets.height; // bottom
	int32_t left_over = offsets.x;		  // left
	int32_t right_over = offsets.width;	  // right

	if (top_over > 0) {
		tab_y = c->mon->m.y;
		th = config.group_bar_height - top_over;
	}
	if (bottom_over > 0)
		th = th - bottom_over;
	if (right_over > 0)
		tw = tw - right_over;
	if (left_over > 0) {
		tab_x = c->mon->m.x;
		tw = tw - left_over;
	}

	if (tw <= 0 || th <= 0) {
		cur = head;
		while (cur) {
			if (cur->group_bar)
				wlr_scene_node_set_enabled(&cur->group_bar->scene_buffer->node,
										   false);
			cur = cur->group_next;
		}
		return;
	} else {
		client_check_tab_node_visible(c);
	}

	int32_t bar_w = tw / count;
	int32_t rem = tw % count;
	int32_t x = tab_x;
	cur = head;

	for (int i = 0; i < count && cur; i++) {
		int32_t w = bar_w + (i < rem ? 1 : 0);
		global_draw_group_bar(cur, x, tab_y, w, th);
		x += w;
		cur = cur->group_next;
	}
}
void client_draw_shield(Client *c, struct ivec2 clip_box) {

	int32_t shield_x = 0;
	int32_t shield_y = 0;
	int32_t shield_width = 0;
	int32_t shield_height = 0;

	if (server.active_capture_count <= 0 || !c->shield_when_capture) {
		if (c->shield->node.enabled) {
			wlr_scene_node_lower_to_bottom(&c->shield->node);
			wlr_scene_node_set_position(&c->shield->node, 0, 0);
			wlr_scene_rect_set_size(c->shield, c->animation.current.width,
									c->animation.current.height);
			wlr_scene_node_set_enabled(&c->shield->node, false);
		}
		return;
	}

	if (client_is_ignore_output_clip(c)) {
		shield_x = c->bw;
		shield_y = c->bw;
		shield_width = c->animation.current.width - 2 * (int32_t)c->bw;
		shield_height = c->animation.current.height - 2 * (int32_t)c->bw;
	} else {
		shield_x = clip_box.x + (int32_t)c->bw;
		shield_y = clip_box.y + (int32_t)c->bw;
		shield_width = c->animation.current.width - 2 * (int32_t)c->bw -
					   clip_box.width - clip_box.x;
		shield_height = c->animation.current.height - 2 * (int32_t)c->bw -
						clip_box.height - clip_box.y;
	}

	if (shield_width <= 0 || shield_height <= 0) {
		wlr_scene_node_set_enabled(&c->shield->node, false);
		return;
	}

	wlr_scene_node_raise_to_top(&c->shield->node);
	wlr_scene_node_set_position(&c->shield->node, shield_x, shield_y);
	wlr_scene_rect_set_size(c->shield, shield_width, shield_height);
	wlr_scene_node_set_enabled(&c->shield->node, true);
}
void client_draw_blur(Client *c, struct ivec2 clip_box) {
	if (c->isfullscreen) {
		if (c->blur->node.enabled)
			wlr_scene_node_set_enabled(&c->blur->node, false);
		return;
	}

	int32_t blur_x = 0;
	int32_t blur_y = 0;
	int32_t blur_width = 0;
	int32_t blur_height = 0;

	if (config.blur && !c->noblur) {

		if (client_is_ignore_output_clip(c)) {
			blur_x = c->bw;
			blur_y = c->bw;
			blur_width = c->animation.current.width - 2 * (int32_t)c->bw;
			blur_height = c->animation.current.height - 2 * (int32_t)c->bw;
		}

		blur_x = clip_box.x + (int32_t)c->bw;
		blur_y = clip_box.y + (int32_t)c->bw;
		blur_width = c->animation.current.width - 2 * (int32_t)c->bw -
					 clip_box.width - clip_box.x;
		blur_height = c->animation.current.height - 2 * (int32_t)c->bw -
					  clip_box.height - clip_box.y;
		wlr_scene_node_set_enabled(&c->blur->node, true);
		wlr_scene_node_set_position(&c->blur->node, blur_x, blur_y);
		wlr_scene_blur_set_size(c->blur, blur_width, blur_height);
	} else {
		wlr_scene_node_set_enabled(&c->blur->node, false);
	}
}

void global_draw_group_bar(Client *c, int32_t x, int32_t y, int32_t width,
						   int32_t height) {
	if (!c->group_bar)
		return;

	wlr_scene_node_set_position(&c->group_bar->scene_buffer->node, x, y);
	mango_group_bar_set_size(c->group_bar, width, height);
}
void client_draw_split_border(Client *c, bool hit_no_border,
							  struct ivec2 offsets) {
	if (c->iskilling || !c->mon || !client_surface(c)->mapped)
		return;

	uint32_t tag = get_client_tag_idx(c);
	const Layout *layout = c->mon->pertag->ltidxs[tag];

	if (hit_no_border || !ISTILED(c) || layout->id != DWINDLE ||
		!config.dwindle_manual_split || c->isfullscreen) {
		if (c->splitindicator[0]->node.enabled)
			wlr_scene_node_set_enabled(&c->splitindicator[0]->node, false);
		if (c->splitindicator[1]->node.enabled)
			wlr_scene_node_set_enabled(&c->splitindicator[1]->node, false);
		return;
	}

	DwindleNode **root = &c->mon->pertag->dwindle_root[tag];
	DwindleNode *dnode = dwindle_find_leaf(*root, c);
	if (!dnode) {
		wlr_scene_node_set_enabled(&c->splitindicator[0]->node, false);
		wlr_scene_node_set_enabled(&c->splitindicator[1]->node, false);
		return;
	}

	if (dnode->custom_leaf_split_h) {
		wlr_scene_node_set_enabled(&c->splitindicator[0]->node, false);
		wlr_scene_node_set_enabled(&c->splitindicator[1]->node, true);
	} else {
		wlr_scene_node_set_enabled(&c->splitindicator[0]->node, true);
		wlr_scene_node_set_enabled(&c->splitindicator[1]->node, false);
	}

	struct wlr_box fullgeom = c->animation.current;
	int32_t bw = (int32_t)c->bw;
	int32_t left = offsets.x, right = offsets.width, top = offsets.y,
			bottom = offsets.height;

	int32_t border_down_width =
		GEZERO(fullgeom.width - 2 * config.border_radius -
			   GEZERO((left + right) - config.border_radius));
	int32_t border_down_height =
		GEZERO(bw - bottom - GEZERO(top + bw - fullgeom.height));

	int32_t border_right_width =
		GEZERO(bw - right - GEZERO(left + bw - fullgeom.width));
	int32_t border_right_height =
		GEZERO(fullgeom.height - 2 * config.border_radius -
			   GEZERO((top + bottom) - config.border_radius));

	int32_t border_down_x =
		GEZERO(config.border_radius + GEZERO(left - config.border_radius));
	int32_t border_down_y =
		GEZERO(fullgeom.height - bw) + GEZERO(top + bw - fullgeom.height);

	int32_t border_right_x =
		GEZERO(fullgeom.width - bw) + GEZERO(left + bw - fullgeom.width);
	int32_t border_right_y =
		GEZERO(config.border_radius + GEZERO(top - config.border_radius));

	set_rect_size(c->splitindicator[0], border_down_width, border_down_height);
	set_rect_size(c->splitindicator[1], border_right_width,
				  border_right_height);
	wlr_scene_node_set_position(&c->splitindicator[0]->node, border_down_x,
								border_down_y);
	wlr_scene_node_set_position(&c->splitindicator[1]->node, border_right_x,
								border_right_y);
}
void client_draw_border(Client *c, struct ivec2 offsets) {
	if (!c || c->iskilling || !client_surface(c)->mapped)
		return;

	if (c->isfullscreen) {
		if (c->border->node.enabled) {
			wlr_scene_node_set_enabled(&c->splitindicator[0]->node, false);
			wlr_scene_node_set_enabled(&c->splitindicator[1]->node, false);
			wlr_scene_node_set_enabled(&c->border->node, false);
			wlr_scene_node_set_position(&c->scene_surface->node, 0, 0);
		}
		return;
	} else {
		if (!c->border->node.enabled)
			wlr_scene_node_set_enabled(&c->border->node, true);
	}

	bool hit_no_border = check_hit_no_border(c);
	client_draw_split_border(c, hit_no_border, offsets);

	struct fx_corner_radii current_corner_location =
		c->isfullscreen || (config.no_radius_when_single && c->mon &&
							c->mon->visible_tiling_clients == 1)
			? corner_radii_none()
			: set_client_corner_location(c);

	if (hit_no_border) {
		c->bw = 0;
		c->fake_no_border = true;
	} else if (!c->isfullscreen && VISIBLEON(c, c->mon)) {
		c->bw = c->isnoborder ? 0 : config.borderpx;
		c->fake_no_border = false;
	}

	struct wlr_box cur = c->animation.current;
	int32_t bw = (int32_t)c->bw;
	int32_t left = offsets.x, right = offsets.width, top = offsets.y,
			bottom = offsets.height;

	int32_t inner_surface_width = GEZERO(cur.width - 2 * bw);
	int32_t inner_surface_height = GEZERO(cur.height - 2 * bw);
	int32_t inner_surface_x = GEZERO(bw - left);
	int32_t inner_surface_y = GEZERO(bw - top);

	int32_t rect_x = left;
	int32_t rect_y = top;
	int32_t rect_width = GEZERO(cur.width - left - right);
	int32_t rect_height = GEZERO(cur.height - top - bottom);

	if (left > c->bw)
		inner_surface_width = inner_surface_width - left + bw;
	if (top > c->bw)
		inner_surface_height = inner_surface_height - top + bw;
	if (right > 0)
		inner_surface_width = MANGO_MIN(cur.width, inner_surface_width + right);
	if (bottom > 0)
		inner_surface_height =
			MANGO_MIN(cur.height, inner_surface_height + bottom);

	struct clipped_region clipped_region = {
		.area = {inner_surface_x, inner_surface_y, inner_surface_width,
				 inner_surface_height},
		.corners = current_corner_location,
	};

	wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
	wlr_scene_rect_set_size(c->border, rect_width, rect_height);
	wlr_scene_node_set_position(&c->border->node, rect_x, rect_y);
	wlr_scene_rect_set_corner_radii(c->border, current_corner_location);
	wlr_scene_rect_set_clipped_region(c->border, clipped_region);
}
struct ivec2 clip_to_hide(Client *c, struct wlr_box *clip_box,
						  struct ivec2 offsets) {
	struct ivec2 offset = {0};

	if (!ISSCROLLTILED(c) && !c->animation.tagining && !c->animation.tagouted &&
		!c->animation.tagouting)
		return offset;

	int32_t bw = (int32_t)c->bw;
	int32_t left = offsets.x, right = offsets.width, top = offsets.y,
			bottom = offsets.height;

	if (left > 0) {
		offset.x = GEZERO(left - bw);
		clip_box->x += offset.x;
		clip_box->width -= offset.x;
	} else if (right > 0) {
		offset.width = GEZERO(right - bw);
		clip_box->width -= offset.width;
	}

	if (top > 0) {
		offset.y = GEZERO(top - bw);
		clip_box->y += offset.y;
		clip_box->height -= offset.y;
	} else if (bottom > 0) {
		offset.height = GEZERO(bottom - bw);
		clip_box->height -= offset.height;
	}

	if ((clip_box->width + bw <= 0 || clip_box->height + bw <= 0) &&
		(ISSCROLLTILED(c) || c->animation.tagouting || c->animation.tagining)) {
		c->is_clip_to_hide = true;
		wlr_scene_node_set_enabled(&c->scene->node, false);
	} else if (c->is_clip_to_hide && VISIBLEON(c, c->mon)) {
		c->is_clip_to_hide = false;
		c->is_logic_hide = false;
		wlr_scene_node_set_enabled(&c->scene->node, true);
	}

	return offset;
}
void client_set_drop_area(Client *c) {
	bool first_draw = false;
	int32_t drop_direction = UNDIR;

	if (!c || !c->mon)
		return;

	if (!c->enable_drop_area_draw && !c->droparea->node.enabled)
		return;

	if (!c->enable_drop_area_draw && c->droparea->node.enabled) {
		wlr_scene_node_lower_to_bottom(&c->droparea->node);
		wlr_scene_node_set_enabled(&c->droparea->node, false);
		return;
	} else if (c->enable_drop_area_draw && !c->droparea->node.enabled) {
		wlr_scene_node_raise_to_top(&c->droparea->node);
		wlr_scene_node_set_enabled(&c->droparea->node, true);
		first_draw = true;
	}

	int32_t bw = (int32_t)c->bw;
	int32_t client_width = c->geom.width - 2 * bw;
	int32_t client_height = c->geom.height - 2 * bw;

	double rel_x = server.cursor->x - c->geom.x - bw;
	double rel_y = server.cursor->y - c->geom.y - bw;

	struct wlr_box drop_box;
	const Layout *cur_layout = c->mon->pertag->ltidxs[get_client_tag_idx(c)];
	bool dwindle_familiar =
		cur_layout->id == DWINDLE && config.dwindle_drop_simple_split;

	if (dwindle_familiar) {
		bool split_h = c->geom.width >= c->geom.height;
		float ratio = config.dwindle_split_ratio;
		if (split_h) {
			if (rel_x < client_width * 0.5) {
				drop_direction = LEFT;
				drop_box.x = bw;
				drop_box.y = bw;
				drop_box.width = (int32_t)(client_width * ratio);
				drop_box.height = client_height;
			} else {
				drop_direction = RIGHT;
				drop_box.x = bw + (int32_t)(client_width * ratio);
				drop_box.y = bw;
				drop_box.width = client_width - (int32_t)(client_width * ratio);
				drop_box.height = client_height;
			}
		} else {
			if (rel_y < client_height * 0.5) {
				drop_direction = UP;
				drop_box.x = bw;
				drop_box.y = bw;
				drop_box.width = client_width;
				drop_box.height = (int32_t)(client_height * ratio);
			} else {
				drop_direction = DOWN;
				drop_box.x = bw;
				drop_box.y = bw + (int32_t)(client_height * ratio);
				drop_box.width = client_width;
				drop_box.height =
					client_height - (int32_t)(client_height * ratio);
			}
		}
	} else if (cur_layout->id == TILE || cur_layout->id == DECK ||
			   cur_layout->id == CENTER_TILE || cur_layout->id == RIGHT_TILE) {
		if (c->ismaster) {
			if (c->mon->visible_tiling_clients == 1) {
				if (rel_x < client_width * 0.5) {
					drop_direction = LEFT;
					drop_box.x = bw;
					drop_box.y = bw;
					drop_box.width = client_width / 2;
					drop_box.height = client_height;
				} else {
					drop_direction = RIGHT;
					drop_box.x = bw + client_width / 2;
					drop_box.y = bw;
					drop_box.width = client_width / 2;
					drop_box.height = client_height;
				}
			} else {
				drop_box.x = bw;
				drop_box.y = bw;
				drop_box.width = client_width;
				drop_box.height = client_height;
				drop_direction = UNDIR;
			}
		} else {
			if (rel_y < client_height * 0.5) {
				drop_direction = UP;
				drop_box.x = bw;
				drop_box.y = bw;
				drop_box.width = client_width;
				drop_box.height = client_height / 2;
			} else {
				drop_direction = DOWN;
				drop_box.x = bw;
				drop_box.y = bw + client_height / 2;
				drop_box.width = client_width;
				drop_box.height = client_height / 2;
			}
		}
	} else if (cur_layout->id == VERTICAL_TILE ||
			   cur_layout->id == VERTICAL_DECK) {
		if (c->ismaster) {
			if (c->mon->visible_tiling_clients == 1) {
				if (rel_y < client_height * 0.5) {
					drop_direction = UP;
					drop_box.x = bw;
					drop_box.y = bw;
					drop_box.width = client_width;
					drop_box.height = client_height / 2;
				} else {
					drop_direction = DOWN;
					drop_box.x = bw;
					drop_box.y = bw + client_height / 2;
					drop_box.width = client_width;
					drop_box.height = client_height / 2;
				}
			} else {
				drop_box.x = bw;
				drop_box.y = bw;
				drop_box.width = client_width;
				drop_box.height = client_height;
				drop_direction = UNDIR;
			}
		} else {
			if (rel_x < client_width * 0.5) {
				drop_direction = LEFT;
				drop_box.x = bw;
				drop_box.y = bw;
				drop_box.width = client_width / 2;
				drop_box.height = client_height;
			} else {
				drop_direction = RIGHT;
				drop_box.x = bw + client_width / 2;
				drop_box.y = bw;
				drop_box.width = client_width / 2;
				drop_box.height = client_height;
			}
		}
	} else {
		double dist_left = rel_x;
		double dist_right = client_width - rel_x;
		double dist_top = rel_y;
		double dist_bottom = client_height - rel_y;

		if (dist_left <= dist_right && dist_left <= dist_top &&
			dist_left <= dist_bottom) {
			drop_direction = LEFT;
			drop_box.x = bw;
			drop_box.y = bw;
			drop_box.width = client_width / 2;
			drop_box.height = client_height;
		} else if (dist_right <= dist_top && dist_right <= dist_bottom) {
			drop_direction = RIGHT;
			drop_box.x = bw + client_width / 2;
			drop_box.y = bw;
			drop_box.width = client_width / 2;
			drop_box.height = client_height;
		} else if (dist_top <= dist_bottom) {
			drop_direction = UP;
			drop_box.x = bw;
			drop_box.y = bw;
			drop_box.width = client_width;
			drop_box.height = client_height / 2;
		} else {
			drop_direction = DOWN;
			drop_box.x = bw;
			drop_box.y = bw + client_height / 2;
			drop_box.width = client_width;
			drop_box.height = client_height / 2;
		}
	}

	if (!first_draw && c->drop_direction == drop_direction)
		return;
	c->drop_direction = drop_direction;

	wlr_scene_node_set_position(&c->droparea->node, drop_box.x, drop_box.y);
	wlr_scene_rect_set_size(c->droparea, drop_box.width, drop_box.height);
}
void client_apply_clip(Client *c, float factor) {
	if (c->iskilling || !client_surface(c)->mapped)
		return;

	/*
	 * Overview card mode: content is shown by the independent card tree; update
	 * the card position/scale here and redraw the decoration nodes to match the
	 * card geometry.
	 */
	if (c->ov_card_tree) {
		struct ivec2 offsets = compute_edge_offsets(c);

		struct wlr_box clip_box;
		struct ivec2 surface_clip_offset;
		client_get_clip(c, &clip_box);
		surface_clip_offset = clip_to_hide(c, &clip_box, offsets);

		/* Decorations are drawn against the card geometry. */
		client_draw_border(c, offsets);
		client_draw_shadow(c, offsets);
		client_draw_groupbar(c, offsets);
		client_draw_blur(c, surface_clip_offset);
		client_draw_shield(c, surface_clip_offset);

		overview_layout_card(c);
		overview_card_set_corner_radii(c, set_client_corner_location(c));
		return;
	}

	struct ivec2 offsets = compute_edge_offsets(c);

	struct wlr_box clip_box;
	bool should_render_client_surface = false;
	struct ivec2 surface_clip_offset;
	BufferData buffer_data;

	struct fx_corner_radii current_corner_location =
		set_client_corner_location(c);

	if (!config.animations && !c->overview_scene_surface) {
		c->animation.running = false;
		c->need_output_flush = false;
		c->animainit_geom = c->current = c->pending = c->animation.current =
			c->geom;

		client_get_clip(c, &clip_box);
		surface_clip_offset = clip_to_hide(c, &clip_box, offsets);

		client_draw_border(c, offsets);
		client_draw_shadow(c, offsets);
		client_draw_groupbar(c, offsets);
		client_draw_blur(c, surface_clip_offset);
		client_draw_shield(c, surface_clip_offset);

		if (clip_box.width <= 0 || clip_box.height <= 0) {
			should_render_client_surface = false;
			wlr_scene_node_set_enabled(&c->scene_surface->node, false);
		} else {
			should_render_client_surface = true;
			wlr_scene_node_set_enabled(&c->scene_surface->node, true);
		}

		if (!should_render_client_surface)
			return;

		/*
		 * X11 is clipped with source_box + dest_size (buffer physical size *
		 * scale, clip logical size); wlr_scene clip would scale content by the
		 * physical size.
		 */
		if (client_is_x11(c))
			client_update_xwayland_clip(c, &clip_box);
		else if (!c->overview_scene_surface)
			wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node,
											   &clip_box);

		buffer_set_effect(c, (BufferData){1.0f, 1.0f, clip_box.width,
										  clip_box.height,
										  current_corner_location, true});
		return;
	}

	int32_t width, height;
	client_actual_size(c, &width, &height);

	struct wlr_box geometry;
	client_get_geometry(c, &geometry);
	clip_box = (struct wlr_box){
		.x = geometry.x,
		.y = geometry.y,
		.width = width,
		.height = height,
	};

	if (client_is_x11(c)) {
		clip_box.x = 0;
		clip_box.y = 0;
	}

	surface_clip_offset = clip_to_hide(c, &clip_box, offsets);

	client_draw_border(c, offsets);
	client_draw_shadow(c, offsets);
	client_draw_groupbar(c, offsets);
	client_draw_shield(c, surface_clip_offset);
	client_draw_blur(c, surface_clip_offset);
	/*
	 * During animation, sync the X11 root surface dest_size / clipping
	 * (source_box + dest_size).
	 */
	if (client_is_x11(c))
		client_update_xwayland_clip(c, &clip_box);
	else
		client_update_xwayland_dest_size(c);

	if (clip_box.width <= 0 || clip_box.height <= 0) {
		should_render_client_surface = false;
		wlr_scene_node_set_enabled(&c->scene_surface->node, false);
	} else {
		should_render_client_surface = true;
		wlr_scene_node_set_enabled(&c->scene_surface->node, true);
	}

	if (!should_render_client_surface)
		return;

	/*
	 * X11 clipping is already done above by client_update_xwayland_clip with
	 * source_box; wlr_scene clip would scale the buffer by its physical size
	 * and cannot be used for X11.
	 */
	if (!c->overview_scene_surface && !client_is_x11(c))
		wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip_box);

	int32_t actual_surface_width =
		geometry.width - surface_clip_offset.x - surface_clip_offset.width;
	int32_t actual_surface_height =
		geometry.height - surface_clip_offset.y - surface_clip_offset.height;

	if (actual_surface_width <= 0 || actual_surface_height <= 0)
		return;

	buffer_data.should_scale = true;
	buffer_data.width = clip_box.width;
	buffer_data.height = clip_box.height;
	buffer_data.corner_location = current_corner_location;

	if (factor == 1.0 && !c->overview_scene_surface) {
		buffer_data.width_scale = 1.0;
		buffer_data.height_scale = 1.0;
	} else {
		buffer_data.width_scale =
			(float)buffer_data.width / actual_surface_width;
		buffer_data.height_scale =
			(float)buffer_data.height / actual_surface_height;
	}

	buffer_set_effect(c, buffer_data);
}
void fadeout_client_animation_next_tick(Client *c) {
	if (!c)
		return;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	int32_t passed_time = timespec_to_ms(&now) - c->animation.time_started;
	double animation_passed =
		c->animation.duration
			? (double)passed_time / (double)c->animation.duration
			: 1.0;

	int32_t type = c->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);

	int32_t width = c->animation.initial.width +
					(c->current.width - c->animation.initial.width) * factor;
	int32_t height = c->animation.initial.height +
					 (c->current.height - c->animation.initial.height) * factor;
	int32_t x = c->animation.initial.x +
				(c->current.x - c->animation.initial.x) * factor;
	int32_t y = c->animation.initial.y +
				(c->current.y - c->animation.initial.y) * factor;

	wlr_scene_node_set_position(&c->scene->node, x, y);

	c->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	double opacity_eased_progress =
		find_animation_curve_at(animation_passed, OPAFADEOUT);
	double percent = config.fadeout_begin_opacity -
					 (opacity_eased_progress * config.fadeout_begin_opacity);
	double opacity = MANGO_MAX(percent, 0);

	if (config.animation_fade_out && !c->nofadeout)
		wlr_scene_node_for_each_buffer(&c->scene->node,
									   scene_buffer_apply_opacity, &opacity);

	if ((c->animation_type_close &&
		 strcmp(c->animation_type_close, "zoom") == 0) ||
		(!c->animation_type_close &&
		 strcmp(config.animation_type_close, "zoom") == 0)) {
		BufferData buffer_data;
		buffer_data.width = width;
		buffer_data.height = height;
		buffer_data.width_scale = animation_passed;
		buffer_data.height_scale = animation_passed;
		wlr_scene_node_for_each_buffer(
			&c->scene->node, snap_scene_buffer_apply_effect, &buffer_data);
	}

	if (animation_passed >= 1.0) {
		wl_list_remove(&c->fadeout_link);
		wlr_scene_node_destroy(&c->scene->node);
		free(c);
	}
}
void client_animation_next_tick(Client *c) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	int32_t passed_time = timespec_to_ms(&now) - c->animation.time_started;
	double animation_passed =
		c->animation.duration
			? (double)passed_time / (double)c->animation.duration
			: 1.0;

	int32_t type = c->animation.action == NONE ? MOVE : c->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);

	int32_t width = c->animation.initial.width +
					(c->current.width - c->animation.initial.width) * factor;
	int32_t height = c->animation.initial.height +
					 (c->current.height - c->animation.initial.height) * factor;
	int32_t x = c->animation.initial.x +
				(c->current.x - c->animation.initial.x) * factor;
	int32_t y = c->animation.initial.y +
				(c->current.y - c->animation.initial.y) * factor;

	wlr_scene_node_set_position(&c->scene->node, x, y);
	c->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	c->is_pending_open_animation = false;
	client_apply_clip(c, factor);

	if (animation_passed >= 1.0) {
		c->animation.action = MOVE;
		c->animation.tagining = false;
		c->animation.running = false;
		c->animation.overining = false;

		if (c->animation.tagouting) {
			c->animation.tagouting = false;
			wlr_scene_node_set_enabled(&c->scene->node, false);
			c->animation.tagouted = true;
			c->animation.current = c->geom;
		}

		Client *pointer_c = NULL;
		double sx, sy;
		node_at_point(server.cursor->x, server.cursor->y, NULL, &pointer_c,
					  NULL, NULL, &sx, &sy);
		struct wlr_surface *surface =
			pointer_c && pointer_c == c ? client_surface(pointer_c) : NULL;

		if (surface && pointer_c == server.selected_monitor->sel &&
			!server.selected_monitor->isoverview)
			wlr_seat_pointer_notify_enter(server.seat, surface, sx, sy);

		c->need_output_flush = false;
	}
}
void init_fadeout_client(Client *c) {
	if (!c->mon || client_is_unmanaged(c))
		return;

	if (!c->scene)
		return;

	if (c->shield_when_capture && server.active_capture_count > 0)
		return;

	if ((c->animation_type_close &&
		 strcmp(c->animation_type_close, "none") == 0) ||
		(!c->animation_type_close &&
		 strcmp(config.animation_type_close, "none") == 0))
		return;

	Client *fadeout_client = ecalloc(1, sizeof(*fadeout_client));

	wlr_scene_node_set_enabled(&c->scene->node, true);
	client_set_border_color(c, config.bordercolor);
	if (c->ov_card_tree) {
		/*
		 * Closing in overview: the fadeout snapshot is based on the
		 * already-scaled card tree. Snapshotting accumulates node coordinates,
		 * but card tree coordinates are relative to (bw,bw), so a direct
		 * snapshot would land near the layer origin; temporarily move the card
		 * tree to the card's absolute screen coordinates before snapshotting.
		 */
		int32_t abs_x = c->animation.current.x + (int32_t)c->bw;
		int32_t abs_y = c->animation.current.y + (int32_t)c->bw;
		wlr_scene_node_set_position(&c->ov_card_tree->node, abs_x, abs_y);
		fadeout_client->scene = wlr_scene_tree_snapshot(
			&c->ov_card_tree->node, server.layers[LyrFadeOut]);
		overview_destroy_card(c);
		c->overview_scene_surface = NULL;
	} else {
		fadeout_client->scene =
			wlr_scene_tree_snapshot(&c->scene->node, server.layers[LyrFadeOut]);
	}
	wlr_scene_node_set_enabled(&c->scene->node, false);

	if (!fadeout_client->scene) {
		free(fadeout_client);
		return;
	}

	fadeout_client->animation.duration = config.animation_duration_close;
	fadeout_client->geom = fadeout_client->current =
		fadeout_client->animainit_geom = fadeout_client->animation.initial =
			c->animation.current;
	fadeout_client->mon = c->mon;
	fadeout_client->animation_type_close = c->animation_type_close;
	fadeout_client->animation.action = CLOSE;
	fadeout_client->bw = c->bw;
	fadeout_client->nofadeout = c->nofadeout;

	fadeout_client->animation.initial.x = 0;
	fadeout_client->animation.initial.y = 0;

	if ((!c->animation_type_close &&
		 strcmp(config.animation_type_close, "fade") == 0) ||
		(c->animation_type_close &&
		 strcmp(c->animation_type_close, "fade") == 0)) {
		fadeout_client->current.x = 0;
		fadeout_client->current.y = 0;
		fadeout_client->current.width = 0;
		fadeout_client->current.height = 0;
	} else if ((c->animation_type_close &&
				strcmp(c->animation_type_close, "slide") == 0) ||
			   (!c->animation_type_close &&
				strcmp(config.animation_type_close, "slide") == 0)) {
		fadeout_client->current.y =
			c->geom.y + c->geom.height / 2 > c->mon->m.y + c->mon->m.height / 2
				? c->mon->m.height - (c->animation.current.y - c->mon->m.y)
				: c->mon->m.y - c->geom.height;
		fadeout_client->current.x = 0;
	} else {
		fadeout_client->current.y =
			(fadeout_client->geom.height -
			 fadeout_client->geom.height * config.zoom_end_ratio) /
			2;
		fadeout_client->current.x =
			(fadeout_client->geom.width -
			 fadeout_client->geom.width * config.zoom_end_ratio) /
			2;
		fadeout_client->current.width =
			fadeout_client->geom.width * config.zoom_end_ratio;
		fadeout_client->current.height =
			fadeout_client->geom.height * config.zoom_end_ratio;
	}

	fadeout_client->animation.time_started = get_now_in_ms();
	wlr_scene_node_set_enabled(&fadeout_client->scene->node, true);
	wl_list_insert(&server.fadeout_clients, &fadeout_client->fadeout_link);

	request_fresh_all_monitors();
}
void client_apply_finish_geometry(Client *c) {
	if (!c || !c->scene)
		return;

	wlr_scene_node_set_position(&c->scene->node, c->pending.x, c->pending.y);
	c->animation.current = c->animainit_geom = c->animation.initial =
		c->pending = c->current = c->geom;
	client_apply_clip(c, 1.0);
	c->need_output_flush = false;
}
void client_commit(Client *c) {
	c->current = c->pending;

	if (c->animation.should_animate) {
		if (!c->animation.running)
			c->animation.current = c->animainit_geom;

		c->animation.initial = c->animainit_geom;
		c->animation.time_started = get_now_in_ms();
		c->animation.running = true;
		c->animation.should_animate = false;
	} else {
		if (!c->animation.running) {
			c->animation.current = c->geom;
		}
	}

	// When animation is disabled, set the surface position and size early
	// so pointer_process_motion focus does not hit the surface from the
	// previous frame.
	if (!config.animations)
		client_apply_finish_geometry(c);

	request_fresh_all_monitors();
}
void client_set_pending_state(Client *c) {
	if (!c || c->iskilling)
		return;

	if (!config.animations)
		c->animation.should_animate = false;
	else if (config.animations && c->animation.tagining)
		c->animation.should_animate = true;
	else if (config.animations && c->animation.action == OVERVIEW &&
			 c->animation.overview_enter_anim_set)
		/* Overview enter animation: force start after setting the zoom; not
		 * forced during pre-arrangement to avoid jitter. */
		c->animation.should_animate = true;
	else if (c == server.grab_client ||
			 (!c->is_pending_open_animation &&
			  wlr_box_equal(&c->current, &c->pending)))
		c->animation.should_animate = false;
	else
		c->animation.should_animate = true;

	if (((c->animation_type_open &&
		  strcmp(c->animation_type_open, "none") == 0) ||
		 (!c->animation_type_open &&
		  strcmp(config.animation_type_open, "none") == 0)) &&
		c->animation.action == OPEN)
		c->animation.duration = 0;

	if (server.start_drag_window) {
		c->animation.should_animate = false;
		c->animation.duration = 0;
	}

	if (c->isnoanimation) {
		c->animation.should_animate = false;
		c->animation.duration = 0;
	}

	client_commit(c);
	c->dirty = true;
}
void resize_apply(Client *c, struct wlr_box geo, ResizeOpts opts) {
	if (!c || !c->mon || !client_surface(c)->mapped)
		return;

	if (!c->mon)
		return;

	c->need_output_flush = true;
	c->dirty = true;

	struct wlr_box *bbox = (opts.interact || c->isfloating || c->isfullscreen)
							   ? &server.scene_geometry
							   : &c->mon->w;
	struct wlr_box clip;

	if (is_scroller_layout(c->mon) &&
		(!c->isfloating || c == server.grab_client)) {
		c->geom = geo;
		c->geom.width = MANGO_MAX(1 + 2 * (int32_t)c->bw, c->geom.width);
		c->geom.height = MANGO_MAX(1 + 2 * (int32_t)c->bw, c->geom.height);
	} else {
		c->geom = geo;
		client_apply_bounds(c, bbox);
	}

	if (!c->isnosizehint && !c->ismaximizescreen && !c->isfullscreen &&
		c->isfloating)
		client_set_size_bound(c);

	if (!c->is_pending_open_animation)
		c->animation.begin_fade_in = false;

	if (c->animation.overining)
		c->animation.action = OVERVIEW;
	else if (c->animation.action == OPEN && !c->animation.tagining &&
			 !c->animation.tagouting && wlr_box_equal(&c->geom, &c->current))
		; /* keep current action */
	else if (c->animation.tagouting) {
		c->animation.duration = config.animation_duration_tag;
		c->animation.action = TAG;
	} else if (c->animation.tagining) {
		c->animation.duration = config.animation_duration_tag;
		c->animation.action = TAG;
	} else if (c->is_pending_open_animation) {
		c->animation.duration = config.animation_duration_open;
		c->animation.action = OPEN;
	} else {
		c->animation.duration = config.animation_duration_move;
		c->animation.action = MOVE;
	}

	if (c->animation.tagouting)
		c->animainit_geom = c->animation.current;
	else if (c->animation.tagining) {
		c->animainit_geom.height = c->animation.current.height;
		c->animainit_geom.width = c->animation.current.width;
	} else if (c->is_pending_open_animation)
		set_client_open_animation(c, c->geom);
	else
		c->animainit_geom = c->animation.current;

	if (c->isnoborder || c->iskilling)
		c->bw = 0;

	bool hit_no_border = check_hit_no_border(c);
	if (hit_no_border) {
		c->bw = 0;
		c->fake_no_border = true;
	}

	if (!c->mon->isoverview)
		c->configure_serial = client_set_size(c, c->geom.width - 2 * c->bw,
											  c->geom.height - 2 * c->bw);

	if (c->configure_serial != 0)
		c->mon->resizing_count_pending++;

	if (c == server.grab_client) {
		struct ivec2 offsets = compute_edge_offsets(c);

		c->animation.running = false;
		c->need_output_flush = false;
		c->animainit_geom = c->current = c->pending = c->animation.current =
			c->geom;
		wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);

		client_draw_border(c, offsets);
		client_get_clip(c, &clip);
		client_draw_groupbar(c, offsets);
		client_draw_shadow(c, offsets);

		struct ivec2 surface_clip_offset = clip_to_hide(c, &clip, offsets);
		client_draw_shield(c, surface_clip_offset);
		client_draw_blur(c, surface_clip_offset);

		if (client_is_x11(c))
			client_update_xwayland_clip(c, &clip);
		else
			wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip);
		return;
	}

	if (!c->animation.tagouting && !c->iskilling)
		c->pending = c->geom;

	if (c->swallowing && c->animation.action == OPEN)
		c->animainit_geom = c->swallowing->animation.current;

	if (c->swallowdby)
		c->animainit_geom = c->geom;

	if ((c->isglobal || c->isunglobal) && c->isfloating &&
		c->animation.action == TAG)
		c->animainit_geom = c->geom;

	if (c->scratchpad_switching_mon && c->isfloating)
		c->animainit_geom = c->geom;

	if (!opts.skip_ov_enter_anim) {
		/* Clears the enter-animation flag (including the focused window) to
		 * avoid re-zooming when focus changes. */
		if (config.animations && c->mon->isoverview && c->animation.overining &&
			!c->animation.overview_enter_anim_set)
			c->animation.overining = false;

		/* Sets the enter zoom animation for all windows except sel. */
		if (config.animations && c->mon->isoverview && c != c->mon->sel &&
			c->animation.action == OVERVIEW &&
			!c->animation.overview_enter_anim_set) {
			c->animation.overview_enter_anim_set = true;
			set_overview_enter_animation(c);
		}
	}

	client_set_pending_state(c);
	client_update_border_color(c);
}

bool client_draw_fadeout_frame(Client *c) {
	if (!c)
		return false;

	fadeout_client_animation_next_tick(c);
	return true;
}

void client_set_unfocused_opacity_animation(Client *c) {
	float *border_color = get_border_color(c);
	wlr_scene_node_raise_to_top(&c->border->node);
	if (!config.animations) {
		client_update_border_color(c);
		return;
	}

	c->opacity_animation.duration = config.animation_duration_focus;
	memcpy(c->opacity_animation.target_border_color, border_color,
		   sizeof(c->opacity_animation.target_border_color));
	c->opacity_animation.target_opacity = c->unfocused_opacity;
	c->opacity_animation.time_started = get_now_in_ms();
	memcpy(c->opacity_animation.initial_border_color,
		   c->opacity_animation.current_border_color,
		   sizeof(c->opacity_animation.initial_border_color));
	c->opacity_animation.initial_opacity = c->opacity_animation.current_opacity;
	c->opacity_animation.running = true;
}
void resize(Client *c, struct wlr_box geo, int32_t interact) {
	resize_apply(c, geo, (ResizeOpts){.interact = interact});
}

void client_set_focused_opacity_animation(Client *c) {
	float *border_color = get_border_color(c);
	wlr_scene_node_lower_to_bottom(&c->border->node);

	if (!config.animations) {
		client_update_border_color(c);
		return;
	}

	c->opacity_animation.duration = config.animation_duration_focus;
	memcpy(c->opacity_animation.target_border_color, border_color,
		   sizeof(c->opacity_animation.target_border_color));
	c->opacity_animation.target_opacity = c->focused_opacity;
	c->opacity_animation.time_started = get_now_in_ms();
	memcpy(c->opacity_animation.initial_border_color,
		   c->opacity_animation.current_border_color,
		   sizeof(c->opacity_animation.initial_border_color));
	c->opacity_animation.initial_opacity = c->opacity_animation.current_opacity;
	c->opacity_animation.running = true;
}

bool client_apply_focus_opacity(Client *c) {
	if (config.blur && !c->noblur && c->blur_opacity != 1.0f &&
		c->animation.action != OPEN) {
		c->blur_opacity = 1.0f;
		wlr_scene_blur_set_strength(c->blur, 1.0f);
		wlr_scene_blur_set_alpha(c->blur, 1.0f);
	}

	float *border_color = get_border_color(c);
	if (c->isfullscreen) {
		c->opacity_animation.running = false;
		client_set_opacity(c, 1);
	} else if (c->animation.running && c->animation.action == OPEN) {
		c->opacity_animation.running = false;
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		int32_t passed_time = timespec_to_ms(&now) - c->animation.time_started;
		double linear_progress =
			c->animation.duration
				? (double)passed_time / (double)c->animation.duration
				: 1.0;

		double opacity_eased_progress =
			find_animation_curve_at(linear_progress, OPAFADEIN);
		float percent = config.animation_fade_in && !c->nofadein
							? opacity_eased_progress
							: 1.0;
		float opacity = c == server.selected_monitor->sel
							? c->focused_opacity
							: c->unfocused_opacity;
		float target_opacity = percent * (1.0 - config.fadein_begin_opacity) +
							   config.fadein_begin_opacity;
		if (target_opacity > opacity)
			target_opacity = opacity;

		memcpy(c->opacity_animation.current_border_color,
			   c->opacity_animation.target_border_color,
			   sizeof(c->opacity_animation.current_border_color));
		c->opacity_animation.current_opacity = target_opacity;
		client_set_opacity(c, target_opacity);
		if (config.blur && !c->noblur && !config.blur_optimized) {
			float blur_val = MIN(percent * (1.0 - config.fadein_begin_opacity) +
									 config.fadein_begin_opacity,
								 1.0);
			c->blur_opacity = blur_val;
			wlr_scene_blur_set_strength(c->blur, blur_val);
			wlr_scene_blur_set_alpha(c->blur, blur_val);
		}
		client_set_border_color(c, c->opacity_animation.target_border_color);
	} else if (config.animations && c->opacity_animation.running) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		int32_t passed_time =
			timespec_to_ms(&now) - c->opacity_animation.time_started;
		double linear_progress =
			c->opacity_animation.duration
				? (double)passed_time / (double)c->opacity_animation.duration
				: 1.0;

		float eased_progress = find_animation_curve_at(linear_progress, FOCUS);

		c->opacity_animation.current_opacity =
			c->opacity_animation.initial_opacity +
			(c->opacity_animation.target_opacity -
			 c->opacity_animation.initial_opacity) *
				eased_progress;
		client_set_opacity(c, c->opacity_animation.current_opacity);

		for (int32_t i = 0; i < 4; i++) {
			c->opacity_animation.current_border_color[i] =
				c->opacity_animation.initial_border_color[i] +
				(c->opacity_animation.target_border_color[i] -
				 c->opacity_animation.initial_border_color[i]) *
					eased_progress;
		}
		client_set_border_color(c, c->opacity_animation.current_border_color);
		if (linear_progress >= 1.0f)
			c->opacity_animation.running = false;
		else
			return true;
	} else if (c == server.selected_monitor->sel) {
		c->opacity_animation.running = false;
		c->opacity_animation.current_opacity = c->focused_opacity;
		memcpy(c->opacity_animation.current_border_color, border_color,
			   sizeof(c->opacity_animation.current_border_color));
		client_set_opacity(c, c->focused_opacity);
	} else {
		c->opacity_animation.running = false;
		c->opacity_animation.current_opacity = c->unfocused_opacity;
		memcpy(c->opacity_animation.current_border_color, border_color,
			   sizeof(c->opacity_animation.current_border_color));
		client_set_opacity(c, c->unfocused_opacity);
	}

	return false;
}

bool client_draw_frame(Client *c) {
	bool need_next_tick = false;
	bool force_render = false;

	if (!c || !client_surface(c)->mapped)
		return false;

	// always render when scene is disabled
	if (c->force_render && !c->scene->node.enabled) {
		force_render = client_force_render(c);
		need_next_tick = force_render || need_next_tick;
	}

	if (!c->need_output_flush)
		return client_apply_focus_opacity(c) || need_next_tick;

	if (config.animations && c->animation.running) {
		need_next_tick = true;
		client_animation_next_tick(c);
	} else {
		client_apply_finish_geometry(c);
	}

	bool need_fade_focus = client_apply_focus_opacity(c);

	return need_next_tick || need_fade_focus;
}
