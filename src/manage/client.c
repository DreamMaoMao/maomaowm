#include "mango/manage/client.h"
#include "mango/common/server.h"
#include "mango/layout/layout.h"
#include "mango/layout/scroll.h"
#include "mango/layout/arrange.h"
#include "mango/layout/dwindle.h"
#include "mango/overview/overview.h"
#include "mango/switcher/switcher.h"
#include "mango/dispatch/bind.h"
#include "mango/animation/client.h"
#include "mango/ext-protocol/foreign-toplevel.h"
#include "mango/ext-protocol/text-input.h"
#include "mango/common/util.h"
#include "mango/manage/monitor.h"
#include "mango/ipc/ipc.h"
#include "mango/manage/misc.h"
#include "mango/input/pointer.h"

/* Placeholder appid/title used when no client surface type can be matched. */
static const char broken[] = "broken";

int32_t client_is_x11(Client *c) {
#ifdef XWAYLAND
	return c->type == X11;
#endif
	return 0;
}
struct wlr_surface *client_surface(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->surface;
#endif
	return c->surface.xdg->surface;
}
int32_t toplevel_from_wlr_surface(struct wlr_surface *s, Client **pc,
								  LayerSurface **pl) {
	struct wlr_xdg_surface *xdg_surface, *tmp_xdg_surface;
	struct wlr_surface *root_surface;
	struct wlr_layer_surface_v1 *layer_surface;
	Client *c = NULL;
	LayerSurface *l = NULL;
	int32_t type = -1;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface;
#endif

	if (!s)
		return -1;
	root_surface = wlr_surface_get_root_surface(s);

#ifdef XWAYLAND
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(root_surface))) {
		c = xsurface->data;
		type = c->type;
		goto end;
	}
#endif

	if ((layer_surface =
			 wlr_layer_surface_v1_try_from_wlr_surface(root_surface))) {
		l = layer_surface->data;
		type = LayerShell;
		goto end;
	}

	xdg_surface = wlr_xdg_surface_try_from_wlr_surface(root_surface);
	while (xdg_surface) {
		tmp_xdg_surface = NULL;
		switch (xdg_surface->role) {
		case WLR_XDG_SURFACE_ROLE_POPUP:
			if (!xdg_surface->popup || !xdg_surface->popup->parent)
				return -1;

			tmp_xdg_surface = wlr_xdg_surface_try_from_wlr_surface(
				xdg_surface->popup->parent);

			if (!tmp_xdg_surface)
				return toplevel_from_wlr_surface(xdg_surface->popup->parent, pc,
												 pl);

			xdg_surface = tmp_xdg_surface;
			break;
		case WLR_XDG_SURFACE_ROLE_TOPLEVEL:
			c = xdg_surface->data;
			type = c->type;
			goto end;
		case WLR_XDG_SURFACE_ROLE_NONE:
			return -1;
		}
	}

end:
	if (pl)
		*pl = l;
	if (pc)
		*pc = c;
	return type;
}
void client_activate_surface(struct wlr_surface *s, int32_t activated) {
	struct wlr_xdg_toplevel *toplevel;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface;
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(s))) {
		if (activated && xsurface->minimized)
			wlr_xwayland_surface_set_minimized(xsurface, false);
		wlr_xwayland_surface_activate(xsurface, activated);
		return;
	}
#endif
	if ((toplevel = wlr_xdg_toplevel_try_from_wlr_surface(s)))
		wlr_xdg_toplevel_set_activated(toplevel, activated);
}

const char *client_get_appid(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->class ? c->surface.xwayland->class
										  : "broken";
#endif
	return c->surface.xdg->toplevel->app_id ? c->surface.xdg->toplevel->app_id
											: "broken";
}

uint32_t get_client_tag_idx(const Client *c) {
	if (!c || (c->tags & TAG0_MASK))
		return 0;
	return get_tags_first_tag_num(c->tags);
}

int32_t client_get_pid(Client *c) {
	pid_t pid;
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->pid;
#endif
	wl_client_get_credentials(c->surface.xdg->client->client, &pid, NULL, NULL);
	return pid;
}
void client_get_clip(Client *c, struct wlr_box *clip) {
	*clip = (struct wlr_box){
		.x = 0,
		.y = 0,
		.width = c->geom.width - 2 * c->bw,
		.height = c->geom.height - 2 * c->bw,
	};

#ifdef XWAYLAND
	if (client_is_x11(c))
		return;
#endif

	clip->x = c->surface.xdg->geometry.x;
	clip->y = c->surface.xdg->geometry.y;
}
void client_get_geometry(Client *c, struct wlr_box *geom) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		/* Converts X11 physical size back to logical size. */
		float scale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;
		geom->x = (int32_t)roundf(c->surface.xwayland->x / scale);
		geom->y = (int32_t)roundf(c->surface.xwayland->y / scale);
		geom->width = (int32_t)roundf(c->surface.xwayland->width / scale);
		geom->height = (int32_t)roundf(c->surface.xwayland->height / scale);
		return;
	}
#endif
	*geom = c->surface.xdg->geometry;
}

Client *client_get_parent(Client *c) {
	Client *p = NULL;
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		if (c->surface.xwayland->parent)
			toplevel_from_wlr_surface(c->surface.xwayland->parent->surface, &p,
									  NULL);
		return p;
	}
#endif
	if (c->surface.xdg->toplevel->parent)
		toplevel_from_wlr_surface(
			c->surface.xdg->toplevel->parent->base->surface, &p, NULL);
	return p;
}
int32_t client_has_children(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return !wl_list_empty(&c->surface.xwayland->children);
#endif
	/* surface.xdg->link is never empty because it always contains at least the
	 * surface itself. */
	return wl_list_length(&c->surface.xdg->link) > 1;
}

const char *client_get_title(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->title ? c->surface.xwayland->title
										  : "broken";
#endif
	return c->surface.xdg->toplevel->title ? c->surface.xdg->toplevel->title
										   : "broken";
}
int32_t client_is_float_type(Client *c) {
	struct wlr_xdg_toplevel *toplevel;
	struct wlr_xdg_toplevel_state state;

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		xcb_size_hints_t *size_hints = surface->size_hints;

		if (!size_hints)
			return 0;

		if (surface->modal)
			return 1;

		if (wlr_xwayland_surface_has_window_type(
				surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DIALOG) ||
			wlr_xwayland_surface_has_window_type(
				surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_SPLASH) ||
			wlr_xwayland_surface_has_window_type(
				surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_TOOLBAR) ||
			wlr_xwayland_surface_has_window_type(
				surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_UTILITY)) {
			return 1;
		}

		return size_hints && size_hints->min_width > 0 &&
			   size_hints->min_height > 0 &&
			   (size_hints->max_width == size_hints->min_width ||
				size_hints->max_height == size_hints->min_height);
	}
#endif

	toplevel = c->surface.xdg->toplevel;
	state = toplevel->current;
	return toplevel->parent || (state.min_width != 0 && state.min_height != 0 &&
								(state.min_width == state.max_width ||
								 state.min_height == state.max_height));
}
int32_t client_is_rendered_on_mon(Client *c, Monitor *m) {
	/* This is needed for when you don't want to check formal assignment,
	 * but rather actual displaying of the pixels.
	 * Usually VISIBLEON suffices and is also faster. */
	struct wlr_surface_output *s;
	int32_t unused_lx, unused_ly;
	if (!wlr_scene_node_coords(&c->scene->node, &unused_lx, &unused_ly))
		return 0;
	wl_list_for_each(s, &client_surface(c)->current_outputs,
					 link) if (s->output == m->wlr_output) return 1;
	return 0;
}

int32_t client_is_unmanaged(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->override_redirect;
#endif
	return 0;
}
void client_notify_enter(struct wlr_surface *s, struct wlr_keyboard *kb) {
	if (kb)
		wlr_seat_keyboard_notify_enter(server.seat, s, kb->keycodes,
									   kb->num_keycodes, &kb->modifiers);
	else
		wlr_seat_keyboard_notify_enter(server.seat, s, NULL, 0, NULL);
}

void client_send_close(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_close(c->surface.xwayland);
		return;
	}
#endif
	wlr_xdg_toplevel_send_close(c->surface.xdg->toplevel);
}
void client_set_border_color(Client *c, const float color[4]) {
	wlr_scene_rect_set_color(c->border, color);
}

void client_set_fullscreen(Client *c, int32_t fullscreen) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_set_fullscreen(c->surface.xwayland, fullscreen);
		return;
	}
#endif
	wlr_xdg_toplevel_set_fullscreen(c->surface.xdg->toplevel, fullscreen);
}

void client_set_scale(struct wlr_surface *s, float scale) {
	wlr_fractional_scale_v1_notify_scale(s, scale);
	wlr_surface_set_preferred_buffer_scale(s, (int32_t)ceilf(scale));
}

/*
 * Clips the XWayland root surface via source_box.
 *
 * The X11 buffer is physical (apps render 1:1) while clip is mango logical
 * visibility. wlr_scene_subsurface_tree_set_clip would treat clip as surface
 * logical coordinates (XWayland state width/height is actually physical) and
 * scale up the content. Instead clip xwl_root_buffer directly with source_box +
 * dest_size: source_box uses physical coordinates (logical clip *
 * xwayland_scale) to sample 1:1, dest_size uses logical coordinates so display
 * stays logically scaled, and the buffer node is moved to the clip origin so
 * visible content stays at its on-screen position when the window overflows to
 * the left instead of spilling off-screen.
 */
void client_update_xwayland_clip(Client *c, struct wlr_box *clip) {
#ifdef XWAYLAND
	if (!c->xwl_root_buffer || !c->xwl_root_buffer->buffer)
		return;
	struct wlr_buffer *buf = c->xwl_root_buffer->buffer;
	float scale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;

	/* Records the clip state so it can be restored after a surface commit. */
	c->xwl_clip = *clip;
	c->xwl_clip_active = true;

	if (clip->width <= 0 || clip->height <= 0)
		return;

	struct wlr_fbox src = {
		.x = (float)clip->x * scale,
		.y = (float)clip->y * scale,
		.width = (float)clip->width * scale,
		.height = (float)clip->height * scale,
	};
	bool zoom_like = clip->x == 0 && clip->y == 0 &&
					 clip->width < c->geom.width - 2 * (int32_t)c->bw &&
					 clip->height < c->geom.height - 2 * (int32_t)c->bw;
	if (zoom_like) {
		src.x = 0;
		src.y = 0;
		src.width = buf->width;
		src.height = buf->height;
	}
	/* Clamps to the physical buffer bounds to prevent out-of-range sampling. */
	if (src.x < 0.f)
		src.x = 0.f;
	if (src.y < 0.f)
		src.y = 0.f;
	if (src.x + src.width > buf->width)
		src.width = buf->width - src.x;
	if (src.y + src.height > buf->height)
		src.height = buf->height - src.y;
	/*
	 * When the clip origin is beyond the buffer, src.width/height can become
	 * negative; guard against invalid source boxes so wlr_scene_buffer does not
	 * misbehave.
	 */
	if (src.width < 0.f)
		src.width = 0.f;
	if (src.height < 0.f)
		src.height = 0.f;

	wlr_scene_buffer_set_source_box(c->xwl_root_buffer, &src);
	wlr_scene_buffer_set_dest_size(c->xwl_root_buffer, clip->width,
								   clip->height);
	/* Moves the buffer node to the clip origin so visible content stays at its
	 * original on-screen position. */
	wlr_scene_node_set_position(&c->xwl_root_buffer->node, clip->x, clip->y);
#endif
}
/* Syncs the dest_size (logical size) of the XWayland root surface. */
void client_update_xwayland_dest_size(Client *c) {
#ifdef XWAYLAND
	if (!c->xwl_root_buffer || !c->xwl_root_buffer->buffer)
		return;
	/*
	 * While source_box clipping is active, restore the clip after surface
	 * commit so a still window is not forced back to full size once the
	 * animation ends.
	 */
	if (c->xwl_clip_active) {
		client_update_xwayland_clip(c, &c->xwl_clip);
		return;
	}
	struct wlr_buffer *buf = c->xwl_root_buffer->buffer;
	float scale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;
	int32_t w, h;
	if (client_is_unmanaged(c)) {
		w = (int32_t)roundf(buf->width / scale);
		h = (int32_t)roundf(buf->height / scale);
	} else {
		struct wlr_box cur = c->animation.current;
		w = cur.width - 2 * (int32_t)c->bw;
		h = cur.height - 2 * (int32_t)c->bw;
	}
	if (w > 0 && h > 0) {
		wlr_scene_buffer_set_dest_size(c->xwl_root_buffer, w, h);
		/* Restores full-buffer sampling and the origin; clears leftover clip
		 * state. */
		struct wlr_fbox full = {
			.x = 0,
			.y = 0,
			.width = buf->width,
			.height = buf->height,
		};
		wlr_scene_buffer_set_source_box(c->xwl_root_buffer, &full);
		wlr_scene_node_set_position(&c->xwl_root_buffer->node, 0, 0);
	}
#endif
}
uint32_t client_set_size(Client *c, uint32_t width, uint32_t height) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		struct wlr_surface_state *state = &surface->surface->current;

		/* Configure uses physical sizes (logical * xscale) so X11 renders 1:1.
		 */
		float xscale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;
		int32_t xw =
			(int32_t)roundf((c->geom.width - 2 * (int32_t)c->bw) * xscale);
		int32_t xh =
			(int32_t)roundf((c->geom.height - 2 * (int32_t)c->bw) * xscale);
		int32_t xx = (int32_t)roundf((c->geom.x + (int32_t)c->bw) * xscale);
		int32_t xy = (int32_t)roundf((c->geom.y + (int32_t)c->bw) * xscale);

		if ((int32_t)state->width == xw && (int32_t)state->height == xh &&
			(int32_t)c->surface.xwayland->x == xx &&
			(int32_t)c->surface.xwayland->y == xy) {
			return 0;
		}

		/*
		 * Until the client acks, state does not update; deduplicate using the
		 * already-requested parameters so identical configures are not resent,
		 * avoiding repeated client re-renders/ uploads.
		 */
		if (c->xwl_req_valid && c->xwl_req_x == xx && c->xwl_req_y == xy &&
			c->xwl_req_w == xw && c->xwl_req_h == xh) {
			return 0;
		}
		c->xwl_req_valid = true;
		c->xwl_req_x = xx;
		c->xwl_req_y = xy;
		c->xwl_req_w = xw;
		c->xwl_req_h = xh;

		xcb_size_hints_t *size_hints = surface->size_hints;
		int32_t width = xw;
		int32_t height = xh;

		if (size_hints && xw < (int32_t)size_hints->min_width)
			width = size_hints->min_width;
		if (size_hints && xh < (int32_t)size_hints->min_height)
			height = size_hints->min_height;

		wlr_xwayland_surface_configure(c->surface.xwayland, xx, xy, width,
									   height);
		return 1;
	}
#endif
	if ((int32_t)width == c->surface.xdg->toplevel->current.width &&
		(int32_t)height == c->surface.xdg->toplevel->current.height)
		return 0;

	return wlr_xdg_toplevel_set_size(c->surface.xdg->toplevel, (int32_t)width,
									 (int32_t)height);
}

void client_set_minimized(Client *c, bool minimize_window) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_set_minimized(c->surface.xwayland,
										   minimize_window);
		return;
	}
#endif

	return;
}

void client_set_maximized(Client *c, bool maximized) {
	struct wlr_xdg_toplevel *toplevel;

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_set_maximized(c->surface.xwayland, maximized,
										   maximized);
		return;
	}
#endif
	toplevel = c->surface.xdg->toplevel;
	wlr_xdg_toplevel_set_maximized(toplevel, maximized);
	return;
}

void client_set_tiled(Client *c, uint32_t edges) {
	struct wlr_xdg_toplevel *toplevel;
#ifdef XWAYLAND
	if (client_is_x11(c) && c->force_fakemaximize) {
		wlr_xwayland_surface_set_maximized(c->surface.xwayland,
										   edges != WLR_EDGE_NONE,
										   edges != WLR_EDGE_NONE);
		return;
	}
#endif

	toplevel = c->surface.xdg->toplevel;

	if (wl_resource_get_version(c->surface.xdg->toplevel->resource) >=
		XDG_TOPLEVEL_STATE_TILED_RIGHT_SINCE_VERSION) {
		wlr_xdg_toplevel_set_tiled(c->surface.xdg->toplevel, edges);
	}

	if (c->force_fakemaximize) {
		wlr_xdg_toplevel_set_maximized(toplevel, edges != WLR_EDGE_NONE);
	}
}

