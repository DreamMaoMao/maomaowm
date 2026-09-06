#include "mango/ext-protocol/tearing.h"
#include "mango/common/server.h"
#include "mango/common/log.h"
#include "mango/manage/client.h"
#include "mango/common/util.h"
#include "mango/manage/monitor.h"

void handle_controller_set_hint(struct wl_listener *listener, void *data) {
	struct tearing_controller *controller =
		wl_container_of(listener, controller, set_hint);
	Client *c = NULL;

	toplevel_from_wlr_surface(controller->tearing_control->surface, &c, NULL);

	if (c) {
		/*
		 * tearing_control->current is actually an enum:
		 * WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC = 0
		 * WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC = 1
		 *
		 * Using it as a bool here allows us to not ship the XML.
		 */
		c->tearing_hint = controller->tearing_control->current;
	}
}
void handle_controller_destroy(struct wl_listener *listener, void *data) {
	struct tearing_controller *controller =
		wl_container_of(listener, controller, destroy);
	wl_list_remove(&controller->set_hint.link);
	wl_list_remove(&controller->destroy.link);
	free(controller);
}
void handle_tearing_new_object(struct wl_listener *listener, void *data) {
	struct wlr_tearing_control_v1 *new_tearing_control = data;

	enum wp_tearing_control_v1_presentation_hint hint =
		wlr_tearing_control_manager_v1_surface_hint_from_surface(
			server.tearing_control, new_tearing_control->surface);
	mango_error(true, WLR_DEBUG,
				"New presentation hint %d received for surface %p", hint,
				new_tearing_control->surface);

	struct tearing_controller *controller =
		ecalloc(1, sizeof(struct tearing_controller));
	controller->tearing_control = new_tearing_control;

	controller->set_hint.notify = handle_controller_set_hint;
	wl_signal_add(&new_tearing_control->events.set_hint, &controller->set_hint);

	controller->destroy.notify = handle_controller_destroy;
	wl_signal_add(&new_tearing_control->events.destroy, &controller->destroy);
}
bool check_tearing_frame_allow(Monitor *m) {
	/* never allow tearing when disabled */
	if (!config.allow_tearing) {
		return false;
	}

	if (!server.selected_monitor)
		return false;

	Client *c = server.selected_monitor->sel;

	/* tearing is only allowed for the output with the active client */
	if (!c || c->mon != m) {
		return false;
	}

	/* allow tearing for any window when requested or forced */
	if (config.allow_tearing == TEARING_ENABLED) {
		if (c->force_tearing == STATE_UNSPECIFIED) {
			return c->tearing_hint;
		} else {
			return c->force_tearing == STATE_ENABLED;
		}
	}

	/* remaining tearing options apply only to full-screen windows */
	if (!c->isfullscreen) {
		return false;
	}

	if (c->force_tearing == STATE_UNSPECIFIED) {
		/* honor the tearing hint or the fullscreen-force preference */
		return c->tearing_hint ||
			   config.allow_tearing == TEARING_FULLSCREEN_ONLY;
	}

	/* honor tearing as requested by action */
	return c->force_tearing == STATE_ENABLED;
}
