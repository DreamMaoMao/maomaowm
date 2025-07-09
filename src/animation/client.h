void client_actual_size(Client *c, unsigned int *width, unsigned int *height) {
	*width = c->animation.current.width - c->bw;

	*height = c->animation.current.height - c->bw;
}

void set_rect_size(struct wlr_scene_rect *rect, int width, int height) {
	wlr_scene_rect_set_size(rect, GEZERO(width), GEZERO(height));
}

void apply_opacity_to_rect_nodes(Client *c, struct wlr_scene_node *node,
								 double animation_passed) {
	if (node->type == WLR_SCENE_NODE_RECT) {
		struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
		// Assuming the rect has a color field and we can modify it
		rect->color[0] = 0;
		rect->color[1] = 0;
		rect->color[2] = 0;
		rect->color[3] = 0;
		wlr_scene_rect_set_color(rect, rect->color);
	}

	// If the node is a tree, recursively traverse its children
	if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_tree *scene_tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &scene_tree->children, link) {
			apply_opacity_to_rect_nodes(c, child, animation_passed);
		}
	}
}

int is_special_animaiton_rule(Client *c) {
	int visible_client_number = 0;
	Client *count_c;
	wl_list_for_each(count_c, &clients, link) {
		if (count_c && VISIBLEON(count_c, selmon) && !count_c->isminied &&
			!count_c->iskilling && !count_c->isfloating) {
			visible_client_number++;
		}
	}

	if (is_scroller_layout(selmon) && !c->isfloating) {
		return DOWN;
	} else if (visible_client_number < 2 && !c->isfloating) {
		return DOWN;
	} else if (visible_client_number == 2 && !c->isfloating && !new_is_master) {
		return RIGHT;
	} else if (!c->isfloating && new_is_master) {
		return LEFT;
	} else {
		return UNDIR;
	}
}

void set_client_open_animaiton(Client *c, struct wlr_box geo) {
	int slide_direction;
	int horizontal, horizontal_value;
	int vertical, vertical_value;
	int special_direction;
	int center_x, center_y;
	if ((!c->animation_type_open && strcmp(animation_type_open, "zoom") == 0) ||
		(c->animation_type_open &&
		 strcmp(c->animation_type_open, "zoom") == 0)) {
		c->animainit_geom.width = geo.width * zoom_initial_ratio;
		c->animainit_geom.height = geo.height * zoom_initial_ratio;
		c->animainit_geom.x = geo.x + (geo.width - c->animainit_geom.width) / 2;
		c->animainit_geom.y =
			geo.y + (geo.height - c->animainit_geom.height) / 2;
		return;
	} else {
		special_direction = is_special_animaiton_rule(c);
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

void snap_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int sx,
									int sy, void *data) {
	animationScale *scale_data = (animationScale *)data;
	wlr_scene_buffer_set_dest_size(buffer, scale_data->width,
								   scale_data->height);
}

void scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int sx, int sy,
							   void *data) {
	animationScale *scale_data = (animationScale *)data;

	if (scale_data->should_scale && scale_data->height_scale < 1 &&
		scale_data->width_scale < 1) {
		scale_data->should_scale = false;
	}

	if (scale_data->should_scale && scale_data->height_scale == 1 &&
		scale_data->width_scale < 1) {
		scale_data->should_scale = false;
	}

	if (scale_data->should_scale && scale_data->height_scale < 1 &&
		scale_data->width_scale == 1) {
		scale_data->should_scale = false;
	}

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);

	if (scene_surface == NULL)
		return;

	struct wlr_surface *surface = scene_surface->surface;

	if (scale_data->should_scale) {

		unsigned int surface_width = surface->current.width;
		unsigned int surface_height = surface->current.height;

		surface_width = scale_data->width_scale < 1
							? surface_width
							: scale_data->width_scale * surface_width;
		surface_height = scale_data->height_scale < 1
							 ? surface_height
							 : scale_data->height_scale * surface_height;

		if (surface_width > scale_data->width &&
			wlr_subsurface_try_from_wlr_surface(surface) == NULL) {
			surface_width = scale_data->width;
		}

		if (surface_height > scale_data->height &&
			wlr_subsurface_try_from_wlr_surface(surface) == NULL) {
			surface_height = scale_data->height;
		}

		if (surface_width > scale_data->width &&
			wlr_subsurface_try_from_wlr_surface(surface) != NULL) {
			return;
		}

		if (surface_height > scale_data->height &&
			wlr_subsurface_try_from_wlr_surface(surface) != NULL) {
			return;
		}

		if (surface_height > 0 && surface_width > 0) {
			wlr_scene_buffer_set_dest_size(buffer, surface_width,
										   surface_height);
		}
	}
	// TODO: blur set, opacity set

	if (wlr_xdg_popup_try_from_wlr_surface(surface) != NULL)
		return;

	float target_opacity = scale_data->percent + fadein_begin_opacity;
	if (target_opacity > scale_data->opacity) {
		target_opacity = scale_data->opacity;
	}
	wlr_scene_buffer_set_opacity(buffer, target_opacity);
}

