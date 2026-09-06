#ifndef __INPUT_SWITCH_H__
#define __INPUT_SWITCH_H__ 1

#include "mango/mango.h"

void switch_toggle(struct wl_listener *listener, void *data);
void createswitch(struct wlr_switch *switch_device);

#endif
