#include "mango/manage/misc.h"
#include "mango/manage/client.h"
#include "mango/manage/layer.h"
#include "mango/mango.h"
#include "mango/common/log.h"
#include "mango/common/globals.h"
#include "mango/data/static_keymap.h"
#include "mango/input/pointer.h"
#include "mango/layout/arrange.h"
#include "mango/common/util.h"
#include "mango/manage/monitor.h"

#include <ctype.h>

pid_t getparentprocess(pid_t p) {
	uint32_t v = 0;

	FILE *f;
	char buf[256];
	snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

	if (!(f = fopen(buf, "r")))
		return 0;

	// 检查fscanf返回值，确保成功读取了1个参数
	if (fscanf(f, "%*u %*s %*c %u", &v) != 1) {
		fclose(f);
		return 0;
	}

	fclose(f);

	return (pid_t)v;
}

int32_t isdescprocess(pid_t p, pid_t c) {
	while (p != c && c != 0)
		c = getparentprocess(c);

	return (int32_t)c;
}

void get_layout_abbr(char *abbr, const char *full_name) {
	// 清空输出缓冲区
	abbr[0] = '\0';

	// 1. 尝试在映射表中查找
	for (int32_t i = 0; layout_mappings[i].full_name != NULL; i++) {
		if (strcmp(full_name, layout_mappings[i].full_name) == 0) {
			strcpy(abbr, layout_mappings[i].abbr);
			return;
		}
	}

	// 2. 尝试从名称中提取并转换为小写
	const char *open = strrchr(full_name, '(');
	const char *close = strrchr(full_name, ')');
	if (open && close && close > open) {
		uint32_t len = close - open - 1;
		if (len > 0 && len <= 4) {
			// 提取并转换为小写
			for (uint32_t j = 0; j < len; j++) {
				abbr[j] = tolower(open[j + 1]);
			}
			abbr[len] = '\0';
			return;
		}
	}

	// 3. 提取前2-3个字母并转换为小写
	uint32_t j = 0;
	for (uint32_t i = 0; full_name[i] != '\0' && j < 3; i++) {
		if (isalpha(full_name[i])) {
			abbr[j++] = tolower(full_name[i]);
		}
	}
	abbr[j] = '\0';

	// 确保至少2个字符
	if (j >= 2) {
		return;
	}

	// 4. 回退方案：使用首字母小写
	if (j == 1) {
		abbr[1] = full_name[1] ? tolower(full_name[1]) : '\0';
		abbr[2] = '\0';
	} else {
		// 5. 最终回退：返回 "xx"
		strcpy(abbr, "xx");
	}
}

