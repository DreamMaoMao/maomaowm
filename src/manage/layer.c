#include "mango/manage/layer.h"
#include "mango/manage/client.h"
#include "mango/layout/arrange.h"
#include "mango/ext-protocol/text-input.h"
#include "mango/common/server.h"
#include "mango/animation/layer.h"
#include "mango/common/log.h"
#include "mango/common/util.h"
#include "mango/ipc/ipc.h"
#include "mango/input/pointer.h"

/* Maps the wlr_layer_shell layer enum to scene layers. */
static const int32_t layermap[] = {LyrBg, LyrBottom, LyrTop, LyrOverlay};

void arrange_layer(Monitor *m, struct wl_list *list,
				   struct wlr_box *usable_area, int32_t exclusive) {
	LayerSurface *l = NULL;
	struct wlr_box full_area = m->m;

	wl_list_for_each(l, list, link) {
		struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;

		if (exclusive != (layer_surface->current.exclusive_zone > 0) ||
			!layer_surface->initialized)
			continue;

		if (l->being_unmapped)
			continue;

		wlr_scene_layer_surface_v1_configure(l->scene_layer, &full_area,
											 usable_area);
		wlr_scene_node_set_position(&l->popups->node, l->scene->node.x,
									l->scene->node.y);
	}
}

void layer_focus(LayerSurface *l) {
	client_focus(NULL, 0);
	mango_im_relay_set_focus(server.input_method_relay,
							 l->layer_surface->surface);
	client_notify_enter(l->layer_surface->surface,
						wlr_seat_get_keyboard(server.seat));
}

void reset_exclusive_layers_focus(Monitor *m) {
	LayerSurface *l = NULL;
	int32_t i;
	bool neet_change_focus_to_client = false;
	uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
		ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
	};

	if (!m)
		return;

	for (i = 0; i < (int32_t)LENGTH(layers_above_shell); i++) {
		wl_list_for_each(l, &m->layers[layers_above_shell[i]], link) {
			if (l == server.exclusive_focus &&
				l->layer_surface->current.keyboard_interactive !=
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {

				server.exclusive_focus = NULL;

				neet_change_focus_to_client = true;
			}

			if (l->layer_surface->surface ==
					server.seat->keyboard_state.focused_surface &&
				l->being_unmapped) {
				neet_change_focus_to_client = true;
			}

			if (l->layer_surface->current.keyboard_interactive ==
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE &&
				l->layer_surface->surface ==
					server.seat->keyboard_state.focused_surface) {
				neet_change_focus_to_client = true;
			}

			if (server.session_locked ||
				l->layer_surface->current.keyboard_interactive !=
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE ||
				l->being_unmapped)
				continue;
			/* Deactivate the focused client. */
			server.exclusive_focus = l;
			neet_change_focus_to_client = false;
			if (l->layer_surface->surface !=
				server.seat->keyboard_state.focused_surface)
				layer_focus(l);
			return;
		}
	}

	if (neet_change_focus_to_client) {
		client_focus(client_focus_top(server.selected_monitor), 1);
	}
}

