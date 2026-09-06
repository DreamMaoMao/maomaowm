#include "mango/manage/misc.h"
#include "mango/manage/client.h"
#include "mango/manage/layer.h"
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/data/static_keymap.h"
#include "mango/input/pointer.h"
#include "mango/layout/arrange.h"
#include "mango/common/util.h"
#include "mango/manage/monitor.h"

#include <ctype.h>

static const LayoutMapping layout_mappings[] = {
	{"English (US)", "us"},
	{"English (UK)", "gb"},
	{"Russian", "ru"},
	{"German", "de"},
	{"French", "fr"},
	{"Spanish", "es"},
	{"Italian", "it"},
	{"Japanese", "jp"},
	{"Chinese", "cn"},
	{"Korean", "kr"},
	{"Arabic", "ar"},
	{"Hebrew", "il"},
	{"Greek", "gr"},
	{"Turkish", "tr"},
	{"Portuguese", "pt"},
	{"Portuguese (Brazil)", "br"},
	{"Swedish", "se"},
	{"Norwegian", "no"},
	{"Danish", "dk"},
	{"Finnish", "fi"},
	{"Polish", "pl"},
	{"Czech", "cz"},
	{"Hungarian", "hu"},
	{"Ukrainian", "ua"},
	{"Belarusian", "by"},
	{"Bulgarian", "bg"},
	{"Croatian", "hr"},
	{"Romanian", "ro"},
	{"Serbian", "rs"},
	{"Slovak", "sk"},
	{"Slovenian", "si"},
	{"Estonian", "ee"},
	{"Latvian", "lv"},
	{"Lithuanian", "lt"},
	{"Dutch", "nl"},
	{"Flemish", "be"},
	{"Swiss German", "ch"},
	{"French (Canada)", "ca"},
	{"French (Switzerland)", "ch-fr"},
	{"Icelandic", "is"},
	{"Maltese", "mt"},
	{"Irish", "ie"},
	{"Albanian", "al"},
	{"Macedonian", "mk"},
	{"Bosnian", "ba"},
	{"Montenegrin", "me"},
	{"Dvorak", "dv"},
	{"Colemak", "cm"},
	{"Workman", "wm"},
	{"Norman", "nm"},
	{"QGMLWY", "qg"},
	{"AZERTY", "az"},
	{"QWERTZ", "qz"},
	{"BÉPO (French ergonomic)", "bepo"},
	{"Neo", "neo"},
	{"Turkish F", "trf"},
	{"Tibetan", "bo"},
	{"Thai", "th"},
	{"Vietnamese", "vn"},
	{"Lao", "la"},
	{"Khmer", "kh"},
	{"Hindi", "in"},
	{"Persian", "ir"},
	{"Urdu", "pk"},
	{"Bangla", "bd"},
	{"Sinhala", "lk"},
	{"Nepali", "np"},
	{"Tamil", "ta"},
	{"Telugu", "te"},
	{"Kannada", "kn"},
	{"Malayalam", "ml"},
	{NULL, NULL} // End marker.
};

pid_t get_parent_process(pid_t p) {
	uint32_t v = 0;

	FILE *f;
	char buf[256];
	snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

	if (!(f = fopen(buf, "r")))
		return 0;

	// Checks the fscanf return value to ensure one argument was read.
	if (fscanf(f, "%*u %*s %*c %u", &v) != 1) {
		fclose(f);
		return 0;
	}

	fclose(f);

	return (pid_t)v;
}

int32_t is_descendant_process(pid_t p, pid_t c) {
	while (p != c && c != 0)
		c = get_parent_process(c);

	return (int32_t)c;
}