int32_t client_should_ignore_focus(Client *c) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;

		if (!surface->hints)
			return 0;

		return !surface->hints->input;
	}
#endif
	return 0;
}

int32_t client_is_x11_popup(Client *c) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		// Handles window types that do not need focus.
		const uint32_t no_focus_types[] = {
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_COMBO,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DND,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_MENU,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_NOTIFICATION,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_POPUP_MENU,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_SPLASH,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_TOOLTIP,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_UTILITY};
		// Checks whether the window type must block focus.
		for (size_t i = 0;
			 i < sizeof(no_focus_types) / sizeof(no_focus_types[0]); ++i) {
			if (wlr_xwayland_surface_has_window_type(surface,
													 no_focus_types[i])) {
				return 1;
			}
		}
	}
#endif
	return 0;
}

int32_t client_should_global(Client *c) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;

		if (surface->sticky)
			return 1;
	}
#endif
	return 0;
}

int32_t client_should_overtop(Client *c) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		if (surface->above)
			return 1;
	}
#endif
	return 0;
}

int32_t client_wants_focus(Client *c) {
#ifdef XWAYLAND
	return client_is_unmanaged(c) &&
		   wlr_xwayland_surface_override_redirect_wants_focus(
			   c->surface.xwayland) &&
		   wlr_xwayland_surface_icccm_input_model(c->surface.xwayland) !=
			   WLR_ICCCM_INPUT_MODEL_NONE;
#endif
	return 0;
}

int32_t client_wants_fullscreen(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->fullscreen;
#endif
	return c->surface.xdg->toplevel->requested.fullscreen;
}

bool client_request_minimize(Client *c, void *data) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_minimize_event *event = data;
		return event->minimize;
	}
#endif

	return c->surface.xdg->toplevel->requested.minimized;
}

bool client_request_maximize(Client *c, void *data) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		return surface->maximized_vert || surface->maximized_horz;
	}
#endif

	return c->surface.xdg->toplevel->requested.maximized;
}

void client_set_size_bound(Client *c) {
	struct wlr_xdg_toplevel *toplevel;
	struct wlr_xdg_toplevel_state state;

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		xcb_size_hints_t *size_hints = surface->size_hints;

		if (!size_hints)
			return;

		/* size_hints are X11 physical sizes; convert back to logical before
		 * comparing. */
		float scale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;
		int32_t min_w = (int32_t)roundf(size_hints->min_width / scale);
		int32_t min_h = (int32_t)roundf(size_hints->min_height / scale);
		int32_t max_w = (int32_t)roundf(size_hints->max_width / scale);
		int32_t max_h = (int32_t)roundf(size_hints->max_height / scale);

		if ((uint32_t)c->geom.width - 2 * c->bw < (uint32_t)min_w && min_w > 0)
			c->geom.width = min_w + 2 * c->bw;
		if ((uint32_t)c->geom.height - 2 * c->bw < (uint32_t)min_h && min_h > 0)
			c->geom.height = min_h + 2 * c->bw;
		if ((uint32_t)c->geom.width - 2 * c->bw > (uint32_t)max_w && max_w > 0)
			c->geom.width = max_w + 2 * c->bw;
		if ((uint32_t)c->geom.height - 2 * c->bw > (uint32_t)max_h && max_h > 0)
			c->geom.height = max_h + 2 * c->bw;
		return;
	}
#endif

	toplevel = c->surface.xdg->toplevel;
	state = toplevel->current;
	if ((uint32_t)c->geom.width - 2 * c->bw < state.min_width &&
		state.min_width > 0) {
		c->geom.width = state.min_width + 2 * c->bw;
	}
	if ((uint32_t)c->geom.height - 2 * c->bw < state.min_height &&
		state.min_height > 0) {
		c->geom.height = state.min_height + 2 * c->bw;
	}
	if ((uint32_t)c->geom.width - 2 * c->bw > state.max_width &&
		state.max_width > 0) {
		c->geom.width = state.max_width + 2 * c->bw;
	}
	if ((uint32_t)c->geom.height - 2 * c->bw > state.max_height &&
		state.max_height > 0) {
		c->geom.height = state.max_height + 2 * c->bw;
	}
}

bool check_hit_no_border(Client *c) {
	bool hit_no_border = false;

	if (!c->mon)
		return false;

	if (c->tags <= 0)
		return false;

	if (!server.render_border) {
		hit_no_border = true;
	}

	if (c->mon && !c->mon->isoverview &&
		c->mon->pertag->no_render_border[get_client_tag_idx(c)]) {
		hit_no_border = true;
	}

	if (config.no_border_when_single && c && c->mon &&
		((ISSCROLLTILED(c) && c->mon->visible_fake_tiling_clients == 1) ||
		 c->mon->visible_clients == 1)) {
		hit_no_border = true;
	}
	return hit_no_border;
}

Client *client_find_terminal(Client *w) {
	Client *c = NULL;

	if (!w->pid || w->isterm || w->noswallow)
		return NULL;

	wl_list_for_each(c, &server.focus_stack, flink) {
		if (c->isterm && !c->swallowdby && c->pid &&
			is_descendant_process(c->pid, w->pid)) {
			return c;
		}
	}

	return NULL;
}

Client *get_client_by_id_or_title(const char *arg_id, const char *arg_title) {
	Client *target_client = NULL;
	const char *appid, *title;
	Client *c = NULL;
	wl_list_for_each(c, &server.clients, link) {
		if (!config.scratchpad_cross_monitor &&
			c->mon != server.selected_monitor) {
			continue;
		}

		if (c->swallowing) {
			appid = client_get_appid(c->swallowing);
			title = client_get_title(c->swallowing);
		} else {
			appid = client_get_appid(c);
			title = client_get_title(c);
		}

		if (!appid) {
			appid = broken;
		}

		if (!title) {
			title = broken;
		}

		if (arg_id && strncmp(arg_id, "none", 4) == 0)
			arg_id = NULL;

		if (arg_title && strncmp(arg_title, "none", 4) == 0)
			arg_title = NULL;

		if ((arg_title && regex_match(arg_title, title) && !arg_id) ||
			(arg_id && regex_match(arg_id, appid) && !arg_title) ||
			(arg_id && regex_match(arg_id, appid) && arg_title &&
			 regex_match(arg_title, title))) {
			target_client = c;
			break;
		}
	}
	return target_client;
}

struct wlr_box // Computes the centered coordinates of the client.
client_center_geometry(Client *c, Monitor *tm, struct wlr_box geom,
					   int32_t offsetx, int32_t offsety) {
	struct wlr_box tempbox;
	int32_t offset = 0;
	int32_t len = 0;
	Monitor *m = tm ? tm : server.selected_monitor;

	if (!m)
		return geom;

	uint32_t cbw = c && check_hit_no_border(c) ? c->bw : 0;

	if ((!c || !c->no_force_center) && m) {
		tempbox.x = m->w.x + (m->w.width - geom.width) / 2;
		tempbox.y = m->w.y + (m->w.height - geom.height) / 2;
	} else {
		tempbox.x = geom.x;
		tempbox.y = geom.y;
	}

	tempbox.width = geom.width;
	tempbox.height = geom.height;

	if (offsetx != 0) {
		len = (m->w.width - tempbox.width - 2 * m->gappoh) / 2;
		offset = len * (offsetx / 100.0);
		tempbox.x += offset;

		// Keeps the window inside the screen.
		if (tempbox.x < m->m.x) {
			tempbox.x = m->m.x - cbw;
		}
		if (tempbox.x + tempbox.width > m->m.x + m->m.width) {
			tempbox.x = m->m.x + m->m.width - tempbox.width + cbw;
		}
	}
	if (offsety != 0) {
		len = (m->w.height - tempbox.height - 2 * m->gappov) / 2;
		offset = len * (offsety / 100.0);
		tempbox.y += offset;

		// Keeps the window inside the screen.
		if (tempbox.y < m->m.y) {
			tempbox.y = m->m.y - cbw;
		}
		if (tempbox.y + tempbox.height > m->m.y + m->m.height) {
			tempbox.y = m->m.y + m->m.height - tempbox.height + cbw;
		}
	}

	return tempbox;
}

/* Helper: Check if rule matches client */
bool is_window_rule_matches(const ConfigWinRule *r, const char *appid,
							const char *title) {
	return (r->title && regex_match(r->title, title) && !r->id) ||
		   (r->id && regex_match(r->id, appid) && !r->title) ||
		   (r->id && regex_match(r->id, appid) && r->title &&
			regex_match(r->title, title));
}

Client *center_tiled_select(Monitor *m) {
	Client *c = NULL;
	Client *target_c = NULL;
	int64_t mini_distance = -1;
	int32_t dirx, diry;
	int64_t distance;
	wl_list_for_each(c, &server.clients, link) {
		if (c && VISIBLEON(c, m) && ISSCROLLTILED(c) &&
			client_surface(c)->mapped && !c->isfloating &&
			!client_is_unmanaged(c)) {
			dirx = c->geom.x + c->geom.width / 2 - (m->w.x + m->w.width / 2);
			diry = c->geom.y + c->geom.height / 2 - (m->w.y + m->w.height / 2);
			distance = dirx * dirx + diry * diry;
			if (distance < mini_distance || mini_distance == -1) {
				mini_distance = distance;
				target_c = c;
			}
		}
	}
	return target_c;
}

Client *find_client_by_direction(Client *tc, const Arg *arg,
								 bool findfloating) {
	Client *c = NULL;
	Client *tempFocusClients = NULL;
	Client *tempSameMonitorFocusClients = NULL;
	int64_t distance = LLONG_MAX;
	int64_t same_monitor_distance = LLONG_MAX;

	int32_t tc_l = tc->geom.x;
	int32_t tc_r = tc->geom.x + tc->geom.width;
	int32_t tc_t = tc->geom.y;
	int32_t tc_b = tc->geom.y + tc->geom.height;
	int32_t tc_cx = tc_l + tc->geom.width / 2;
	int32_t tc_cy = tc_t + tc->geom.height / 2;

	for (int32_t step = 0; step < 2; step++) {
		if (step == 1 && tempFocusClients)
			break;

		wl_list_for_each(c, &server.clients, link) {
			if (!c || !c->mon || c == tc)
				continue;
			if (!findfloating && c->isfloating)
				continue;
			if (!VISIBLEON(c, c->mon))
				continue;
			if (c->isunglobal)
				continue;
			if (!config.focus_cross_monitor && c->mon != tc->mon)
				continue;
			if (!(c->tags & c->mon->tagset[c->mon->seltags]) && !c->isglobal)
				continue;

			int32_t c_l = c->geom.x;
			int32_t c_r = c->geom.x + c->geom.width;
			int32_t c_t = c->geom.y;
			int32_t c_b = c->geom.y + c->geom.height;
			int32_t c_cx = c_l + c->geom.width / 2;
			int32_t c_cy = c_t + c->geom.height / 2;

			int64_t main_dist = 0;
			int64_t orth_dist = 0;
			bool match_dir = false;

			switch (arg->i) {
			case LEFT:
				if (c_cx < tc_cx || (c_cx == tc_cx && c_l < tc_l)) {
					match_dir = true;
					main_dist = tc_l - c_r;
					orth_dist = (c_b < tc_t)
									? (tc_t - c_b)
									: ((c_t > tc_b) ? (c_t - tc_b) : 0);
				}
				break;
			case RIGHT:
				if (c_cx > tc_cx || (c_cx == tc_cx && c_l > tc_l)) {
					match_dir = true;
					main_dist = c_l - tc_r;
					orth_dist = (c_b < tc_t)
									? (tc_t - c_b)
									: ((c_t > tc_b) ? (c_t - tc_b) : 0);
				}
				break;
			case UP:
				if (c_cy < tc_cy || (c_cy == tc_cy && c_t < tc_t)) {
					match_dir = true;
					main_dist = tc_t - c_b;
					orth_dist = (c_r < tc_l)
									? (tc_l - c_r)
									: ((c_l > tc_r) ? (c_l - tc_r) : 0);
				}
				break;
			case DOWN:
				if (c_cy > tc_cy || (c_cy == tc_cy && c_t > tc_t)) {
					match_dir = true;
					main_dist = c_t - tc_b;
					orth_dist = (c_r < tc_l)
									? (tc_l - c_r)
									: ((c_l > tc_r) ? (c_l - tc_r) : 0);
				}
				break;
			default:
				continue;
			}

			if (!match_dir)
				continue;

			/*
			 * When focusdir_only_zone_overlap is enabled, directional focus
			 * requires the target window to overlap the current window on the
			 * orthogonal axis.
			 */
			if (config.focusdir_only_zone_overlap && orth_dist != 0)
				continue;

			if (step == 0) {
				if (!tc->mon || c->mon != tc->mon)
					continue;
				if (!tc->mon->isoverview &&
					!client_is_in_same_stack(tc, c, NULL))
					continue;
			}

			int64_t penalty = 0;
			if (main_dist < 0) {
				penalty = 10000000000LL;
				main_dist = -main_dist;
			}

			int64_t tmp_distance =
				penalty + (main_dist * main_dist) + (orth_dist * orth_dist);

			if (tmp_distance < distance) {
				distance = tmp_distance;
				tempFocusClients = c;
			}
			if (c->mon == tc->mon && tmp_distance < same_monitor_distance) {
				same_monitor_distance = tmp_distance;
				tempSameMonitorFocusClients = c;
			}
		}
	}

	if (tempSameMonitorFocusClients)
		return tempSameMonitorFocusClients;
	return tempFocusClients;
}

Client *direction_select(const Arg *arg) {

	Client *tc = arg->tc ? arg->tc : server.selected_monitor->sel;

	if (!tc)
		return NULL;

	if (tc && (tc->isfullscreen || tc->ismaximizescreen) &&
		(!is_scroller_layout(server.selected_monitor) || tc->isfloating)) {
		return NULL;
	}

	return find_client_by_direction(tc, arg, true);
}

/* We probably should change the name of this, it sounds like
 * will focus the topmost client of this mon, when actually will
 * only return that client */
Client *client_focus_top(Monitor *m) {
	Client *c = NULL;
	wl_list_for_each(c, &server.focus_stack, flink) {
		if (c->iskilling || c->isunglobal)
			continue;
		if (VISIBLEON(c, m))
			return c;
	}
	return NULL;
}

Client *get_next_stack_client(Client *c, bool reverse) {
	if (!c || !c->mon)
		return NULL;

	Client *next = NULL;
	if (reverse) {
		wl_list_for_each_reverse(next, &c->link, link) {
			if (&next->link == &server.clients)
				continue; /* wrap past the sentinel node */

			if (next->isunglobal)
				continue;

			if (next != c && next->mon && VISIBLEON(next, c->mon))
				return next;
		}
	} else {
		wl_list_for_each(next, &c->link, link) {
			if (&next->link == &server.clients)
				continue; /* wrap past the sentinel node */

			if (next->isunglobal)
				continue;

			if (next != c && next->mon && VISIBLEON(next, c->mon))
				return next;
		}
	}
	return NULL;
}

float *get_border_color(Client *c) {

	if (c->mon != server.selected_monitor) {
		return config.bordercolor;
	} else if (c->isurgent) {
		return config.urgentcolor;
	} else if (c->is_in_scratchpad && server.selected_monitor &&
			   c == server.selected_monitor->sel) {
		return config.scratchpadcolor;
	} else if (c->isglobal && server.selected_monitor &&
			   c == server.selected_monitor->sel) {
		return config.globalcolor;
	} else if (c->isoverlay && server.selected_monitor &&
			   c == server.selected_monitor->sel) {
		return config.overlaycolor;
	} else if (c->ismaximizescreen && server.selected_monitor &&
			   c == server.selected_monitor->sel) {
		return config.maximizescreencolor;
	} else if (server.selected_monitor && c == server.selected_monitor->sel) {
		return config.focuscolor;
	} else {
		return config.bordercolor;
	}
}

int32_t is_single_bit_set(uint32_t x) { return x && !(x & (x - 1)); }

bool client_only_in_one_tag(Client *c) {
	if (c && (c->tags & TAG0_MASK))
		return true;
	uint32_t masked = c->tags & TAGMASK;
	if (is_single_bit_set(masked)) {
		return true;
	} else {
		return false;
	}
}