void arrange_layers(Monitor *m) {
	int32_t i;
	struct wlr_box usable_area = m->m;

	if (!m->wlr_output->enabled)
		return;

	if (m->iscleanuping)
		return;

	/* Arrange exclusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrange_layer(m, &m->layers[i], &usable_area, 1);

	if (!wlr_box_equal(&usable_area, &m->w)) {
		m->w = usable_area;
		arrange(m, false, false);
	}

	/* Arrange non-exlusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrange_layer(m, &m->layers[i], &usable_area, 0);
}

void iter_layer_scene_buffers(struct wlr_scene_buffer *buffer, int32_t sx,
							  int32_t sy, void *user_data) {
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface) {
		return;
	}

	LayerSurface *l = (LayerSurface *)user_data;

	wlr_scene_node_set_enabled(&l->blur->node, true);
	wlr_scene_blur_set_transparency_mask_source(l->blur, buffer);
	wlr_scene_blur_set_size(l->blur, l->geom.width, l->geom.height);

	if (config.blur_optimized) {
		wlr_scene_blur_set_should_only_blur_bottom_layer(l->blur, true);
	} else {
		wlr_scene_blur_set_should_only_blur_bottom_layer(l->blur, false);
	}
}

void layer_flush_blur_background(LayerSurface *l) {
	if (!config.blur)
		return;

	// If the background layer changed, mark the optimized blurred background
	// cache as needing an update.
	if (l->layer_surface->current.layer ==
		ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
		if (l->mon) {
			wlr_scene_optimized_blur_mark_dirty(l->mon->blur);
		}
	}
}

void handle_layer_surface_map(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, map);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	int32_t ji;
	ConfigLayerRule *r;

	l->mapped = 1;

	if (!l->mon)
		return;
	strncpy(l->mon->last_open_surface, layer_surface->namespace,
			sizeof(l->mon->last_open_surface) -
				1); // Copies at most 255 characters.
	l->mon->last_open_surface[sizeof(l->mon->last_open_surface) - 1] =
		'\0'; // Ensures the string is null-terminated.

	// Initializes the geometry position.
	get_layer_target_geometry(l, &l->geom);

	l->noanim = 0;
	l->dirty = false;
	l->shield_when_capture = false;
	l->noblur = 0;
	l->shadow = NULL;
	l->need_output_flush = true;

	// Applies the layer rule.
	for (ji = 0; ji < config.layer_rules_count; ji++) {
		if (regex_match(config.layer_rules[ji].layer_name,
						l->layer_surface->namespace)) {

			r = &config.layer_rules[ji];
			APPLY_INT_PROP(l, r, shield_when_capture);
			APPLY_INT_PROP(l, r, noblur);
			APPLY_INT_PROP(l, r, noanim);
			APPLY_INT_PROP(l, r, noshadow);
			APPLY_STRING_PROP(l, r, animation_type_open);
			APPLY_STRING_PROP(l, r, animation_type_close);
		}
	}

	// Initializes the shield.
	l->shield =
		wlr_scene_rect_create(l->scene, 0, 0, (float[4]){0, 0, 0, 0xff});
	l->shield->node.data = l;
	wlr_scene_node_lower_to_bottom(&l->shield->node);
	wlr_scene_node_set_enabled(&l->shield->node, false);

	// Initializes the shadow.
	if (layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
		if (layer_surface->current.exclusive_zone == 0) {
			l->shadow = wlr_scene_shadow_create(
				l->scene, 0, 0, config.border_radius, config.shadows_blur,
				config.shadowscolor);
			wlr_scene_node_lower_to_bottom(&l->shadow->node);
			wlr_scene_node_set_enabled(&l->shadow->node, true);
		}

		l->blur = wlr_scene_blur_create(l->scene, 0, 0);
		wlr_scene_node_lower_to_bottom(&l->blur->node);
	}

	// Initializes the animation.
	if (config.animations && config.layer_animations && !l->noanim) {
		l->animation.duration = config.animation_duration_open;
		l->animation.action = OPEN;
		layer_set_pending_state(l);
	} else {
		l->animainit_geom = l->animation.current = l->current = l->pending =
			l->geom;
	}

	// Refreshes the layout so windows notice exclude_zone changes and sets the
	// exclusive surface.
	arrange_layers(l->mon);
	reset_exclusive_layers_focus(l->mon);
	printstatus(IPC_WATCH_LAST_OPEN_SURFACE);
}

void handle_layer_surface_commit(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, surface_commit);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	struct wlr_scene_tree *scene_layer =
		server.layers[layermap[layer_surface->current.layer]];
	struct wlr_layer_surface_v1_state old_state;
	struct wlr_box box = l->geom;

	if (l->layer_surface->initial_commit) {
		client_set_scale(layer_surface->surface, l->mon->wlr_output->scale);

		/* Temporarily set the layer's current state to pending
		 * so that we can easily arrange it */
		old_state = l->layer_surface->current;
		l->layer_surface->current = l->layer_surface->pending;
		arrange_layers(l->mon);
		l->layer_surface->current = old_state;
		// Focus is only set before map for interactive-on-demand layers.
		if (!server.exclusive_focus &&
			l->layer_surface->current.keyboard_interactive ==
				ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
			layer_focus(l);
		}
		return;
	}

	// Checks whether the surface has a buffer.
	// Empty buffer: only hides; the mapped state is unchanged.
	if (l->mapped && !layer_surface->surface->buffer) {
		wlr_scene_node_set_enabled(&l->scene->node, false);
		return;
	} else {
		wlr_scene_node_set_enabled(&l->scene->node, true);
	}

	get_layer_target_geometry(l, &box);

	if (!wlr_box_equal(&box, &l->geom)) {
		l->geom.x = box.x;
		l->geom.y = box.y;
		l->geom.width = box.width;
		l->geom.height = box.height;

		if (config.animations && config.layer_animations && !l->noanim &&
			l->mapped &&
			layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM &&
			layer_surface->current.layer !=
				ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
			l->animation.action = MOVE;
			l->animation.duration = config.animation_duration_move;
			l->need_output_flush = true;
			layer_set_pending_state(l);
		} else {
			l->animainit_geom = l->animation.current = l->current = l->pending =
				l->geom;
			l->need_output_flush = true;
		}
	}

	if (config.blur && config.blur_layer) {

		if (!l->noblur &&
			layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM &&
			layer_surface->current.layer !=
				ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND &&
			l->blur) {

			wlr_scene_node_for_each_buffer(&l->scene->node,
										   iter_layer_scene_buffers, l);
		}
	}

	layer_flush_blur_background(l);

	if (layer_surface->current.committed == 0 &&
		l->mapped == layer_surface->surface->mapped)
		return;
	l->mapped = layer_surface->surface->mapped;

	if (layer_surface->current.committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
		if (scene_layer != l->scene->node.parent) {
			wlr_scene_node_reparent(&l->scene->node, scene_layer);
			wl_list_remove(&l->link);
			wl_list_insert(&l->mon->layers[layer_surface->current.layer],
						   &l->link);
			wlr_scene_node_reparent(
				&l->popups->node,
				(layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP
					 ? server.layers[LyrTop]
					 : scene_layer));
		}
	}

	arrange_layers(l->mon);

	if (layer_surface->current.committed &
		WLR_LAYER_SURFACE_V1_STATE_KEYBOARD_INTERACTIVITY) {
		reset_exclusive_layers_focus(l->mon);
	}
}

