#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <wayland-server-core.h>

void handle_input_device_destroy(struct wl_listener *listener, void *data);

void handle_new_input_device(struct wl_listener *listener, void *data);

#endif
