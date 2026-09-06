#ifndef __EXT_PROTOCOL_FOREIGN_TOPLEVEL_H__
#define __EXT_PROTOCOL_FOREIGN_TOPLEVEL_H__ 1

#include "mango/common/types.h"
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

void handle_foreign_activate_request(struct wl_listener *listener, void *data);
void handle_foreign_maximize_request(struct wl_listener *listener, void *data);
void handle_foreign_minimize_request(struct wl_listener *listener, void *data);
void handle_foreign_fullscreen_request(struct wl_listener *listener,
									   void *data);
void handle_foreign_close_request(struct wl_listener *listener, void *data);
void handle_foreign_destroy(struct wl_listener *listener, void *data);
void add_foreign_toplevel(Client *c);
void reset_foreign_tolevel(Client *c, Monitor *oldmon, Monitor *newmon);

#endif