void buffer_set_effect(Client *c, animationScale data) {

	if (!c || c->iskilling)
		return;

	if (c->animation.tagouting || c->animation.tagouted ||
		c->animation.tagining) {
		data.should_scale = false;
	}

	if (c == grabc)
		data.should_scale = false;

	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   scene_buffer_apply_effect, &data);
}

void apply_border(Client *c) {
	bool hit_no_border = false;

	if (c->iskilling || !client_surface(c)->mapped)
		return;

	hit_no_border = check_hit_no_border(c);

	if (hit_no_border && smartgaps) {
		c->bw = 0;
		c->fake_no_border = true;
	} else if (hit_no_border && !smartgaps) {
		set_rect_size(c->border[0], 0, 0);
		set_rect_size(c->border[1], 0, 0);
		set_rect_size(c->border[2], 0, 0);
		set_rect_size(c->border[3], 0, 0);
		wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
		c->fake_no_border = true;
		return;
	} else if (!c->isfullscreen && VISIBLEON(c, c->mon)) {
		c->bw = c->isnoborder ? 0 : borderpx;
		c->fake_no_border = false;
	}

	struct wlr_box fullgeom = c->animation.current;
	// 一但在GEZERO如果使用无符号，那么其他数据也会转换为无符号导致没有负数出错
	int bw = (int)c->bw;

	int right_offset, bottom_offset, left_offset, top_offset;

	if (c == grabc) {
		right_offset = 0;
		bottom_offset = 0;
		left_offset = 0;
		top_offset = 0;
	} else {
		right_offset =
			GEZERO(c->animation.current.x + c->animation.current.width -
				   c->mon->m.x - c->mon->m.width);
		bottom_offset =
			GEZERO(c->animation.current.y + c->animation.current.height -
				   c->mon->m.y - c->mon->m.height);

		left_offset = GEZERO(c->mon->m.x - c->animation.current.x);
		top_offset = GEZERO(c->mon->m.y - c->animation.current.y);
	}

	int border_up_width = GEZERO(fullgeom.width - left_offset - right_offset);
	int border_up_height =
		GEZERO(bw - top_offset - GEZERO(bottom_offset + bw - fullgeom.height));

	int border_down_width = GEZERO(fullgeom.width - left_offset - right_offset);
	int border_down_height =
		GEZERO(bw - bottom_offset - GEZERO(top_offset + bw - fullgeom.height));

	int border_left_width =
		GEZERO(bw - left_offset - GEZERO(right_offset + bw - fullgeom.width));
	int border_left_height =
		GEZERO(fullgeom.height - top_offset - bottom_offset);

	int border_right_width =
		GEZERO(bw - right_offset - GEZERO(left_offset + bw - fullgeom.width));
	int border_right_height =
		GEZERO(fullgeom.height - top_offset - bottom_offset);

	int border_up_x = GEZERO(left_offset);
	int border_up_y = GEZERO(top_offset);

	int border_down_x = GEZERO(left_offset);
	int border_down_y = GEZERO(fullgeom.height - bw) +
						GEZERO(top_offset + bw - fullgeom.height);

	int border_left_x = GEZERO(left_offset);
	int border_left_y = GEZERO(top_offset);

	int border_right_x =
		GEZERO(fullgeom.width - bw) + GEZERO(left_offset + bw - fullgeom.width);
	int border_right_y = GEZERO(top_offset);

	wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
	set_rect_size(c->border[0], border_up_width, border_up_height);
	set_rect_size(c->border[1], border_down_width, border_down_height);
	set_rect_size(c->border[2], border_left_width, border_left_height);
	set_rect_size(c->border[3], border_right_width, border_right_height);
	wlr_scene_node_set_position(&c->border[0]->node, border_up_x, border_up_y);
	wlr_scene_node_set_position(&c->border[2]->node, border_left_x,
								border_left_y);
	wlr_scene_node_set_position(&c->border[1]->node, border_down_x,
								border_down_y);
	wlr_scene_node_set_position(&c->border[3]->node, border_right_x,
								border_right_y);
}

