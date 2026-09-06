#ifndef __INPUT_SWITCH_H__
#define __INPUT_SWITCH_H__ 1

#include "mango/common/types.h"
#include <wayland-server-core.h>

typedef struct Switch {
	struct wl_list link;
	struct wlr_switch *wlr_switch;
	struct wl_listener toggle;
	InputDevice *input_dev;
} Switch;

void handle_switch_toggle(struct wl_listener *listener, void *data);
void switch_create(struct wlr_switch *switch_device);

#endif
