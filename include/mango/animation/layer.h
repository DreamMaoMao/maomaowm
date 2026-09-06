#ifndef __ANIMATION_LAYER_H__
#define __ANIMATION_LAYER_H__ 1

#include "mango/mango.h"

void layer_actual_size(LayerSurface *l, int32_t *width, int32_t *height);
void get_layer_area_bound(LayerSurface *l, struct wlr_box *bound);
void get_layer_target_geometry(LayerSurface *l, struct wlr_box *target_box);
void set_layer_dir_animaiton(LayerSurface *l, struct wlr_box *geo);
void layer_draw_shield(LayerSurface *l);
void layer_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer,
									 int32_t sx, int32_t sy, void *data);
void layer_fadeout_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer,
											 int32_t sx, int32_t sy,
											 void *data);
void fadeout_layer_animation_next_tick(LayerSurface *l);
void layer_animation_next_tick(LayerSurface *l);

void init_fadeout_layers(LayerSurface *l);
void layer_set_pending_state(LayerSurface *l);
void layer_commit(LayerSurface *l);
bool layer_draw_frame(LayerSurface *l);
bool layer_draw_fadeout_frame(LayerSurface *l);

#endif