struct ivec2 clip_to_hide(Client *c, struct wlr_box *clip_box) {
	int offsetx = 0, offsety = 0, offsetw = 0, offseth = 0;
	struct ivec2 offset = {0, 0, 0, 0};

	if (!ISTILED(c) && !c->animation.tagining && !c->animation.tagouted &&
		!c->animation.tagouting)
		return offset;

	int bottom_out_offset =
		GEZERO(c->animation.current.y + c->animation.current.height -
			   c->mon->m.y - c->mon->m.height);
	int right_out_offset =
		GEZERO(c->animation.current.x + c->animation.current.width -
			   c->mon->m.x - c->mon->m.width);
	int left_out_offset = GEZERO(c->mon->m.x - c->animation.current.x);
	int top_out_offset = GEZERO(c->mon->m.y - c->animation.current.y);

	// 必须转换为int，否计算会没有负数导致判断错误
	int bw = (int)c->bw;

	/*
	  计算窗口表面超出屏幕四个方向的偏差，避免窗口超出屏幕
	  需要主要border超出屏幕的时候不计算如偏差之内而是
	  要等窗口表面超出才开始计算偏差
	*/
	if (ISTILED(c) || c->animation.tagining || c->animation.tagouted ||
		c->animation.tagouting) {
		if (left_out_offset > 0) {
			offsetx = GEZERO(left_out_offset - bw);
			clip_box->x = clip_box->x + offsetx;
			clip_box->width = clip_box->width - offsetx;
		} else if (right_out_offset > 0) {
			offsetw = GEZERO(right_out_offset - bw);
			clip_box->width = clip_box->width - offsetw;
		}

		if (top_out_offset > 0) {
			offsety = GEZERO(top_out_offset - bw);
			clip_box->y = clip_box->y + offsety;
			clip_box->height = clip_box->height - offsety;
		} else if (bottom_out_offset > 0) {
			offseth = GEZERO(bottom_out_offset - bw);
			clip_box->height = clip_box->height - offseth;
		}
	}

	// 窗口表面超出屏幕四个方向的偏差
	offset.x = offsetx;
	offset.y = offsety;
	offset.width = offsetw;
	offset.height = offseth;

	if ((clip_box->width + bw <= 0 || clip_box->height + bw <= 0) &&
		(ISTILED(c) || c->animation.tagouting || c->animation.tagining)) {
		c->is_clip_to_hide = true;
		wlr_scene_node_set_enabled(&c->scene->node, false);
	} else if (c->is_clip_to_hide && VISIBLEON(c, c->mon)) {
		c->is_clip_to_hide = false;
		wlr_scene_node_set_enabled(&c->scene->node, true);
	}

	return offset;
}

