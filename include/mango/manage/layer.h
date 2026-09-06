#ifndef __MANAGE_LAYER_H__
#define __MANAGE_LAYER_H__ 1

#include "mango/mango.h"

void arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area,
				  int32_t exclusive);
void focuslayer(LayerSurface *l);
void reset_exclusive_layers_focus(Monitor *m);
void arrangelayers(Monitor *m);
void maplayersurfacenotify(struct wl_listener *listener, void *data);
void commitlayersurfacenotify(struct wl_listener *listener, void *data);
bool popup_unconstrain(Popup *popup);
void destroypopup(struct wl_listener *listener, void *data);
void commitpopup(struct wl_listener *listener, void *data);
void repositionpopup(struct wl_listener *listener, void *data);
void createpopup(struct wl_listener *listener, void *data);
void createlayersurface(struct wl_listener *listener, void *data);
void destroylayernodenotify(struct wl_listener *listener, void *data);
void unmaplayersurfacenotify(struct wl_listener *listener, void *data);

#endif
