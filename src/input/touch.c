#include <wlr/types/wlr_touch.h>
#include "mango/input/touch.h"
#include "mango/common/globals.h"
#include "mango/common/log.h"
#include "mango/manage/monitor.h"
#include "mango/manage/misc.h"
#include "mango/manage/client.h"
#include "mango/input/pointer.h"
#include <wayland-client-core.h>
#include "mango/mango.h"
#include "mango/ipc/ipc.h"
#include "mango/common/util.h"

void touch_up(struct wl_listener *listener, void *data);
void touch_cancel(struct wl_listener *listener, void *data);
void touch_frame(struct wl_listener *listener, void *data);
struct wlr_surface *touch_get_coords(struct wlr_touch *touch, double x,
									 double y, double *x_offset,
									 double *y_offset, Client **pc);
void touch_apply_xwayland_scale(struct wlr_surface *surface, double *sx,
								double *sy);
void touch_finish_all(void);

struct touch_point {
	int32_t touch_id;
	double x_offset;
	double y_offset;
	struct wlr_surface *surface;
	struct wl_listener surface_destroy;
	bool touch_protocol; // true=触摸协议，false=鼠标模拟
	struct wl_list link;
};

struct wl_list touch_points;
// 指针模拟状态：仅第一个不支持触摸的触点模拟指针，pointer_touch_id 隔离多指
bool simulating_pointer_from_touch = false;
int32_t pointer_touch_id = -1;
struct wl_listener cursor_touch_down = {.notify = touch_down};
struct wl_listener cursor_touch_up = {.notify = touch_up};
struct wl_listener cursor_touch_cancel = {.notify = touch_cancel};
struct wl_listener cursor_touch_motion = {.notify = touch_motion};
struct wl_listener cursor_touch_frame = {.notify = touch_frame};

// 将触摸设备映射到 touch_map_to_mon 指定的显示器。
// 支持热插拔输出和配置重载，每次触摸按下时重新应用。
void touch_apply_monitor_mapping(struct wlr_touch *touch) {
	if (!config.touch_map_to_mon)
		return;

	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (match_monitor_spec(config.touch_map_to_mon, m)) {
			wlr_cursor_map_input_to_output(cursor, &touch->base, m->wlr_output);
			mango_error(true, WLR_DEBUG, "Mapping touch %s to output %s",
						touch->base.name, config.touch_map_to_mon);
			return;
		}
	}
}

void createtouch(struct wlr_touch *touch) {
	struct libinput_device *device = NULL;

	if (wlr_input_device_is_libinput(&touch->base) &&
		(device = wlr_libinput_get_device_handle(&touch->base))) {
		if (libinput_device_config_send_events_get_modes(device))
			libinput_device_config_send_events_set_mode(
				device, config.send_events_mode);
	}
	wlr_cursor_attach_input_device(cursor, &touch->base);
	touch_apply_monitor_mapping(touch);
}

void touch_point_surface_destroy(struct wl_listener *listener, void *data) {
	struct touch_point *point =
		wl_container_of(listener, point, surface_destroy);
	point->surface = NULL;
	wl_list_remove(&listener->link);
	wl_list_init(&listener->link);
}

// 将 [0,1] 归一化坐标转为布局坐标，查找触点下方的 surface，
// 并计算布局->surface 偏移以便后续以相对坐标上报
struct wlr_surface *touch_get_coords(struct wlr_touch *touch, double x,
									 double y, double *x_offset,
									 double *y_offset, Client **pc) {
	double lx, ly, sx, sy;
	struct wlr_surface *surface = NULL;
	Client *c = NULL;

	wlr_cursor_absolute_to_layout_coords(cursor, &touch->base, x, y, &lx, &ly);

	xytonode(lx, ly, &surface, &c, NULL, NULL, &sx, &sy);

	*x_offset = lx - sx;
	*y_offset = ly - sy;

	if (pc)
		*pc = c;

	return surface;
}

// 触摸模拟鼠标，绝对移动，无指针设备的触摸屏回退
void touch_emulate_move_absolute(struct wlr_touch *touch, double x, double y,
								 uint32_t time) {
	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(cursor, &touch->base, x, y, &lx, &ly);
	double dx = lx - cursor->x;
	double dy = ly - cursor->y;
	motionnotify(time, &touch->base, dx, dy, dx, dy);
	wlr_seat_pointer_notify_frame(seat);
}