void client_apply_clip(Client *c) {

	if (c->iskilling || !client_surface(c)->mapped)
		return;

	struct wlr_box clip_box;
	bool should_render_client_surface = false;
	struct ivec2 offset;
	animationScale scale_data;

	float percent =
		c->animation.action == OPEN && animation_fade_in && !c->nofadein
			? (double)c->animation.passed_frames / c->animation.total_frames
			: 1.0;
	float opacity = c->isfullscreen	   ? 1
					: c == selmon->sel ? c->focused_opacity
									   : c->unfocused_opacity;

	int bw = (int)c->bw;

	if (!animations) {
		c->animation.running = false;
		c->need_output_flush = false;
		c->animainit_geom = c->current = c->pending = c->animation.current =
			c->geom;

		client_get_clip(c, &clip_box);

		offset = clip_to_hide(c, &clip_box);

		apply_border(c);

		scale_data.opacity = c->isfullscreen	? 1
							 : c == selmon->sel ? c->focused_opacity
												: c->unfocused_opacity;

		if (clip_box.width <= 0 || clip_box.height <= 0) {
			return;
		}

		wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip_box);
		buffer_set_effect(
			c, (animationScale){0, 0, 0, 0, opacity, opacity, false});
		return;
	}

	// 获取窗口动画实时位置矩形
	unsigned int width, height;
	client_actual_size(c, &width, &height);

	// 计算出除了边框的窗口实际剪切大小
	struct wlr_box geometry;
	client_get_geometry(c, &geometry);
	clip_box = (struct wlr_box){
		.x = geometry.x,
		.y = geometry.y,
		.width = width - bw,
		.height = height - bw,
	};

	if (client_is_x11(c)) {
		clip_box.x = 0;
		clip_box.y = 0;
	}

	// 检测窗口是否需要剪切超出屏幕部分，如果需要就调整实际要剪切的矩形
	offset = clip_to_hide(c, &clip_box);

	// 应用窗口装饰
	apply_border(c);

	// 如果窗口剪切区域已经剪切到0，则不渲染窗口表面
	if (clip_box.width <= 0 || clip_box.height <= 0) {
		should_render_client_surface = false;
		wlr_scene_node_set_enabled(&c->scene_surface->node, false);
	} else {
		should_render_client_surface = true;
		wlr_scene_node_set_enabled(&c->scene_surface->node, true);
	}

	// 不用在执行下面的窗口表面剪切和缩放等效果操作
	if (!should_render_client_surface) {
		return;
	}

	// 应用窗口表面剪切
	wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip_box);

	// 获取剪切后的表面的实际大小用于计算缩放
	int acutal_surface_width = geometry.width - offset.x - offset.width;
	int acutal_surface_height = geometry.height - offset.y - offset.height;

	if (acutal_surface_width <= 0 || acutal_surface_height <= 0)
		return;

	scale_data.should_scale = true;
	scale_data.width = clip_box.width;
	scale_data.height = clip_box.height;
	scale_data.width_scale = (float)scale_data.width / acutal_surface_width;
	scale_data.height_scale = (float)scale_data.height / acutal_surface_height;
	scale_data.percent = percent;
	scale_data.opacity = opacity;
	buffer_set_effect(c, scale_data);
}

