#include "mango/input/switch.h"
#include "mango/common/server.h"
#include "mango/config/parse_config.h"
#include "mango/ipc/ipc.h"
#include "mango/input/device.h"
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_switch.h>

void handle_switch_toggle(struct wl_listener *listener, void *data) {
	// Gets the struct that contains the listener.
	Switch *sw = wl_container_of(listener, sw, toggle);

	// Handles the switch event.
	struct wlr_switch_toggle_event *event = data;
	SwitchBinding *s;
	int32_t ji;

	ipc_notify_device_event(&sw->wlr_switch->base);

	for (ji = 0; ji < config.switch_bindings_count; ji++) {
		s = &config.switch_bindings[ji];
		if ((s->iscommonmode ||
			 (s->isdefaultmode && server.key_mode.isdefault) ||
			 (strcmp(server.key_mode.mode, s->mode) == 0)) &&
			event->switch_state == s->fold && s->func) {
			s->func(&s->arg);
			return;
		}
	}
}

void switch_create(struct wlr_switch *switch_device) {
	struct libinput_device *device = NULL;

	if (wlr_input_device_is_libinput(&switch_device->base) &&
		(device = wlr_libinput_get_device_handle(&switch_device->base))) {

		InputDevice *input_dev = calloc(1, sizeof(InputDevice));
		input_dev->wlr_device = &switch_device->base;
		input_dev->libinput_device = device;
		input_dev->device_data = NULL; // Initialized to NULL.

		input_dev->destroy_listener.notify = handle_input_device_destroy;
		wl_signal_add(&switch_device->base.events.destroy,
					  &input_dev->destroy_listener);

		// Creates Switch-specific data.
		Switch *sw = calloc(1, sizeof(Switch));
		sw->wlr_switch = switch_device;
		sw->toggle.notify = handle_switch_toggle;
		sw->input_dev = input_dev;

		// Stores the Switch pointer in the input_device.
		input_dev->device_data = sw;

		// Adds the toggle listener.
		wl_signal_add(&switch_device->events.toggle, &sw->toggle);

		// Adds it to the global list.
		wl_list_insert(&server.input_devices, &input_dev->link);
	}
}