bool client_is_in_same_stack(Client *sc, Client *tc, Client *fc) {
	if (!sc || !tc || !sc->mon)
		return false;

	uint32_t id = sc->mon->pertag->ltidxs[get_client_tag_idx(sc)]->id;

	if ((id != SCROLLER && id != VERTICAL_SCROLLER) &&
		tc->mon != server.selected_monitor &&
		(tc->isfullscreen || tc->ismaximizescreen))
		return true;

	if (id == MONOCLE) {
		return true;
	}

	if (id == SCROLLER || id == VERTICAL_SCROLLER) {
		Client *source_stack_head = scroll_get_stack_head_client(sc);
		Client *target_stack_head = scroll_get_stack_head_client(tc);
		Client *fc_head = fc ? scroll_get_stack_head_client(fc) : NULL;

		if (fc && fc_head == source_stack_head)
			return false;
		if (source_stack_head == target_stack_head)
			return true;
		else
			return false;
	}

	if (id == TILE || id == VERTICAL_TILE || id == DECK ||
		id == VERTICAL_DECK || id == RIGHT_TILE) {
		if (tc->ismaster ^ sc->ismaster)
			return false;
		if (fc && !(fc->ismaster ^ sc->ismaster))
			return false;
		else
			return true;
	}

	if (id == CENTER_TILE) {
		if (tc->ismaster ^ sc->ismaster)
			return false;
		if (fc && !(fc->ismaster ^ sc->ismaster))
			return false;
		if (sc->geom.x == tc->geom.x)
			return true;
		else
			return false;
	}

	return false;
}

Client *get_focused_stack_client(Client *sc, Client *custom_focus_client) {
	if (!sc || sc->isfloating || !server.selected_monitor)
		return sc;

	Client *tc = NULL;
	Client *fc = custom_focus_client ? custom_focus_client
									 : server.selected_monitor->sel;

	if (fc->isfloating || sc->isfloating)
		return sc;

	wl_list_for_each(tc, &server.focus_stack, flink) {
		if (tc->iskilling || tc->isunglobal)
			continue;
		if (!VISIBLEON(tc, sc->mon))
			continue;
		if (tc == fc)
			continue;

		if (client_is_in_same_stack(sc, tc, fc)) {
			return tc;
		}
	}
	return sc;
}

void apply_rule_properties(Client *c, const ConfigWinRule *r) {
	APPLY_INT_PROP(c, r, isterm);
	APPLY_INT_PROP(c, r, allow_csd);
	APPLY_INT_PROP(c, r, force_fakemaximize);
	APPLY_INT_PROP(c, r, force_tiled_state);
	APPLY_INT_PROP(c, r, force_tearing);
	APPLY_INT_PROP(c, r, noswallow);
	APPLY_INT_PROP(c, r, nofocus);
	APPLY_INT_PROP(c, r, nofadein);
	APPLY_INT_PROP(c, r, nofadeout);
	APPLY_INT_PROP(c, r, no_force_center);
	APPLY_INT_PROP(c, r, isfloating);
	APPLY_INT_PROP(c, r, isfullscreen);
	APPLY_INT_PROP(c, r, isfakefullscreen);
	APPLY_INT_PROP(c, r, isnoborder);
	APPLY_INT_PROP(c, r, isnoshadow);
	APPLY_INT_PROP(c, r, isnoradius);
	APPLY_INT_PROP(c, r, isnoanimation);
	APPLY_INT_PROP(c, r, isopensilent);
	APPLY_INT_PROP(c, r, istagsilent);
	APPLY_INT_PROP(c, r, isnamedscratchpad);
	APPLY_INT_PROP(c, r, isglobal);
	APPLY_INT_PROP(c, r, isoverlay);
	APPLY_INT_PROP(c, r, shield_when_capture);
	APPLY_INT_PROP(c, r, ignore_maximize);
	APPLY_INT_PROP(c, r, ignore_minimize);
	APPLY_INT_PROP(c, r, isnosizehint);
	APPLY_INT_PROP(c, r, idleinhibit_when_focus);
	APPLY_INT_PROP(c, r, vrr_only_fullscreen);
	APPLY_INT_PROP(c, r, force_render);
	APPLY_INT_PROP(c, r, activation_bypass);
	APPLY_INT_PROP(c, r, isunglobal);
	APPLY_INT_PROP(c, r, noblur);
	APPLY_INT_PROP(c, r, allow_shortcuts_inhibit);

	APPLY_FLOAT_PROP(c, r, scroller_proportion);
	APPLY_FLOAT_PROP(c, r, scroller_proportion_single);
	APPLY_FLOAT_PROP(c, r, focused_opacity);
	APPLY_FLOAT_PROP(c, r, unfocused_opacity);

	APPLY_STRING_PROP(c, r, animation_type_open);
	APPLY_STRING_PROP(c, r, animation_type_close);
}
void set_float_malposition(Client *tc) {
	Client *c = NULL;
	int32_t x, y, offset, xreverse, yreverse;
	x = tc->geom.x;
	y = tc->geom.y;
	xreverse = 1;
	yreverse = 1;

	if (!tc || !tc->mon)
		return;

	offset = MANGO_MIN(tc->mon->w.width / 20, tc->mon->w.height / 20);

	wl_list_for_each(c, &server.clients, link) {
		if (c->isfloating && c != tc && VISIBLEON(c, tc->mon) &&
			abs(x - c->geom.x) < offset && abs(y - c->geom.y) < offset) {

			x = c->geom.x + offset * xreverse;
			y = c->geom.y + offset * yreverse;
			if (x < tc->mon->w.x) {
				x = x + offset;
				xreverse = 1;
			}

			if (y < tc->mon->w.y) {
				y = y + offset;
				yreverse = 1;
			}

			if (x + tc->geom.width > tc->mon->w.x + tc->mon->w.width) {
				x = x - offset;
				xreverse = -1;
			}

			if (y + tc->geom.height > tc->mon->w.y + tc->mon->w.height) {
				y = y - offset;
				yreverse = -1;
			}
		}
	}

	tc->float_geom.x = tc->geom.x = x;
	tc->float_geom.y = tc->geom.y = y;
}

void client_reset_mon_tags(Client *c, Monitor *mon, uint32_t newtags) {
	if (!newtags && mon && !mon->isoverview) {
		c->tags = mon->tagset[mon->seltags];
	} else if (!newtags && mon && mon->isoverview) {
		c->tags = mon->ovbk_current_tagset;
	} else if (newtags) {
		uint32_t masked =
			(newtags & TAG0_MASK) ? TAG0_MASK : (newtags & TAGMASK);
		c->tags = masked ? masked : mon->tagset[mon->seltags];
	} else {
		c->tags = mon->tagset[mon->seltags];
	}
}

void check_match_tag_floating_rule(Client *c, Monitor *mon) {
	if (c->tags && !c->isfloating && mon && !c->swallowing &&
		mon->pertag->open_as_floating[get_tags_first_tag_num(c->tags)]) {
		c->isfloating = 1;
	}
}

void client_apply_rules(Client *c) {
	/* rule matching */
	const char *appid, *title;
	uint32_t i, newtags = 0;
	const ConfigWinRule *r;
	Monitor *m = NULL;
	Client *fc = NULL;
	Client *parent = NULL;

	if (!c)
		return;

	parent = client_get_parent(c);

	Monitor *mon =
		parent && parent->mon ? parent->mon : server.selected_monitor;

	c->isfloating = client_is_float_type(c) || parent;

	client_update_geometry(c);

	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (i = 0; i < config.window_rules_count; i++) {

		r = &config.window_rules[i];

		// rule matching
		if (!is_window_rule_matches(r, appid, title))
			continue;

		// set general properties
		apply_rule_properties(c, r);

		// // set tags
		if (r->tags) {
			newtags |= r->tags;
		} else if (parent) {
			newtags = parent->tags;
		}

		// set monitor of client
		wl_list_for_each(m, &server.monitors, link) {
			if (match_monitor_spec(r->monitor, m)) {
				mon = m;
			}
		}

		if (c->isnamedscratchpad) {
			c->isfloating = 1;
		}

		if (r->scroller_proportion > 0.0f) {
			c->iscustom_scroller_proportion = 1;
		}

		if (r->scroller_proportion_single > 0.0f) {
			c->iscustom_scroller_proportion_single = 1;
		}

		// set geometry of floating client

		if (r->width > 1)
			c->float_geom.width = r->width;
		else if (r->width > 0 && r->width <= 1)
			c->float_geom.width = round(mon->m.width * r->width);
		if (r->height > 1)
			c->float_geom.height = r->height;
		else if (r->height > 0 && r->height <= 1)
			c->float_geom.height = round(mon->m.height * r->height);

		if (r->width > 0 || r->height > 0) {
			c->iscustomsize = 1;
		}

		if (r->offsetx || r->offsety) {
			c->iscustompos = 1;
			c->float_geom = c->geom = client_center_geometry(
				c, mon, c->float_geom, r->offsetx, r->offsety);
		}
		if (c->isfloating) {
			c->geom = c->float_geom.width > 0 && c->float_geom.height > 0
						  ? c->float_geom
						  : c->geom;
			if (!c->isnosizehint)
				client_set_size_bound(c);
		}
	}

	if (newtags == 0 && parent && (parent->tags & TAG0_MASK)) {
		newtags = TAG0_MASK;
	} else if (newtags == 0 && is_special_active(mon)) {
		newtags = TAG0_MASK;
	}

	if (mon)
		set_size_per(mon, c);

	// if no geom rule hit and is normal winodw, use the center pos and record
	// the hit size
	if (!c->iscustompos &&
		(!client_is_x11(c) || (c->geom.x == 0 && c->geom.y == 0))) {
		struct wlr_box pending_center_geom =
			c->iscustomsize ? c->float_geom : c->geom;
		c->float_geom = c->geom =
			client_center_geometry(c, mon, pending_center_geom, 0, 0);
	} else if (!c->iscustomsize) {
		c->float_geom = c->geom;
	}

	/*-----------------------apply rule action-------------------------*/

	// rule action only apply after map not apply in the init commit
	struct wlr_surface *surface = client_surface(c);
	if (!surface || !surface->mapped)
		return;

	// apply swallow rule
	c->pid = client_get_pid(c);
	if (!c->noswallow && !c->isfloating && !client_is_float_type(c) &&
		!c->surface.xdg->initial_commit) {
		Client *p = client_find_terminal(c);
		if (p && !p->isminimized) {
			c->swallowing = p;
			p->swallowdby = c;

			client_replace(c, p, false, true);

			mon = p->mon;
			newtags = p->tags;
		}
	}

	int32_t fullscreen_state_backup =
		c->isfullscreen || client_wants_fullscreen(c);

	bool should_init_get_focus =
		!c->isopensilent &&
		!(client_is_x11_popup(c) && client_should_ignore_focus(c)) && mon &&
		(!c->istagsilent || !newtags || (newtags & mon->tagset[mon->seltags]));

	if (!should_init_get_focus) {
		wl_list_safe_reinsert_prev(&server.focus_stack, &c->flink);
	}

	client_set_monitor(c, mon, newtags, should_init_get_focus);
	client_reparent_group(c);

	if (!c->isfloating) {
		c->old_stack_inner_per = c->stack_inner_per;
		c->old_master_inner_per = c->master_inner_per;
	}

	if (c->mon &&
		!(c->mon == server.selected_monitor &&
		  c->tags & c->mon->tagset[c->mon->seltags]) &&
		!c->isopensilent && !c->istagsilent) {
		c->animation.tag_from_rule = true;
		client_view_on_monitor(&(Arg){.ui = c->tags}, true, c->mon, true);
	}

	client_apply_fullscreen(c, fullscreen_state_backup, true);

	if (c->isfakefullscreen) {
		client_set_fake_fullscreen(c, 1);
	}

	/*
	if there is a new non-floating window in the current tag, the fullscreen
	window in the current tag will exit fullscreen and participate in tiling
	 */
	wl_list_for_each(fc, &server.clients,
					 link) if (fc && fc != c && c->tags & fc->tags && c->mon &&
							   VISIBLEON(fc, c->mon) && ISFULLSCREEN(fc) &&
							   !c->isfloating) {
		clear_fullscreen_flag(fc);
		arrange(c->mon, false, false);
	}

	if (c->isfloating && !c->iscustompos && !c->isnamedscratchpad) {
		wl_list_safe_reinsert_prev(&server.clients, &c->link);
		set_float_malposition(c);
	}

	// apply named scratchpad rule
	if (c->isnamedscratchpad) {
		apply_named_scratchpad(c);
	}

	// apply overlay rule
	if (c->isoverlay && c->scene) {
		wlr_scene_node_reparent(&c->scene->node, server.layers[LyrOverlay]);
		wlr_scene_node_raise_to_top(&c->scene->node);
	}
}

void apply_window_snap(Client *c) {
	int32_t snap_up = 99999, snap_down = 99999, snap_left = 99999,
			snap_right = 99999;
	int32_t snap_up_temp = 0, snap_down_temp = 0, snap_left_temp = 0,
			snap_right_temp = 0;
	int32_t snap_up_screen = 0, snap_down_screen = 0, snap_left_screen = 0,
			snap_right_screen = 0;
	int32_t snap_up_mon = 0, snap_down_mon = 0, snap_left_mon = 0,
			snap_right_mon = 0;

	uint32_t cbw = !server.render_border || c->fake_no_border ? c->bw : 0;
	uint32_t tcbw;
	uint32_t cx, cy, cw, ch, tcx, tcy, tcw, tch;
	cx = c->geom.x + cbw;
	cy = c->geom.y + cbw;
	cw = c->geom.width - 2 * cbw;
	ch = c->geom.height - 2 * cbw;

	Client *tc = NULL;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (!c->isfloating || !config.enable_floating_snap)
		return;

	wl_list_for_each(tc, &server.clients, link) {
		if (tc && tc->isfloating && !tc->iskilling &&
			client_surface(tc)->mapped && VISIBLEON(tc, c->mon)) {

			tcbw = !server.render_border || tc->fake_no_border ? tc->bw : 0;
			tcx = tc->geom.x + tcbw;
			tcy = tc->geom.y + tcbw;
			tcw = tc->geom.width - 2 * tcbw;
			tch = tc->geom.height - 2 * tcbw;

			snap_left_temp = cx - tcx - tcw;
			snap_right_temp = tcx - cx - cw;
			snap_up_temp = cy - tcy - tch;
			snap_down_temp = tcy - cy - ch;

			if (snap_left_temp < snap_left && snap_left_temp >= 0) {
				snap_left = snap_left_temp;
			}
			if (snap_right_temp < snap_right && snap_right_temp >= 0) {
				snap_right = snap_right_temp;
			}
			if (snap_up_temp < snap_up && snap_up_temp >= 0) {
				snap_up = snap_up_temp;
			}
			if (snap_down_temp < snap_down && snap_down_temp >= 0) {
				snap_down = snap_down_temp;
			}
		}
	}

	snap_left_mon = cx - c->mon->m.x;
	snap_right_mon = c->mon->m.x + c->mon->m.width - cx - cw;
	snap_up_mon = cy - c->mon->m.y;
	snap_down_mon = c->mon->m.y + c->mon->m.height - cy - ch;

	if (snap_up_mon >= 0 && snap_up_mon < snap_up)
		snap_up = snap_up_mon;
	if (snap_down_mon >= 0 && snap_down_mon < snap_down)
		snap_down = snap_down_mon;
	if (snap_left_mon >= 0 && snap_left_mon < snap_left)
		snap_left = snap_left_mon;
	if (snap_right_mon >= 0 && snap_right_mon < snap_right)
		snap_right = snap_right_mon;

	snap_left_screen = cx - c->mon->w.x;
	snap_right_screen = c->mon->w.x + c->mon->w.width - cx - cw;
	snap_up_screen = cy - c->mon->w.y;
	snap_down_screen = c->mon->w.y + c->mon->w.height - cy - ch;

	if (snap_up_screen >= 0 && snap_up_screen < snap_up)
		snap_up = snap_up_screen;
	if (snap_down_screen >= 0 && snap_down_screen < snap_down)
		snap_down = snap_down_screen;
	if (snap_left_screen >= 0 && snap_left_screen < snap_left)
		snap_left = snap_left_screen;
	if (snap_right_screen >= 0 && snap_right_screen < snap_right)
		snap_right = snap_right_screen;

	if (snap_left < snap_right && snap_left < config.snap_distance) {
		c->geom.x = c->geom.x - snap_left;
	}

	if (snap_right <= snap_left && snap_right < config.snap_distance) {
		c->geom.x = c->geom.x + snap_right;
	}

	if (snap_up < snap_down && snap_up < config.snap_distance) {
		c->geom.y = c->geom.y - snap_up;
	}

	if (snap_down <= snap_up && snap_down < config.snap_distance) {
		c->geom.y = c->geom.y + snap_down;
	}

	c->float_geom = c->geom;
	resize(c, c->geom, 0);
}
/*
 * Client management: window lifecycle, rules, focus, tiled/floating/fullscreen
 * state switching, and XWayland client handling.
 */