void get_layout_abbr(char *abbr, const char *full_name) {
	// Clears the output buffer.
	abbr[0] = '\0';

	// 1. Try to find the name in the mapping table.
	for (int32_t i = 0; layout_mappings[i].full_name != NULL; i++) {
		if (strcmp(full_name, layout_mappings[i].full_name) == 0) {
			strcpy(abbr, layout_mappings[i].abbr);
			return;
		}
	}

	// 2. Try to extract from the name and convert to lowercase.
	const char *open = strrchr(full_name, '(');
	const char *close = strrchr(full_name, ')');
	if (open && close && close > open) {
		uint32_t len = close - open - 1;
		if (len > 0 && len <= 4) {
			// Extracts and converts to lowercase.
			for (uint32_t j = 0; j < len; j++) {
				abbr[j] = tolower(open[j + 1]);
			}
			abbr[len] = '\0';
			return;
		}
	}

	// 3. Take the first 2-3 letters and convert to lowercase.
	uint32_t j = 0;
	for (uint32_t i = 0; full_name[i] != '\0' && j < 3; i++) {
		if (isalpha(full_name[i])) {
			abbr[j++] = tolower(full_name[i]);
		}
	}
	abbr[j] = '\0';

	// Ensures at least 2 characters.
	if (j >= 2) {
		return;
	}

	// 4. Fallback: use the lowercase first letter.
	if (j == 1) {
		abbr[1] = full_name[1] ? tolower(full_name[1]) : '\0';
		abbr[2] = '\0';
	} else {
		// 5. Final fallback: return "xx".
		strcpy(abbr, "xx");
	}
}

Client *client_at_point(double x, double y) {
	Client *c = NULL, *tmp = NULL;
	wl_list_for_each_safe(c, tmp, &server.clients, link) {
		if (VISIBLEON(c, c->mon) && c->animation.current.x <= x &&
			c->animation.current.y <= y &&
			c->animation.current.x + c->animation.current.width >= x &&
			c->animation.current.y + c->animation.current.height >= y) {
			return c;
		}
	}
	return NULL;
}
bool layer_ignores_focus(LayerSurface *l) {
	if (!l || !l->layer_surface)
		return true;
	struct wlr_surface *s = l->layer_surface->surface;
	return !pixman_region32_not_empty(&s->input_region) ||
		   l->layer_surface->current.keyboard_interactive ==
			   ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
}

void node_at_point(double x, double y, struct wlr_surface **psurface,
				   Client **pc, LayerSurface **pl, MangoGroupBar **gb,
				   double *nx, double *ny) {
	struct wlr_scene_node *node = NULL, *pnode = NULL;
	struct wlr_surface *surface = NULL;
	Client *c = NULL;
	LayerSurface *l = NULL;
	MangoGroupBar *mangogroupbar = NULL;
	int32_t layer;
	Client *ovc = NULL;

	if (psurface)
		*psurface = NULL;
	if (pc)
		*pc = NULL;
	if (pl)
		*pl = NULL;
	if (gb)
		*gb = NULL;

	for (layer = NUM_LAYERS - 1; layer >= 0; layer--) {
		if (layer == LyrFadeOut)
			continue;

		node = wlr_scene_node_at(&server.layers[layer]->node, x, y, nx, ny);
		if (!node)
			continue;

		Monitor *cm = monitor_at_point(x, y);
		if (cm && cm->special_dim_rect && cm->special_dim_rect->node.enabled &&
			node == &cm->special_dim_rect->node) {
			c = NULL;
			l = NULL;
			surface = NULL;
			mangogroupbar = NULL;
			break;
		}

		if (node->type == WLR_SCENE_NODE_BUFFER) {
			struct wlr_scene_surface *scene_surface =
				wlr_scene_surface_try_from_buffer(
					wlr_scene_buffer_from_node(node));
			if (scene_surface) {
				surface = scene_surface->surface;
			}
		}

		if (layer == LyrIMPopup) {
			c = NULL;
			l = NULL;
		} else {
			void *data = NULL;
			for (pnode = node; pnode; pnode = &pnode->parent->node) {
				if (pnode->data) {
					data = pnode->data;
					break;
				}
			}

			if (data) {
				Client *temp_c = (Client *)data;
				if (temp_c->type == LayerShell) {
					l = (LayerSurface *)temp_c;
				} else if (temp_c->type == GroupBar) {
					mangogroupbar = (MangoGroupBar *)temp_c;
				} else if (temp_c->type == XDGShell || temp_c->type == X11) {
					c = temp_c;
				}
			}
		}

		if (node->type == WLR_SCENE_NODE_RECT) {
			if (c && (c->type == XDGShell || c->type == X11)) {
				surface = client_surface(c);
			}

			if (l && l->type == LayerShell) {
				surface = l->layer_surface->surface;
			}
		}

		break;
	}

	if (psurface)
		*psurface = surface;
	if (pc)
		*pc = c;
	if (pl)
		*pl = l;
	if (gb)
		*gb = mangogroupbar;

	if (server.selected_monitor && server.selected_monitor->isoverview) {
		ovc = client_at_point(x, y);

		if (ovc && (!l || layer_ignores_focus(l))) {
			if (pc)
				*pc = ovc;
			if (psurface)
				*psurface = ovc ? client_surface(ovc) : NULL;
			if (pl)
				*pl = NULL;
			if (gb)
				*gb = NULL;
		}
	}
}
/*
 * Extra protocol: xdg-decoration, session lock, drm lease, image capture,
 * idle inhibit, and seat selection (clipboard) misc protocol handling.
 */
