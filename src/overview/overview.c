#include "mango/overview/overview.h"
#include "mango/layout/layout.h"
#include "mango/common/server.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include "mango/animation/client.h"
#include "mango/common/util.h"

// Returns 0 when the target window shares its tag with other windows.
uint32_t want_restore_fullscreen(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &server.clients, link) {
		if (c && c != target_client && c->tags == target_client->tags &&
			c == server.selected_monitor->sel && c->mon &&
			c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id !=
				SCROLLER &&
			c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id !=
				VERTICAL_SCROLLER) {
			return 0;
		}
	}

	return 1;
}

// Recomputes layout after surface commit (scene_surface commit resets
// dest/source and needs to be reapplied).
void handle_overview_card_surface_commit(struct wl_listener *listener,
										 void *data) {
	struct ov_card_surface *entry = wl_container_of(listener, entry, commit);
	if (entry->c && entry->c->ov_card_tree)
		overview_layout_card(entry->c);
}

// Removes and frees the node when the surface is destroyed.
void handle_overview_card_surface_destroy(struct wl_listener *listener,
										  void *data) {
	struct ov_card_surface *entry = wl_container_of(listener, entry, destroy);
	wl_list_remove(&entry->link);
	wl_list_remove(&entry->commit.link);
	wl_list_remove(&entry->destroy.link);
	free(entry);
}

// Creates a card scene_surface node for every surface (including subsurfaces).
void overview_card_surface_add(struct wlr_surface *surface, int sx, int sy,
							   void *data) {
	Client *c = data;
	if (!c->ov_card_tree)
		return;

	struct ov_card_surface *entry = ecalloc(1, sizeof(*entry));
	entry->c = c;
	entry->surface = surface;
	entry->sx = sx;
	entry->sy = sy;
	entry->is_root = (surface == client_surface(c));

	entry->scene_surface = wlr_scene_surface_create(c->ov_card_tree, surface);
	if (!entry->scene_surface) {
		free(entry);
		return;
	}
	entry->buffer = entry->scene_surface->buffer;
	wlr_scene_buffer_set_filter_mode(entry->buffer, WLR_SCALE_FILTER_BILINEAR);
	entry->buffer->node.data = c; /* Hit test. */

	entry->commit.notify = handle_overview_card_surface_commit;
	wl_signal_add(&surface->events.commit, &entry->commit);
	entry->destroy.notify = handle_overview_card_surface_destroy;
	wl_signal_add(&surface->events.destroy, &entry->destroy);

	wl_list_insert(&c->ov_card_surfaces, &entry->link);
}
// Updates card position and scale from the current geometry; content origin
// uses client_get_clip geometry offset.
void overview_layout_card(Client *c) {
	if (!c->ov_card_tree)
		return;

	struct wlr_box geo = c->animation.current;
	if (geo.width <= 0 || geo.height <= 0)
		client_get_geometry(c, &geo);
	int32_t w = geo.width - 2 * (int32_t)c->bw;
	int32_t h = geo.height - 2 * (int32_t)c->bw;
	if (w <= 0 || h <= 0)
		return;

	wlr_scene_node_set_position(&c->ov_card_tree->node, c->bw, c->bw);

	// Content origin (geometry offset) and card content size.
	struct wlr_box clip;
	client_get_clip(c, &clip);

	float content_w, content_h;
#ifdef XWAYLAND
	struct wlr_surface *s = client_surface(c);
	if (client_is_x11(c)) {
		content_w = s->current.width;
		content_h = s->current.height;
	} else
#endif
	{
		content_w = c->surface.xdg->geometry.width;
		content_h = c->surface.xdg->geometry.height;
	}
	if (content_w <= 0 || content_h <= 0)
		return;

	float scale_x = (float)w / content_w;
	float scale_y = (float)h / content_h;

	struct ov_card_surface *entry;
	wl_list_for_each(entry, &c->ov_card_surfaces, link) {
		struct wlr_surface *es = entry->surface;
		/* current.width/height are logical coordinates; buffer_width/height are
		 * pixel coordinates. */
		float lw = es->current.width;
		float lh = es->current.height;

		if (entry->is_root) {
			/*
			 * The root surface is clipped to fill the card; source_box is in
			 * buffer pixels, converted by ratio.
			 */
			float ratio_x =
				es->current.width > 0
					? (float)es->current.buffer_width / es->current.width
					: 1.0f;
			float ratio_y =
				es->current.height > 0
					? (float)es->current.buffer_height / es->current.height
					: 1.0f;
			wlr_scene_node_set_position(&entry->buffer->node, 0, 0);
			wlr_scene_buffer_set_dest_size(entry->buffer, w, h);
			struct wlr_fbox src = {
				.x = clip.x * ratio_x,
				.y = clip.y * ratio_y,
				.width = content_w * ratio_x,
				.height = content_h * ratio_y,
			};
			wlr_scene_buffer_set_source_box(entry->buffer, &src);
		} else {
			/* Subsurfaces are positioned and scaled relative to the content
			 * origin. */
			int px = (int)((entry->sx - clip.x) * scale_x);
			int py = (int)((entry->sy - clip.y) * scale_y);
			wlr_scene_node_set_position(&entry->buffer->node, px, py);
			wlr_scene_buffer_set_dest_size(entry->buffer, (int)(lw * scale_x),
										   (int)(lh * scale_y));
		}
	}
}

