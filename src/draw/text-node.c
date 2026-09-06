#include "mango/draw/text-node.h"

#include <cairo.h>
#include <drm_fourcc.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_buffer.h>

static GHashTable *font_desc_cache = NULL;

PangoFontDescription *get_cached_font_desc(const char *font_desc) {
	if (!font_desc_cache) {
		font_desc_cache =
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
								  (GDestroyNotify)pango_font_description_free);
	}

	PangoFontDescription *desc =
		g_hash_table_lookup(font_desc_cache, font_desc);
	if (!desc) {
		desc = pango_font_description_from_string(font_desc);
		g_hash_table_insert(font_desc_cache, g_strdup(font_desc), desc);
	}
	return desc;
}

void mango_text_global_finish(void) {
	if (font_desc_cache) {
		g_hash_table_destroy(font_desc_cache);
		font_desc_cache = NULL;
	}
}

void text_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct mango_text_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	free(buf);
}

bool text_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
									   uint32_t flags, void **data,
									   uint32_t *format, size_t *stride) {
	(void)flags;
	struct mango_text_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	*data = cairo_image_surface_get_data(buf->surface);
	*format = DRM_FORMAT_ARGB8888;
	*stride = cairo_image_surface_get_stride(buf->surface);
	return true;
}

void text_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {}

static const struct wlr_buffer_impl text_buffer_impl = {
	.destroy = text_buffer_destroy,
	.begin_data_ptr_access = text_buffer_begin_data_ptr_access,
	.end_data_ptr_access = text_buffer_end_data_ptr_access,
};

MangoJumpLabel *mango_jump_label_node_create(struct wlr_scene_tree *parent,
											 DecorateDrawData data) {
	MangoJumpLabel *node = calloc(1, sizeof(*node));
	if (!node)
		return NULL;

	node->scene_buffer = wlr_scene_buffer_create(parent, NULL);
	if (!node->scene_buffer) {
		free(node);
		return NULL;
	}

	memcpy(node->fg_color, data.fg_color, sizeof(node->fg_color));
	memcpy(node->bg_color, data.bg_color, sizeof(node->bg_color));
	memcpy(node->focus_fg_color, data.focus_fg_color,
		   sizeof(node->focus_fg_color));
	memcpy(node->focus_bg_color, data.focus_bg_color,
		   sizeof(node->focus_bg_color));
	memcpy(node->border_color, data.border_color, sizeof(node->border_color));
	node->border_width = data.border_width;
	node->corner_radius = data.corner_radius;
	node->padding_x = data.padding_x;
	node->padding_y = data.padding_y;
	node->font_desc =
		g_strdup(data.font_desc ? data.font_desc : "monospace Bold 16");

	node->cached_text = NULL;
	node->cached_scale = -1.0f;
	node->cached_font_desc = NULL;
	node->focused = false;
	node->cached_focused = false;

	node->measure_surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	node->measure_cr = cairo_create(node->measure_surface);
	node->measure_context = pango_cairo_create_context(node->measure_cr);
	node->measure_layout = pango_layout_new(node->measure_context);
	node->measure_scale = 1.0f;

	node->scene_buffer->node.data = NULL;

	return node;
}

void mango_jump_label_node_destroy(MangoJumpLabel *node) {
	if (!node)
		return;

	if (node->buffer) {
		wlr_buffer_drop(&node->buffer->base);
		node->buffer = NULL;
	}

	if (node->surface) {
		cairo_surface_destroy(node->surface);
		node->surface = NULL;
	}

	if (node->measure_layout)
		g_object_unref(node->measure_layout);
	if (node->measure_context)
		g_object_unref(node->measure_context);
	if (node->measure_cr)
		cairo_destroy(node->measure_cr);
	if (node->measure_surface)
		cairo_surface_destroy(node->measure_surface);

	wlr_scene_node_destroy(&node->scene_buffer->node);

	g_free(node->font_desc);
	g_free(node->cached_text);
	g_free(node->cached_font_desc);

	free(node);
}