Client *xytoclient(double x, double y) {
	Client *c = NULL, *tmp = NULL;
	wl_list_for_each_safe(c, tmp, &clients, link) {
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

void xytonode(double x, double y, struct wlr_surface **psurface, Client **pc,
			  LayerSurface **pl, MangoGroupBar **gb, double *nx, double *ny) {
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

		node = wlr_scene_node_at(&layers[layer]->node, x, y, nx, ny);
		if (!node)
			continue;

		Monitor *cm = xytomon(x, y);
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

	if (selmon && selmon->isoverview) {
		ovc = xytoclient(x, y);

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
 * 额外协议：xdg-decoration、session lock、drm lease、image capture、
 * idle inhibit、seat selection（剪切板）等杂项协议处理。
 */
void checkidleinhibitor(struct wlr_surface *exclude) {
	int32_t inhibited = 0;
	Client *c = NULL;
	struct wlr_surface *surface = NULL;
	struct wlr_idle_inhibitor_v1 *inhibitor;

	wl_list_for_each(inhibitor, &idle_inhibit_mgr->inhibitors, link) {
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

	wlr_idle_notifier_v1_set_inhibited(idle_notifier, inhibited);
}

void destroydecoration(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, destroy_decoration);

	wl_list_remove(&c->destroy_decoration.link);
	wl_list_remove(&c->set_decoration_mode.link);
	c->decoration = NULL;
}

void createdecoration(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;
	Client *c = deco->toplevel->base->data;
	c->decoration = deco;

	LISTEN(&deco->events.request_mode, &c->set_decoration_mode,
		   requestdecorationmode);
	LISTEN(&deco->events.destroy, &c->destroy_decoration, destroydecoration);

	requestdecorationmode(&c->set_decoration_mode, deco);
}

void createidleinhibitor(struct wl_listener *listener, void *data) {
	struct wlr_idle_inhibitor_v1 *idle_inhibitor = data;
	LISTEN_STATIC(&idle_inhibitor->events.destroy, destroyidleinhibitor);

	checkidleinhibitor(NULL);
}

void createlocksurface(struct wl_listener *listener, void *data) {
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
		   destroylocksurface);

	if (m == selmon)
		client_notify_enter(lock_surface->surface, wlr_seat_get_keyboard(seat));
}

void destroyidleinhibitor(struct wl_listener *listener, void *data) {
	/* `data` is the wlr_surface of the idle inhibitor being destroyed,
	 * at this point the idle inhibitor is still in the list of the manager
	 */
	checkidleinhibitor(wlr_surface_get_root_surface(data));
	wl_list_remove(&listener->link);
	free(listener);
}

void destroylock(SessionLock *lock, int32_t unlock) {
	wlr_seat_keyboard_notify_clear_focus(seat);
	if ((locked = !unlock))
		goto destroy;

	if (locked_bg->node.enabled) {
		wlr_scene_node_set_enabled(&locked_bg->node, false);
	}

	focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);

destroy:
	wl_list_remove(&lock->new_surface.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->destroy.link);

	wlr_scene_node_destroy(&lock->scene->node);
	cur_lock = NULL;
	free(lock);
}

void destroylocksurface(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, destroy_lock_surface);
	struct wlr_session_lock_surface_v1 *surface,
		*lock_surface = m->lock_surface;

	m->lock_surface = NULL;
	wl_list_remove(&m->destroy_lock_surface.link);

	if (lock_surface->surface != seat->keyboard_state.focused_surface) {
		if (exclusive_focus && !locked) {
			reset_exclusive_layers_focus(m);
		}
		return;
	}

	if (locked && cur_lock && !wl_list_empty(&cur_lock->surfaces)) {
		surface = wl_container_of(cur_lock->surfaces.next, surface, link);
		client_notify_enter(surface->surface, wlr_seat_get_keyboard(seat));
	} else if (!locked) {
		reset_exclusive_layers_focus(selmon);
	} else {
		wlr_seat_keyboard_clear_focus(seat);
	}
}

void destroysessionlock(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, destroy);
	destroylock(lock, 0);
}

void locksession(struct wl_listener *listener, void *data) {
	struct wlr_session_lock_v1 *session_lock = data;
	SessionLock *lock;
	if (!config.allow_lock_transparent) {
		wlr_scene_node_set_enabled(&locked_bg->node, true);
	}
	if (cur_lock) {
		wlr_session_lock_v1_destroy(session_lock);
		return;
	}
	lock = session_lock->data = ecalloc(1, sizeof(*lock));
	focusclient(NULL, 0);

	lock->scene = wlr_scene_tree_create(layers[LyrBlock]);
	cur_lock = lock->lock = session_lock;
	locked = 1;

	LISTEN(&session_lock->events.new_surface, &lock->new_surface,
		   createlocksurface);
	LISTEN(&session_lock->events.destroy, &lock->destroy, destroysessionlock);
	LISTEN(&session_lock->events.unlock, &lock->unlock, unlocksession);

	wlr_session_lock_v1_send_locked(session_lock);
}
void handle_new_foreign_toplevel_capture_request(struct wl_listener *listener,
												 void *data) {
	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request
		*request = data;
	Client *c = request->toplevel_handle->data;

	if (c->image_capture_source == NULL) {
		c->image_capture_source =
			wlr_ext_image_capture_source_v1_create_with_scene_node(
				&c->image_capture_scene->tree.node, event_loop, alloc, drw);
		if (c->image_capture_source == NULL) {
			return;
		}
	}

	wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
		request, c->image_capture_source);
}

