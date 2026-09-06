#ifndef jump_label_node_H
#define jump_label_node_H

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <wlr/types/wlr_scene.h>

// 原有结构体，假设已存在
typedef struct {
	float fg_color[4];
	float bg_color[4];
	float focus_fg_color[4];
	float focus_bg_color[4];
	float border_color[4];
	int32_t border_width;
	int32_t corner_radius;
	int32_t padding_x;
	int32_t padding_y;
	const char *font_desc;
} DecorateDrawData;

struct mango_text_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};
typedef struct {
	struct wlr_scene_buffer *scene_buffer;
	struct mango_text_buffer *buffer;
	cairo_surface_t *surface;
	int surface_pixel_w, surface_pixel_h;

	float fg_color[4];
	float bg_color[4];
	float focus_fg_color[4];
	float focus_bg_color[4];
	float border_color[4];
	int32_t border_width;
	int32_t corner_radius;
	int32_t padding_x;
	int32_t padding_y;
	char *font_desc;

	// 缓存
	char *cached_text;
	char *cached_font_desc;
	float cached_scale;
	float cached_fg_color[4];
	float cached_bg_color[4];
	float cached_focus_fg_color[4];
	float cached_focus_bg_color[4];
	float cached_border_color[4];
	int32_t cached_border_width;
	int32_t cached_corner_radius;
	int32_t cached_padding_x;
	int32_t cached_padding_y;
	bool cached_focused;

	bool focused;

	// 测量
	cairo_surface_t *measure_surface;
	cairo_t *measure_cr;
	PangoContext *measure_context;
	PangoLayout *measure_layout;
	float measure_scale;

	int32_t logical_width;
	int32_t logical_height;
} MangoJumpLabel;

typedef struct {
	uint32_t type; // must at first in struct
	struct wlr_scene_buffer *scene_buffer;
	struct mango_text_buffer *buffer;
	cairo_surface_t *surface;
	int surface_pixel_w, surface_pixel_h;
	void *node_data; // 存储窗口指针

	// 初始配置
	float fg_color[4];
	float bg_color[4];
	float focus_fg_color[4];
	float focus_bg_color[4];
	float border_color[4];
	int32_t border_width;
	int32_t corner_radius;
	int32_t padding_x;
	int32_t padding_y;
	char *font_desc;

	// 尺寸
	int32_t target_width;
	int32_t target_height;

	// 缓存
	char *cached_text;
	char *cached_font_desc;
	float cached_scale;
	float cached_fg_color[4];
	float cached_bg_color[4];
	float cached_focus_fg_color[4];
	float cached_focus_bg_color[4];
	float cached_border_color[4];
	int32_t cached_border_width;
	int32_t cached_corner_radius;
	int32_t cached_padding_x;
	int32_t cached_padding_y;
	int32_t cached_target_width;
	int32_t cached_target_height;
	bool cached_focused;

	bool focused;

	// 上次绘制参数（用于尺寸变化重绘）
	char *last_text;
	float last_scale;

	// 测量
	cairo_surface_t *measure_surface;
	cairo_t *measure_cr;
	PangoContext *measure_context;
	PangoLayout *measure_layout;
	float measure_scale;

	int32_t logical_width;
	int32_t logical_height;
} MangoGroupBar;

void mango_text_global_finish(void);
MangoJumpLabel *mango_jump_label_node_create(struct wlr_scene_tree *parent,
											 DecorateDrawData data);
void mango_jump_label_node_destroy(MangoJumpLabel *node);
void mango_jump_label_node_set_background(MangoJumpLabel *node, float r,
										  float g, float b, float a);
void mango_jump_label_node_set_border(MangoJumpLabel *node, float r, float g,
									  float b, float a, int32_t width,
									  int32_t radius);
void mango_jump_label_node_set_padding(MangoJumpLabel *node, int32_t pad_x,
									   int32_t pad_y);
void mango_jump_label_node_update(MangoJumpLabel *node, const char *text,
								  float scale);

MangoGroupBar *mango_group_bar_create(void *cdata, uint32_t type,
									  struct wlr_scene_tree *parent,
									  DecorateDrawData data, int32_t width,
									  int32_t height);
void mango_group_bar_destroy(MangoGroupBar *node);
void mango_group_bar_set_size(MangoGroupBar *node, int32_t width,
							  int32_t height);
void mango_group_bar_update(MangoGroupBar *node, const char *text, float scale);

void mango_jump_label_node_set_focus(MangoJumpLabel *node, bool focused);
void mango_group_bar_set_focus(MangoGroupBar *node, bool focused);

void mango_group_bar_set_colors(MangoGroupBar *node, const float fg[4],
								const float bg[4]);
void mango_jump_label_node_apply_config(MangoJumpLabel *node,
										const DecorateDrawData *data);
void mango_group_bar_apply_config(MangoGroupBar *node,
								  const DecorateDrawData *data);
#endif // jump_label_node_H