void check_idle_inhibitor(struct wlr_surface *exclude) {
	int32_t inhibited = 0;
	Client *c = NULL;
	struct wlr_surface *surface = NULL;
	struct wlr_idle_inhibitor_v1 *inhibitor;

	wl_list_for_each(inhibitor, &server.idle_inhibit_manager->inhibitors,
					 link) {
		surface = wlr_surface_get_root_surface(inhibitor->surface);

		if (exclude == surface) {
			continue;
		}

		toplevel_from_wlr_surface(inhibitor->surface, &c, NULL);

		if (config.idleinhibit_ignore_visible) {
			inhibited = 1;
			break;
		}

		struct wlr_scene_tree *tree = surface->data;
		if (!tree || (tree->node.enabled && (!c || !c->animation.tagouting))) {
			inhibited = 1;
			break;
		}
	}

	wlr_idle_notifier_v1_set_inhibited(server.idle_notifier, inhibited);
}

void handle_xdg_decoration_destroy(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, destroy_decoration);

	wl_list_remove(&c->destroy_decoration.link);
	wl_list_remove(&c->set_decoration_mode.link);
	c->decoration = NULL;
}

void handle_new_xdg_decoration(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;
	Client *c = deco->toplevel->base->data;
	c->decoration = deco;

	LISTEN(&deco->events.request_mode, &c->set_decoration_mode,
		   handle_xdg_decoration_mode_request);
	LISTEN(&deco->events.destroy, &c->destroy_decoration,
		   handle_xdg_decoration_destroy);

	handle_xdg_decoration_mode_request(&c->set_decoration_mode, deco);
}

void handle_new_idle_inhibitor(struct wl_listener *listener, void *data) {
	struct wlr_idle_inhibitor_v1 *idle_inhibitor = data;
	LISTEN_STATIC(&idle_inhibitor->events.destroy,
				  handle_idle_inhibitor_destroy);

	check_idle_inhibitor(NULL);
}

void handle_session_lock_new_surface(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	Monitor *m = lock_surface->output->data;
	struct wlr_scene_tree *scene_tree = lock_surface->surface->data =
		wlr_scene_subsurface_tree_create(lock->scene, lock_surface->surface);
	m->lock_surface = lock_surface;

	wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
	wlr_session_lock_surface_v1_configure(lock_surface, m->m.width,
										  m->m.height);

	LISTEN(&lock_surface->events.destroy, &m->destroy_lock_surface,
		   handle_session_lock_surface_destroy);

	if (m == server.selected_monitor)
		client_notify_enter(lock_surface->surface,
							wlr_seat_get_keyboard(server.seat));
}

void handle_idle_inhibitor_destroy(struct wl_listener *listener, void *data) {
	/* `data` is the wlr_surface of the idle inhibitor being destroyed,
	 * at this point the idle inhibitor is still in the list of the manager
	 */
	check_idle_inhibitor(wlr_surface_get_root_surface(data));
	wl_list_remove(&listener->link);
	free(listener);
}