// 触摸模拟鼠标：按键（复用普通指针的按键处理逻辑）
void touch_emulate_button(uint32_t button, enum wl_pointer_button_state state,
						  uint32_t time) {
	struct wlr_pointer_button_event ev = {
		.button = button,
		.state = state,
		.time_msec = time,
	};
	if (!handle_buttonpress(&ev))
		wlr_seat_pointer_notify_button(seat, ev.time_msec, ev.button, ev.state);
	wlr_seat_pointer_notify_frame(seat);
}

// X11 窗口 surface 局部坐标映射：xwayland_ignore_scale 下 X11 窗口以物理
// 尺寸渲染，surface 局部坐标需乘 xwayland_scale 才能命中正确位置
void touch_apply_xwayland_scale(struct wlr_surface *surface, double *sx,
								double *sy) {
#ifdef XWAYLAND
	Client *c = NULL;
	toplevel_from_wlr_surface(surface, &c, NULL);
	if (c && client_is_x11(c) && config.xwayland_ignore_scale &&
		c->xwayland_scale > 0.f) {
		*sx *= c->xwayland_scale;
		*sy *= c->xwayland_scale;
	}
#endif
}

void touch_down(struct wl_listener *listener, void *data) {
	struct wlr_touch_down_event *event = data;
	struct touch_point *point = ecalloc(1, sizeof(*point));
	double x_offset = 0.0, y_offset = 0.0;
	Client *c = NULL;

	ipc_notify_device_event(&event->touch->base);

	// 全局禁用触屏时忽略触摸事件；若正在指针模拟则一并清理结束
	if (!config.touch_enable) {
		touch_finish_all();
		return;
	}

	touch_apply_monitor_mapping(event->touch);

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	point->surface = touch_get_coords(event->touch, event->x, event->y,
									  &x_offset, &y_offset, &c);
	point->touch_id = event->touch_id;
	point->x_offset = x_offset;
	point->y_offset = y_offset;
	// 触点位于接受触摸的 surface 上则走触摸协议，否则回退鼠标模拟
	point->touch_protocol =
		point->surface && wlr_surface_accepts_touch(point->surface, seat);

	wl_list_insert(&touch_points, &point->link);
	int touch_point_count = wl_list_length(&touch_points);

	// 触摸输入时隐藏光标
	hidecursor(NULL);

	if (point->touch_protocol) {
		// 触点走触摸协议：退出指针模拟并清空指针焦点避免干扰
		simulating_pointer_from_touch = false;
		wlr_seat_pointer_notify_clear_focus(seat);

		double lx, ly, sx, sy;
		wlr_cursor_absolute_to_layout_coords(cursor, &event->touch->base,
											 event->x, event->y, &lx, &ly);
		sx = lx - x_offset;
		sy = ly - y_offset;

		// X11 窗口 surface 局部坐标映射（xwayland_ignore_scale 下为物理尺寸）
		touch_apply_xwayland_scale(point->surface, &sx, &sy);

		if (touch_point_count == 1)
			wlr_cursor_warp_absolute(cursor, &event->touch->base, event->x,
									 event->y);

		wl_signal_add(&point->surface->events.destroy, &point->surface_destroy);
		point->surface_destroy.notify = touch_point_surface_destroy;

		wlr_seat_touch_notify_down(seat, point->surface, event->time_msec,
								   event->touch_id, sx, sy);
	} else if (config.touch_enable_mouse_emulation &&
			   !simulating_pointer_from_touch) {
		// 回退指针模拟：仅第一个触点模拟指针，pointer_touch_id 用于多指隔离
		simulating_pointer_from_touch = true;
		pointer_touch_id = event->touch_id;
		touch_emulate_move_absolute(event->touch, event->x, event->y,
									event->time_msec);
		touch_emulate_button(BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED,
							 event->time_msec);
	}
	// 否则：surface 不支持触摸且未启用模拟，或已有触点正在模拟，忽略该触点
}