bool popup_unconstrain(Popup *popup) {
	struct wlr_xdg_popup *wlr_popup = popup->wlr_popup;
	Client *c = NULL;
	LayerSurface *l = NULL;
	int32_t type = -1;

	if (!wlr_popup || !wlr_popup->parent) {
		return false;
	}

	struct wlr_scene_node *parent_node = wlr_popup->parent->data;
	if (!parent_node) {
		mango_error(true, WLR_ERROR, "Popup parent has no scene node");
		return false;
	}

	type = toplevel_from_wlr_surface(wlr_popup->base->surface, &c, &l);
	if ((type == LayerShell && (!l || !l->mon)) ||
		(type != LayerShell && (!c || !c->mon))) {
		return true;
	}

	struct wlr_box usable = type == LayerShell ? l->mon->m : c->mon->w;

	int lx, ly;
	struct wlr_box constraint_box;

	if (type == LayerShell) {
		wlr_scene_node_coords(&l->scene_layer->tree->node, &lx, &ly);
		constraint_box.x = usable.x - lx;
		constraint_box.y = usable.y - ly;
		constraint_box.width = usable.width;
		constraint_box.height = usable.height;
	} else {
		constraint_box.x =
			usable.x - (c->geom.x + c->bw - c->surface.xdg->current.geometry.x);
		constraint_box.y =
			usable.y - (c->geom.y + c->bw - c->surface.xdg->current.geometry.y);
		constraint_box.width = usable.width;
		constraint_box.height = usable.height;
	}

	wlr_xdg_popup_unconstrain_from_box(wlr_popup, &constraint_box);
	return false;
}

void handle_popup_destroy(struct wl_listener *listener, void *data) {
	Popup *popup = wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->reposition.link);
	free(popup);
}