void fadeout_client_animation_next_tick(Client *c) {
	if (!c)
		return;

	animationScale scale_data;

	double animation_passed =
		(double)c->animation.passed_frames / c->animation.total_frames;
	int type = c->animation.action = c->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);
	unsigned int width =
		c->animation.initial.width +
		(c->current.width - c->animation.initial.width) * factor;
	unsigned int height =
		c->animation.initial.height +
		(c->current.height - c->animation.initial.height) * factor;

	unsigned int x = c->animation.initial.x +
					 (c->current.x - c->animation.initial.x) * factor;
	unsigned int y = c->animation.initial.y +
					 (c->current.y - c->animation.initial.y) * factor;

	wlr_scene_node_set_position(&c->scene->node, x, y);

	c->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	double opacity = MAX(fadeout_begin_opacity - animation_passed, 0);

	if (animation_fade_out && !c->nofadeout)
		wlr_scene_node_for_each_buffer(&c->scene->node,
									   scene_buffer_apply_opacity, &opacity);

	apply_opacity_to_rect_nodes(c, &c->scene->node, animation_passed);

	if ((c->animation_type_close &&
		 strcmp(c->animation_type_close, "zoom") == 0) ||
		(!c->animation_type_close &&
		 strcmp(animation_type_close, "zoom") == 0)) {

		scale_data.width = width;
		scale_data.height = height;
		scale_data.width_scale = animation_passed;
		scale_data.height_scale = animation_passed;

		wlr_scene_node_for_each_buffer(
			&c->scene->node, snap_scene_buffer_apply_effect, &scale_data);
	}

	if (animation_passed == 1.0) {
		wl_list_remove(&c->fadeout_link);
		wlr_scene_node_destroy(&c->scene->node);
		free(c);
		c = NULL;
	} else {
		c->animation.passed_frames++;
	}
}

void client_animation_next_tick(Client *c) {
	double animation_passed =
		(double)c->animation.passed_frames / c->animation.total_frames;

	int type = c->animation.action == NONE ? MOVE : c->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);

	Client *pointer_c = NULL;
	double sx = 0, sy = 0;
	struct wlr_surface *surface = NULL;

	unsigned int width =
		c->animation.initial.width +
		(c->current.width - c->animation.initial.width) * factor;
	unsigned int height =
		c->animation.initial.height +
		(c->current.height - c->animation.initial.height) * factor;

	unsigned int x = c->animation.initial.x +
					 (c->current.x - c->animation.initial.x) * factor;
	unsigned int y = c->animation.initial.y +
					 (c->current.y - c->animation.initial.y) * factor;

	wlr_scene_node_set_position(&c->scene->node, x, y);
	c->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	c->is_open_animation = false;

	if (animation_passed == 1.0) {

		// clear the open action state
		// To prevent him from being mistaken that
		// it's still in the opening animation in resize
		c->animation.action = MOVE;

		c->animation.tagining = false;
		c->animation.running = false;

		if (c->animation.tagouting) {
			c->animation.tagouting = false;
			wlr_scene_node_set_enabled(&c->scene->node, false);
			client_set_suspended(c, true);
			c->animation.tagouted = true;
			c->animation.current = c->geom;
		}

		xytonode(cursor->x, cursor->y, NULL, &pointer_c, NULL, &sx, &sy);

		surface =
			pointer_c && pointer_c == c ? client_surface(pointer_c) : NULL;
		if (surface && pointer_c == selmon->sel) {
			wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		}

		// end flush in next frame, not the current frame
		c->need_output_flush = false;
	} else {
		c->animation.passed_frames++;
	}
}