void client_update_geometry(Client *c) {
	if (client_is_x11(c)) {
#ifdef XWAYLAND
		/* Resolve xwayland_scale before reading geometry; otherwise physical
		 * sizes are returned. */
		xwayland_apply_scale(c);
		client_get_geometry(c, &c->geom);
		if (c->isfloating) {
			fix_xwayland_coordinate(&c->geom);
			c->float_geom = c->geom;
		}
#endif
	}
}

void client_init_xwayland(Client *c) {
	if (client_is_x11(c)) {
#ifdef XWAYLAND
		/* Records the XWayland root buffer node. */
		struct wlr_scene_node *child;
		wl_list_for_each(child, &c->scene_surface->children, link) {
			if (child->type != WLR_SCENE_NODE_BUFFER)
				continue;
			struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(child);
			if (wlr_scene_surface_try_from_buffer(buffer)) {
				c->xwl_root_buffer = buffer;
				/* Scene hit-testing must convert logical coordinates to X11
				 * physical coordinates. */
				c->xwl_root_buffer->point_accepts_input =
					xwayland_scene_buffer_point_accepts_input;
				break;
			}
		}
		/* After scene processing, force the root surface to display its logical
		 * size. */
		LISTEN(&client_surface(c)->events.commit, &c->commmitx11,
			   handle_xwayland_surface_commit);
#endif
	}
}

bool client_init_unmanaged(Client *c) {
	if (client_is_unmanaged(c)) {
#ifdef XWAYLAND
		/* Unmanaged clients always are floating */
		xwayland_apply_scale(c);
		/* After applying the scale, recompute c->geom (logical size). */
		client_get_geometry(c, &c->geom);
		struct wlr_box geo = c->geom;
		fix_xwayland_coordinate(&geo);
		struct wlr_box xgeo = geo;
		xwayland_logical_to_x11(&xgeo, c->xwayland_scale);
		wlr_scene_node_set_position(&c->scene->node, geo.x, geo.y);
		wlr_xwayland_surface_configure(c->surface.xwayland, xgeo.x, xgeo.y,
									   xgeo.width, xgeo.height);
		/*
		 * Set dest_size immediately from the buffer actual size (logical =
		 * buffer/scale) so the first frame does not show at physical size and
		 * get scaled before a commit corrects it.
		 */
		client_update_xwayland_dest_size(c);
		LISTEN(&c->surface.xwayland->events.set_geometry, &c->set_geometry,
			   handle_xwayland_surface_set_geometry);
		wlr_scene_node_reparent(&c->scene->node, server.layers[LyrOverlay]);
		if (client_wants_focus(c)) {
			client_focus(c, 1);
			server.exclusive_focus = c;
		}
		return true;
#endif
	}
	return false;
}

void client_apply_xwayland(Client *c) {
	if (client_is_x11(c)) {
#ifdef XWAYLAND
		/* c->mon is only determined after applyrules/setmon; apply XWayland
		 * scaling here. */
		xwayland_apply_scale(c);
		c->overview_backup_geom = c->geom;
#endif
	}
}
bool xwayland_scene_buffer_point_accepts_input(struct wlr_scene_buffer *buffer,
											   double *sx, double *sy) {
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface)
		return false;

	double tx = *sx, ty = *sy;
	struct wlr_scene_node *node = &buffer->node;
	while (node && !node->data)
		node = node->parent ? &node->parent->node : NULL;
	Client *c = node ? node->data : NULL;
	if (c && client_is_x11(c)) {
#ifdef XWAYLAND
		if (config.xwayland_ignore_scale && c->xwayland_scale > 0.f) {
			tx *= c->xwayland_scale;
			ty *= c->xwayland_scale;
		}
#endif
	}
	return wlr_surface_point_accepts_input(scene_surface->surface, tx, ty);
}

// fix for 0.5
void handle_new_xdg_toplevel(struct wl_listener *listener, void *data) {
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup,
	 * or when wlr_layer_shell receives a new popup from a layer.
	 * If you want to do something tricky with popups you should check if
	 * its parent is wlr_xdg_shell or wlr_layer_shell */
	struct wlr_xdg_toplevel *toplevel = data;
	Client *c = NULL;

	/* Allocate a Client for this surface */
	c = toplevel->base->data = ecalloc(1, sizeof(*c));
	c->surface.xdg = toplevel->base;
	c->bw = config.borderpx;

	LISTEN(&toplevel->base->surface->events.commit, &c->commit,
		   handle_client_commit);
	LISTEN(&toplevel->base->surface->events.map, &c->map, handle_client_map);
	LISTEN(&toplevel->base->surface->events.unmap, &c->unmap,
		   handle_client_unmap);
	LISTEN(&toplevel->events.destroy, &c->destroy, handle_client_destroy);
	LISTEN(&toplevel->events.request_fullscreen, &c->fullscreen,
		   handle_client_request_fullscreen);
	LISTEN(&toplevel->events.request_maximize, &c->maximize,
		   handle_client_request_maximize);
	LISTEN(&toplevel->events.request_minimize, &c->minimize,
		   handle_client_request_minimize);
	LISTEN(&toplevel->events.set_title, &c->set_title, handle_client_set_title);
}

void init_client_properties(Client *c) {
#ifdef XWAYLAND
	c->xwl_req_valid = false;
	c->xwl_req_x = 0;
	c->xwl_req_y = 0;
	c->xwl_req_w = 0;
	c->xwl_req_h = 0;
#endif
	c->blur_opacity = 1.0f;
	c->is_logic_hide = false;
	c->isgroupfocusing = false;
	c->group_prev = NULL;
	c->group_next = NULL;
	c->grid_col_per = 1.0f;
	c->grid_row_per = 1.0f;
	c->jump_label_node = NULL;
	c->group_bar = NULL;
	c->overview_scene_surface = NULL;
	c->drop_direction = UNDIR;
	c->enable_drop_area_draw = false;
	c->isfocusing = false;
	c->isfloating = 0;
	c->isfakefullscreen = 0;
	c->isnoanimation = 0;
	c->isopensilent = 0;
	c->istagsilent = 0;
	c->noswallow = 0;
	c->isterm = 0;
	c->noblur = 0;
	c->tearing_hint = 0;
	c->overview_isfullscreenbak = 0;
	c->overview_ismaximizescreenbak = 0;
	c->overview_isfloatingbak = 0;
	c->pid = 0;
	c->swallowdby = NULL;
	c->swallowing = NULL;
	c->ismaster = 0;
	c->old_ismaster = 0;
	c->isleftstack = 0;
	c->ismaximizescreen = 0;
	c->isfullscreen = 0;
	c->need_float_size_reduce = 0;
	c->iskilling = 0;
	c->isglobal = 0;
	c->isminimized = 0;
	c->isoverlay = 0;
	c->isunglobal = 0;
	c->is_in_scratchpad = 0;
	c->isnamedscratchpad = 0;
	c->is_scratchpad_show = 0;
	c->need_float_size_reduce = 0;
	c->is_clip_to_hide = 0;
	c->is_restoring_from_ov = 0;
	c->isurgent = 0;
	c->need_output_flush = 0;
	c->scroller_proportion = config.scroller_default_proportion;
	c->is_pending_open_animation = true;
	c->drag_to_tile = false;
	c->scratchpad_switching_mon = false;
	c->fake_no_border = false;
	c->focused_opacity = config.focused_opacity;
	c->unfocused_opacity = config.unfocused_opacity;
	c->nofocus = 0;
	c->nofadein = 0;
	c->nofadeout = 0;
	c->no_force_center = 0;
	c->isnoborder = 0;
	c->isnosizehint = 0;
	c->isnoradius = 0;
	c->isnoshadow = 0;
	c->ignore_maximize = 1;
	c->ignore_minimize = 1;
	c->iscustomsize = 0;
	c->iscustompos = 0;
	c->iscustom_scroller_proportion = 0;
	c->iscustom_scroller_proportion_single = 0;
	c->master_mfact_per = 0.0f;
	c->master_inner_per = 0.0f;
	c->stack_inner_per = 0.0f;
	c->old_stack_inner_per = 0.0f;
	c->old_master_inner_per = 0.0f;
	c->old_master_mfact_per = 0.0f;
	c->isterm = 0;
	c->allow_csd = 0;
	c->force_fakemaximize = 0;
	c->force_tiled_state = 1;
	c->force_tearing = 0;
	c->allow_shortcuts_inhibit = SHORTCUTS_INHIBIT_ENABLE;
	c->idleinhibit_when_focus = 0;
	c->vrr_only_fullscreen = 0;
	/* On unmap while in overview, destroy the card tree first to avoid a leak.
	 */
	overview_destroy_card(c);
	c->ov_card_tree = NULL;
	wl_list_init(&c->ov_card_surfaces);
	c->force_render = 0;
	c->activation_bypass = 0;
	c->scroller_proportion_single = 0.0f;
	c->float_geom.width = 0;
	c->float_geom.height = 0;
	c->float_geom.x = 0;
	c->float_geom.y = 0;
	c->stack_proportion = 0.0f;
	memset(c->oldmonname, 0, sizeof(c->oldmonname));
	memcpy(c->opacity_animation.initial_border_color, config.bordercolor,
		   sizeof(c->opacity_animation.initial_border_color));
	memcpy(c->opacity_animation.current_border_color, config.bordercolor,
		   sizeof(c->opacity_animation.current_border_color));
	c->opacity_animation.initial_opacity = c->unfocused_opacity;
	c->opacity_animation.current_opacity = c->unfocused_opacity;
	c->animation.tagining = false;
	c->animation.running = false;
	c->animation.overining = false;
	c->animation.overview_enter_anim_set = false;
	c->animation.tagouting = false;
	c->animation.tagouted = false;

	c->image_capture_scene_surface = NULL;
	c->image_capture_tree = NULL;
	c->image_capture_source = NULL;

	wl_list_init(&c->link);
	wl_list_init(&c->flink);
}

void // old fix to 0.5
handle_client_map(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	Client *at_client = NULL;
	Client *c = wl_container_of(listener, c, map);
	int32_t i = 0;

	c->id = generate_client_id();

	/* Create scene tree for this client and its border */
	c->scene = client_surface(c)->data =
		wlr_scene_tree_create(server.layers[LyrTile]);
	wlr_scene_node_set_enabled(&c->scene->node, c->type != XDGShell);
	c->scene_surface =
		c->type == XDGShell
			? wlr_scene_xdg_surface_create(c->scene, c->surface.xdg)
			: wlr_scene_subsurface_tree_create(c->scene, client_surface(c));
	c->scene->node.data = c->scene_surface->node.data = c;

	client_init_xwayland(c);

	if (!client_is_x11(c))
		client_get_geometry(c, &c->geom);

	if (client_is_x11(c))
		init_client_properties(c);

	// set special window properties
	if (client_is_unmanaged(c) || client_is_x11_popup(c)) {
		c->bw = 0;
		c->isnoborder = 1;
	} else {
		c->bw = config.borderpx;
	}

	if (client_should_global(c)) {
		c->isunglobal = 1;
	}

	// init client geom
	if (!client_is_x11(c)) {
		c->geom.width += 2 * c->bw;
		c->geom.height += 2 * c->bw;
		c->overview_backup_geom = c->geom;
	}

	struct wlr_ext_foreign_toplevel_handle_v1_state foreign_toplevel_state = {
		.app_id = client_get_appid(c),
		.title = client_get_title(c),
	};

	c->image_capture_scene = wlr_scene_create();
	c->ext_foreign_toplevel = wlr_ext_foreign_toplevel_handle_v1_create(
		server.foreign_toplevel_list, &foreign_toplevel_state);
	c->ext_foreign_toplevel->data = c;

	if (client_is_x11(c)) {
		c->image_capture_scene_surface = wlr_scene_surface_create(
			&c->image_capture_scene->tree, client_surface(c));
	} else {
		c->image_capture_tree = wlr_scene_xdg_surface_create(
			&c->image_capture_scene->tree, c->surface.xdg);
	}

	/* Handle unmanaged clients first so we can return prior create borders
	 */
	if (client_init_unmanaged(c))
		return;
	// extra node

	for (i = 0; i < 2; i++) {
		c->splitindicator[i] = wlr_scene_rect_create(
			c->scene, 0, 0,
			c->isurgent ? config.urgentcolor : config.splitcolor);
		c->splitindicator[i]->node.data = c;
		wlr_scene_node_lower_to_bottom(&c->splitindicator[i]->node);
		wlr_scene_node_set_enabled(&c->splitindicator[i]->node, false);
	}

	client_add_group_bar(c);

	c->droparea = wlr_scene_rect_create(c->scene, 0, 0, config.dropcolor);
	wlr_scene_node_lower_to_bottom(&c->droparea->node);
	wlr_scene_node_set_position(&c->droparea->node, 0, 0);
	wlr_scene_node_set_enabled(&c->droparea->node, false);

	c->border = wlr_scene_rect_create(
		c->scene, 0, 0, c->isurgent ? config.urgentcolor : config.bordercolor);
	wlr_scene_node_lower_to_bottom(&c->border->node);
	wlr_scene_node_set_position(&c->border->node, 0, 0);
	wlr_scene_rect_set_corner_radii(c->border,
									corner_radii_all(config.border_radius));
	wlr_scene_node_set_enabled(&c->border->node, true);

	c->shadow =
		wlr_scene_shadow_create(c->scene, 0, 0, config.border_radius,
								config.shadows_blur, config.shadowscolor);

	c->blur = wlr_scene_blur_create(c->scene, 0, 0);
	wlr_scene_node_lower_to_bottom(&c->blur->node);

	wlr_scene_node_lower_to_bottom(&c->shadow->node);
	wlr_scene_node_set_enabled(&c->shadow->node, true);

	c->shield =
		wlr_scene_rect_create(c->scene, 0, 0, (float[4]){0, 0, 0, 0xff});
	c->shield->node.data = c;
	wlr_scene_node_lower_to_bottom(&c->shield->node);
	wlr_scene_node_set_enabled(&c->shield->node, false);

	if (config.new_is_master && server.selected_monitor &&
		!is_scroller_layout(server.selected_monitor))
		// tile at the top
		wl_list_insert(&server.clients,
					   &c->link); // The new window is master; its head is
								  // pushed to the stack.
	else if (server.selected_monitor &&
			 is_scroller_layout(server.selected_monitor) &&
			 server.selected_monitor->visible_scroll_tiling_clients > 0) {

		if (server.selected_monitor->sel &&
			ISSCROLLTILED(server.selected_monitor->sel) &&
			VISIBLEON(server.selected_monitor->sel, server.selected_monitor)) {
			at_client =
				scroll_get_stack_tail_client(server.selected_monitor->sel);
		} else {
			at_client = center_tiled_select(server.selected_monitor);
		}

		if (at_client) {
			wl_list_insert(&at_client->link, &c->link);
		} else {
			wl_list_insert(server.clients.prev,
						   &c->link); // Pushed to the stack tail.
		}
	} else
		wl_list_insert(server.clients.prev,
					   &c->link); // Pushed to the stack tail.

	wl_list_insert(&server.focus_stack, &c->flink);

	client_apply_rules(c);

	client_apply_xwayland(c);

	if (!c->isfloating || c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
								WLR_EDGE_RIGHT);
	}

	// apply buffer effects of client
	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   iter_xdg_scene_buffers, c);
	wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);

	// set border color
	client_update_border_color(c);

	if (c->mon && c->mon->isoverview) {
		overview_backup_surface(c);
	}

	// make sure the animation is open type
	c->is_pending_open_animation = true;
	resize(c, c->geom, 0);
	printstatus(IPC_WATCH_ARRANGGE);
}

