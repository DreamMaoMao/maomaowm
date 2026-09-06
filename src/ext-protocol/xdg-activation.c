#include "mango/ext-protocol/xdg-activation.h"
#include "mango/common/globals.h"
#include "mango/manage/client.h"
#include <stdlib.h>
#include "mango/common/util.h"
#include "mango/ipc/ipc.h"

void handle_xdg_activation_token_destroy(struct wl_listener *listener,
										 void *data) {
	struct mango_xdg_activation_token *token =
		wl_container_of(listener, token, destroy);
	wl_list_remove(&token->destroy.link);
	free(token);
}

void handle_xdg_activation_new_token(struct wl_listener *listener, void *data) {
	struct wlr_xdg_activation_token_v1 *wlr_token = data;

	struct mango_xdg_activation_token *token = ecalloc(1, sizeof(*token));
	if (!token)
		return;

	token->wlr_token = wlr_token;
	token->had_focused_surface = wlr_token->surface != NULL;
	wlr_token->data = token;

	token->destroy.notify = handle_xdg_activation_token_destroy;
	wl_signal_add(&wlr_token->events.destroy, &token->destroy);
}

/* Tokens from spawn are trusted; client tokens need a focused surface. */
bool xdg_activation_token_can_activate(
	struct wlr_xdg_activation_token_v1 *wlr_token) {
	if (!wlr_token)
		return false;
	struct mango_xdg_activation_token *token = wlr_token->data;
	if (token && token->internal)
		return true;
	if (!wlr_token->seat)
		return false;
	return token && token->had_focused_surface;
}

void handle_xdg_activation_request_activate(struct wl_listener *listener,
											void *data) {
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	Client *c = NULL;
	toplevel_from_wlr_surface(event->surface, &c, NULL);

	if (!c || !c->foreign_toplevel)
		return;

	/* activation_bypass skips auth and goes straight to the normal path */
	if (c->activation_bypass) {
		urgent(listener, data);
		return;
	}

	if (xdg_activation_token_can_activate(event->token)) {
		urgent(listener, data);
	} else {
		/* valid token, just not usable for activation; flag urgent,
		 * but leave the focused window alone */
		if (c == focustop(selmon))
			return;
		c->isurgent = 1;
		if (client_surface(c)->mapped)
			setborder_color(c);
		printstatus(IPC_WATCH_ARRANGGE);
	}
}
void handle_xdg_activation_destroy(struct wl_listener *listener, void *data) {
	wl_list_remove(&activation_request_activate_listener.link);
	wl_list_remove(&activation_new_token_listener.link);
	wl_list_remove(&activation_destroy_listener.link);
	activation = NULL;
}

void xdg_activation_init() {
	activation = wlr_xdg_activation_v1_create(dpy);
	if (!activation)
		return;

	activation_request_activate_listener.notify =
		handle_xdg_activation_request_activate;
	wl_signal_add(&activation->events.request_activate,
				  &activation_request_activate_listener);

	activation_new_token_listener.notify = handle_xdg_activation_new_token;
	wl_signal_add(&activation->events.new_token,
				  &activation_new_token_listener);

	activation_destroy_listener.notify = handle_xdg_activation_destroy;
	wl_signal_add(&activation->events.destroy, &activation_destroy_listener);
}

/* Make a token for spawn to export as XDG_ACTIVATION_TOKEN. */
const char *xdg_activation_v1_export_token(void) {
	if (!activation)
		return NULL;
	struct wlr_xdg_activation_token_v1 *wlr_token =
		wlr_xdg_activation_token_v1_create(activation);
	if (!wlr_token)
		return NULL;

	struct mango_xdg_activation_token *token = ecalloc(1, sizeof(*token));
	if (!token) {
		wlr_xdg_activation_token_v1_destroy(wlr_token);
		return NULL;
	}
	token->wlr_token = wlr_token;
	token->had_focused_surface = false;
	token->internal = true;
	wlr_token->data = token;
	token->destroy.notify = handle_xdg_activation_token_destroy;
	wl_signal_add(&wlr_token->events.destroy, &token->destroy);

	return wlr_xdg_activation_token_v1_get_name(wlr_token);
}

struct wlr_xdg_activation_v1 *activation;
struct wl_listener activation_request_activate_listener;
struct wl_listener activation_new_token_listener;
struct wl_listener activation_destroy_listener;