void init_fadeout_client(Client *c) {

	if (!c->mon || client_is_unmanaged(c))
		return;

	if (!c->scene) {
		return;
	}

	if (c->animation_type_close &&
		strcmp(c->animation_type_close, "none") == 0) {
		return;
	}

	Client *fadeout_cient = ecalloc(1, sizeof(*fadeout_cient));

	wlr_scene_node_set_enabled(&c->scene->node, true);
	client_set_border_color(c, bordercolor);
	fadeout_cient->scene =
		wlr_scene_tree_snapshot(&c->scene->node, layers[LyrFadeOut]);
	wlr_scene_node_set_enabled(&c->scene->node, false);

	if (!fadeout_cient->scene) {
		free(fadeout_cient);
		return;
	}

	fadeout_cient->animation.duration = animation_duration_close;
	fadeout_cient->geom = fadeout_cient->current =
		fadeout_cient->animainit_geom = fadeout_cient->animation.initial =
			c->animation.current;
	fadeout_cient->mon = c->mon;
	fadeout_cient->animation_type_close = c->animation_type_close;
	fadeout_cient->animation.action = CLOSE;
	fadeout_cient->bw = c->bw;
	fadeout_cient->nofadeout = c->nofadeout;

	// 这里snap节点的坐标设置是使用的相对坐标，所以不能加上原来坐标
	// 这跟普通node有区别

	fadeout_cient->animation.initial.x = 0;
	fadeout_cient->animation.initial.y = 0;
	if ((c->animation_type_close &&
		 strcmp(c->animation_type_close, "slide") == 0) ||
		(!c->animation_type_close &&
		 strcmp(animation_type_close, "slide") == 0)) {
		fadeout_cient->current.y =
			c->geom.y + c->geom.height / 2 > c->mon->m.y + c->mon->m.height / 2
				? c->mon->m.height -
					  (c->animation.current.y - c->mon->m.y) // down out
				: c->mon->m.y - c->geom.height;				 // up out
		fadeout_cient->current.x = 0; // x无偏差，垂直划出
	} else {
		fadeout_cient->current.y =
			(fadeout_cient->geom.height -
			 fadeout_cient->geom.height * zoom_initial_ratio) /
			2;
		fadeout_cient->current.x =
			(fadeout_cient->geom.width -
			 fadeout_cient->geom.width * zoom_initial_ratio) /
			2;
		fadeout_cient->current.width =
			fadeout_cient->geom.width * zoom_initial_ratio;
		fadeout_cient->current.height =
			fadeout_cient->geom.height * zoom_initial_ratio;
	}

	fadeout_cient->animation.passed_frames = 0;
	fadeout_cient->animation.total_frames =
		fadeout_cient->animation.duration / output_frame_duration_ms();
	wlr_scene_node_set_enabled(&fadeout_cient->scene->node, true);
	wl_list_insert(&fadeout_clients, &fadeout_cient->fadeout_link);
}

void client_commit(Client *c) {
	c->current = c->pending; // 设置动画的结束位置

	if (c->animation.should_animate) {
		if (!c->animation.running) {
			c->animation.current = c->animainit_geom;
		}

		c->animation.initial = c->animainit_geom;
		// 设置动画速度
		c->animation.passed_frames = 0;
		c->animation.total_frames =
			c->animation.duration / output_frame_duration_ms();

		// 标记动画开始
		c->animation.running = true;
		c->animation.should_animate = false;
	}
	// 请求刷新屏幕
	wlr_output_schedule_frame(c->mon->wlr_output);
}

void client_set_pending_state(Client *c) {

	// 判断是否需要动画
	if (!animations) {
		c->animation.should_animate = false;
	} else if (animations && c->animation.tagining) {
		c->animation.should_animate = true;
	} else if (!animations || c == grabc ||
			   (!c->is_open_animation &&
				wlr_box_equal(&c->current, &c->pending))) {
		c->animation.should_animate = false;
	} else {
		c->animation.should_animate = true;
	}

	// 开始动画
	client_commit(c);
	c->dirty = true;
}