void session_lock_cleanup(SessionLock *lock, int32_t unlock) {
	wlr_seat_keyboard_notify_clear_focus(server.seat);
	if ((server.session_locked = !unlock))
		goto destroy;

	if (server.locked_bg->node.enabled) {
		wlr_scene_node_set_enabled(&server.locked_bg->node, false);
	}

	client_focus(client_focus_top(server.selected_monitor), 1);
	pointer_process_motion(0, NULL, 0, 0, 0, 0);

destroy:
	wl_list_remove(&lock->new_surface.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->destroy.link);

	wlr_scene_node_destroy(&lock->scene->node);
	server.current_lock = NULL;
	free(lock);
}

void handle_session_lock_surface_destroy(struct wl_listener *listener,
										 void *data) {
	Monitor *m = wl_container_of(listener, m, destroy_lock_surface);
	struct wlr_session_lock_surface_v1 *surface,
		*lock_surface = m->lock_surface;

	m->lock_surface = NULL;
	wl_list_remove(&m->destroy_lock_surface.link);

	if (lock_surface->surface != server.seat->keyboard_state.focused_surface) {
		if (server.exclusive_focus && !server.session_locked) {
			reset_exclusive_layers_focus(m);
		}
		return;
	}

	if (server.session_locked && server.current_lock &&
		!wl_list_empty(&server.current_lock->surfaces)) {
		surface =
			wl_container_of(server.current_lock->surfaces.next, surface, link);
		client_notify_enter(surface->surface,
							wlr_seat_get_keyboard(server.seat));
	} else if (!server.session_locked) {
		reset_exclusive_layers_focus(server.selected_monitor);
	} else {
		wlr_seat_keyboard_clear_focus(server.seat);
	}
}

void handle_session_lock_destroy(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, destroy);
	session_lock_cleanup(lock, 0);
}

void handle_new_session_lock(struct wl_listener *listener, void *data) {
	struct wlr_session_lock_v1 *session_lock = data;
	SessionLock *lock;
	if (!config.allow_lock_transparent) {
		wlr_scene_node_set_enabled(&server.locked_bg->node, true);
	}
	if (server.current_lock) {
		wlr_session_lock_v1_destroy(session_lock);
		return;
	}
	lock = session_lock->data = ecalloc(1, sizeof(*lock));
	client_focus(NULL, 0);

	lock->scene = wlr_scene_tree_create(server.layers[LyrBlock]);
	server.current_lock = lock->lock = session_lock;
	server.session_locked = 1;

	LISTEN(&session_lock->events.new_surface, &lock->new_surface,
		   handle_session_lock_new_surface);
	LISTEN(&session_lock->events.destroy, &lock->destroy,
		   handle_session_lock_destroy);
	LISTEN(&session_lock->events.unlock, &lock->unlock,
		   handle_session_lock_unlock);

	wlr_session_lock_v1_send_locked(session_lock);
}
void handle_new_foreign_toplevel_capture_request(struct wl_listener *listener,
												 void *data) {
	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request
		*request = data;
	Client *c = request->toplevel_handle->data;

	if (c->shield_when_capture)
		return;

	if (c->image_capture_source == NULL) {
		c->image_capture_source =
			wlr_ext_image_capture_source_v1_create_with_scene_node(
				&c->image_capture_scene->tree.node, server.event_loop,
				server.allocator, server.renderer);
		if (c->image_capture_source == NULL) {
			return;
		}
	}

	wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
		request, c->image_capture_source);
}

// Callback when a capture session is destroyed.
void handle_session_destroy(struct wl_listener *listener, void *data) {
	struct capture_session_tracker *tracker =
		wl_container_of(listener, tracker, session_destroy);
	server.active_capture_count--;
	wl_list_remove(&tracker->session_destroy.link);

	Client *c = NULL;
	wl_list_for_each(c, &server.clients, link) {
		if (c->shield_when_capture && !c->iskilling && VISIBLEON(c, c->mon)) {
			arrange(c->mon, false, false);
		}
	}

	mango_error(true, WLR_DEBUG, "Capture session ended, active count: %d",
				server.active_capture_count);
	free(tracker);
}