void handle_client_commit(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, commit);
	struct wlr_box *new_geo;

	/* Overview card nodes are independent scene_surfaces that auto-update on
	 * commit; nothing to handle here. */

	if (c->surface.xdg->initial_commit) {
		// xdg client will first enter this before mapnotify
		init_client_properties(c);
		client_apply_rules(c);
		if (c->mon) {
			client_set_scale(client_surface(c), c->mon->wlr_output->scale);
		}
		client_set_monitor(
			c, NULL, 0, true); /* Make sure to reapply rules in mapnotify() */

		uint32_t serial = wlr_xdg_surface_schedule_configure(c->surface.xdg);
		if (serial > 0) {
			c->configure_serial = serial;
		}

		uint32_t wm_caps = WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN;

		if (!c->ignore_minimize)
			wm_caps |= WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;

		if (!c->ignore_maximize)
			wm_caps |= WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE;

		wlr_xdg_toplevel_set_wm_capabilities(c->surface.xdg->toplevel, wm_caps);

		if (c->mon) {
			wlr_xdg_toplevel_set_bounds(c->surface.xdg->toplevel,
										c->mon->w.width - 2 * c->bw,
										c->mon->w.height - 2 * c->bw);
		}

		if (c->decoration)
			handle_xdg_decoration_mode_request(&c->set_decoration_mode,
											   c->decoration);
		return;
	}

	if (!c || c->iskilling || c->animation.tagouting || c->animation.tagouted ||
		c->animation.tagining)
		return;

	if (c->configure_serial &&
		c->configure_serial <= c->surface.xdg->current.configure_serial)
		c->configure_serial = 0;

	if (!c->dirty) {
		new_geo = &c->surface.xdg->geometry;
		c->dirty = new_geo->width != c->geom.width - 2 * c->bw ||
				   new_geo->height != c->geom.height - 2 * c->bw ||
				   new_geo->x != 0 || new_geo->y != 0;
	}

	if (c == server.grab_client || !c->dirty)
		return;

	resize(c, c->geom, 0);

	new_geo = &c->surface.xdg->geometry;
	c->dirty = new_geo->width != c->geom.width - 2 * c->bw ||
			   new_geo->height != c->geom.height - 2 * c->bw ||
			   new_geo->x != 0 || new_geo->y != 0;
}

void handle_client_unmap(struct wl_listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown.
	 */
	Client *c = wl_container_of(listener, c, unmap);
	Monitor *m = NULL;
	Client *nextfocus = NULL;
	c->iskilling = 1;
	switcher_remove_client(c);
	struct ScrollerStackNode *target_node =
		c->mon ? find_scroller_node(
					 c->mon->pertag->scroller_state[get_client_tag_idx(c)], c)
			   : NULL;
	struct ScrollerStackNode *prev_node =
		target_node ? target_node->prev_in_stack : NULL;
	struct ScrollerStackNode *next_node =
		target_node ? target_node->next_in_stack : NULL;

	if (config.animations && !c->is_clip_to_hide && !c->isminimized &&
		(!c->mon || VISIBLEON(c, c->mon)))
		init_fadeout_client(c);

	// If the client is in a stack, remove it from the stack

	if (c->swallowing) {
		c->swallowing->mon = c->mon;
		client_replace(c->swallowing, c, false, true);
	} else if ((c->group_next || c->group_prev) && c->isgroupfocusing) {
		Client *group_replacement =
			c->group_next ? c->group_next : c->group_prev;
		group_replacement->mon = c->mon;
		client_replace(group_replacement, c, false, false);
	} else {
		scroller_remove_client(c);
		dwindle_remove_client(c);
	}

	if (c == server.grab_client) {
		server.cursor_mode = CurNormal;
		server.grab_client = NULL;
		if (server.drop_client) {
			server.drop_client->enable_drop_area_draw = false;
			client_set_drop_area(server.drop_client);
			server.drop_client = NULL;
		}
	}

	if (c == server.drop_client) {
		server.drop_client = NULL;
	}

	wl_list_for_each(m, &server.monitors, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		if (c == m->sel) {
			m->sel = NULL;
		}
		if (c == m->prevsel) {
			m->prevsel = NULL;
		}
	}

	if (c->mon && c->mon == server.selected_monitor) {
		if (next_node && !c->swallowing) {
			nextfocus = next_node->client;
		} else if (prev_node && !c->swallowing) {
			nextfocus = prev_node->client;
		} else {
			nextfocus = client_focus_top(server.selected_monitor);
		}

		if (nextfocus && !VISIBLEON(nextfocus, server.selected_monitor)) {
			nextfocus = client_focus_top(server.selected_monitor);
		}

		if (nextfocus) {
			client_focus(nextfocus, 1);
		}

		if (!nextfocus && server.selected_monitor->isoverview) {
			Arg arg = {0};
			toggle_overview(&arg);
		}
	}

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		if (c->commmitx11.link.prev && c->commmitx11.link.next &&
			c->commmitx11.link.prev != &c->commmitx11.link) {
			wl_list_remove(&c->commmitx11.link);
			wl_list_init(&c->commmitx11.link);
		}
	}
#endif

	if (client_is_unmanaged(c)) {
#ifdef XWAYLAND
		if (client_is_x11(c)) {
			if (c->set_geometry.link.prev && c->set_geometry.link.next &&
				c->set_geometry.link.prev != &c->set_geometry.link) {
				wl_list_remove(&c->set_geometry.link);
				wl_list_init(&c->set_geometry.link);
			}
		}
#endif
		if (c == server.exclusive_focus)
			server.exclusive_focus = NULL;
		if (client_surface(c) == server.seat->keyboard_state.focused_surface)
			client_focus(client_focus_top(server.selected_monitor), 1);
	} else {

		client_group_detach(c);

		wl_list_remove(&c->link);
		client_set_monitor(c, NULL, 0, true);
		wl_list_remove(&c->flink);
	}

	if (c->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
		c->foreign_toplevel = NULL;
	}

	if (c->ext_foreign_toplevel) {
		wlr_ext_foreign_toplevel_handle_v1_destroy(c->ext_foreign_toplevel);
		c->ext_foreign_toplevel = NULL;
	}

	if (c->swallowing) {
		client_set_maximize_screen(c->swallowing, c->ismaximizescreen, true);
		client_apply_fullscreen(c->swallowing, c->isfullscreen, true);
		c->swallowing->swallowdby = NULL;
		c->swallowing = NULL;
	}

	if (c->swallowdby) {
		c->swallowdby->swallowing = NULL;
		c->swallowdby = NULL;
	}

	if (c->jump_label_node) {
		mango_jump_label_node_destroy(c->jump_label_node);
		c->jump_label_node = NULL;
	}

	if (c->group_bar) {
		mango_group_bar_destroy(c->group_bar);
		c->group_bar = NULL;
	}

	if (c->image_capture_scene) {
		wlr_scene_node_destroy(&c->image_capture_scene->tree.node);
		c->image_capture_scene = NULL;
	}

	init_client_properties(c);

	wlr_scene_node_destroy(&c->scene->node);
	printstatus(IPC_WATCH_ARRANGGE);
	pointer_process_motion(0, NULL, 0, 0, 0, 0);
}

void // 0.7 custom
handle_client_destroy(struct wl_listener *listener, void *data) {
	/* Called when the xdg_toplevel is destroyed. */
	Client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->set_title.link);
	wl_list_remove(&c->fullscreen.link);
	wl_list_remove(&c->maximize.link);
	wl_list_remove(&c->minimize.link);
#ifdef XWAYLAND
	if (c->type != XDGShell) {
		wl_list_remove(&c->activate.link);
		wl_list_remove(&c->associate.link);
		wl_list_remove(&c->configure.link);
		wl_list_remove(&c->dissociate.link);
		wl_list_remove(&c->set_hints.link);
	} else
#endif
	{
		wl_list_remove(&c->commit.link);
		wl_list_remove(&c->map.link);
		wl_list_remove(&c->unmap.link);
	}
	/*
	 * Decoration listeners are attached to deco->events; wlroots tears down
	 * decorations around the toplevel, so remove the listeners when the
	 * client/toplevel is destroyed.
	 */
	if (c->decoration) {
		wl_list_remove(&c->destroy_decoration.link);
		wl_list_remove(&c->set_decoration_mode.link);
	}
	switcher_remove_client(c);
	free(c);
}

void // 0.6
handle_client_request_fullscreen(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, fullscreen);

	if (!c || c->iskilling)
		return;

	client_apply_fullscreen(c, client_wants_fullscreen(c), true);
}

void handle_client_request_maximize(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, maximize);

	if (!c || !c->mon || c->iskilling || c->ignore_maximize)
		return;

	if (!client_is_x11(c) && !c->surface.xdg->initialized) {
		return;
	}

	if (client_request_maximize(c, data)) {
		client_set_maximize_screen(c, 1, true);
	} else {
		client_set_maximize_screen(c, 0, true);
	}
}

void handle_client_request_minimize(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, minimize);

	if (!c || !c->mon || c->iskilling || c->isminimized)
		return;

	if (client_request_minimize(c, data) && !c->ignore_minimize) {
		if (!c->isminimized)
			set_minimized(c);
		client_set_minimized(c, true);
	} else {
		if (c->isminimized)
			unminimize(c);
		client_set_minimized(c, false);
	}
}

void handle_client_set_title(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_title);

	if (!c || c->iskilling)
		return;

	const char *title;
	title = client_get_title(c);
	mango_group_bar_update(c->group_bar, title,
						   c->mon ? c->mon->wlr_output->scale : 1.0f);
	if (title && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel, title);
	if (title && c->ext_foreign_toplevel) {
		wlr_ext_foreign_toplevel_handle_v1_update_state(
			c->ext_foreign_toplevel,
			&(struct wlr_ext_foreign_toplevel_handle_v1_state){
				.title = title,
				.app_id = c->ext_foreign_toplevel->app_id,
			});
	}
	if (c == client_focus_top(c->mon))
		printstatus(IPC_WATCH_ARRANGGE);
}
void // 17 fix to 0.5
handle_client_activation_request(struct wl_listener *listener, void *data) {
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	Client *c = NULL;
	toplevel_from_wlr_surface(event->surface, &c, NULL);

	if (!c || !c->foreign_toplevel)
		return;

	if (config.focus_on_activate && !c->istagsilent &&
		c != server.selected_monitor->sel) {
		if (!(c->mon == server.selected_monitor &&
			  c->tags & c->mon->tagset[c->mon->seltags]))
			client_view_on_monitor(&(Arg){.ui = c->tags}, true, c->mon, true);
		client_focus(c, 1);
	} else if (c != client_focus_top(server.selected_monitor)) {
		c->isurgent = 1;
		if (client_surface(c)->mapped)
			client_update_border_color(c);
		printstatus(IPC_WATCH_ARRANGGE);
	}
}

void pending_kill_client(Client *c) {
	if (!c || c->iskilling)
		return;
	client_send_close(c);
}

void iter_xdg_scene_buffers(struct wlr_scene_buffer *buffer, int32_t sx,
							int32_t sy, void *user_data) {
	Client *c = user_data;

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface) {
		return;
	}

	struct wlr_surface *surface = scene_surface->surface;
	/* we dont blur subsurfaces */
	if (wlr_subsurface_try_from_wlr_surface(surface) != NULL)
		return;

	if (config.blur && c && !c->noblur) {
		if (config.blur_optimized) {
			wlr_scene_blur_set_should_only_blur_bottom_layer(c->blur, true);
		} else {
			wlr_scene_blur_set_should_only_blur_bottom_layer(c->blur, false);
		}
	}
}

void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int32_t sx,
								int32_t sy, void *data) {
	wlr_scene_buffer_set_opacity(buffer, *(double *)data);
}

void client_set_opacity(Client *c, double opacity) {
	opacity = CLAMP_FLOAT(opacity, 0.0f, 1.0f);
	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   scene_buffer_apply_opacity, &opacity);
}

void client_focus(Client *c, int32_t lift) {

	Client *last_focus_client = NULL;
	Monitor *um = NULL;

	struct wlr_surface *old_keyboard_focus_surface =
		server.seat->keyboard_state.focused_surface;

	if (server.session_locked)
		return;

	if (c && c->iskilling)
		return;

	if (c && !client_surface(c)->mapped)
		return;

	if (c && client_should_ignore_focus(c) && client_is_x11_popup(c))
		return;

	if (c && c->nofocus)
		return;

	/* Raise client in stacking order if requested */
	if (c && lift) {
		client_raise_group(c);
	}

	if (c && client_surface(c) == old_keyboard_focus_surface &&
		server.selected_monitor && server.selected_monitor->sel)
		return;

	if (server.selected_monitor && server.selected_monitor->sel &&
		server.selected_monitor->sel != c &&
		server.selected_monitor->sel->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_set_activated(
			server.selected_monitor->sel->foreign_toplevel, false);
	}

	if (c && !c->iskilling && !client_is_unmanaged(c) && c->mon) {

		last_focus_client =
			server.selected_monitor ? server.selected_monitor->sel : NULL;
		server.selected_monitor = c->mon;
		server.selected_monitor->prevsel = server.selected_monitor->sel;
		server.selected_monitor->sel = c;
		c->isfocusing = true;

		check_keep_idle_inhibit(c);
		check_vrr_enable(c);

		if (last_focus_client && !last_focus_client->iskilling &&
			last_focus_client != c) {
			last_focus_client->isfocusing = false;
			client_set_unfocused_opacity_animation(last_focus_client);
		}

		client_set_focused_opacity_animation(c);

		// decide whether need to re-arrange

		// change focus link position
		wl_list_remove(&c->flink);
		wl_list_insert(&server.focus_stack, &c->flink);

		if (c && server.selected_monitor->prevsel &&
			TAGMATCH(server.selected_monitor->prevsel,
					 server.selected_monitor) &&
			TAGMATCH(c, server.selected_monitor) && !c->isfloating &&
			(is_scroller_layout(server.selected_monitor) ||
			 is_monocle_layout(server.selected_monitor))) {
			arrange(server.selected_monitor, false, false);
		}

		// change border color
		c->isurgent = 0;
	}

	// update other monitor focus disappear
	wl_list_for_each(um, &server.monitors, link) {
		if (um->wlr_output->enabled && um != server.selected_monitor &&
			um->sel && !um->sel->iskilling && um->sel->isfocusing) {

			um->sel->isfocusing = false;
			client_set_unfocused_opacity_animation(um->sel);

			if (um->sel->foreign_toplevel) {
				wlr_foreign_toplevel_handle_v1_set_activated(
					um->sel->foreign_toplevel, false);
			}
		}
	}

	if (c && !c->iskilling && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);

	/* Deactivate old client if focus is changing */
	if (old_keyboard_focus_surface &&
		(!c || client_surface(c) != old_keyboard_focus_surface)) {
		/* If an exclusive_focus layer is focused, don't focus or activate
		 * the client, but only update its position in focus_stack to render its
		 * border with focuscolor and focus it after the exclusive_focus
		 * layer is closed. */
		Client *w = NULL;
		LayerSurface *l = NULL;
		int32_t type =
			toplevel_from_wlr_surface(old_keyboard_focus_surface, &w, &l);
		if (type == LayerShell && l->scene->node.enabled &&
			l->layer_surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP &&
			l == server.exclusive_focus) {
			return;
		} else if (w && w == server.exclusive_focus && client_wants_focus(w)) {
			return;
			/* Don't deactivate old_keyboard_focus_surface client if the new
			 * one wants focus, as this causes issues with winecfg and
			 * probably other clients */
		} else if (w && !client_is_unmanaged(w) &&
				   (!c || !client_wants_focus(c))) {
			client_activate_surface(old_keyboard_focus_surface, 0);
		}
	}
	printstatus(IPC_WATCH_ARRANGGE);

	if (!c) {

		if (server.selected_monitor && server.selected_monitor->sel &&
			(!VISIBLEON(server.selected_monitor->sel,
						server.selected_monitor) ||
			 server.selected_monitor->sel->iskilling ||
			 !client_surface(server.selected_monitor->sel)->mapped)) {
			server.selected_monitor->sel->isfocusing = false;
			client_set_unfocused_opacity_animation(
				server.selected_monitor->sel);
			server.selected_monitor->sel = NULL;
		}

		// clear text input focus state
		mango_im_relay_set_focus(server.input_method_relay, NULL);
		wlr_seat_keyboard_notify_clear_focus(server.seat);
		check_vrr_enable(c);
		if (server.active_constraint) {
			pointer_constrain_cursor(NULL);
		}
		return;
	}

	/* Change cursor surface */
	pointer_process_motion(0, NULL, 0, 0, 0, 0);

	// set text input focus
	// must before client_notify_enter,
	// otherwise the position of text_input will be wrong.
	mango_im_relay_set_focus(server.input_method_relay, client_surface(c));

	/* Have a client, so focus its top-level wlr_surface */
	client_notify_enter(client_surface(c), wlr_seat_get_keyboard(server.seat));

	/* Activate the new client */
	client_activate_surface(client_surface(c), 1);

	if (server.active_constraint &&
		server.active_constraint->surface != client_surface(c)) {
		pointer_constrain_cursor(NULL);
	}

	struct wlr_pointer_constraint_v1 *constraint;
	wl_list_for_each(constraint, &server.pointer_constraints->constraints,
					 link) {
		if (constraint->surface == client_surface(c)) {
			pointer_constrain_cursor(constraint);
			break;
		}
	}
}