void mango_jump_label_node_set_background(MangoJumpLabel *node, float r,
										  float g, float b, float a) {
	if (!node)
		return;
	node->bg_color[0] = r;
	node->bg_color[1] = g;
	node->bg_color[2] = b;
	node->bg_color[3] = a;
}

void mango_jump_label_node_set_border(MangoJumpLabel *node, float r, float g,
									  float b, float a, int32_t width,
									  int32_t radius) {
	if (!node)
		return;
	node->border_color[0] = r;
	node->border_color[1] = g;
	node->border_color[2] = b;
	node->border_color[3] = a;
	node->border_width = width > 0 ? width : 0;
	node->corner_radius = radius;
}

void mango_jump_label_node_set_padding(MangoJumpLabel *node, int32_t pad_x,
									   int32_t pad_y) {
	if (!node)
		return;
	node->padding_x = pad_x >= 0 ? pad_x : 0;
	node->padding_y = pad_y >= 0 ? pad_y : 0;
}

void get_text_pixel_size(MangoJumpLabel *node, const char *text, float scale,
						 int32_t *out_w, int32_t *out_h) {
	if (node->measure_scale != scale) {
		pango_cairo_context_set_resolution(node->measure_context, 96.0 * scale);
		node->measure_scale = scale;
	}

	PangoFontDescription *desc = get_cached_font_desc(node->font_desc);
	pango_layout_set_font_description(node->measure_layout, desc);
	pango_layout_set_text(node->measure_layout, text, -1);

	pango_layout_get_pixel_size(node->measure_layout, out_w, out_h);
}

void draw_rounded_rect(cairo_t *cr, double x, double y, double w, double h,
					   double r) {
	// Draws nothing when width/height are not positive.
	if (w <= 0.0 || h <= 0.0)
		return;

	double degrees = G_PI / 180.0;
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -90 * degrees, 0 * degrees);
	cairo_arc(cr, x + w - r, y + h - r, r, 0 * degrees, 90 * degrees);
	cairo_arc(cr, x + r, y + h - r, r, 90 * degrees, 180 * degrees);
	cairo_arc(cr, x + r, y + r, r, 180 * degrees, 270 * degrees);
	cairo_close_path(cr);
}

