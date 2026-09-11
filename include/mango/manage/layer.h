#ifndef __MANAGE_LAYER_H__
#define __MANAGE_LAYER_H__ 1

#include "mango/animation/common.h"
#include "mango/common/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wlr/util/box.h>

typedef struct LayerSurface {
	/* Must keep these three elements in this order */
	uint32_t type; // must at first in struct
	struct wlr_box geom, current, pending, animainit_geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_rect *shield;
	struct wlr_scene_shadow *shadow;
	struct wlr_scene_blur *blur;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	struct wl_list fadeout_link;
	int32_t mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;

	struct mango_animation animation;
	bool dirty;
	int32_t noblur;
	int32_t noanim;
	int32_t noshadow;
	int32_t animation_direction;
	char *animation_type_open;
	char *animation_type_close;
	bool shield_when_capture;
	bool need_output_flush;
	bool being_unmapped;
} LayerSurface;

typedef struct Popup {
	uint32_t type; // must at first in struct
	struct wlr_xdg_popup *wlr_popup;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener reposition;
} Popup;

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