void client_active(Client *c) {
	uint32_t target;

	if (client_is_unmanaged(c)) {
		client_focus(c, 1);
		return;
	}

	if (c->swallowdby || !c->mon)
		return;

	if (c->isminimized) {
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		c->is_scratchpad_show = 0;
		client_update_border_color(c);
		show_hide_client(c);
		arrange(c->mon, true, false);
		return;
	}

	target = get_tags_first_tag(c->tags);
	client_view_on_monitor(&(Arg){.ui = target}, true, c->mon, true);
	client_focus(c, 1);
}

void client_view_on_monitor(const Arg *arg, bool want_animation, Monitor *m,
							bool changefocus) {
	uint32_t i, tmptag;

	if (!m || (arg->ui != (~0 & TAGMASK) && m->isoverview)) {
		return;
	}

	if (arg->ui == 0) {
		return;
	}

	if (arg->ui == UINT32_MAX) {
		if (m->tagset[0] != m->tagset[1]) {
			m->pertag->prevtag = get_tags_first_tag_num(m->tagset[m->seltags]);
			m->seltags ^= 1; /* toggle sel tagset */
			m->pertag->curtag = get_tags_first_tag_num(m->tagset[m->seltags]);
			goto toggleseltags;
		} else {
			return;
		}
	}

	if ((m->tagset[m->seltags] & arg->ui & (TAGMASK | TAG0_MASK)) != 0) {
		want_animation = false;
	}

	m->seltags ^= 1; /* toggle sel tagset */

	if (arg->ui & (TAGMASK | TAG0_MASK)) {
		m->tagset[m->seltags] = arg->ui & (TAGMASK | TAG0_MASK);
		tmptag = m->pertag->curtag;

		if (arg->ui & TAG0_MASK)
			m->pertag->curtag = 0;
		else if (arg->ui == (~0 & TAGMASK))
			m->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i) && i < (uint32_t)config.tag_num &&
						arg->ui != 0;
				 i++)
				;
			m->pertag->curtag = i >= (uint32_t)config.tag_num
									? (uint32_t)config.tag_num
									: i + 1;
		}

		m->pertag->prevtag =
			tmptag == m->pertag->curtag ? m->pertag->prevtag : tmptag;
	} else {
		tmptag = m->pertag->prevtag;
		m->pertag->prevtag = m->pertag->curtag;
		m->pertag->curtag = tmptag;
	}

toggleseltags:

	if (changefocus)
		client_focus(client_focus_top(m), 1);
	arrange(m, want_animation, true);
	printstatus(IPC_WATCH_ARRANGGE);
}

void client_switch_view(const Arg *arg, bool want_animation) {
	Monitor *m = NULL;
	if (arg->i) {
		client_view_on_monitor(arg, want_animation, server.selected_monitor,
							   true);
		wl_list_for_each(m, &server.monitors, link) {
			if (!m->wlr_output->enabled || m == server.selected_monitor)
				continue;
			// only arrange, not change monitor focus
			client_view_on_monitor(arg, want_animation, m, false);
		}
	} else {
		client_view_on_monitor(arg, want_animation, server.selected_monitor,
							   true);
	}
}

void tag_client(const Arg *arg, Client *target_client) {
	Client *fc = NULL;
	if (target_client && (arg->ui & (TAGMASK | TAG0_MASK))) {

		target_client->tags =
			(arg->ui & TAG0_MASK) ? TAG0_MASK : (arg->ui & TAGMASK);
		client_reparent_group(target_client);

		wl_list_for_each(fc, &server.clients, link) {
			if (fc && fc != target_client && target_client->tags & fc->tags &&
				ISFULLSCREEN(fc) && !target_client->isfloating) {
				clear_fullscreen_flag(fc);
			}
		}
		if (arg->ui & TAG0_MASK) {
			arrange(target_client->mon, false, false);
		}
		client_switch_view(&(Arg){.ui = arg->ui, .i = arg->i}, true);

	} else {
		client_switch_view(arg, true);
	}

	client_focus(target_client, 1);
	printstatus(IPC_WATCH_ARRANGGE);
}

void show_hide_client(Client *c) {
	uint32_t target = 1;

	if (c->mon)
		set_size_per(c->mon, c);
	target = get_tags_first_tag(c->oldtags);

	if (!c->is_in_scratchpad) {
		tag_client(&(Arg){.ui = target}, c);
	} else {
		c->tags = c->mini_restore_tag ? c->mini_restore_tag : c->oldtags;
		c->isminimized = 0;
		if (c->mon)
			arrange(c->mon, false, false);
	}
	client_pending_minimized_state(c, 0);
	client_focus(c, 1);

	if (c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);
}

void client_set_monitor(Client *c, Monitor *m, uint32_t newtags, bool focus) {
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;

	if (oldmon && oldmon->sel == c) {
		oldmon->sel = NULL;
	}

	if (oldmon && oldmon->prevsel == c) {
		oldmon->prevsel = NULL;
	}

	c->mon = m;

	/* Scene graph sends surface leave/enter events on move and resize */
	if (oldmon)
		arrange(oldmon, false, false);
	if (m) {
		/* Make sure window actually overlaps with the monitor */
		reset_foreign_tolevel(c, oldmon, m);
		resize(c, c->geom, 0);
		client_reset_mon_tags(c, m, newtags);
		check_match_tag_floating_rule(c, m);
		client_set_floating(c, c->isfloating);
		client_apply_fullscreen(c, c->isfullscreen,
								true); /* This will call arrange(c->mon) */
	}

	if (focus && !client_is_x11_popup(c)) {
		client_focus(client_focus_top(server.selected_monitor), 1);
	}
}

void client_change_mon(Client *c, Monitor *m) {
	client_set_monitor(c, m, c->tags, true);
	if (c->isfloating) {
		c->float_geom = c->geom =
			client_center_geometry(c, c->mon, c->geom, 0, 0);
	}
}

void view_insert_shift_tags(Monitor *m, uint32_t target) {
	Client *c;
	uint32_t map[tag_num_MAX + 1] = {0};
	uint32_t i;

	if (!m || target < 1 || target >= (uint32_t)config.tag_num)
		return;

	if (get_tag_status((uint32_t)config.tag_num, m))
		return;

	for (i = 1; i <= (uint32_t)config.tag_num; i++) {
		if (i < target)
			map[i] = i;
		else if (i < (uint32_t)config.tag_num)
			map[i] = i + 1;
		else
			map[i] = i;
	}

	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || c->iskilling || c->is_logic_hide)
			continue;
		c->tags = tag_remap_mask(c->tags, map);
	}

	m->tagset[m->seltags] = tag_remap_mask(m->tagset[m->seltags], map);
	m->tagset[m->seltags ^ 1] = tag_remap_mask(m->tagset[m->seltags ^ 1], map);
	m->ovbk_current_tagset = tag_remap_mask(m->ovbk_current_tagset, map);
	m->ovbk_prev_tagset = tag_remap_mask(m->ovbk_prev_tagset, map);

	if (m->pertag->curtag <= (uint32_t)config.tag_num && map[m->pertag->curtag])
		m->pertag->curtag = map[m->pertag->curtag];
	if (m->pertag->prevtag <= (uint32_t)config.tag_num &&
		map[m->pertag->prevtag])
		m->pertag->prevtag = map[m->pertag->prevtag];

	for (i = (uint32_t)config.tag_num - 1; i >= target; i--) {
		tag_gather_move_pertag(m, i + 1, i);
	}
}

void // 0.5
client_set_floating(Client *c, int32_t floating) {

	Client *fc = NULL;
	struct wlr_box target_box;
	int32_t old_floating_state = c->isfloating;
	c->isfloating = floating;
	bool window_size_outofrange = false;

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	target_box = c->geom;

	if (floating == 1 && c != server.grab_client) {

		if (c->isfullscreen) {
			client_pending_fullscreen_state(c, 0);
			client_set_fullscreen(c, 0);
		}

		client_pending_maximized_state(c, 0);
		exit_scroller_stack(c);

		// Recomputes the centered coordinates.
		if (!client_is_x11(c) && !c->iscustompos)
			target_box = client_center_geometry(c, c->mon, target_box, 0, 0);
		else
			target_box = c->geom;

		// restore to the memeroy geom
		if (c->float_geom.width > 0 && c->float_geom.height > 0) {
			if (c->mon &&
				c->float_geom.width >= c->mon->w.width - config.gappoh) {
				c->float_geom.width = c->mon->w.width * 0.9;
				window_size_outofrange = true;
			}
			if (c->mon &&
				c->float_geom.height >= c->mon->w.height - config.gappov) {
				c->float_geom.height = c->mon->w.height * 0.9;
				window_size_outofrange = true;
			}
			if (window_size_outofrange) {
				c->float_geom =
					client_center_geometry(c, c->mon, c->float_geom, 0, 0);
			}
			resize(c, c->float_geom, 0);
		} else {
			resize(c, target_box, 0);
		}

		c->need_float_size_reduce = 0;
	} else if (c->isfloating && c == server.grab_client) {
		c->need_float_size_reduce = 0;
	} else {
		c->need_float_size_reduce = 1;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		// Makes fullscreen windows on the current tag exit fullscreen so they
		// join tiling.
		wl_list_for_each(fc, &server.clients,
						 link) if (fc && fc != c && VISIBLEON(fc, c->mon) &&
								   c->tags & fc->tags && ISFULLSCREEN(fc) &&
								   old_floating_state) {
			clear_fullscreen_flag(fc);
		}
	}

	client_reparent_group(c);

	if (c->isfloating) {
		set_size_per(c->mon, c);
	}

	if (!c->force_fakemaximize)
		client_set_maximized(c, false);

	if (!c->isfloating || c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
								WLR_EDGE_RIGHT);
	} else {
		client_set_tiled(c, WLR_EDGE_NONE);
	}

	arrange(c->mon, false, false);

	if (!c->isfloating) {
		c->old_master_inner_per = c->master_inner_per;
		c->old_stack_inner_per = c->stack_inner_per;
	}

	client_update_border_color(c);
	printstatus(IPC_WATCH_ARRANGGE);
}

void client_apply_fullscreen(
	Client *c, int32_t fullscreen,
	bool rearrange) // Uses the custom fullscreen proxy for its own fullscreen.
{

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling ||
		c == server.grab_client)
		return;

	if (c->mon->isoverview)
		return;

	c->isfullscreen = fullscreen;

	client_set_fullscreen(c, fullscreen);
	client_pending_fullscreen_state(c, fullscreen);

	if (fullscreen) {

		if (c->ismaximizescreen && !c->force_fakemaximize) {
			client_set_maximized(c, false);
		}

		client_pending_maximized_state(c, 0);

		exit_scroller_stack(c);
		c->isfakefullscreen = 0;

		c->bw = 0;
		wlr_scene_node_raise_to_top(
			&c->scene->node); // Raises the view to the top.
		if (!is_scroller_layout(c->mon) || c->isfloating)
			resize(c, c->mon->m, 1);

	} else {
		c->bw = c->isnoborder ? 0 : config.borderpx;
		if (c->isfloating)
			client_set_floating(c, 1);
	}

	client_reparent_group(c);
	check_vrr_enable(c);

	if (rearrange)
		arrange(c->mon, false, false);
}

void client_set_fake_fullscreen(Client *c, int32_t fakefullscreen) {
	c->isfakefullscreen = fakefullscreen;
	if (!c->mon)
		return;

	if (c->isfullscreen)
		client_apply_fullscreen(c, 0, true);

	client_set_fullscreen(c, fakefullscreen);
}

void client_set_maximize_screen(Client *c, int32_t maximizescreen,
								bool rearrange) {
	struct wlr_box maximizescreen_box;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling ||
		c == server.grab_client)
		return;

	if (c->mon->isoverview)
		return;

	client_pending_maximized_state(c, maximizescreen);

	if (maximizescreen) {

		if (c->isfullscreen) {
			client_pending_fullscreen_state(c, 0);
			client_set_fullscreen(c, 0);
		}

		exit_scroller_stack(c);

		maximizescreen_box.x = c->mon->w.x + config.gappoh;
		maximizescreen_box.y = c->mon->w.y + config.gappov;
		maximizescreen_box.width = c->mon->w.width - 2 * config.gappoh;
		maximizescreen_box.height = c->mon->w.height - 2 * config.gappov;

		if (c->group_next || c->group_prev) {
			maximizescreen_box.height -= config.group_bar_height;
			maximizescreen_box.y += config.group_bar_height;
		}

		wlr_scene_node_raise_to_top(&c->scene->node);
		if (!is_scroller_layout(c->mon) || c->isfloating)
			resize(c, maximizescreen_box, 0);
	} else {
		c->bw = c->isnoborder ? 0 : config.borderpx;
		if (c->isfloating)
			client_set_floating(c, 1);
	}

	client_reparent_group(c);

	if (!c->force_fakemaximize && !c->ismaximizescreen) {
		client_set_maximized(c, false);
	} else if (!c->force_fakemaximize && c->ismaximizescreen) {
		client_set_maximized(c, true);
	}

	if (rearrange)
		arrange(c->mon, false, false);
}

void reset_maximizescreen_size(Client *c) {
	struct wlr_box geom;
	geom.x = c->mon->w.x + config.gappoh;
	geom.y = c->mon->w.y + config.gappov;
	geom.width = c->mon->w.width - 2 * config.gappoh;
	geom.height = c->mon->w.height - 2 * config.gappov;

	if (c->group_next || c->group_prev) {
		geom.height -= config.group_bar_height;
		geom.y += config.group_bar_height;
	}

	resize(c, geom, 0);
}

void set_minimized(Client *c) {

	if (!c || !c->mon || c == server.grab_client)
		return;

	c->isglobal = 0;

	c->oldtags = c->mon->tagset[c->mon->seltags];
	c->mini_restore_tag = c->tags;
	c->tags = 0;
	client_pending_minimized_state(c, 1);
	c->is_in_scratchpad = 1;
	c->is_scratchpad_show = 0;
	client_reparent_group(c);

	client_focus(client_focus_top(server.selected_monitor), 1);
	arrange(c->mon, false, false);

	if (c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,
													 false);

	wl_list_remove(&c->link); // Removes it from its previous position.
	wl_list_insert(server.clients.prev, &c->link); // Inserts it at the tail.
}

void unminimize(Client *c) {
	if (c && c->is_in_scratchpad && c->is_scratchpad_show) {
		client_pending_minimized_state(c, 0);
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		client_reparent_group(c);
		client_update_border_color(c);
		return;
	}

	if (c && c->isminimized) {
		show_hide_client(c);
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		client_reparent_group(c);
		client_update_border_color(c);
		arrange(c->mon, false, false);
		return;
	}
}

void exit_scroller_stack(Client *c) {
	if (!c || !c->mon)
		return;

	uint32_t tag = get_client_tag_idx(c);
	struct TagScrollerState *st = c->mon->pertag->scroller_state[tag];
	if (st) {
		struct ScrollerStackNode *n = find_scroller_node(st, c);
		if (n) {
			scroller_node_remove(st, n);
			return;
		}
	}
}

void clear_fullscreen_and_maximized_state(Monitor *m) {
	Client *fc = NULL;
	wl_list_for_each(fc, &server.clients, link) {
		if (fc && VISIBLEON(fc, m) && ISFULLSCREEN(fc)) {
			clear_fullscreen_flag(fc);
		}
	}
}