void mango_jump_label_node_update(MangoJumpLabel *node, const char *text,
								  float scale) {
	if (!node || !text)
		return;
	if (scale <= 0.0f)
		scale = 1.0f;

	// Dirty check.
	if (node->cached_scale == scale && node->cached_font_desc &&
		strcmp(node->cached_font_desc, node->font_desc) == 0 &&
		node->cached_text && strcmp(node->cached_text, text) == 0 &&
		memcmp(node->cached_fg_color, node->fg_color, sizeof(node->fg_color)) ==
			0 &&
		memcmp(node->cached_bg_color, node->bg_color, sizeof(node->bg_color)) ==
			0 &&
		memcmp(node->cached_focus_fg_color, node->focus_fg_color,
			   sizeof(node->focus_fg_color)) == 0 &&
		memcmp(node->cached_focus_bg_color, node->focus_bg_color,
			   sizeof(node->focus_bg_color)) == 0 &&
		memcmp(node->cached_border_color, node->border_color,
			   sizeof(node->border_color)) == 0 &&
		node->cached_border_width == node->border_width &&
		node->cached_corner_radius == node->corner_radius &&
		node->cached_padding_x == node->padding_x &&
		node->cached_padding_y == node->padding_y &&
		node->cached_focused == node->focused) {
		return;
	}

	// Updates the cache.
	g_free(node->cached_text);
	node->cached_text = g_strdup(text);
	g_free(node->cached_font_desc);
	node->cached_font_desc = g_strdup(node->font_desc);
	node->cached_scale = scale;
	memcpy(node->cached_fg_color, node->fg_color, sizeof(node->fg_color));
	memcpy(node->cached_bg_color, node->bg_color, sizeof(node->bg_color));
	memcpy(node->cached_focus_fg_color, node->focus_fg_color,
		   sizeof(node->focus_fg_color));
	memcpy(node->cached_focus_bg_color, node->focus_bg_color,
		   sizeof(node->focus_bg_color));
	memcpy(node->cached_border_color, node->border_color,
		   sizeof(node->border_color));
	node->cached_border_width = node->border_width;
	node->cached_corner_radius = node->corner_radius;
	node->cached_padding_x = node->padding_x;
	node->cached_padding_y = node->padding_y;
	node->cached_focused = node->focused;

	int32_t text_pixel_w, text_pixel_h;
	get_text_pixel_size(node, text, scale, &text_pixel_w, &text_pixel_h);

	if (text_pixel_w <= 0 || text_pixel_h <= 0) {
		wlr_scene_buffer_set_buffer(node->scene_buffer, NULL);
		if (node->buffer) {
			wlr_buffer_drop(&node->buffer->base);
			node->buffer = NULL;
		}
		if (node->surface) {
			cairo_surface_destroy(node->surface);
			node->surface = NULL;
		}
		node->logical_width = 0;
		node->logical_height = 0;
		wlr_scene_buffer_set_dest_size(node->scene_buffer, 0, 0);
		return;
	}

	int32_t logical_text_w = (int32_t)(text_pixel_w / scale + 0.5f);
	int32_t logical_text_h = (int32_t)(text_pixel_h / scale + 0.5f);
	int32_t box_logical_w = logical_text_w + 2 * node->padding_x;
	int32_t box_logical_h = logical_text_h + 2 * node->padding_y;

	// Physical pixel size includes the border to avoid drawing outside it.
	int32_t required_pixel_w =
		(int32_t)((box_logical_w + 2 * node->border_width) * scale + 0.5f);
	int32_t required_pixel_h =
		(int32_t)((box_logical_h + 2 * node->border_width) * scale + 0.5f);
	if (required_pixel_w < 1)
		required_pixel_w = 1;
	if (required_pixel_h < 1)
		required_pixel_h = 1;

	bool surface_size_changed = (!node->surface) ||
								(node->surface_pixel_w != required_pixel_w) ||
								(node->surface_pixel_h != required_pixel_h);

	if (surface_size_changed) {
		if (node->buffer) {
			wlr_buffer_drop(&node->buffer->base);
			node->buffer = NULL;
		}
		if (node->surface) {
			cairo_surface_destroy(node->surface);
			node->surface = NULL;
		}

		node->surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, required_pixel_w, required_pixel_h);
		node->surface_pixel_w = required_pixel_w;
		node->surface_pixel_h = required_pixel_h;
	}

	cairo_t *cr = cairo_create(node->surface);

	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	double border = node->border_width * scale;
	double bg_x = border;
	double bg_y = border;
	double bg_w = box_logical_w * scale;
	double bg_h = box_logical_h * scale;

	double radius;
	if (node->corner_radius < 0) {
		radius = (bg_w < bg_h ? bg_w : bg_h) / 2.0;
	} else {
		radius = node->corner_radius * scale;
	}
	if (radius > bg_w / 2.0)
		radius = bg_w / 2.0;
	if (radius > bg_h / 2.0)
		radius = bg_h / 2.0;
	if (radius < 0.0)
		radius = 0.0;

	const float *active_bg =
		node->focused ? node->focus_bg_color : node->bg_color;
	const float *active_fg =
		node->focused ? node->focus_fg_color : node->fg_color;

	bool draw_bg = (active_bg[3] > 0.0f) && (bg_w > 0.0 && bg_h > 0.0);
	bool draw_border =
		(node->border_width > 0) && (node->border_color[3] > 0.0f);

	if (draw_bg) {
		cairo_set_source_rgba(cr, active_bg[0], active_bg[1], active_bg[2],
							  active_bg[3]);
		if (radius > 0.0) {
			draw_rounded_rect(cr, bg_x, bg_y, bg_w, bg_h, radius);
			cairo_fill(cr);
		} else {
			cairo_rectangle(cr, bg_x, bg_y, bg_w, bg_h);
			cairo_fill(cr);
		}
	}

	// Text is drawn only when the background area has valid space.
	if (bg_w > 0.0 && bg_h > 0.0) {
		cairo_save(cr);
		double text_x = (node->border_width + node->padding_x) * scale;
		double text_y = (node->border_width + node->padding_y) * scale;
		cairo_translate(cr, text_x, text_y);

		PangoContext *ctx = pango_cairo_create_context(cr);
		pango_cairo_context_set_resolution(ctx, 96.0 * scale);
		PangoLayout *layout = pango_layout_new(ctx);
		PangoFontDescription *desc = get_cached_font_desc(node->font_desc);
		pango_layout_set_font_description(layout, desc);
		pango_layout_set_text(layout, text, -1);

		cairo_set_source_rgba(cr, active_fg[0], active_fg[1], active_fg[2],
							  active_fg[3]);
		pango_cairo_show_layout(cr, layout);

		g_object_unref(layout);
		g_object_unref(ctx);
		cairo_restore(cr);
	}

	if (draw_border) {
		cairo_set_source_rgba(cr, node->border_color[0], node->border_color[1],
							  node->border_color[2], node->border_color[3]);
		cairo_set_line_width(cr, border);

		double half_lw = border * 0.5;
		double bx = bg_x - half_lw;
		double by = bg_y - half_lw;
		double bw = bg_w + border;
		double bh = bg_h + border;

		// Ensures the border rectangle stays in bounds and has positive
		// width/height.
		if (bx < 0.0) {
			bw += bx; // bx is negative, so bw shrinks.
			bx = 0.0;
		}
		if (by < 0.0) {
			bh += by;
			by = 0.0;
		}
		if (bx + bw > (double)node->surface_pixel_w)
			bw = (double)node->surface_pixel_w - bx;
		if (by + bh > (double)node->surface_pixel_h)
			bh = (double)node->surface_pixel_h - by;
		if (bw < 0.0)
			bw = 0.0;
		if (bh < 0.0)
			bh = 0.0;

		if (bw > 0.0 && bh > 0.0) {
			if (radius > 0.0) {
				double outer_radius = radius + half_lw;
				if (outer_radius < 0.0)
					outer_radius = 0.0;
				draw_rounded_rect(cr, bx, by, bw, bh, outer_radius);
			} else {
				cairo_rectangle(cr, bx, by, bw, bh);
			}
			cairo_stroke(cr);
		}
	}

	cairo_surface_flush(node->surface);
	cairo_destroy(cr);

	if (node->buffer) {
		wlr_buffer_drop(&node->buffer->base);
		node->buffer = NULL;
	}

	struct mango_text_buffer *buf = calloc(1, sizeof(*buf));
	if (!buf) {
		return;
	}
	wlr_buffer_init(&buf->base, &text_buffer_impl, node->surface_pixel_w,
					node->surface_pixel_h);
	buf->surface = node->surface;
	node->buffer = buf;

	wlr_scene_buffer_set_buffer(node->scene_buffer, &buf->base);

	node->logical_width = box_logical_w + 2 * node->border_width;
	node->logical_height = box_logical_h + 2 * node->border_width;
	wlr_scene_buffer_set_dest_size(node->scene_buffer, node->logical_width,
								   node->logical_height);
}

