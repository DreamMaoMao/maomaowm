#include "mango/animation/layer.h"
#include "mango/animation/common.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/manage/client.h"

void layer_actual_size(LayerSurface *l, int32_t *width, int32_t *height) {
	if (l->animation.running) {
		*width = l->animation.current.width;
		*height = l->animation.current.height;
	} else {
		*width = l->geom.width;
		*height = l->geom.height;
	}
}

void get_layer_area_bound(LayerSurface *l, struct wlr_box *bound) {
	const struct wlr_layer_surface_v1_state *state = &l->layer_surface->current;

	if (state->exclusive_zone > 0 || state->exclusive_zone == -1)
		*bound = l->mon->m;
	else
		*bound = l->mon->w;
}
void get_layer_target_geometry(LayerSurface *l, struct wlr_box *target_box) {

	if (!l || !l->mapped)
		return;

	const struct wlr_layer_surface_v1_state *state = &l->layer_surface->current;

	// restriction area
	// waybar is usually greater than 0, meaning it occupies a certain amount of
	// area, so position calculation should also use the entire area as a
	// baseline if it's -1, it may mean exclusive use of all available space if
	// it's 0, it should mean using the available area outside the
	// exclusive_zone
	struct wlr_box bounds;
	if (state->exclusive_zone > 0 || state->exclusive_zone == -1)
		bounds = l->mon->m;
	else
		bounds = l->mon->w;

	// Initialize geometry position
	struct wlr_box box = {.width = state->desired_width,
						  .height = state->desired_height};

	// horizontal positioning
	const int32_t both_horiz =
		ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	if (box.width == 0) {
		box.x = bounds.x;
	} else if ((state->anchor & both_horiz) == both_horiz) {
		box.x = bounds.x + ((bounds.width - box.width) / 2);
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
		box.x = bounds.x;
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
		box.x = bounds.x + (bounds.width - box.width);
	} else {
		box.x = bounds.x + ((bounds.width - box.width) / 2);
	}

	// vertical positioning
	const int32_t both_vert =
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	if (box.height == 0) {
		box.y = bounds.y;
	} else if ((state->anchor & both_vert) == both_vert) {
		box.y = bounds.y + ((bounds.height - box.height) / 2);
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
		box.y = bounds.y;
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
		box.y = bounds.y + (bounds.height - box.height);
	} else {
		box.y = bounds.y + ((bounds.height - box.height) / 2);
	}

	// apply margins
	if (box.width == 0) {
		box.x += state->margin.left;
		box.width = bounds.width - (state->margin.left + state->margin.right);
	} else {
		if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
			box.x += state->margin.left;
		} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
			box.x -= state->margin.right;
		}
	}

	if (box.height == 0) {
		box.y += state->margin.top;
		box.height = bounds.height - (state->margin.top + state->margin.bottom);
	} else {
		if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
			box.y += state->margin.top;
		} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
			box.y -= state->margin.bottom;
		}
	}

	target_box->x = box.x;
	target_box->y = box.y;
	target_box->width = box.width;
	target_box->height = box.height;
}
void set_layer_dir_animaiton(LayerSurface *l, struct wlr_box *geo) {
	int32_t slide_direction;
	int32_t horizontal, horizontal_value;
	int32_t vertical, vertical_value;
	int32_t center_x, center_y;

	if (!l)
		return;

	struct wlr_box usable_area;
	get_layer_area_bound(l, &usable_area);

	geo->width = l->geom.width;
	geo->height = l->geom.height;

	center_x = l->geom.x + l->geom.width / 2;
	center_y = l->geom.y + l->geom.height / 2;
	horizontal =
		center_x > usable_area.x + usable_area.width / 2 ? RIGHT : LEFT;
	horizontal_value = horizontal == LEFT
						   ? center_x - usable_area.x
						   : usable_area.x + usable_area.width - center_x;
	vertical = center_y > usable_area.y + usable_area.height / 2 ? DOWN : UP;
	vertical_value = vertical == UP
						 ? center_y - l->mon->w.y
						 : usable_area.y + usable_area.height - center_y;
	slide_direction = horizontal_value < vertical_value ? horizontal : vertical;

	switch (slide_direction) {
	case UP:
		geo->x = l->geom.x;
		geo->y = usable_area.y - l->geom.height;
		break;
	case DOWN:
		geo->x = l->geom.x;
		geo->y = usable_area.y + usable_area.height;
		break;
	case LEFT:
		geo->x = usable_area.x - l->geom.width;
		geo->y = l->geom.y;
		break;
	case RIGHT:
		geo->x = usable_area.x + usable_area.width;
		geo->y = l->geom.y;
		break;
	default:
		geo->x = l->geom.x;
		geo->y = 0 - l->geom.height;
	}
}
void layer_draw_shield(LayerSurface *l) {
	int32_t width, height;

	if (!l->mapped)
		return;

	if (server.active_capture_count > 0 && l->shield_when_capture) {

		layer_actual_size(l, &width, &height);

		if (width <= 0 || height <= 0) {
			wlr_scene_node_set_enabled(&l->shield->node, false);
			return;
		}

		wlr_scene_node_raise_to_top(&l->shield->node);
		wlr_scene_node_set_position(&l->shield->node, 0, 0);
		wlr_scene_rect_set_size(l->shield, width, height);
		wlr_scene_node_set_enabled(&l->shield->node, true);
	} else {
		if (l->shield->node.enabled) {
			wlr_scene_node_lower_to_bottom(&l->shield->node);
			wlr_scene_node_set_position(&l->shield->node, 0, 0);
			wlr_scene_node_set_enabled(&l->shield->node, false);
		}
	}
}
void layer_draw_shadow(LayerSurface *l) {
	if (!l->mapped || !l->shadow)
		return;

	if (!config.shadows || !config.layer_shadows || l->noshadow) {
		wlr_scene_shadow_set_size(l->shadow, 0, 0);
		return;
	}

	int32_t width, height;
	layer_actual_size(l, &width, &height);

	int32_t delta = config.shadows_size;

	struct wlr_box layer_box = {
		.x = 0,
		.y = 0,
		.width = width,
		.height = height,
	};

	struct wlr_box shadow_box = {
		.x = config.shadows_position_x,
		.y = config.shadows_position_y,
		.width = width + 2 * delta,
		.height = height + 2 * delta,
	};

	struct wlr_box intersection_box;
	wlr_box_intersection(&intersection_box, &layer_box, &shadow_box);
	intersection_box.x -= config.shadows_position_x;
	intersection_box.y -= config.shadows_position_y;

	struct clipped_region clipped_region = {
		.area = intersection_box,
		.corners = corner_radii_all(config.border_radius),
	};

	wlr_scene_node_set_position(&l->shadow->node, shadow_box.x, shadow_box.y);

	wlr_scene_shadow_set_size(l->shadow, shadow_box.width, shadow_box.height);
	wlr_scene_shadow_set_clipped_region(l->shadow, clipped_region);
}
void layer_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer,
									 int32_t sx, int32_t sy, void *data) {
	BufferData *buffer_data = (BufferData *)data;

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);

	if (scene_surface == NULL)
		return;

	struct wlr_surface *surface = scene_surface->surface;

	int32_t surface_width = surface->current.width * buffer_data->width_scale;
	int32_t surface_height =
		surface->current.height * buffer_data->height_scale;

	if (surface_height > 0 && surface_width > 0) {
		wlr_scene_buffer_set_dest_size(buffer, surface_width, surface_height);
	}
}
void layer_fadeout_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer,
											 int32_t sx, int32_t sy,
											 void *data) {
	BufferData *buffer_data = (BufferData *)data;
	wlr_scene_buffer_set_dest_size(buffer, buffer_data->width,
								   buffer_data->height);
}
void fadeout_layer_animation_next_tick(LayerSurface *l) {
	if (!l)
		return;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	int32_t passed_time = timespec_to_ms(&now) - l->animation.time_started;
	double animation_passed =
		l->animation.duration
			? (double)passed_time / (double)l->animation.duration
			: 1.0;

	int32_t type = l->animation.action = l->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);
	int32_t width = l->animation.initial.width +
					(l->current.width - l->animation.initial.width) * factor;
	int32_t height = l->animation.initial.height +
					 (l->current.height - l->animation.initial.height) * factor;

	int32_t x = l->animation.initial.x +
				(l->current.x - l->animation.initial.x) * factor;
	int32_t y = l->animation.initial.y +
				(l->current.y - l->animation.initial.y) * factor;

	wlr_scene_node_set_position(&l->scene->node, x, y);

	BufferData buffer_data;
	buffer_data.width = width;
	buffer_data.height = height;

	if ((!l->animation_type_close &&
		 strcmp(config.layer_animation_type_close, "zoom") == 0) ||
		(l->animation_type_close &&
		 strcmp(l->animation_type_close, "zoom") == 0)) {
		wlr_scene_node_for_each_buffer(&l->scene->node,
									   layer_fadeout_scene_buffer_apply_effect,
									   &buffer_data);
	}

	l->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	double opacity_eased_progress =
		find_animation_curve_at(animation_passed, OPAFADEOUT);

	double percent = config.fadeout_begin_opacity -
					 (opacity_eased_progress * config.fadeout_begin_opacity);

	double opacity = MANGO_MAX(percent, 0.0f);

	if (config.animation_fade_out)
		wlr_scene_node_for_each_buffer(&l->scene->node,
									   scene_buffer_apply_opacity, &opacity);

	if (animation_passed >= 1.0) {
		wl_list_remove(&l->fadeout_link);
		wlr_scene_node_destroy(&l->scene->node);
		free(l);
		l = NULL;
	}
}
void layer_animation_next_tick(LayerSurface *l) {
	if (!l || !l->mapped)
		return;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	int32_t passed_time = timespec_to_ms(&now) - l->animation.time_started;
	double animation_passed =
		l->animation.duration
			? (double)passed_time / (double)l->animation.duration
			: 1.0;

	int32_t type = l->animation.action == NONE ? MOVE : l->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);

	int32_t width = l->animation.initial.width +
					(l->current.width - l->animation.initial.width) * factor;
	int32_t height = l->animation.initial.height +
					 (l->current.height - l->animation.initial.height) * factor;

	int32_t x = l->animation.initial.x +
				(l->current.x - l->animation.initial.x) * factor;
	int32_t y = l->animation.initial.y +
				(l->current.y - l->animation.initial.y) * factor;

	double opacity_eased_progress =
		find_animation_curve_at(animation_passed, OPAFADEIN);

	double opacity = MANGO_MIN(config.fadein_begin_opacity +
								   opacity_eased_progress *
									   (1.0 - config.fadein_begin_opacity),
							   1.0f);

	if (config.animation_fade_in) {
		if (config.blur && !l->noblur && !config.blur_optimized) {
			wlr_scene_blur_set_strength(l->blur, opacity);
			wlr_scene_blur_set_alpha(l->blur, opacity);
		}
		wlr_scene_node_for_each_buffer(&l->scene->node,
									   scene_buffer_apply_opacity, &opacity);
	}

	wlr_scene_node_set_position(&l->scene->node, x, y);

	BufferData buffer_data;
	if (factor == 1.0) {
		buffer_data.width_scale = 1.0f;
		buffer_data.height_scale = 1.0f;
	} else {
		buffer_data.width_scale = (float)width / (float)l->current.width;
		buffer_data.height_scale = (float)height / (float)l->current.height;
	}

	if ((!l->animation_type_open &&
		 strcmp(config.layer_animation_type_open, "zoom") == 0) ||
		(l->animation_type_open &&
		 strcmp(l->animation_type_open, "zoom") == 0)) {
		wlr_scene_node_for_each_buffer(
			&l->scene->node, layer_scene_buffer_apply_effect, &buffer_data);
	}

	l->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	if (config.blur && config.blur_layer && !l->noblur && l->blur)
		wlr_scene_blur_set_size(l->blur, l->animation.current.width,
								l->animation.current.height);

	if (animation_passed >= 1.0) {
		l->animation.running = false;
		l->need_output_flush = false;
		l->animation.action = MOVE;
	}
}
void init_fadeout_layers(LayerSurface *l) {
	if (!config.animations || !config.layer_animations || l->noanim) {
		return;
	}

	if (!l->mon || !l->scene)
		return;

	if (l->shield_when_capture) {
		return;
	}

	if ((l->animation_type_close &&
		 strcmp(l->animation_type_close, "none") == 0) ||
		(!l->animation_type_close &&
		 strcmp(config.layer_animation_type_close, "none") == 0)) {
		return;
	}

	if (l->layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM ||
		l->layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND)
		return;

	LayerSurface *fadeout_layer = ecalloc(1, sizeof(*fadeout_layer));

	struct wlr_box usable_area;
	get_layer_area_bound(l, &usable_area);

	wlr_scene_node_set_enabled(&l->scene->node, true);
	fadeout_layer->scene =
		wlr_scene_tree_snapshot(&l->scene->node, server.layers[LyrFadeOut]);
	wlr_scene_node_set_enabled(&l->scene->node, false);

	if (!fadeout_layer->scene) {
		free(fadeout_layer);
		return;
	}

	fadeout_layer->animation.duration = config.animation_duration_close;
	fadeout_layer->geom = fadeout_layer->current =
		fadeout_layer->animainit_geom = fadeout_layer->animation.initial =
			l->animation.current;
	fadeout_layer->mon = l->mon;
	fadeout_layer->animation.action = CLOSE;
	fadeout_layer->animation_type_close = l->animation_type_close;
	fadeout_layer->animation_type_open = l->animation_type_open;

	// Here the coordinate setting for snap nodes uses relative coordinates;
	// absolute coordinates cannot be used This differs from ordinary nodes

	fadeout_layer->animation.initial.x = 0;
	fadeout_layer->animation.initial.y = 0;

	if ((!l->animation_type_close &&
		 strcmp(config.layer_animation_type_close, "zoom") == 0) ||
		(l->animation_type_close &&
		 strcmp(l->animation_type_close, "zoom") == 0)) {
		// Calculate the absolute coordinates and size to be set
		fadeout_layer->current.width =
			(float)l->animation.current.width * config.zoom_end_ratio;
		fadeout_layer->current.height =
			(float)l->animation.current.height * config.zoom_end_ratio;
		fadeout_layer->current.x = usable_area.x + usable_area.width / 2 -
								   fadeout_layer->current.width / 2;
		fadeout_layer->current.y = usable_area.y + usable_area.height / 2 -
								   fadeout_layer->current.height / 2;
		// calculate the deviation coordinates;
		// size is not needed because subsequent settings do not use its size
		// deviation, but instead directly scale the buffer
		fadeout_layer->current.x =
			fadeout_layer->current.x - l->animation.current.x;
		fadeout_layer->current.y =
			fadeout_layer->current.y - l->animation.current.y;

	} else if ((!l->animation_type_close &&
				strcmp(config.layer_animation_type_close, "slide") == 0) ||
			   (l->animation_type_close &&
				strcmp(l->animation_type_close, "slide") == 0)) {
		// Get the ending absolute coordinates and size of the slide animation
		set_layer_dir_animaiton(l, &fadeout_layer->current);
		// Also calculate the deviation coordinates and size that can be set
		fadeout_layer->current.x = fadeout_layer->current.x - l->geom.x;
		fadeout_layer->current.y = fadeout_layer->current.y - l->geom.y;
		fadeout_layer->current.width =
			fadeout_layer->current.width - l->geom.width;
		fadeout_layer->current.height =
			fadeout_layer->current.height - l->geom.height;
	} else {
		// the coordinates and size of the fade animation do not need to change
		fadeout_layer->current.x = 0;
		fadeout_layer->current.y = 0;
		fadeout_layer->current.width = 0;
		fadeout_layer->current.height = 0;
	}

	// animation start time
	fadeout_layer->animation.time_started = get_now_in_ms();

	// Insert the node into the close animation linked list;
	// during screen refresh, it will check whether there are nodes in the list
	// that can be applied to animation.
	wlr_scene_node_set_enabled(&fadeout_layer->scene->node, true);
	wl_list_insert(&server.fadeout_layers, &fadeout_layer->fadeout_link);

	// Requests a screen refresh.
	if (l->mon)
		wlr_output_schedule_frame(l->mon->wlr_output);
}
void layer_set_pending_state(LayerSurface *l) {
	if (!l || !l->mapped)
		return;

	struct wlr_box usable_area;
	get_layer_area_bound(l, &usable_area);

	l->pending = l->geom;

	if (l->animation.action == OPEN && !l->animation.running) {

		if ((!l->animation_type_open &&
			 strcmp(config.layer_animation_type_open, "zoom") == 0) ||
			(l->animation_type_open &&
			 strcmp(l->animation_type_open, "zoom") == 0)) {
			l->animainit_geom.width = l->geom.width * config.zoom_initial_ratio;
			l->animainit_geom.height =
				l->geom.height * config.zoom_initial_ratio;
			l->animainit_geom.x = usable_area.x + usable_area.width / 2 -
								  l->animainit_geom.width / 2;
			l->animainit_geom.y = usable_area.y + usable_area.height / 2 -
								  l->animainit_geom.height / 2;
		} else if ((!l->animation_type_open &&
					strcmp(config.layer_animation_type_open, "slide") == 0) ||
				   (l->animation_type_open &&
					strcmp(l->animation_type_open, "slide") == 0)) {

			set_layer_dir_animaiton(l, &l->animainit_geom);
		} else {
			l->animainit_geom.x = l->geom.x;
			l->animainit_geom.y = l->geom.y;
			l->animainit_geom.width = l->geom.width;
			l->animainit_geom.height = l->geom.height;
		}
	} else {
		l->animainit_geom = l->animation.current;
	}
	if (!config.animations || !config.layer_animations || l->noanim ||
		l->layer_surface->current.layer ==
			ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND ||
		l->layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM) {
		l->animation.should_animate = false;
	} else {
		l->animation.should_animate = true;
	}

	if (((l->animation_type_open &&
		  strcmp(l->animation_type_open, "none") == 0) ||
		 (!l->animation_type_open &&
		  strcmp(config.layer_animation_type_open, "none") == 0)) &&
		l->animation.action == OPEN) {
		l->animation.should_animate = false;
	}

	// Starts the animation.
	layer_commit(l);
	l->dirty = true;
}
void layer_commit(LayerSurface *l) {
	if (!l || !l->mapped)
		return;

	l->current = l->pending; // set the end position of the animation

	if (l->animation.should_animate) {
		if (!l->animation.running) {
			l->animation.current = l->animainit_geom;
		}

		l->animation.initial = l->animainit_geom;
		l->animation.time_started = get_now_in_ms();

		// mark the animation start
		l->animation.running = true;
		l->animation.should_animate = false;
	}
	// request screen refresh
	if (l->mon)
		wlr_output_schedule_frame(l->mon->wlr_output);
}
bool layer_draw_frame(LayerSurface *l) {
	if (!l || !l->mapped)
		return false;

	if (!l->need_output_flush)
		return false;

	if (l->layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_TOP &&
		l->layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
		return false;
	}

	if (config.animations && config.layer_animations && l->animation.running &&
		!l->noanim) {
		layer_animation_next_tick(l);
		layer_draw_shield(l);
		layer_draw_shadow(l);
	} else {
		layer_draw_shield(l);
		layer_draw_shadow(l);
		l->need_output_flush = false;
	}
	return true;
}
bool layer_draw_fadeout_frame(LayerSurface *l) {
	if (!l)
		return false;

	fadeout_layer_animation_next_tick(l);
	return true;
}