/* Clears the fullscreen flag and restores the border zeroed at fullscreen. */
void clear_fullscreen_flag(Client *c) {

	if ((c->mon->pertag->ltidxs[get_client_tag_idx(c)]->id == SCROLLER ||
		 c->mon->pertag->ltidxs[get_client_tag_idx(c)]->id ==
			 VERTICAL_SCROLLER) &&
		!c->isfloating) {
		return;
	}

	if (c->isfullscreen) {
		client_apply_fullscreen(c, false, true);
	}

	if (c->ismaximizescreen) {
		client_set_maximize_screen(c, 0, true);
	}
}

void client_pending_fullscreen_state(Client *c, int32_t isfullscreen) {
	c->isfullscreen = isfullscreen;

	if (c->foreign_toplevel && !c->iskilling)
		wlr_foreign_toplevel_handle_v1_set_fullscreen(c->foreign_toplevel,
													  isfullscreen);
}

void client_pending_maximized_state(Client *c, int32_t ismaximized) {
	c->ismaximizescreen = ismaximized;
	if (c->foreign_toplevel && !c->iskilling)
		wlr_foreign_toplevel_handle_v1_set_maximized(c->foreign_toplevel,
													 ismaximized);
}

void client_pending_minimized_state(Client *c, int32_t isminimized) {
	c->isminimized = isminimized;
	if (c->foreign_toplevel && !c->iskilling)
		wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel,
													 isminimized);
}

void show_scratchpad(Client *c) {
	c->is_scratchpad_show = 1;
	if (c->isfullscreen || c->ismaximizescreen) {
		client_pending_fullscreen_state(c, 0);
		client_pending_maximized_state(c, 0);
		c->bw = c->isnoborder ? 0 : config.borderpx;
	}

	/* return if fullscreen */
	if (!c->isfloating) {
		client_set_floating(c, 1);
		c->geom.width = c->iscustomsize
							? c->float_geom.width
							: c->mon->w.width * config.scratchpad_width_ratio;
		c->geom.height =
			c->iscustomsize ? c->float_geom.height
							: c->mon->w.height * config.scratchpad_height_ratio;
		// Recomputes the centered coordinates.
		c->float_geom = c->geom = c->animainit_geom = c->animation.current =
			client_center_geometry(c, c->mon, c->geom, 0, 0);
		c->iscustomsize = 1;
		resize(c, c->geom, 0);
	}

	client_reparent_group(c);
	c->oldtags = c->mon->tagset[c->mon->seltags];
	wl_list_safe_reinsert_next(&server.clients, &c->link);
	show_hide_client(c);
	client_update_border_color(c);
}

bool switch_scratchpad_client_state(Client *c) {

	if (config.scratchpad_cross_monitor && server.selected_monitor &&
		c->mon != server.selected_monitor && c->is_in_scratchpad) {
		// Saves the original monitor for size computation.
		Monitor *oldmon = c->mon;
		c->scratchpad_switching_mon = true;
		c->mon = server.selected_monitor;
		reset_foreign_tolevel(c, oldmon, c->mon);
		client_update_oldmonname_record(c, server.selected_monitor);

		// Adjusts the window size for the new monitor.
		c->float_geom.width =
			(int32_t)(c->float_geom.width * c->mon->w.width / oldmon->w.width);
		c->float_geom.height = (int32_t)(c->float_geom.height *
										 c->mon->w.height / oldmon->w.height);

		c->float_geom = client_center_geometry(c, c->mon, c->float_geom, 0, 0);

		// Only a visible scratchpad needs focus and returns true.
		if (c->is_scratchpad_show) {
			c->tags = get_tags_first_tag(
				server.selected_monitor
					->tagset[server.selected_monitor->seltags]);
			resize(c, c->float_geom, 0);
			arrange(server.selected_monitor, false, false);
			client_focus(c, 1);
			c->scratchpad_switching_mon = false;
			return true;
		} else {
			resize(c, c->float_geom, 0);
			c->scratchpad_switching_mon = false;
		}
	}

	// visible on this tag -> hide
	if (c->is_in_scratchpad && c->is_scratchpad_show && c->mon &&
		(c->mon->tagset[c->mon->seltags] & c->tags)) {
		set_minimized(c);
		return true;
	} else if (c->is_in_scratchpad && c->mon) {
		// not visible on this tag: move the scratchpad here and show it
		c->tags = c->mon->tagset[c->mon->seltags];
		c->oldtags = c->tags;
		c->mini_restore_tag = c->tags;
		if (c->is_scratchpad_show) {
			arrange(c->mon, false, false);
			client_focus(c, 1);
		} else {
			show_scratchpad(c);
		}
		return true;
	}

	return false;
}

void apply_named_scratchpad(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &server.clients, link) {

		if (!config.scratchpad_cross_monitor &&
			c->mon != server.selected_monitor) {
			continue;
		}

		if (config.single_scratchpad && c->is_in_scratchpad &&
			c->is_scratchpad_show && c != target_client) {
			set_minimized(c);
		}
	}

	if (!target_client->is_in_scratchpad) {
		set_minimized(target_client);
		switch_scratchpad_client_state(target_client);
	} else
		switch_scratchpad_client_state(target_client);
}

void client_update_border_color(Client *c) {
	if (!c || !c->mon)
		return;

	float *border_color = get_border_color(c);
	memcpy(c->opacity_animation.target_border_color, border_color,
		   sizeof(c->opacity_animation.target_border_color));
	client_set_border_color(c, border_color);
}

void client_exchange(Client *c1, Client *c2) {
	if (c1 == NULL || c2 == NULL ||
		(!config.exchange_cross_monitor && c1->mon != c2->mon)) {
		return;
	}

	Monitor *m1 = c1->mon;
	Monitor *m2 = c2->mon;
	const Layout *layout1 = m1->pertag->ltidxs[get_client_tag_idx(c1)];
	const Layout *layout2 = m2->pertag->ltidxs[get_client_tag_idx(c2)];

	if (layout1->id == SCROLLER || layout2->id == SCROLLER ||
		layout1->id == VERTICAL_SCROLLER || layout2->id == VERTICAL_SCROLLER) {
		exchange_two_scroller_clients(c1, c2);
		return;
	}

	if (layout1->id == DWINDLE && layout2->id == DWINDLE) {
		dwindle_swap_clients(c1, c2);
		return;
	}

	client_swap_layout_properties(c1, c2);

	wl_list_swap(&c1->link, &c2->link);

	if (m1 != m2) {
		client_swap_monitors_and_tags(c1, c2);
	}

	finish_exchange_arrange_and_focus(c1, c2, m1, m2);
}

void client_replace(Client *c, Client *w, bool is_group_change_member,
					bool is_swallow) {
	c->bw = w->bw;
	c->isfloating = w->isfloating;
	c->isurgent = w->isurgent;
	c->is_in_scratchpad = w->is_in_scratchpad;
	c->is_scratchpad_show = w->is_scratchpad_show;
	c->tags = w->tags;
	c->geom = w->geom;
	c->float_geom = w->float_geom;
	c->stack_inner_per = w->stack_inner_per;
	c->master_inner_per = w->master_inner_per;
	c->master_mfact_per = w->master_mfact_per;
	c->scroller_proportion = w->scroller_proportion;
	c->isglobal = w->isglobal;
	c->overview_backup_geom = w->overview_backup_geom;
	c->animation.current = w->animation.current;
	c->stack_proportion = w->stack_proportion;
	c->is_logic_hide = w->is_logic_hide;

	if (is_swallow || !is_group_change_member) {
		client_group_replace(w, c);
	}

	w->is_logic_hide = true;
	mango_group_bar_set_focus(c->group_bar, c->isgroupfocusing);

	/* If the old window is in overview, destroy its card tree. */
	overview_destroy_card(w);
	if (w->overview_scene_surface) {
		w->overview_scene_surface = NULL;
	}

	if (c->mon && c->mon->isoverview) {
		overview_backup_surface(c);
	}

	if (w->group_bar && !is_group_change_member) {
		wlr_scene_node_set_enabled(&w->group_bar->scene_buffer->node, false);
	}

	if (w->jump_label_node) {
		wlr_scene_node_set_enabled(&w->jump_label_node->scene_buffer->node,
								   false);
	}

	wl_list_safe_reinsert_next(&w->link, &c->link);
	wl_list_safe_reinsert_prev(&w->flink, &c->flink);
	wlr_scene_node_set_enabled(&w->scene->node, false);

	if (!c->is_logic_hide) {
		wlr_scene_node_set_enabled(&c->scene->node, true);
		/* In overview the real surface tree is replaced by the card tree, so it
		 * stays disabled. */
		if (!c->ov_card_tree)
			wlr_scene_node_set_enabled(&c->scene_surface->node, true);
	}

	if (w->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_output_leave(w->foreign_toplevel,
													w->mon->wlr_output);
		wlr_foreign_toplevel_handle_v1_destroy(w->foreign_toplevel);
		w->foreign_toplevel = NULL;
	}

	if (!c->foreign_toplevel && c->mon)
		add_foreign_toplevel(c);
	else if (c->foreign_toplevel && c->mon) {
		wlr_foreign_toplevel_handle_v1_output_enter(c->foreign_toplevel,
													c->mon->wlr_output);
	}

	client_pending_fullscreen_state(c, w->isfullscreen);
	client_pending_maximized_state(c, w->ismaximizescreen);
	client_pending_minimized_state(c, w->isminimized);

	if (!w->mon)
		return;

	const Layout *layout = w->mon->pertag->ltidxs[get_client_tag_idx(w)];

	if (layout->id == DWINDLE || layout->id == SCROLLER ||
		layout->id == VERTICAL_SCROLLER) {

		for (uint32_t t = 0; t < PERTAG_SLOTS; t++) {
			/* dwindle */

			if (layout->id == DWINDLE) {

				DwindleNode **root = &w->mon->pertag->dwindle_root[t];
				dwindle_remove(root, c);
				DwindleNode *dnode = dwindle_find_leaf(*root, w);
				if (dnode)
					dnode->client = c;
			}

			// scroller
			if (layout->id == SCROLLER || layout->id == VERTICAL_SCROLLER) {
				struct TagScrollerState *st = w->mon->pertag->scroller_state[t];
				if (!st)
					continue;
				struct ScrollerStackNode *cn = find_scroller_node(st, c);
				if (cn)
					scroller_node_remove(st, cn);

				struct ScrollerStackNode *wn = find_scroller_node(st, w);
				if (wn)
					wn->client = c;
			}
		}
	}

	/* Syncs the global client fields of the currently active tag. */
	if (layout->id == SCROLLER || layout->id == VERTICAL_SCROLLER) {
		sync_scroller_state_to_clients(w->mon, get_client_tag_idx(w));
	}
}

void client_update_oldmonname_record(Client *c, Monitor *m) {
	if (!c || c->iskilling || !client_surface(c)->mapped)
		return;
	memset(c->oldmonname, 0, sizeof(c->oldmonname));
	strncpy(c->oldmonname, m->wlr_output->name, sizeof(c->oldmonname) - 1);
	c->oldmonname[sizeof(c->oldmonname) - 1] = '\0';
}

void client_apply_bounds(Client *c, struct wlr_box *bbox) {
	/* set minimum possible */
	c->geom.width = MANGO_MAX(1 + 2 * (int32_t)c->bw, c->geom.width);
	c->geom.height = MANGO_MAX(1 + 2 * (int32_t)c->bw, c->geom.height);

	if (c->geom.x >= bbox->x + bbox->width)
		c->geom.x = bbox->x + bbox->width - c->geom.width;
	if (c->geom.y >= bbox->y + bbox->height)
		c->geom.y = bbox->y + bbox->height - c->geom.height;
	if (c->geom.x + c->geom.width <= bbox->x)
		c->geom.x = bbox->x;
	if (c->geom.y + c->geom.height <= bbox->y)
		c->geom.y = bbox->y;
}

void client_swap_layout_properties(Client *c1, Client *c2) {
	// grid property swap
	double grid_col_per = c1->grid_col_per;
	double grid_row_per = c1->grid_row_per;
	int32_t grid_col_idx = c1->grid_col_idx;
	int32_t grid_row_idx = c1->grid_row_idx;

	c1->grid_col_per = c2->grid_col_per;
	c1->grid_row_per = c2->grid_row_per;
	c1->grid_col_idx = c2->grid_col_idx;
	c1->grid_row_idx = c2->grid_row_idx;

	c2->grid_col_per = grid_col_per;
	c2->grid_row_per = grid_row_per;
	c2->grid_col_idx = grid_col_idx;
	c2->grid_row_idx = grid_row_idx;

	// master / stack property swap
	double master_inner_per = c1->master_inner_per;
	double master_mfact_per = c1->master_mfact_per;
	double stack_inner_per = c1->stack_inner_per;

	c1->master_inner_per = c2->master_inner_per;
	c1->master_mfact_per = c2->master_mfact_per;
	c1->stack_inner_per = c2->stack_inner_per;

	c2->master_inner_per = master_inner_per;
	c2->master_mfact_per = master_mfact_per;
	c2->stack_inner_per = stack_inner_per;
}

void client_swap_monitors_and_tags(Client *c1, Client *c2) {
	Monitor *tmp_mon = c2->mon;
	uint32_t tmp_tags = c2->tags;
	c2->mon = c1->mon;
	c1->mon = tmp_mon;
	c2->tags = c1->tags;
	c1->tags = tmp_tags;
}

void finish_exchange_arrange_and_focus(Client *c1, Client *c2, Monitor *m1,
									   Monitor *m2) {
	if (m1 != m2) {
		arrange(c1->mon, false, false);
		arrange(c2->mon, false, false);
	} else {
		arrange(c1->mon, false, false);
	}

	wl_list_safe_reinsert_next(&c1->flink, &c2->flink);

	if (config.warpcursor)
		pointer_warp_to_client(c1);
}

void client_tile_resize(Client *c, struct wlr_box geo, int32_t interact) {
	if (!ISFAKETILED(c) || !c->mon)
		return;

	if (!c->mon->isoverview && !c->isfullscreen &&
		(c->group_next || c->group_prev)) {
		geo.y = geo.y + config.group_bar_height;
		geo.height -= config.group_bar_height;
	}

	if ((!c->isfullscreen && !c->ismaximizescreen) ||
		is_scroller_layout(c->mon)) {
		resize(c, geo, interact);
	}
}

uint32_t generate_client_id(void) { return ++server.next_client_id; }

void client_pending_force_kill(Client *c) {
	if (!c)
		return;
	kill(c->pid, SIGKILL);
}

void client_add_jump_label_node(Client *c) {
	c->jump_label_node =
		mango_jump_label_node_create(c->scene, config.jumplabeldata);
	if (!c->jump_label_node)
		return;
	/* In overview, labels must be displayed above the card tree. */
	if (c->ov_card_tree)
		wlr_scene_node_raise_to_top(&c->jump_label_node->scene_buffer->node);
	else
		wlr_scene_node_lower_to_bottom(&c->jump_label_node->scene_buffer->node);
	wlr_scene_node_set_enabled(&c->jump_label_node->scene_buffer->node, false);
}

// scene layer a client belongs to; shown scratchpads join the special
// layers while the special workspace is active
uint32_t client_target_layer(Client *c) {
	if (c->isoverlay)
		return LyrOverlay;

	bool special_overlay = (c->tags & TAG0_MASK) ||
						   (is_special_active(c->mon) && c->is_in_scratchpad &&
							c->is_scratchpad_show && !c->isminimized);

	if (special_overlay)
		return c->isfloating || c->isfullscreen ? LyrSpecialTop
			   : c->ismaximizescreen			? LyrSpecialMaximize
												: LyrSpecialTile;

	return c->isfloating || c->isfullscreen ? LyrTop
		   : c->ismaximizescreen			? LyrMaximize
											: LyrTile;
}

// sync client scene to its target layer
void client_sync_layer(Client *c) {
	if (!c || !c->scene || !c->mon)
		return;
	if (c->scene->node.parent != server.layers[client_target_layer(c)])
		client_reparent_group(c);
}

void client_add_group_bar(Client *c) {

	if (config.group_bar_height <= 0) {
		return;
	}

	uint32_t layer = client_target_layer(c);

	c->group_bar = mango_group_bar_create(c, GroupBar, server.layers[layer],
										  config.groupbardata, 0, 0);
	wlr_scene_node_lower_to_bottom(&c->group_bar->scene_buffer->node);
	wlr_scene_node_set_enabled(&c->group_bar->scene_buffer->node, false);
	mango_group_bar_update(c->group_bar, client_get_title(c),
						   c->mon ? c->mon->wlr_output->scale
						   : server.selected_monitor
							   ? server.selected_monitor->wlr_output->scale
							   : 1.0f);
}