void mango_jump_label_node_set_focus(MangoJumpLabel *node, bool focused) {
	if (!node || node->focused == focused)
		return;
	node->focused = focused;
	if (node->cached_text && node->cached_scale > 0.0f) {
		mango_jump_label_node_update(node, node->cached_text,
									 node->cached_scale);
	}
}

MangoGroupBar *mango_group_bar_create(void *cdata, uint32_t type,
									  struct wlr_scene_tree *parent,
									  DecorateDrawData data, int32_t width,
									  int32_t height) {
	MangoGroupBar *mangobar = calloc(1, sizeof(*mangobar));
	if (!mangobar)
		return NULL;

	mangobar->scene_buffer = wlr_scene_buffer_create(parent, NULL);
	if (!mangobar->scene_buffer) {
		free(mangobar);
		return NULL;
	}

	memcpy(mangobar->fg_color, data.fg_color, sizeof(mangobar->fg_color));
	memcpy(mangobar->bg_color, data.bg_color, sizeof(mangobar->bg_color));
	memcpy(mangobar->focus_fg_color, data.focus_fg_color,
		   sizeof(mangobar->focus_fg_color));
	memcpy(mangobar->focus_bg_color, data.focus_bg_color,
		   sizeof(mangobar->focus_bg_color));
	memcpy(mangobar->border_color, data.border_color,
		   sizeof(mangobar->border_color));
	mangobar->border_width = data.border_width;
	mangobar->corner_radius = data.corner_radius;
	mangobar->padding_x = data.padding_x;
	mangobar->padding_y = data.padding_y;
	mangobar->font_desc =
		g_strdup(data.font_desc ? data.font_desc : "monospace Bold 16");

	mangobar->target_width = width;
	mangobar->target_height = height;
	mangobar->focused = false;
	mangobar->cached_focused = false;

	mangobar->measure_surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	mangobar->measure_cr = cairo_create(mangobar->measure_surface);
	mangobar->measure_context =
		pango_cairo_create_context(mangobar->measure_cr);
	mangobar->measure_layout = pango_layout_new(mangobar->measure_context);
	mangobar->measure_scale = 1.0f;

	mangobar->cached_scale = -1.0f;
	mangobar->last_text = NULL;
	mangobar->last_scale = 0.0f;

	mangobar->type = type;
	mangobar->node_data = cdata;

	mangobar->scene_buffer->node.data = mangobar;

	return mangobar;
}