void handle_popup_commit(struct wl_listener *listener, void *data) {
	Popup *popup = wl_container_of(listener, popup, commit);

	struct wlr_surface *surface = data;
	bool should_destroy = false;
	struct wlr_xdg_popup *wlr_popup =
		wlr_xdg_popup_try_from_wlr_surface(surface);

	if (!wlr_popup->base->initial_commit)
		return;

	if (!wlr_popup->parent || !wlr_popup->parent->data) {
		should_destroy = true;
		goto cleanup_popup_commit;
	}

	wlr_scene_node_raise_to_top(wlr_popup->parent->data);

	wlr_popup->base->surface->data =
		wlr_scene_xdg_surface_create(wlr_popup->parent->data, wlr_popup->base);

	popup->wlr_popup = wlr_popup;

	should_destroy = popup_unconstrain(popup);

cleanup_popup_commit:

	wl_list_remove(&popup->commit.link);
	popup->commit.notify = NULL;

	if (should_destroy) {
		wlr_xdg_popup_destroy(wlr_popup);
	}
}

void handle_popup_reposition(struct wl_listener *listener, void *data) {
	Popup *popup = wl_container_of(listener, popup, reposition);
	(void)popup_unconstrain(popup);
}

void handle_new_xdg_popup(struct wl_listener *listener, void *data) {
	struct wlr_xdg_popup *wlr_popup = data;

	Popup *popup = calloc(1, sizeof(Popup));
	if (!popup)
		return;

	popup->type = XdgPopup;

	popup->destroy.notify = handle_popup_destroy;
	wl_signal_add(&wlr_popup->events.destroy, &popup->destroy);

	popup->commit.notify = handle_popup_commit;
	wl_signal_add(&wlr_popup->base->surface->events.commit, &popup->commit);

	popup->reposition.notify = handle_popup_reposition;
	wl_signal_add(&wlr_popup->events.reposition, &popup->reposition);
}

void handle_new_layer_surface(struct wl_listener *listener, void *data) {
	struct wlr_layer_surface_v1 *layer_surface = data;
	LayerSurface *l = NULL;
	struct wlr_surface *surface = layer_surface->surface;
	struct wlr_scene_tree *scene_layer =
		server.layers[layermap[layer_surface->pending.layer]];

	if (!layer_surface->output &&
		!(layer_surface->output = server.selected_monitor
									  ? server.selected_monitor->wlr_output
									  : NULL)) {
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	l = layer_surface->data = ecalloc(1, sizeof(*l));
	l->type = LayerShell;
	LISTEN(&surface->events.map, &l->map, handle_layer_surface_map);
	LISTEN(&surface->events.commit, &l->surface_commit,
		   handle_layer_surface_commit);
	LISTEN(&surface->events.unmap, &l->unmap, handle_layer_surface_unmap);

	l->layer_surface = layer_surface;
	l->mon = layer_surface->output->data;
	l->scene_layer =
		wlr_scene_layer_surface_v1_create(scene_layer, layer_surface);
	l->scene = l->scene_layer->tree;
	l->popups = surface->data = wlr_scene_tree_create(
		layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP
			? server.layers[LyrTop]
			: scene_layer);
	l->scene->node.data = l->popups->node.data = l;

	LISTEN(&l->scene->node.events.destroy, &l->destroy,
		   handle_layer_node_destroy);

	wl_list_insert(&l->mon->layers[layer_surface->pending.layer], &l->link);
}

void handle_layer_node_destroy(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, destroy);

	wl_list_remove(&l->link);
	wl_list_remove(&l->destroy.link);
	wl_list_remove(&l->map.link);
	wl_list_remove(&l->unmap.link);
	wl_list_remove(&l->surface_commit.link);
	wlr_scene_node_destroy(&l->popups->node);
	free(l);
}

void handle_layer_surface_unmap(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, unmap);

	l->mapped = 0;
	l->being_unmapped = true;

	init_fadeout_layers(l);

	wlr_scene_node_set_enabled(&l->scene->node, false);

	if (l == server.exclusive_focus)
		server.exclusive_focus = NULL;

	if (l->layer_surface->output && (l->mon = l->layer_surface->output->data))
		arrange_layers(l->mon);

	reset_exclusive_layers_focus(l->mon);

	pointer_process_motion(0, NULL, 0, 0, 0, 0);
	layer_flush_blur_background(l);
	wlr_scene_node_destroy(&l->shadow->node);
	l->shadow = NULL;
	l->being_unmapped = false;
}
