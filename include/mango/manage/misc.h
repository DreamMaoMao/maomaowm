#ifndef __MANAGE_MISC_H__
#define __MANAGE_MISC_H__ 1

#include "mango/common/types.h"
#include <sys/types.h>

pid_t get_parent_process(pid_t p);
int32_t is_descendant_process(pid_t p, pid_t c);
void get_layout_abbr(char *abbr, const char *full_name);
Client *client_at_point(double x, double y);
bool layer_ignores_focus(LayerSurface *l);
void node_at_point(double x, double y, struct wlr_surface **psurface,
				   Client **pc, LayerSurface **pl, MangoGroupBar **gb,
				   double *nx, double *ny);

/*
 * Extra protocol handlers: xdg-decoration, session lock, drm lease,
 * image capture, idle inhibit, and seat selection (clipboard).
 */
void check_idle_inhibitor(struct wlr_surface *exclude);
void handle_xdg_decoration_destroy(struct wl_listener *listener, void *data);
void handle_new_xdg_decoration(struct wl_listener *listener, void *data);
void handle_new_idle_inhibitor(struct wl_listener *listener, void *data);
void handle_session_lock_new_surface(struct wl_listener *listener, void *data);
void handle_idle_inhibitor_destroy(struct wl_listener *listener, void *data);
void session_lock_cleanup(SessionLock *lock, int32_t unlock);
void handle_session_lock_surface_destroy(struct wl_listener *listener,
										 void *data);
void handle_session_lock_destroy(struct wl_listener *listener, void *data);
void handle_new_session_lock(struct wl_listener *listener, void *data);
void handle_new_foreign_toplevel_capture_request(struct wl_listener *listener,
												 void *data);
// Callback when a capture session is destroyed
void handle_session_destroy(struct wl_listener *listener, void *data);
// Callback when a new capture session is created
void handle_ext_image_copy_capture_new_session(struct wl_listener *listener,
											   void *data);
void handle_xdg_decoration_mode_request(struct wl_listener *listener,
										void *data);
void handle_drm_lease_request(struct wl_listener *listener, void *data);
void handle_request_set_primary_selection(struct wl_listener *listener,
										  void *data);
void handle_request_set_selection(struct wl_listener *listener, void *data);
void check_keep_idle_inhibit(Client *c);
int32_t idle_keep_inhibit(void *data);
void handle_session_lock_unlock(struct wl_listener *listener, void *data);

#endif
