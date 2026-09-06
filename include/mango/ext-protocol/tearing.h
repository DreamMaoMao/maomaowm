#ifndef __EXT_PROTOCOL_TEARING_H__
#define __EXT_PROTOCOL_TEARING_H__ 1

#include "mango/mango.h"
#include <stdbool.h>
#include <wlr/types/wlr_tearing_control_v1.h>

struct tearing_controller {
	struct wlr_tearing_control_v1 *tearing_control;
	struct wl_listener set_hint;
	struct wl_listener destroy;
};

/* Declarations */
void handle_controller_set_hint(struct wl_listener *listener, void *data);
void handle_controller_destroy(struct wl_listener *listener, void *data);

void handle_tearing_new_object(struct wl_listener *listener, void *data);
bool check_tearing_frame_allow(Monitor *m);

#endif