// Callback when a new capture session is created.
void handle_ext_image_copy_capture_new_session(struct wl_listener *listener,
											   void *data) {
	struct wlr_ext_image_copy_capture_session_v1 *capture_session = data;

	struct capture_session_tracker *tracker = calloc(1, sizeof(*tracker));
	if (!tracker) {
		mango_error(true, WLR_ERROR,
					"Failed to allocate capture session tracker");
		return;
	}
	tracker->session = capture_session;
	tracker->session_destroy.notify = handle_session_destroy;
	// Listens to the session destroy signal to decrease the count when the
	// session ends.
	wl_signal_add(&capture_session->events.destroy, &tracker->session_destroy);

	server.active_capture_count++;

	Client *c = NULL;
	wl_list_for_each(c, &server.clients, link) {
		if (c->shield_when_capture && !c->iskilling && VISIBLEON(c, c->mon)) {
			arrange(c->mon, false, false);
		}
	}

	mango_error(true, WLR_DEBUG,
				"New capture session started, active count: %d",
				server.active_capture_count);
}

void handle_xdg_decoration_mode_request(struct wl_listener *listener,
										void *data) {
	Client *c = wl_container_of(listener, c, set_decoration_mode);
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;

	if (c->surface.xdg->initialized) {
		// Gets the mode requested by the client.
		enum wlr_xdg_toplevel_decoration_v1_mode requested_mode =
			deco->requested_mode;

		// If the client did not specify one, use the default mode.
		if (!c->allow_csd) {
			requested_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
		}

		wlr_xdg_toplevel_decoration_v1_set_mode(c->decoration, requested_mode);
	}
}

void handle_drm_lease_request(struct wl_listener *listener, void *data) {
	struct wlr_drm_lease_request_v1 *req = data;
	struct wlr_drm_lease_v1 *lease = wlr_drm_lease_request_v1_grant(req);

	if (!lease) {
		mango_error(true, WLR_ERROR, "Failed to grant lease request");
		wlr_drm_lease_request_v1_reject(req);
	}
}

void handle_request_set_primary_selection(struct wl_listener *listener,
										  void *data) {
	/* This event is raised by the seat when a client wants to set the
	 * selection, usually when the user copies something. wlroots allows
	 * compositors to ignore such requests if they so choose, but in dwl we
	 * always honor
	 */
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(server.seat, event->source, event->serial);
}

void handle_request_set_selection(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the
	 * selection, usually when the user copies something. wlroots allows
	 * compositors to ignore such requests if they so choose, but in dwl we
	 * always honor
	 */
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server.seat, event->source, event->serial);
}

void check_keep_idle_inhibit(Client *c) {
	if (c && c->idleinhibit_when_focus && server.keep_idle_inhibit_source) {
		wl_event_source_timer_update(server.keep_idle_inhibit_source, 1000);
	}
}

int32_t idle_keep_inhibit(void *data) {
	if (!server.idle_inhibit_manager) {
		wl_event_source_timer_update(server.keep_idle_inhibit_source, 0);
		return 1;
	}

	if (server.session && !server.session->active) {
		wl_event_source_timer_update(server.keep_idle_inhibit_source, 0);
		return 1;
	}

	if (!server.selected_monitor || !server.selected_monitor->sel ||
		!server.selected_monitor->sel->idleinhibit_when_focus) {
		wl_event_source_timer_update(server.keep_idle_inhibit_source, 0);
		return 1;
	}

	if (server.seat && server.idle_notifier) {
		wlr_idle_notifier_v1_notify_activity(server.idle_notifier, server.seat);
		wl_event_source_timer_update(server.keep_idle_inhibit_source, 1000);
	}
	return 1;
}

void handle_session_lock_unlock(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, unlock);
	session_lock_cleanup(lock, 1);
}
