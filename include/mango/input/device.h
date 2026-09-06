#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "mango/common/types.h"
#include <stdbool.h>
#include <wayland-server-core.h>

typedef struct InputDevice {
	struct wl_list link;
	struct wlr_input_device *wlr_device;
	struct libinput_device *libinput_device;
	struct wl_listener destroy_listener;
	void *device_data;
	bool standalone; /* Keyboard matched by a devicerule; independent from the
						default keyboard group. */
	struct wl_listener
		key_watch; /* Records the device that emitted the last key event. */
} InputDevice;

void handle_input_device_destroy(struct wl_listener *listener, void *data);

void handle_new_input_device(struct wl_listener *listener, void *data);

#endif