void client_focus_group_member(Client *c) {
	if (!c->group_prev && !c->group_next)
		return;

	if (c->isgroupfocusing)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur_focusing = NULL;
	while (head) {
		if (head->isgroupfocusing) {
			cur_focusing = head;
			break;
		}
		head = head->group_next;
	}

	if (!cur_focusing || !cur_focusing->mon)
		return;

	if (cur_focusing && cur_focusing->mon->isoverview)
		return;

	cur_focusing->isgroupfocusing = false;
	c->mon = cur_focusing->mon;
	client_replace(c, cur_focusing, true, false);
	mango_group_bar_set_focus(cur_focusing->group_bar, false);

	c->isgroupfocusing = true;
	mango_group_bar_set_focus(c->group_bar, true);

	client_reparent_group(c);

	client_focus(c, 1);

	arrange(c->mon, false, false);
}

void client_check_tab_node_visible(Client *c) {

	if (!c || !c->mon)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (!c->mon->isoverview && cur->group_bar &&
			(cur->group_next || cur->group_prev) && TAGMATCH(c, c->mon) &&
			ISNORMAL(c) && !c->isfullscreen) {
			wlr_scene_node_set_enabled(&cur->group_bar->scene_buffer->node,
									   true);
		} else {
			wlr_scene_node_set_enabled(&cur->group_bar->scene_buffer->node,
									   false);
		}
		cur = cur->group_next;
	}
}

void client_raise_group(Client *c) {
	if (!c || !c->mon)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->group_bar) {
			wlr_scene_node_raise_to_top(&cur->group_bar->scene_buffer->node);
		}
		wlr_scene_node_raise_to_top(&cur->scene->node);
		cur = cur->group_next;
	}
}

void client_reparent_group(Client *c) {
	if (!c || !c->mon)
		return;

	int32_t layer = client_target_layer(c);

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->group_bar) {
			wlr_scene_node_reparent(&cur->group_bar->scene_buffer->node,
									server.layers[layer]);
		}
		wlr_scene_node_reparent(&cur->scene->node, server.layers[layer]);
		cur = cur->group_next;
	}
}

void client_handle_decorate_click(MangoGroupBar *gb) {

	if (!gb)
		return;

	if (gb->node_data) {
		Client *c = gb->node_data;
		client_focus_group_member(c);
	}
}

void client_set_group_mon(Client *c, Monitor *m) {
	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		client_change_mon(cur, m);
		cur = cur->group_next;
	}
}

void client_set_group_config(Client *c) {
	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->jump_label_node)
			mango_jump_label_node_apply_config(cur->jump_label_node,
											   &config.jumplabeldata);
		wlr_scene_rect_set_color(cur->droparea, config.dropcolor);
		wlr_scene_rect_set_color(cur->splitindicator[0], config.splitcolor);
		wlr_scene_rect_set_color(cur->splitindicator[1], config.splitcolor);
		mango_group_bar_apply_config(cur->group_bar, &config.groupbardata);
		cur = cur->group_next;
	}
}

void client_group_detach(Client *c) {
	if (c->group_prev)
		c->group_prev->group_next = c->group_next;
	if (c->group_next)
		c->group_next->group_prev = c->group_prev;
	c->group_prev = NULL;
	c->group_next = NULL;
	c->isgroupfocusing = false;
}

void client_group_replace(Client *old, Client *new) {
	client_group_detach(new);

	new->group_prev = old->group_prev;
	new->group_next = old->group_next;
	if (old->group_prev)
		old->group_prev->group_next = new;
	if (old->group_next)
		old->group_next->group_prev = new;
	old->group_prev = NULL;
	old->group_next = NULL;

	if (old->is_logic_hide || (!new->group_prev && !new->group_next)) {
		new->isgroupfocusing = false;
	} else {
		new->isgroupfocusing = old->isgroupfocusing;
	}
}

void mango_surface_frame_done(struct wlr_surface *surface, int sx, int sy,
							  void *data) {
	(void)sx;
	(void)sy;
	wlr_surface_send_frame_done(surface, data);
}
// Feeds frame callbacks to all surfaces (including subsurfaces) of hidden
// windows so clients keep rendering in overview previews (stops frame callback
// throttling). wlr_scene_node_for_each_buffer cannot walk the original
// scene_surface tree: after snapshotting it is disabled, and scenefx skips
// disabled nodes (wlr_scene.c scene_node_for_each_scene_buffer), so no surface
// gets fed and ordinary windows stall without frame callbacks.
void client_send_frame_done(Client *c, const struct timespec *now) {
	struct wlr_surface *s = client_surface(c);
	if (!s)
		return;
	wlr_surface_for_each_surface(s, mango_surface_frame_done, (void *)now);
}

bool client_force_render(Client *c) {
	if (!c || !c->mon || c->iskilling || !client_surface(c)->mapped ||
		c->scene->node.enabled)
		return false;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	client_send_frame_done(c, &now);
	return true;
}
/*
 * Gets the monitor of the current XWayland client (falls back to the selected
 * monitor when not bound yet).
 */
#ifdef XWAYLAND
Monitor *xwayland_monitor(Client *c) {
	Monitor *m = c ? c->mon : NULL;
	if (!m)
		m = server.selected_monitor;
	return m;
}

/* X11 coordinate scale relative to logical coordinates: monitor scale with fzs,
 * otherwise 1. */
float xwayland_client_scale(Client *c) {
	if (config.xwayland_ignore_scale) {
		Monitor *m = xwayland_monitor(c);
		/* Uses a float scale so windows display exactly 1:1. */
		return m ? m->wlr_output->scale : 1.0f;
	}
	return 1.0f;
}

/* Tells X11 clients at which resolution to render. */
float xwayland_preferred_scale(Client *c) {
	if (config.xwayland_ignore_scale)
		return 1.0f;
	Monitor *m = xwayland_monitor(c);
	return m ? m->wlr_output->scale : 1.0f;
}

/* Updates the XWayland scale and notifies the client. */
void xwayland_apply_scale(Client *c) {
	if (!client_is_x11(c) || !client_surface(c))
		return;
	c->xwayland_scale = xwayland_client_scale(c);
	client_set_scale(client_surface(c), xwayland_preferred_scale(c));
}

/* Wayland logical coordinates -> X11 physical size (X11 = logical * scale). */
void xwayland_logical_to_x11(struct wlr_box *box, float scale) {
	if (scale <= 0.f)
		scale = 1.f;
	box->x = (int32_t)roundf(box->x * scale);
	box->y = (int32_t)roundf(box->y * scale);
	box->width = (int32_t)roundf(box->width * scale);
	box->height = (int32_t)roundf(box->height * scale);
}

/* X11 physical size -> Wayland logical coordinates (logical = X11 / scale). */
void xwayland_x11_to_logical(struct wlr_box *box, float scale) {
	if (scale <= 0.f)
		scale = 1.f;
	box->x = (int32_t)roundf(box->x / scale);
	box->y = (int32_t)roundf(box->y / scale);
	box->width = (int32_t)roundf(box->width / scale);
	box->height = (int32_t)roundf(box->height / scale);
}

void fix_xwayland_coordinate(struct wlr_box *geom) {
	if (!server.selected_monitor)
		return;

	// 1. If the window is already inside the currently active monitor, return.
	if (geom->x >= server.selected_monitor->m.x &&
		geom->x <=
			server.selected_monitor->m.x + server.selected_monitor->m.width &&
		geom->y >= server.selected_monitor->m.y &&
		geom->y <=
			server.selected_monitor->m.y + server.selected_monitor->m.height)
		return;

	geom->x = server.selected_monitor->m.x +
			  (server.selected_monitor->m.width - geom->width) / 2;
	geom->y = server.selected_monitor->m.y +
			  (server.selected_monitor->m.height - geom->height) / 2;
}

void handle_xwayland_surface_request_activate(struct wl_listener *listener,
											  void *data) {
	Client *c = wl_container_of(listener, c, activate);
	bool need_arrange = false;

	if (!c || c->iskilling || !c->foreign_toplevel || client_is_unmanaged(c))
		return;

	if (c && c->swallowdby)
		return;

	if (c->isminimized) {
		client_pending_minimized_state(c, 0);
		c->tags = c->mini_restore_tag;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		client_update_border_color(c);
		if (VISIBLEON(c, c->mon)) {
			need_arrange = true;
		}
	}

	if (config.focus_on_activate && !c->istagsilent &&
		c != server.selected_monitor->sel) {
		if (!(c->mon == server.selected_monitor &&
			  c->tags & c->mon->tagset[c->mon->seltags]))
			client_view_on_monitor(&(Arg){.ui = c->tags}, true, c->mon, true);
		wlr_xwayland_surface_activate(c->surface.xwayland, 1);
		client_focus(c, 1);
		need_arrange = true;
	} else if (c != client_focus_top(server.selected_monitor)) {
		c->isurgent = 1;
		if (client_surface(c)->mapped)
			client_update_border_color(c);
	}

	if (need_arrange) {
		arrange(c->mon, false, false);
	}

	printstatus(IPC_WATCH_ARRANGGE);
}

void handle_xwayland_surface_request_configure(struct wl_listener *listener,
											   void *data) {
	Client *c = wl_container_of(listener, c, configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	struct wlr_box new_geo;
	new_geo.x = event->x;
	new_geo.y = event->y;
	new_geo.width = event->width;
	new_geo.height = event->height;
	/* event is in X11 physical sizes; convert back to Wayland logical
	 * coordinates. */
	xwayland_x11_to_logical(&new_geo, c->xwayland_scale);
	fix_xwayland_coordinate(&new_geo);

	if (!client_surface(c) || !client_surface(c)->mapped) {
		struct wlr_box xgeo = new_geo;
		xwayland_logical_to_x11(&xgeo, c->xwayland_scale);
		wlr_xwayland_surface_configure(c->surface.xwayland, xgeo.x, xgeo.y,
									   xgeo.width, xgeo.height);
		return;
	}

	if (client_is_unmanaged(c)) {
		struct wlr_box xgeo = new_geo;
		xwayland_logical_to_x11(&xgeo, c->xwayland_scale);
		wlr_scene_node_set_position(&c->scene->node, new_geo.x, new_geo.y);
		wlr_xwayland_surface_configure(c->surface.xwayland, xgeo.x, xgeo.y,
									   xgeo.width, xgeo.height);
		return;
	}

	if (c->isfloating && c != server.grab_client) {
		new_geo.x = new_geo.x - c->bw;
		new_geo.y = new_geo.y - c->bw;
		new_geo.width = new_geo.width + c->bw * 2;
		new_geo.height = new_geo.height + c->bw * 2;
		fix_xwayland_coordinate(&new_geo);

		resize(c,
			   (struct wlr_box){.x = new_geo.x,
								.y = new_geo.y,
								.width = new_geo.width,
								.height = new_geo.height},
			   0);
	} else {
		arrange(c->mon, false, false);
	}
}

void handle_new_xwayland_surface(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = data;
	Client *c = NULL;

	/* Allocate a Client for this surface */
	c = xsurface->data = ecalloc(1, sizeof(*c));
	c->surface.xwayland = xsurface;
	c->type = X11;
	/* Listen to the various events it can emit */
	LISTEN(&xsurface->events.associate, &c->associate,
		   handle_xwayland_surface_associate);
	LISTEN(&xsurface->events.destroy, &c->destroy, handle_client_destroy);
	LISTEN(&xsurface->events.dissociate, &c->dissociate,
		   handle_xwayland_surface_dissociate);
	LISTEN(&xsurface->events.request_activate, &c->activate,
		   handle_xwayland_surface_request_activate);
	LISTEN(&xsurface->events.request_configure, &c->configure,
		   handle_xwayland_surface_request_configure);
	LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen,
		   handle_client_request_fullscreen);
	LISTEN(&xsurface->events.set_hints, &c->set_hints,
		   handle_xwayland_surface_set_hints);
	LISTEN(&xsurface->events.set_title, &c->set_title, handle_client_set_title);
	LISTEN(&xsurface->events.request_maximize, &c->maximize,
		   handle_client_request_maximize);
	LISTEN(&xsurface->events.request_minimize, &c->minimize,
		   handle_client_request_minimize);
}

void handle_xwayland_surface_commit(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, commmitx11);
	struct wlr_surface_state *state = &c->surface.xwayland->surface->current;

	/* Overview card nodes are independent scene_surfaces that auto-update on
	 * commit. */

	/*
	 * state->width/height and xwayland->x/y are X11 physical sizes (= c->geom *
	 * scale); convert to logical before scene operations.
	 */
	float xscale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;
	int32_t xw = (int32_t)roundf((c->geom.width - 2 * (int32_t)c->bw) * xscale);
	int32_t xh =
		(int32_t)roundf((c->geom.height - 2 * (int32_t)c->bw) * xscale);
	int32_t xx = (int32_t)roundf((c->geom.x + (int32_t)c->bw) * xscale);
	int32_t xy = (int32_t)roundf((c->geom.y + (int32_t)c->bw) * xscale);

	if (xw == (int32_t)state->width && xh == (int32_t)state->height &&
		(int32_t)c->surface.xwayland->x == xx &&
		(int32_t)c->surface.xwayland->y == xy) {
		c->configure_serial = 0;
	}

	/* After scene processing, force the root surface to display its logical
	 * size. */
	client_update_xwayland_dest_size(c);
}

void handle_xwayland_surface_associate(struct wl_listener *listener,
									   void *data) {
	Client *c = wl_container_of(listener, c, associate);

	LISTEN(&client_surface(c)->events.map, &c->map, handle_client_map);
	LISTEN(&client_surface(c)->events.unmap, &c->unmap, handle_client_unmap);
}

void handle_xwayland_surface_dissociate(struct wl_listener *listener,
										void *data) {
	Client *c = wl_container_of(listener, c, dissociate);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
	c->xwl_root_buffer = NULL;
	c->xwl_clip_active = false;
}

void handle_xwayland_surface_set_hints(struct wl_listener *listener,
									   void *data) {
	Client *c = wl_container_of(listener, c, set_hints);
	struct wlr_surface *surface = client_surface(c);
	if (c == client_focus_top(server.selected_monitor) || !c ||
		!c->surface.xwayland->hints)
		return;

	c->isurgent = xcb_icccm_wm_hints_get_urgency(c->surface.xwayland->hints);
	printstatus(IPC_WATCH_ARRANGGE);

	if (c->isurgent && surface && surface->mapped)
		client_update_border_color(c);
}

void handle_xwayland_ready(struct wl_listener *listener, void *data) {
	struct wlr_xcursor *xcursor;

	/* assign the one and only seat */
	wlr_xwayland_set_seat(server.xwayland, server.seat);

	/* The default cursor is loaded at the monitor scale to avoid upscaling
	 * under HiDPI. */
	float cursor_scale =
		server.selected_monitor &&
				server.selected_monitor->wlr_output->scale > 0.f
			? server.selected_monitor->wlr_output->scale
			: 1.f;
	if ((xcursor = wlr_xcursor_manager_get_xcursor(server.cursor_manager,
												   "default", cursor_scale))) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		struct wlr_buffer *buffer = wlr_xcursor_image_get_buffer(image);
		wlr_xwayland_set_cursor(server.xwayland, buffer,
								xcursor->images[0]->hotspot_x,
								xcursor->images[0]->hotspot_y);
	}

	/* xwayland can't auto sync the keymap, so we do it manually
	  and we need to wait the xwayland completely inited
	 */
	wl_event_source_timer_update(server.sync_keymap, 500);
}

void handle_xwayland_surface_set_geometry(struct wl_listener *listener,
										  void *data) {
	Client *c = wl_container_of(listener, c, set_geometry);
	struct wlr_box geo = {
		.x = c->surface.xwayland->x,
		.y = c->surface.xwayland->y,
		.width = c->surface.xwayland->width,
		.height = c->surface.xwayland->height,
	};
	/* xwayland->x/y are X11 physical sizes; convert back to Wayland logical
	 * coordinates. */
	xwayland_x11_to_logical(&geo, c->xwayland_scale);
	wlr_scene_node_set_position(&c->scene->node, geo.x, geo.y);
	pointer_process_motion(0, NULL, 0, 0, 0, 0);
}

#endif
