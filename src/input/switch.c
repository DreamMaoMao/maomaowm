#include "mango/input/switch.h"
#include "mango/common/globals.h"
#include "mango/ipc/ipc.h"
#include "mango/input/device.h"

void switch_toggle(struct wl_listener *listener, void *data) {
	// 获取包含监听器的结构体
	Switch *sw = wl_container_of(listener, sw, toggle);

	// 处理切换事件
	struct wlr_switch_toggle_event *event = data;
	SwitchBinding *s;
	int32_t ji;

	ipc_notify_device_event(&sw->wlr_switch->base);

	for (ji = 0; ji < config.switch_bindings_count; ji++) {
		s = &config.switch_bindings[ji];
		if ((s->iscommonmode || (s->isdefaultmode && keymode.isdefault) ||
			 (strcmp(keymode.mode, s->mode) == 0)) &&
			event->switch_state == s->fold && s->func) {
			s->func(&s->arg);
			return;
		}
	}
}

void createswitch(struct wlr_switch *switch_device) {
	struct libinput_device *device = NULL;

	if (wlr_input_device_is_libinput(&switch_device->base) &&
		(device = wlr_libinput_get_device_handle(&switch_device->base))) {

		InputDevice *input_dev = calloc(1, sizeof(InputDevice));
		input_dev->wlr_device = &switch_device->base;
		input_dev->libinput_device = device;
		input_dev->device_data = NULL; // 初始化为 NULL

		input_dev->destroy_listener.notify = destroyinputdevice;
		wl_signal_add(&switch_device->base.events.destroy,
					  &input_dev->destroy_listener);

		// 创建 Switch 特定数据
		Switch *sw = calloc(1, sizeof(Switch));
		sw->wlr_switch = switch_device;
		sw->toggle.notify = switch_toggle;
		sw->input_dev = input_dev;

		// 将 Switch 指针保存到 input_device 中
		input_dev->device_data = sw;

		// 添加 toggle 监听器
		wl_signal_add(&switch_device->events.toggle, &sw->toggle);

		// 添加到全局列表
		wl_list_insert(&inputdevices, &input_dev->link);
	}
}
