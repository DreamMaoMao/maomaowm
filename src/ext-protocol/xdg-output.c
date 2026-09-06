#include "mango/ext-protocol/xdg-output.h"
#include "mango/common/server.h"
#include "mango/config/parse_config.h"
#include "mango/manage/monitor.h"
#ifdef XWAYLAND
#include <wlr/xwayland.h>
#endif

static struct wl_global *xdg_output_global;
static struct wl_list xdg_outputs;
static const struct zxdg_output_v1_interface xdg_output_impl;
static const struct zxdg_output_manager_v1_interface xdg_output_manager_impl;

/*
 * The XWayland X server is itself a Wayland client; decide dynamically on each
 * send instead of caching at resource creation to avoid XWayland restart/init
 * ordering issues.
 */
bool xdg_output_resource_is_xwayland(struct wl_resource *resource) {
#ifdef XWAYLAND
	if (server.xwayland && server.xwayland->server &&
		server.xwayland->server->client == wl_resource_get_client(resource))
		return true;
#endif
	return false;
}
/* Computes logical and physical (rotation-aware) values from the current output
 * state. */
void xdg_output_get_values(struct MangoXDGOutput *output, int32_t *lx,
						   int32_t *ly, int32_t *lw, int32_t *lh, int32_t *px,
						   int32_t *py, int32_t *pw, int32_t *ph) {
	int32_t x = 0, y = 0;
	Monitor *m = output->wlr_output->data;
	if (m) {
		x = m->m.x;
		y = m->m.y;
	}

	float scale =
		output->wlr_output->scale > 0.f ? output->wlr_output->scale : 1.f;

	/*
	 * Logical size = transformed (rotation-aware) effective resolution / scale,
	 * kept strictly consistent with wl_output.mode/transform/scale instead of
	 * relying on m->m (layout cache). wlr_output->width/height cannot be used
	 * directly: they are unrotated mode sizes, so with 90/270 rotation the
	 * logical width/height would not swap and tools like grim/slurp that
	 * compute screenshots/selections from xdg-output logical sizes would
	 * capture an unrotated buffer.
	 */
	int32_t tw, th;

	// Gets the physical resolution after rotation, without scaling.
	wlr_output_transformed_resolution(output->wlr_output, &tw, &th);

	// Wayland apps should use the logical resolution, i.e. the scaled
	// width/height.
	int32_t w = (int32_t)roundf(tw / scale);
	int32_t h = (int32_t)roundf(th / scale);

	// Logical resolution/coordinates used by scaled Wayland apps, matching the
	// original wlroots implementation.
	*lx = x;
	*ly = y;
	*lw = w;
	*lh = h;

	// xwayland-scaled coordinates must be mapped back to actual physical
	// monitor coordinates to trick XWayland apps into thinking they are still
	// at their original physical resolution.
	*px = (int32_t)roundf(x * scale);
	*py = (int32_t)roundf(y * scale);
	*pw = tw; // Uses the physical resolution width/height directly.
	*ph = th;
}
void xdg_output_send_details(struct MangoXDGOutput *output,
							 struct wl_resource *resource) {
	int32_t lx, ly, lw, lh, px, py, pw, ph;
	xdg_output_get_values(output, &lx, &ly, &lw, &lh, &px, &py, &pw, &ph);

	int32_t x, y, w, h;
	if (xdg_output_resource_is_xwayland(resource) &&
		config.xwayland_ignore_scale) {
		/*
		 * Tricks XWayland: mango places X11 windows in a physical coordinate
		 * space of logical*scale, so tell XWayland the screen origin is at
		 * logical*scale and the rectangle is physical (rotation-aware).
		 */
		x = px;
		y = py;
		w = pw;
		h = ph;
	} else {
		/* xwayland_ignore_scale=0: use logical coordinates like normal clients.
		 */
		x = lx;
		y = ly;
		w = lw;
		h = lh;
	}

	zxdg_output_v1_send_logical_position(resource, x, y);
	zxdg_output_v1_send_logical_size(resource, w, h);

	if (wl_resource_get_version(resource) <
		MANGO_XDG_OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
		zxdg_output_v1_send_done(resource);

	/* Syncs the last sent logical values for change detection in
	 * xdg_output_update(). */
	output->last_lx = lx;
	output->last_ly = ly;
	output->last_lw = lw;
	output->last_lh = lh;
	output->sent = true;
	if (xdg_output_resource_is_xwayland(resource)) {
		output->last_px = px;
		output->last_py = py;
		output->last_pw = pw;
		output->last_ph = ph;
		output->xwl_sent = true;
	}
}
/* Whether the logical values seen by normal clients differ from the last send.
 */
bool xdg_output_logical_changed(struct MangoXDGOutput *output) {
	int32_t lx, ly, lw, lh, px, py, pw, ph;
	xdg_output_get_values(output, &lx, &ly, &lw, &lh, &px, &py, &pw, &ph);

	/*
	 * Without a baseline (no zxdg_output_v1 resource was ever bound) treat it
	 * as unchanged to avoid scheduling a useless wl_output.done.
	 */
	if (!output->sent)
		return false;

	return output->last_lx != lx || output->last_ly != ly ||
		   output->last_lw != lw || output->last_lh != lh;
}

/*
 * Whether the physical values seen by XWayland differ from the last send.
 * The physical baseline is compared independently and updated in place (the X
 * server usually does not bind zxdg_output_v1, so xwl_sent cannot be relied
 * on); done is only re-sent when physical values actually change.
 */
bool xdg_output_xwayland_changed(struct MangoXDGOutput *output) {
	int32_t lx, ly, lw, lh, px, py, pw, ph;
	xdg_output_get_values(output, &lx, &ly, &lw, &lh, &px, &py, &pw, &ph);

	bool changed = output->last_px != px || output->last_py != py ||
				   output->last_pw != pw || output->last_ph != ph;
	output->last_px = px;
	output->last_py = py;
	output->last_pw = pw;
	output->last_ph = ph;
	return changed;
}

/*
 * Updates the xdg-output details for this output. XWayland resources are always
 * re-sent (mango coordinates XWayland based on receiving position/size after
 * every layout/config change, even when values are unchanged); normal clients
 * are only re-sent when logical values change, matching the wlroots standard
 * implementation.
 */
void xdg_output_update(struct MangoXDGOutput *output) {
	bool logical_changed = xdg_output_logical_changed(output);
	struct MangoXDGOutputResource *res;
	wl_list_for_each(res, &output->resources, link) {
		if (!xdg_output_resource_is_xwayland(res->resource) && !logical_changed)
			continue;
		xdg_output_send_details(output, res->resource);
	}
}

void handle_xdg_output_resource_destroy(struct wl_resource *resource) {
	struct MangoXDGOutputResource *res = wl_resource_get_user_data(resource);
	if (!res)
		return;
	wl_list_remove(&res->link);
	free(res);
}
void handle_xdg_output_destroy(struct wl_client *client,
							   struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

void handle_xdg_output_manager_destroy(struct wl_client *client,
									   struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

/* Output description changed (clients with version >= 3 need a resend). */
void handle_xdg_output_description(struct wl_listener *listener, void *data) {
	struct MangoXDGOutput *output =
		wl_container_of(listener, output, description);
	struct wlr_output *wlr_output = output->wlr_output;
	if (!wlr_output->description)
		return;

	struct MangoXDGOutputResource *res;
	wl_list_for_each(res, &output->resources, link) {
		if (wl_resource_get_version(res->resource) >=
			MANGO_XDG_OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION)
			zxdg_output_v1_send_description(res->resource,
											wlr_output->description);
	}
}
struct MangoXDGOutput *xdg_output_find(struct wlr_output *wlr_output) {
	struct MangoXDGOutput *output;
	wl_list_for_each(output, &xdg_outputs, link) {
		if (output->wlr_output == wlr_output)
			return output;
	}
	return NULL;
}

struct MangoXDGOutput *xdg_output_create(struct wlr_output *wlr_output) {
	struct MangoXDGOutput *output = calloc(1, sizeof(*output));
	if (!output)
		return NULL;
	output->wlr_output = wlr_output;
	wl_list_init(&output->resources);
	output->description.notify = handle_xdg_output_description;
	wl_signal_add(&wlr_output->events.description, &output->description);
	wl_list_insert(&xdg_outputs, &output->link);
	return output;
}

void xdg_output_destroy(struct MangoXDGOutput *output) {
	/*
	 * Makes all resources inert: zxdg_output_v1 is created by clients and only
	 * clients can destroy it. Destroying it directly makes libwayland send
	 * delete_id for that id; if the client reuses the id while its own destroy
	 * request is still in flight, the next request hits an unknown object.
	 */
	struct MangoXDGOutputResource *res, *tmp;
	wl_list_for_each_safe(res, tmp, &output->resources, link) {
		wl_resource_set_user_data(res->resource, NULL);
		wl_list_remove(&res->link);
		free(res);
	}
	wl_list_remove(&output->description.link);
	wl_list_remove(&output->link);
	free(output);
}
void handle_xdg_output_manager_get_output(struct wl_client *client,
										  struct wl_resource *manager_resource,
										  uint32_t id,
										  struct wl_resource *output_resource) {
	struct wlr_output *wlr_output = wlr_output_from_resource(output_resource);

	struct wl_resource *resource =
		wl_resource_create(client, &zxdg_output_v1_interface,
						   wl_resource_get_version(manager_resource), id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	/* Invalid output: keep it inert and send no events. */
	if (!wlr_output) {
		wl_resource_set_implementation(resource, &xdg_output_impl, NULL,
									   handle_xdg_output_resource_destroy);
		return;
	}

	struct MangoXDGOutput *output = xdg_output_find(wlr_output);
	if (!output)
		output = xdg_output_create(wlr_output);
	if (!output) {
		wl_client_post_no_memory(client);
		wl_resource_destroy(resource);
		return;
	}

	struct MangoXDGOutputResource *res = calloc(1, sizeof(*res));
	if (!res) {
		wl_client_post_no_memory(client);
		wl_resource_destroy(resource);
		return;
	}
	res->resource = resource;
	wl_list_insert(&output->resources, &res->link);
	wl_resource_set_implementation(resource, &xdg_output_impl, res,
								   handle_xdg_output_resource_destroy);

	uint32_t version = wl_resource_get_version(resource);
	if (version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION && wlr_output->name)
		zxdg_output_v1_send_name(resource, wlr_output->name);
	if (version >= ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION &&
		wlr_output->description)
		zxdg_output_v1_send_description(resource, wlr_output->description);

	xdg_output_send_details(output, resource);

	if (wl_resource_get_version(output_resource) >=
			WL_OUTPUT_DONE_SINCE_VERSION &&
		version >= MANGO_XDG_OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
		wl_output_send_done(output_resource);
}
void xdg_output_manager_bind(struct wl_client *client, void *data,
							 uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(
		client, &zxdg_output_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &xdg_output_manager_impl, NULL,
								   NULL);
}
/*
 * Updates xdg-output details for all outputs and schedules wl_output.done when
 * values actually change. done is the transaction boundary at which clients
 * apply wl_output/xdg-output changes: wlroots only schedules it itself on
 * mode/scale/geometry changes, so pure layout moves need it added here. done is
 * broadcast to every wl_output client on the output, so it is only sent when
 * values change; if only the XWayland-side values changed (logical layout
 * unchanged), done is sent only to the XWayland wl_output resource to avoid
 * disturbing normal clients.
 */
void xdg_output_update_all(void) {
	struct MangoXDGOutput *output, *tmp;
	wl_list_for_each_safe(output, tmp, &xdg_outputs, link) {
		bool logical_changed = xdg_output_logical_changed(output);
		bool xwayland_changed = xdg_output_xwayland_changed(output);

		xdg_output_update(output);

		if (logical_changed) {
			wlr_output_schedule_done(output->wlr_output);
		} else if (xwayland_changed) {
			struct wl_resource *wl_res;
			wl_resource_for_each(wl_res, &output->wlr_output->resources) {
				if (wl_resource_get_version(wl_res) >=
						WL_OUTPUT_DONE_SINCE_VERSION &&
					xdg_output_resource_is_xwayland(wl_res))
					wl_output_send_done(wl_res);
			}
		}
	}
}
/*
 * When an output is removed, makes its xdg-output resources inert instead of
 * destroying them. Called from cleanup_monitor() (the wlr_output destroy
 * listener); idempotent.
 */
void xdg_output_cleanup_output(struct wlr_output *wlr_output) {
	struct MangoXDGOutput *output = xdg_output_find(wlr_output);
	if (output)
		xdg_output_destroy(output);
}

void xdg_output_init(void) {
	wl_list_init(&xdg_outputs);
	xdg_output_global = wl_global_create(
		server.display, &zxdg_output_manager_v1_interface,
		MANGO_XDG_OUTPUT_MANAGER_VERSION, NULL, xdg_output_manager_bind);
	if (!xdg_output_global)
		wlr_log(WLR_ERROR, "failed to create zxdg_output_manager_v1 global");
}

static struct wl_global *xdg_output_global;
static struct wl_list xdg_outputs;

static const struct zxdg_output_v1_interface xdg_output_impl = {
	.destroy = handle_xdg_output_destroy,
};

static const struct zxdg_output_manager_v1_interface xdg_output_manager_impl = {
	.destroy = handle_xdg_output_manager_destroy,
	.get_xdg_output = handle_xdg_output_manager_get_output,
};
