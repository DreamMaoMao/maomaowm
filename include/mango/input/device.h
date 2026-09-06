#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <wayland-server-core.h>

void destroyinputdevice(struct wl_listener *listener, void *data);

void inputdevice(struct wl_listener *listener, void *data);

#endif