// 会话销毁时的回调
void handle_session_destroy(struct wl_listener *listener, void *data) {
	struct capture_session_tracker *tracker =
		wl_container_of(listener, tracker, session_destroy);
	active_capture_count--;
	wl_list_remove(&tracker->session_destroy.link);

	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {
		if (c->shield_when_capture && !c->iskilling && VISIBLEON(c, c->mon)) {
			arrange(c->mon, false, false);
		}
	}

	mango_error(true, WLR_DEBUG, "Capture session ended, active count: %d",
				active_capture_count);
	free(tracker);
}

// 新会话创建时的回调
void handle_iamge_copy_capture_new_session(struct wl_listener *listener,
										   void *data) {
	struct wlr_ext_image_copy_capture_session_v1 *session = data;

	struct capture_session_tracker *tracker = calloc(1, sizeof(*tracker));
	if (!tracker) {
		mango_error(true, WLR_ERROR,
					"Failed to allocate capture session tracker");
		return;
	}
	tracker->session = session;
	tracker->session_destroy.notify = handle_session_destroy;
	// 监听会话的 destroy 信号，以便在会话结束时减少计数
	wl_signal_add(&session->events.destroy, &tracker->session_destroy);

	active_capture_count++;

	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {
		if (c->shield_when_capture && !c->iskilling && VISIBLEON(c, c->mon)) {
			arrange(c->mon, false, false);
		}
	}

	mango_error(true, WLR_DEBUG,
				"New capture session started, active count: %d",
				active_capture_count);
}

void requestdecorationmode(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_decoration_mode);
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;

	if (c->surface.xdg->initialized) {
		// 获取客户端请求的模式
		enum wlr_xdg_toplevel_decoration_v1_mode requested_mode =
			deco->requested_mode;

		// 如果客户端没有指定，使用默认模式
		if (!c->allow_csd) {
			requested_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
		}

		wlr_xdg_toplevel_decoration_v1_set_mode(c->decoration, requested_mode);
	}
}

void requestdrmlease(struct wl_listener *listener, void *data) {
	struct wlr_drm_lease_request_v1 *req = data;
	struct wlr_drm_lease_v1 *lease = wlr_drm_lease_request_v1_grant(req);

	if (!lease) {
		mango_error(true, WLR_ERROR, "Failed to grant lease request");
		wlr_drm_lease_request_v1_reject(req);
	}
}

void setpsel(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the
	 * selection, usually when the user copies something. wlroots allows
	 * compositors to ignore such requests if they so choose, but in dwl we
	 * always honor
	 */
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

void setsel(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the
	 * selection, usually when the user copies something. wlroots allows
	 * compositors to ignore such requests if they so choose, but in dwl we
	 * always honor
	 */
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat, event->source, event->serial);
}

void check_keep_idle_inhibit(Client *c) {
	if (c && c->idleinhibit_when_focus && keep_idle_inhibit_source) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 1000);
	}
}

int32_t keep_idle_inhibit(void *data) {
	if (!idle_inhibit_mgr) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 0);
		return 1;
	}

	if (session && !session->active) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 0);
		return 1;
	}

	if (!selmon || !selmon->sel || !selmon->sel->idleinhibit_when_focus) {
		wl_event_source_timer_update(keep_idle_inhibit_source, 0);
		return 1;
	}

	if (seat && idle_notifier) {
		wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
		wl_event_source_timer_update(keep_idle_inhibit_source, 1000);
	}
	return 1;
}

void unlocksession(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, unlock);
	destroylock(lock, 1);
}