void mango_group_bar_destroy(MangoGroupBar *node) {
	if (!node)
		return;

	if (node->buffer) {
		wlr_buffer_drop(&node->buffer->base);
		node->buffer = NULL;
	}

	if (node->surface) {
		cairo_surface_destroy(node->surface);
		node->surface = NULL;
	}
	if (node->measure_surface) {
		cairo_surface_destroy(node->measure_surface);
		node->measure_surface = NULL;
	}

	if (node->measure_layout)
		g_object_unref(node->measure_layout);
	if (node->measure_context)
		g_object_unref(node->measure_context);
	if (node->measure_cr)
		cairo_destroy(node->measure_cr);

	wlr_scene_node_destroy(&node->scene_buffer->node);

	g_free(node->font_desc);
	g_free(node->cached_text);
	g_free(node->cached_font_desc);
	g_free(node->last_text);
	free(node);
}

void mango_group_bar_set_size(MangoGroupBar *node, int32_t width,
							  int32_t height) {
	if (!node)
		return;

	if (width < 0)
		width = 0;
	if (height < 0)
		height = 0;

	if (node->target_width == width && node->target_height == height)
		return;

	node->target_width = width;
	node->target_height = height;

	const char *redraw_text = node->last_text ? node->last_text : "";
	float redraw_scale = node->last_scale > 0.0f ? node->last_scale : 1.0f;

	mango_group_bar_update(node, redraw_text, redraw_scale);
}