void touch_motion(struct wl_listener *listener, void *data) {
	struct wlr_touch_motion_event *event = data;
	struct touch_point *point;

	ipc_notify_device_event(&event->touch->base);

	if (!config.touch_enable) {
		touch_finish_all();
		return;
	}

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	wl_list_for_each(point, &touch_points, link) {
		if (point->touch_id != event->touch_id)
			continue;

		if (point->touch_protocol && point->surface) {
			double lx, ly;
			wlr_cursor_absolute_to_layout_coords(cursor, &event->touch->base,
												 event->x, event->y, &lx, &ly);
			double sx = lx - point->x_offset;
			double sy = ly - point->y_offset;

			touch_apply_xwayland_scale(point->surface, &sx, &sy);

			wlr_seat_touch_notify_motion(seat, event->time_msec,
										 event->touch_id, sx, sy);
		} else if (config.touch_enable_mouse_emulation &&
				   simulating_pointer_from_touch &&
				   point->touch_id == pointer_touch_id) {
			touch_emulate_move_absolute(event->touch, event->x, event->y,
										event->time_msec);
		}
		return;
	}
}

void touch_up(struct wl_listener *listener, void *data) {
	struct wlr_touch_up_event *event = data;
	struct touch_point *point, *tmp;

	ipc_notify_device_event(&event->touch->base);

	if (!config.touch_enable) {
		touch_finish_all();
		return;
	}

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	wl_list_for_each_safe(point, tmp, &touch_points, link) {
		if (point->touch_id != event->touch_id)
			continue;

		if (point->touch_protocol && point->surface) {
			wlr_seat_touch_notify_up(seat, event->time_msec, event->touch_id);
			wl_list_remove(&point->surface_destroy.link);
		} else if (config.touch_enable_mouse_emulation &&
				   simulating_pointer_from_touch &&
				   point->touch_id == pointer_touch_id) {
			touch_emulate_button(BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED,
								 event->time_msec);
			simulating_pointer_from_touch = false;
			pointer_touch_id = -1;
		}

		wl_list_remove(&point->link);
		free(point);
		break;
	}
}

// 触摸被系统取消（如全局手势抢占）：触摸协议触点取消整个会话，
// 指针模拟触点则释放鼠标并结束模拟
void touch_cancel(struct wl_listener *listener, void *data) {
	struct wlr_touch_cancel_event *event = data;
	struct touch_point *point, *tmp;

	ipc_notify_device_event(&event->touch->base);

	if (!config.touch_enable) {
		touch_finish_all();
		return;
	}

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	wl_list_for_each_safe(point, tmp, &touch_points, link) {
		if (point->touch_id != event->touch_id)
			continue;

		if (point->touch_protocol && point->surface) {
			// NULL 表示取消 seat 上所有触摸焦点
			wlr_seat_touch_notify_cancel(seat, NULL);
			wl_list_remove(&point->surface_destroy.link);
		} else if (config.touch_enable_mouse_emulation &&
				   simulating_pointer_from_touch &&
				   point->touch_id == pointer_touch_id) {
			touch_emulate_button(BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED,
								 event->time_msec);
			simulating_pointer_from_touch = false;
			pointer_touch_id = -1;
		}

		wl_list_remove(&point->link);
		free(point);
		break;
	}
}

void touch_frame(struct wl_listener *listener, void *data) {
	// 指针模拟期间发送指针 frame，否则发送触摸 frame
	if (simulating_pointer_from_touch)
		wlr_seat_pointer_notify_frame(seat);
	else
		wlr_seat_touch_notify_frame(seat);
}

// 清空所有进行中的触摸点；正在指针模拟时先释放鼠标左键
void touch_finish_all(void) {
	struct touch_point *point, *tmp;

	if (simulating_pointer_from_touch && config.touch_enable_mouse_emulation)
		touch_emulate_button(BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED, 0);
	simulating_pointer_from_touch = false;
	pointer_touch_id = -1;

	wl_list_for_each_safe(point, tmp, &touch_points, link) {
		if (point->touch_protocol && point->surface)
			wl_list_remove(&point->surface_destroy.link);
		wl_list_remove(&point->link);
		free(point);
	}
}