void resize(Client *c, struct wlr_box geo, int interact) {

	// 动画设置的起始函数，这里用来计算一些动画的起始值
	// 动画起始位置大小是由于c->animainit_geom确定的

	if (!c || !c->mon || !client_surface(c)->mapped)
		return;

	struct wlr_box *bbox;
	struct wlr_box clip;

	if (!c->mon)
		return;

	c->need_output_flush = true;

	// oldgeom = c->geom;
	bbox = (interact || c->isfloating || c->isfullscreen) ? &sgeom : &c->mon->w;

	if (is_scroller_layout(c->mon) && (!c->isfloating || c == grabc)) {
		c->geom = geo;
		c->geom.width = MAX(1 + 2 * (int)c->bw, c->geom.width);
		c->geom.height = MAX(1 + 2 * (int)c->bw, c->geom.height);
	} else { // 这里会限制不允许窗口划出屏幕
		client_set_bounds(
			c, geo.width,
			geo.height); // 去掉这个推荐的窗口大小,因为有时推荐的窗口特别大导致平铺异常
		c->geom = geo;
		applybounds(
			c,
			bbox); // 去掉这个推荐的窗口大小,因为有时推荐的窗口特别大导致平铺异常
	}

	if (!c->is_open_animation) {
		c->animation.begin_fade_in = false;
	}

	if (c->animation.action == OPEN && !c->animation.tagining &&
		!c->animation.tagouting && wlr_box_equal(&c->geom, &c->current)) {
		c->animation.action = c->animation.action;
	} else if (c->animation.tagouting) {
		c->animation.duration = animation_duration_tag;
		c->animation.action = TAG;
	} else if (c->animation.tagining) {
		c->animation.duration = animation_duration_tag;
		c->animation.action = TAG;
	} else if (c->is_open_animation) {
		c->animation.duration = animation_duration_open;
		c->animation.action = OPEN;
	} else {
		c->animation.duration = animation_duration_move;
		c->animation.action = MOVE;
	}

	// 动画起始位置大小设置
	if (c->animation.tagouting) {
		c->animainit_geom = c->animation.current;
	} else if (c->animation.tagining) {
		c->animainit_geom.height = c->animation.current.height;
		c->animainit_geom.width = c->animation.current.width;
	} else if (c->is_open_animation) {
		set_client_open_animaiton(c, c->geom);
	} else {
		c->animainit_geom = c->animation.current;
	}

	if (c->isnoborder || c->iskilling) {
		c->bw = 0;
	}

	// c->geom 是真实的窗口大小和位置，跟过度的动画无关，用于计算布局
	c->configure_serial = client_set_size(c, c->geom.width - 2 * c->bw,
										  c->geom.height - 2 * c->bw);

	if (c == grabc) {
		c->animation.running = false;
		c->need_output_flush = false;
		c->animainit_geom = c->current = c->pending = c->animation.current =
			c->geom;
		wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);

		apply_border(c);
		client_get_clip(c, &clip);
		wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip);
		return;
	}
	// 如果不是工作区切换时划出去的窗口，就让动画的结束位置，就是上面的真实位置和大小
	// c->pending 决定动画的终点，一般在其他调用resize的函数的附近设置了
	if (!c->animation.tagouting && !c->iskilling) {
		c->pending = c->geom;
	}

	if (c->swallowedby && c->animation.action == OPEN) {
		c->animainit_geom = c->swallowedby->animation.current;
	}

	if (c->swallowing) {
		c->animainit_geom = c->geom;
	}

	if ((c->isglobal || c->isunglobal) && c->isfloating &&
		c->animation.action == TAG) {
		c->animainit_geom = c->geom;
	}

	if (c->animation_type_open && strcmp(c->animation_type_open, "none") == 0 &&
		c->animation.action == OPEN) {
		c->animainit_geom = c->geom;
	}

	// 开始应用动画设置
	client_set_pending_state(c);

	setborder_color(c);
}

bool client_draw_fadeout_frame(Client *c) {
	if (!c)
		return false;

	fadeout_client_animation_next_tick(c);
	return true;
}

bool client_draw_frame(Client *c) {

	if (!c || !client_surface(c)->mapped)
		return false;

	if (c->isfullscreen) {
		client_set_opacity(c, 1);
	} else if (c == selmon->sel && !c->animation.running) {
		client_set_opacity(c, c->focused_opacity);
	} else if (!c->animation.running) {
		client_set_opacity(c, c->unfocused_opacity);
	}

	if (!c->need_output_flush)
		return false;

	if (animations && c->animation.running) {
		client_animation_next_tick(c);
		client_apply_clip(c);
	} else {
		wlr_scene_node_set_position(&c->scene->node, c->pending.x,
									c->pending.y);
		c->animation.current = c->animainit_geom = c->animation.initial =
			c->pending = c->current = c->geom;
		client_apply_clip(c);
		c->need_output_flush = false;
	}
	return true;
}