void mango_group_bar_update(MangoGroupBar *node, const char *text,
							float scale) {
	if (!node || !text)
		return;
	if (scale <= 0.0f)
		scale = 1.0f;

	char *safe_text = g_strdup(text);

	g_free(node->last_text);
	node->last_text = safe_text; // Ownership transfer.
	node->last_scale = scale;

	// Dirty check.
	if (node->cached_scale == scale && node->cached_font_desc &&
		strcmp(node->cached_font_desc, node->font_desc) == 0 &&
		node->cached_text && strcmp(node->cached_text, safe_text) == 0 &&
		memcmp(node->cached_fg_color, node->fg_color, sizeof(node->fg_color)) ==
			0 &&
		memcmp(node->cached_bg_color, node->bg_color, sizeof(node->bg_color)) ==
			0 &&
		memcmp(node->cached_focus_fg_color, node->focus_fg_color,
			   sizeof(node->focus_fg_color)) == 0 &&
		memcmp(node->cached_focus_bg_color, node->focus_bg_color,
			   sizeof(node->focus_bg_color)) == 0 &&
		memcmp(node->cached_border_color, node->border_color,
			   sizeof(node->border_color)) == 0 &&
		node->cached_border_width == node->border_width &&
		node->cached_corner_radius == node->corner_radius &&
		node->cached_padding_x == node->padding_x &&
		node->cached_padding_y == node->padding_y &&
		node->cached_target_width == node->target_width &&
		node->cached_target_height == node->target_height &&
		node->cached_focused == node->focused) {
		return;
	}

	// Updates the cache.
	g_free(node->cached_text);
	node->cached_text = g_strdup(safe_text);

	g_free(node->cached_font_desc);
	node->cached_font_desc = g_strdup(node->font_desc);
	node->cached_scale = scale;
	memcpy(node->cached_fg_color, node->fg_color, sizeof(node->fg_color));
	memcpy(node->cached_bg_color, node->bg_color, sizeof(node->bg_color));
	memcpy(node->cached_focus_fg_color, node->focus_fg_color,
		   sizeof(node->focus_fg_color));
	memcpy(node->cached_focus_bg_color, node->focus_bg_color,
		   sizeof(node->focus_bg_color));
	memcpy(node->cached_border_color, node->border_color,
		   sizeof(node->border_color));
	node->cached_border_width = node->border_width;
	node->cached_corner_radius = node->corner_radius;
	node->cached_padding_x = node->padding_x;
	node->cached_padding_y = node->padding_y;
	node->cached_target_width = node->target_width;
	node->cached_target_height = node->target_height;
	node->cached_focused = node->focused;

	if (node->target_width <= 0 || node->target_height <= 0) {
		wlr_scene_buffer_set_buffer(node->scene_buffer, NULL);
		if (node->buffer) {
			wlr_buffer_drop(&node->buffer->base);
			node->buffer = NULL;
		}
		if (node->surface) {
			cairo_surface_destroy(node->surface);
			node->surface = NULL;
		}
		node->logical_width = 0;
		node->logical_height = 0;
		wlr_scene_buffer_set_dest_size(node->scene_buffer, 0, 0);
		return;
	}

	int32_t box_logical_w = node->target_width - 2 * node->border_width;
	int32_t box_logical_h = node->target_height - 2 * node->border_width;
	if (box_logical_w < 0)
		box_logical_w = 0;
	if (box_logical_h < 0)
		box_logical_h = 0;

	// Surface physical size includes the border to avoid drawing outside it.
	int32_t required_pixel_w = (int32_t)(node->target_width * scale + 0.5f);
	int32_t required_pixel_h = (int32_t)(node->target_height * scale + 0.5f);
	// If the border would extend beyond the target area, grow the surface size
	// to fully contain it.
	double border_phys = node->border_width * scale;
	if (border_phys > 0.0) {
		// The border stroke extends outward by half the line width, so extra
		// space is needed.
		int extra_w = (int32_t)ceil(border_phys * 0.5);
		int extra_h = (int32_t)ceil(border_phys * 0.5);
		required_pixel_w += extra_w;
		required_pixel_h += extra_h;
	}
	if (required_pixel_w < 1)
		required_pixel_w = 1;
	if (required_pixel_h < 1)
		required_pixel_h = 1;

	bool surface_size_changed = (!node->surface) ||
								(node->surface_pixel_w != required_pixel_w) ||
								(node->surface_pixel_h != required_pixel_h);

	if (surface_size_changed) {
		if (node->buffer) {
			wlr_buffer_drop(&node->buffer->base);
			node->buffer = NULL;
		}
		if (node->surface) {
			cairo_surface_destroy(node->surface);
			node->surface = NULL;
		}
		node->surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, required_pixel_w, required_pixel_h);
		node->surface_pixel_w = required_pixel_w;
		node->surface_pixel_h = required_pixel_h;
	}

	cairo_t *cr = cairo_create(node->surface);

	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	double bg_x = border_phys;
	double bg_y = border_phys;
	double bg_w = box_logical_w * scale;
	double bg_h = box_logical_h * scale;

	double radius;
	if (node->corner_radius < 0) {
		radius = (bg_w < bg_h ? bg_w : bg_h) / 2.0;
	} else {
		radius = node->corner_radius * scale;
	}
	if (radius > bg_w / 2.0)
		radius = bg_w / 2.0;
	if (radius > bg_h / 2.0)
		radius = bg_h / 2.0;
	if (radius < 0.0)
		radius = 0.0;

	const float *active_bg =
		node->focused ? node->focus_bg_color : node->bg_color;
	const float *active_fg =
		node->focused ? node->focus_fg_color : node->fg_color;

	bool draw_bg = (active_bg[3] > 0.0f) && (bg_w > 0.0 && bg_h > 0.0);
	bool draw_border =
		(node->border_width > 0) && (node->border_color[3] > 0.0f);

	if (draw_bg) {
		cairo_set_source_rgba(cr, active_bg[0], active_bg[1], active_bg[2],
							  active_bg[3]);
		if (radius > 0.0) {
			draw_rounded_rect(cr, bg_x, bg_y, bg_w, bg_h, radius);
			cairo_fill(cr);
		} else {
			cairo_rectangle(cr, bg_x, bg_y, bg_w, bg_h);
			cairo_fill(cr);
		}
	}

	// Only runs when the background area has space.
	if (bg_w > 0.0 && bg_h > 0.0) {
		int32_t text_area_logical_w = box_logical_w - 2 * node->padding_x;
		int32_t text_area_logical_h = box_logical_h - 2 * node->padding_y;
		if (text_area_logical_w > 0 && text_area_logical_h > 0) {
			cairo_save(cr);

			double text_x = (node->border_width + node->padding_x) * scale;
			double text_y = (node->border_width + node->padding_y) * scale;
			double text_area_w = text_area_logical_w * scale;
			double text_area_h = text_area_logical_h * scale;

			PangoContext *ctx = pango_cairo_create_context(cr);
			pango_cairo_context_set_resolution(ctx, 96.0 * scale);
			PangoLayout *layout = pango_layout_new(ctx);
			PangoFontDescription *desc = get_cached_font_desc(node->font_desc);
			pango_layout_set_font_description(layout, desc);
			pango_layout_set_text(layout, safe_text, -1);

			pango_layout_set_wrap(layout, PANGO_WRAP_NONE);
			pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
			pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
			pango_layout_set_width(layout, (int)(text_area_w * PANGO_SCALE));

			int text_pixel_w, text_pixel_h;
			pango_layout_get_pixel_size(layout, &text_pixel_w, &text_pixel_h);
			double y_offset = (text_area_h - text_pixel_h) / 2.0;
			if (y_offset < 0)
				y_offset = 0;

			cairo_translate(cr, text_x, text_y + y_offset);

			cairo_set_source_rgba(cr, active_fg[0], active_fg[1], active_fg[2],
								  active_fg[3]);
			pango_cairo_show_layout(cr, layout);

			g_object_unref(layout);
			g_object_unref(ctx);
			cairo_restore(cr);
		}
	}

	if (draw_border) {
		cairo_set_source_rgba(cr, node->border_color[0], node->border_color[1],
							  node->border_color[2], node->border_color[3]);
		cairo_set_line_width(cr, border_phys);

		double half_lw = border_phys * 0.5;
		double bx = bg_x - half_lw;
		double by = bg_y - half_lw;
		double bw = bg_w + border_phys;
		double bh = bg_h + border_phys;

		// Ensures the border rectangle stays in bounds and has positive
		// width/height.
		if (bx < 0.0) {
			bw += bx;
			bx = 0.0;
		}
		if (by < 0.0) {
			bh += by;
			by = 0.0;
		}
		if (bx + bw > (double)node->surface_pixel_w)
			bw = (double)node->surface_pixel_w - bx;
		if (by + bh > (double)node->surface_pixel_h)
			bh = (double)node->surface_pixel_h - by;
		if (bw < 0.0)
			bw = 0.0;
		if (bh < 0.0)
			bh = 0.0;

		if (bw > 0.0 && bh > 0.0) {
			if (radius > 0.0) {
				double outer_radius = radius + half_lw;
				if (outer_radius < 0.0)
					outer_radius = 0.0;
				draw_rounded_rect(cr, bx, by, bw, bh, outer_radius);
			} else {
				cairo_rectangle(cr, bx, by, bw, bh);
			}
			cairo_stroke(cr);
		}
	}

	cairo_surface_flush(node->surface);
	cairo_destroy(cr);

	if (node->buffer) {
		wlr_buffer_drop(&node->buffer->base);
		node->buffer = NULL;
	}

	struct mango_text_buffer *buf = calloc(1, sizeof(*buf));
	if (!buf)
		return;

	wlr_buffer_init(&buf->base, &text_buffer_impl, node->surface_pixel_w,
					node->surface_pixel_h);
	buf->surface = node->surface;
	node->buffer = buf;

	wlr_scene_buffer_set_buffer(node->scene_buffer, &buf->base);

	node->logical_width = node->target_width;
	node->logical_height = node->target_height;
	wlr_scene_buffer_set_dest_size(node->scene_buffer, node->logical_width,
								   node->logical_height);
}

