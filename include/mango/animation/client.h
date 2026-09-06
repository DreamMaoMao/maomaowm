#ifndef __ANIMATION_CLIENT_H__
#define __ANIMATION_CLIENT_H__ 1

#include "mango/mango.h"

bool client_is_ignore_output_clip(Client *c);
struct ivec2 compute_edge_offsets(Client *c);

void client_actual_size(Client *client, int32_t *width, int32_t *height);
void set_rect_size(struct wlr_scene_rect *rect, int32_t width, int32_t height);

struct fx_corner_radii set_client_corner_location(Client *client);

bool is_horizontal_stack_layout(Monitor *monitor);
bool is_horizontal_right_stack_layout(Monitor *monitor);

int32_t is_special_animation_rule(Client *client);
void set_overview_enter_animation(Client *c);
void set_client_open_animation(Client *c, struct wlr_box geo);

void snap_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int32_t sx,
									int32_t sy, void *data);
void scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int32_t sx,
							   int32_t sy, void *data);
void buffer_set_effect(Client *c, BufferData data);

void client_draw_shadow(Client *c, struct ivec2 offsets);
void client_draw_groupbar(Client *c, struct ivec2 offsets);
void global_draw_group_bar(Client *c, int32_t x, int32_t y, int32_t width,
						   int32_t height);
void client_draw_shield(Client *c, struct ivec2 clip_box);
void client_draw_blur(Client *c, struct ivec2 clip_box);
void client_draw_split_border(Client *c, bool hit_no_border,
							  struct ivec2 offsets);
void client_draw_border(Client *c, struct ivec2 offsets);
struct ivec2 clip_to_hide(Client *c, struct wlr_box *clip_box,
						  struct ivec2 offsets);
void client_set_drop_area(Client *c);

/* ---------- central rendering entry point ---------- */
void client_apply_clip(Client *c, float factor);
void fadeout_client_animation_next_tick(Client *c);
void client_animation_next_tick(Client *c);
void init_fadeout_client(Client *c);

/* Applies the final window state when animation is disabled: position,
 * clipping/visibility, and geometry state are synchronized. */
void client_apply_finish_geometry(Client *c);
void client_commit(Client *c);
void client_set_pending_state(Client *c);

typedef struct ResizeOpts {
	bool interact; // Interactive resize (window resized by mouse drag).
	bool skip_ov_enter_anim; // Pre-arrangement stage: skip the overview enter
							 // zoom.
} ResizeOpts;

void resize_apply(Client *c, struct wlr_box geo, ResizeOpts opts);
void resize(Client *c, struct wlr_box geo, int32_t interact);
bool client_draw_fadeout_frame(Client *c);
void client_set_focused_opacity_animation(Client *c);
void client_set_unfocused_opacity_animation(Client *c);
bool client_apply_focus_opacity(Client *c);
bool client_draw_frame(Client *c);

#endif
