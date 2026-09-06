#include "mango/mango.h"
#include "mango/manage/client.h"
#include "mango/layout/arrange.h"
#include "mango/ext-protocol/foreign-toplevel.h"
#include "mango/common/server.h"

void handle_foreign_activate_request(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_activate_request);

	client_active(c);
}

void handle_foreign_maximize_request(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_maximize_request);
	struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;

	if (c->swallowdby || !c->mon)
		return;

	if (c->ismaximizescreen && !event->maximized) {
		client_set_maximize_screen(c, 0, true);
		return;
	}

	if (!c->ismaximizescreen && event->maximized) {
		client_set_maximize_screen(c, 1, true);
		return;
	}
}
void handle_foreign_minimize_request(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_minimize_request);
	struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;

	if (c->swallowdby || !c->mon)
		return;

	if (!c->isminimized && event->minimized) {
		set_minimized(c);
		return;
	}

	if (c->isminimized && !event->minimized) {
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		c->is_scratchpad_show = 0;
		client_update_border_color(c);
		show_hide_client(c);
		arrange(c->mon, true, false);
		return;
	}
}
void handle_foreign_fullscreen_request(struct wl_listener *listener,
									   void *data) {
	Client *c = wl_container_of(listener, c, foreign_fullscreen_request);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;

	if (c->swallowdby || !c->mon)
		return;

	if (c->isfullscreen && !event->fullscreen) {
		client_apply_fullscreen(c, 0, true);
		return;
	}

	if (!c->isfullscreen && event->fullscreen) {
		client_apply_fullscreen(c, 1, true);
		return;
	}
}

void handle_foreign_close_request(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_close_request);
	pending_kill_client(c);
}
void handle_foreign_destroy(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_destroy);
	wl_list_remove(&c->foreign_activate_request.link);
	wl_list_remove(&c->foreign_minimize_request.link);
	wl_list_remove(&c->foreign_maximize_request.link);
	wl_list_remove(&c->foreign_fullscreen_request.link);
	wl_list_remove(&c->foreign_close_request.link);
	wl_list_remove(&c->foreign_destroy.link);
	c->foreign_toplevel = NULL;
}
void add_foreign_toplevel(Client *c) {
	if (!c || !c->mon || !c->mon->wlr_output || !c->mon->wlr_output->enabled)
		return;

	c->foreign_toplevel =
		wlr_foreign_toplevel_handle_v1_create(server.foreign_toplevel_manager);
	// Listens for external requests on the window.
	if (c->foreign_toplevel) {
		LISTEN(&(c->foreign_toplevel->events.request_activate),
			   &c->foreign_activate_request, handle_foreign_activate_request);
		LISTEN(&(c->foreign_toplevel->events.request_minimize),
			   &c->foreign_minimize_request, handle_foreign_minimize_request);
		LISTEN(&(c->foreign_toplevel->events.request_maximize),
			   &c->foreign_maximize_request, handle_foreign_maximize_request);
		LISTEN(&(c->foreign_toplevel->events.request_fullscreen),
			   &c->foreign_fullscreen_request,
			   handle_foreign_fullscreen_request);
		LISTEN(&(c->foreign_toplevel->events.request_close),
			   &c->foreign_close_request, handle_foreign_close_request);
		LISTEN(&(c->foreign_toplevel->events.destroy), &c->foreign_destroy,
			   handle_foreign_destroy);
		// Sets the external toplevel handle id to the app id.
		const char *appid;
		appid = client_get_appid(c);
		if (appid)
			wlr_foreign_toplevel_handle_v1_set_app_id(c->foreign_toplevel,
													  appid);
		// Sets the external toplevel handle title to the app title.
		const char *title;
		title = client_get_title(c);
		if (title)
			wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel,
													 title);
		// Sets the external toplevel handle monitor to the current monitor.
		wlr_foreign_toplevel_handle_v1_output_enter(c->foreign_toplevel,
													c->mon->wlr_output);
	}
}
void reset_foreign_tolevel(Client *c, Monitor *oldmon, Monitor *newmon) {
	if (!c)
		return;

	if (!c->foreign_toplevel) {
		add_foreign_toplevel(c);
		return;
	}

	if (oldmon == newmon)
		return;

	if (oldmon)
		wlr_foreign_toplevel_handle_v1_output_leave(c->foreign_toplevel,
													oldmon->wlr_output);

	if (newmon)
		wlr_foreign_toplevel_handle_v1_output_enter(c->foreign_toplevel,
													newmon->wlr_output);
}