// Destroys the card tree and frees all surface nodes.
void overview_destroy_card(Client *c) {
	if (!c->ov_card_tree)
		return;

	struct ov_card_surface *entry, *tmp;
	wl_list_for_each_safe(entry, tmp, &c->ov_card_surfaces, link) {
		wl_list_remove(&entry->commit.link);
		wl_list_remove(&entry->destroy.link);
		wl_list_remove(&entry->link);
		free(entry);
	}

	wlr_scene_node_destroy(&c->ov_card_tree->node);
	c->ov_card_tree = NULL;
}
// Applies rounded corners to all buffer nodes of the card.
void overview_card_set_corner_radii(Client *c, struct fx_corner_radii corners) {
	struct ov_card_surface *entry;
	wl_list_for_each(entry, &c->ov_card_surfaces, link)
		wlr_scene_buffer_set_corner_radii(entry->buffer, corners);
}

// Entering overview: saves and disables the real scene_surface tree and builds
// an independent card tree to display content.
void overview_backup_surface(Client *c) {
	if (c->ov_card_tree)
		return;
	/* A swallowed/hidden window is treated as fully gone: no card is created
	 * for it and its hidden state is not reset. */
	if (c->is_logic_hide)
		return;
	if (!client_surface(c) || !client_surface(c)->mapped)
		return;

	// Disables the real surface tree.
	c->overview_scene_surface = c->scene_surface;
	wlr_scene_node_set_enabled(&c->scene_surface->node, false);

	// In overview every tag window must show its card and must not be disabled
	// by the subtree hiding logic.
	c->is_logic_hide = false;
	c->is_clip_to_hide = false;
	wlr_scene_node_set_enabled(&c->scene->node, true);

	c->ov_card_tree = wlr_scene_tree_create(c->scene);
	if (!c->ov_card_tree)
		return;
	c->ov_card_tree->node.data = c; // Hit test.

	// Walks the surface tree and creates a card node per surface.
	wlr_surface_for_each_surface(client_surface(c), overview_card_surface_add,
								 c);

	// The card tree is created at the scene top; enabled jump labels are raised
	// above the cards.
	if (c->jump_label_node && c->jump_label_node->scene_buffer->node.enabled)
		wlr_scene_node_raise_to_top(&c->jump_label_node->scene_buffer->node);

	overview_layout_card(c);

	// Feeds one frame to start the render loop (later driven by scene_surface
	// frame-done).
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	client_send_frame_done(c, &now);
}
// Saves the window old state when switching from the normal view to overview.
void overview_backup(Client *c) {
	c->overview_isfloatingbak = c->isfloating;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->overview_ismaximizescreenbak = c->ismaximizescreen;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->animation.tagining = false;
	c->animation.tagouted = false;
	c->animation.tagouting = false;
	c->overview_backup_geom = c->geom;
	c->overview_backup_bw = c->bw;
	if (c->isfloating) {
		c->isfloating = 0;
	}

	overview_backup_surface(c);

	if (c->isfullscreen || c->ismaximizescreen) {
		client_pending_fullscreen_state(
			c, 0); // Clears the window fullscreen flag.
		client_pending_maximized_state(c, 0);
	}
	c->bw = c->isnoborder ? 0 : config.borderpx;

	client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
							WLR_EDGE_RIGHT);
}
// Restores window state when switching back from overview to the normal view.
void overview_restore(Client *c, const Arg *arg) {
	/* Swallowed/hidden windows stay as they are (not restored, not shown). */
	if (c->is_logic_hide)
		return;

	c->isfloating = c->overview_isfloatingbak;
	c->isfullscreen = c->overview_isfullscreenbak;
	c->ismaximizescreen = c->overview_ismaximizescreenbak;
	c->overview_isfloatingbak = 0;
	c->overview_isfullscreenbak = 0;
	c->overview_ismaximizescreenbak = 0;
	c->geom = c->overview_backup_geom;
	c->bw = c->overview_backup_bw;
	c->animation.tagining = false;
	c->is_restoring_from_ov = (arg->ui & c->tags & TAGMASK) == 0 ? true : false;

	// Destroys the card tree and restores the real scene_surface tree.
	overview_destroy_card(c);
	if (c->overview_scene_surface) {
		c->scene_surface = c->overview_scene_surface;
		c->overview_scene_surface = NULL;
		wlr_scene_node_set_enabled(&c->scene_surface->node, true);
	}

	if (c->isfloating) {
		// XRaiseWindow(display, c->win); // Raise the floating window to the
		// top
		resize(c, c->overview_backup_geom, 0);
	} else if (c->isfullscreen || c->ismaximizescreen) {
		if (want_restore_fullscreen(c) && c->ismaximizescreen) {
			client_set_maximize_screen(c, 1, false);
		} else if (want_restore_fullscreen(c) && c->isfullscreen) {
			client_apply_fullscreen(c, 1, false);
		} else {
			client_pending_fullscreen_state(c, 0);
			client_pending_maximized_state(c, 0);
			client_apply_fullscreen(c, false, false);
		}
	} else {
		if (c->is_restoring_from_ov) {
			c->is_restoring_from_ov = false;
			resize(c, c->overview_backup_geom, 0);
		}
	}

	if (c->bw == 0 && !c->isfullscreen) { // Windows created while in overview
										  // mode have no bw record.
		c->bw = c->isnoborder ? 0 : config.borderpx;
	}

	if (c->isfloating && !c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_NONE);
	}
}
