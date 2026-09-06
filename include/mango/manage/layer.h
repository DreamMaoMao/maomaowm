#ifndef __MANAGE_LAYER_H__
#define __MANAGE_LAYER_H__ 1

#include "mango/mango.h"

void arrange_layer(Monitor *m, struct wl_list *list,
				   struct wlr_box *usable_area, int32_t exclusive);
void layer_focus(LayerSurface *l);
void reset_exclusive_layers_focus(Monitor *m);
void arrange_layers(Monitor *m);
void iter_layer_scene_buffers(struct wlr_scene_buffer *buffer, int32_t sx,
							  int32_t sy, void *user_data);
void layer_flush_blur_background(LayerSurface *l);
void handle_layer_surface_map(struct wl_listener *listener, void *data);
void handle_layer_surface_commit(struct wl_listener *listener, void *data);
bool popup_unconstrain(Popup *popup);
void handle_popup_destroy(struct wl_listener *listener, void *data);
void handle_popup_commit(struct wl_listener *listener, void *data);
void handle_popup_reposition(struct wl_listener *listener, void *data);
void handle_new_xdg_popup(struct wl_listener *listener, void *data);
void handle_new_layer_surface(struct wl_listener *listener, void *data);
void handle_layer_node_destroy(struct wl_listener *listener, void *data);
void handle_layer_surface_unmap(struct wl_listener *listener, void *data);

#endif
