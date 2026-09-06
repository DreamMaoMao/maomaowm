#include "mango/input/device.h"
#include "mango/mango.h"
#include "mango/input/keyboard.h"
#include "mango/input/tablet.h"
#include "mango/input/pointer.h"
#include "mango/input/touch.h"
#include "mango/input/switch.h"
#include "mango/common/globals.h"

void destroyinputdevice(struct wl_listener *listener, void *data) {
	InputDevice *input_dev =
		wl_container_of(listener, input_dev, destroy_listener);

	if (input_dev->device_data) {
		switch (input_dev->wlr_device->type) {
		case WLR_INPUT_DEVICE_SWITCH: {
			Switch *sw = (Switch *)input_dev->device_data;
			wl_list_remove(&sw->toggle.link);
			free(sw);
			break;
		}
		default:
			break;
		}
		input_dev->device_data = NULL;
	}

	if (input_dev->wlr_device->type == WLR_INPUT_DEVICE_KEYBOARD)
		wl_list_remove(&input_dev->key_watch.link);
	wl_list_remove(&input_dev->link);
	wl_list_remove(&input_dev->destroy_listener.link);
	free(input_dev);
}

void inputdevice(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available.
	 * when the backend is a headless backend, this event will never be
	 * triggered.
	 */
	struct wlr_input_device *device = data;
	uint32_t caps;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		createkeyboard(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TABLET:
		createtablet(device);
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		createtabletpad(device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		createpointer(wlr_pointer_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		createtouch(wlr_touch_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		createswitch(wlr_switch_from_input_device(device));
		break;
	default:
		/* TODO handle other input device types */
		break;
	}

	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In dwl we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability.
	 */
	/* TODO do we actually require a cursor? */
	caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH;
	if (!wl_list_empty(&kb_group->wlr_group->devices) ||
		!wl_list_empty(&standalone_keyboards))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(seat, caps);
}
