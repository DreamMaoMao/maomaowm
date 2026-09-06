#ifndef __MANAGE_MISC_H__
#define __MANAGE_MISC_H__ 1

#include "mango/mango.h"
#include <sys/types.h>

pid_t getparentprocess(pid_t p);
int32_t isdescprocess(pid_t p, pid_t c);
void get_layout_abbr(char *abbr, const char *full_name);
Client *xytoclient(double x, double y);
bool layer_ignores_focus(LayerSurface *l);
void xytonode(double x, double y, struct wlr_surface **psurface, Client **pc,
			  LayerSurface **pl, MangoGroupBar **gb, double *nx, double *ny);

/*
 * 额外协议：xdg-decoration、session lock、drm lease、image capture、
 * idle inhibit、seat selection（剪切板）等杂项协议处理。
 */
void checkidleinhibitor(struct wlr_surface *exclude);
void destroydecoration(struct wl_listener *listener, void *data);
void createdecoration(struct wl_listener *listener, void *data);
void createidleinhibitor(struct wl_listener *listener, void *data);
void createlocksurface(struct wl_listener *listener, void *data);
void destroyidleinhibitor(struct wl_listener *listener, void *data);
void destroylock(SessionLock *lock, int32_t unlock);
void destroylocksurface(struct wl_listener *listener, void *data);
void destroysessionlock(struct wl_listener *listener, void *data);
void locksession(struct wl_listener *listener, void *data);
void handle_new_foreign_toplevel_capture_request(struct wl_listener *listener,
												 void *data);
// 会话销毁时的回调
void handle_session_destroy(struct wl_listener *listener, void *data);
// 新会话创建时的回调
void handle_iamge_copy_capture_new_session(struct wl_listener *listener,
										   void *data);
void requestdecorationmode(struct wl_listener *listener, void *data);
void requestdrmlease(struct wl_listener *listener, void *data);
void setpsel(struct wl_listener *listener, void *data);
void setsel(struct wl_listener *listener, void *data);
void check_keep_idle_inhibit(Client *c);
int32_t keep_idle_inhibit(void *data);
void unlocksession(struct wl_listener *listener, void *data);

#endif
