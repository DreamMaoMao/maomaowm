#ifndef __EXT_PROTOCOL_XDG_OUTPUT_H__
#define __EXT_PROTOCOL_XDG_OUTPUT_H__ 1

/* Custom xdg-output: reports physical coordinates/sizes to XWayland when
 * xwayland_ignore_scale is set. */
#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>

#include "xdg-output-unstable-v1-protocol.h"

#define MANGO_XDG_OUTPUT_MANAGER_VERSION 3
#define MANGO_XDG_OUTPUT_DONE_DEPRECATED_SINCE_VERSION 3
#define MANGO_XDG_OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3

/* A zxdg_output_v1 resource bound by a client. */
struct MangoXDGOutputResource {
	struct wl_resource *resource;
	struct wl_list link; /* xdg_output->resources */
};

/* The xdg-output state corresponding to a wlr_output. */
struct MangoXDGOutput {
	struct wl_list link; /* xdg_outputs */
	struct wlr_output *wlr_output;
	struct wl_list resources; /* MangoXDGOutputResource.link */
	struct {
		struct wl_listener description;
	};
	/*
	 * Last logical values sent to normal (non-XWayland) clients; used to decide
	 * whether a resend is needed so that reload_config and other no-op changes
	 * do not disturb clients that adjust DPI/window size from xdg-output.
	 */
	int32_t last_lx, last_ly, last_lw, last_lh;
	bool sent;
	/*
	 * Last physical values sent to XWayland; used to decide whether XWayland
	 * needs wl_output.done (e.g. when xwayland_ignore_scale changes without a
	 * layout change, logical values stay the same but the values XWayland
	 * receives change).
	 */
	int32_t last_px, last_py, last_pw, last_ph;
	bool xwl_sent;
};

/* Declarations */
/*
 * The XWayland X server is itself a Wayland client; decide dynamically on each
 * send instead of caching at resource creation to avoid XWayland restart/init
 * ordering issues.
 */
bool xdg_output_resource_is_xwayland(struct wl_resource *resource);
/* Computes logical and physical (rotation-aware) values from the current output
 * state. */
void xdg_output_get_values(struct MangoXDGOutput *output, int32_t *lx,
						   int32_t *ly, int32_t *lw, int32_t *lh, int32_t *px,
						   int32_t *py, int32_t *pw, int32_t *ph);
void xdg_output_send_details(struct MangoXDGOutput *output,
							 struct wl_resource *resource);
/* Whether the logical values seen by normal clients differ from the last send.
 */
bool xdg_output_logical_changed(struct MangoXDGOutput *output);
/*
 * Whether the physical values seen by XWayland differ from the last send.
 * The physical baseline is compared independently and updated in place (the X
 * server usually does not bind zxdg_output_v1, so xwl_sent cannot be relied
 * on); done is only re-sent when physical values actually change.
 */
bool xdg_output_xwayland_changed(struct MangoXDGOutput *output);
/*
 * Updates the xdg-output details for this output. XWayland resources are always
 * re-sent (mango coordinates XWayland based on receiving position/size after
 * every layout/config change, even if the values are unchanged); normal clients
 * are only re-sent when logical values change, matching the wlroots standard
 * implementation.
 */
void xdg_output_update(struct MangoXDGOutput *output);
void handle_xdg_output_resource_destroy(struct wl_resource *resource);
void handle_xdg_output_destroy(struct wl_client *client,
							   struct wl_resource *resource);
void handle_xdg_output_manager_destroy(struct wl_client *client,
									   struct wl_resource *resource);
void handle_xdg_output_description(struct wl_listener *listener, void *data);
struct MangoXDGOutput *xdg_output_find(struct wlr_output *wlr_output);
struct MangoXDGOutput *xdg_output_create(struct wlr_output *wlr_output);
void xdg_output_destroy(struct MangoXDGOutput *output);
void handle_xdg_output_manager_get_output(struct wl_client *client,
										  struct wl_resource *manager_resource,
										  uint32_t id,
										  struct wl_resource *output_resource);
void xdg_output_manager_bind(struct wl_client *client, void *data,
							 uint32_t version, uint32_t id);
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
void xdg_output_update_all(void);
/*
 * When an output is removed, makes its xdg-output resources inert instead of
 * destroying them. Called from cleanup_monitor() (the wlr_output destroy
 * listener); idempotent.
 */
void xdg_output_cleanup_output(struct wlr_output *wlr_output);
void xdg_output_init(void);

#endif