void mango_group_bar_set_focus(MangoGroupBar *node, bool focused) {
	if (!node || node->focused == focused)
		return;
	node->focused = focused;
	if (node->last_text) {
		float scale = node->last_scale > 0.0f ? node->last_scale : 1.0f;
		mango_group_bar_update(node, node->last_text, scale);
	}
}

void mango_group_bar_set_colors(MangoGroupBar *node, const float fg[4],
								const float bg[4]) {
	if (!node)
		return;

	memcpy(node->fg_color, fg, sizeof(node->fg_color));
	memcpy(node->bg_color, bg, sizeof(node->bg_color));

	if (!node->focused && node->last_text) {
		float scale = node->last_scale > 0.0f ? node->last_scale : 1.0f;
		mango_group_bar_update(node, node->last_text, scale);
	}
}

void mango_jump_label_node_apply_config(MangoJumpLabel *node,
										const DecorateDrawData *data) {
	if (!node || !data)
		return;

	memcpy(node->fg_color, data->fg_color, sizeof(node->fg_color));
	memcpy(node->bg_color, data->bg_color, sizeof(node->bg_color));
	memcpy(node->focus_fg_color, data->focus_fg_color,
		   sizeof(node->focus_fg_color));
	memcpy(node->focus_bg_color, data->focus_bg_color,
		   sizeof(node->focus_bg_color));
	memcpy(node->border_color, data->border_color, sizeof(node->border_color));
	node->border_width = data->border_width;
	node->corner_radius = data->corner_radius;
	node->padding_x = data->padding_x;
	node->padding_y = data->padding_y;

	g_free(node->font_desc);
	node->font_desc =
		g_strdup(data->font_desc ? data->font_desc : "monospace Bold 16");

	if (node->cached_text && node->cached_scale > 0.0f) {
		mango_jump_label_node_update(node, node->cached_text,
									 node->cached_scale);
	}
}

void mango_group_bar_apply_config(MangoGroupBar *node,
								  const DecorateDrawData *data) {
	if (!node || !data)
		return;

	memcpy(node->fg_color, data->fg_color, sizeof(node->fg_color));
	memcpy(node->bg_color, data->bg_color, sizeof(node->bg_color));
	memcpy(node->focus_fg_color, data->focus_fg_color,
		   sizeof(node->focus_fg_color));
	memcpy(node->focus_bg_color, data->focus_bg_color,
		   sizeof(node->focus_bg_color));
	memcpy(node->border_color, data->border_color, sizeof(node->border_color));
	node->border_width = data->border_width;
	node->corner_radius = data->corner_radius;
	node->padding_x = data->padding_x;
	node->padding_y = data->padding_y;

	g_free(node->font_desc);
	node->font_desc =
		g_strdup(data->font_desc ? data->font_desc : "monospace Bold 16");

	if (node->last_text) {
		float scale = node->last_scale > 0.0f ? node->last_scale : 1.0f;
		mango_group_bar_update(node, node->last_text, scale);
	}
}
