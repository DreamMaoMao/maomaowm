#ifndef __INPUT_TABLET_H__
#define __INPUT_TABLET_H__ 1

#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_v2.h>

void tablet_create(struct wlr_input_device *device);
void handle_tablet_destroy(struct wl_listener *listener, void *data);
void tablet_pad_create(struct wlr_input_device *device);
void handle_tablet_pad_destroy(struct wl_listener *listener, void *data);
void handle_tablet_pad_tablet_destroy(struct wl_listener *listener, void *data);
void handle_tablet_pad_attach(struct wl_listener *listener, void *data);
void handle_tablet_tool_surface_destroy(struct wl_listener *listener,
										void *data);
void handle_tablet_tool_destroy(struct wl_listener *listener, void *data);
void handle_tablet_tool_set_cursor(struct wl_listener *listener, void *data);

void handle_tablet_tool_proximity(struct wl_listener *listener, void *data);
void handle_tablet_tool_axis(struct wl_listener *listener, void *data);
void handle_tablet_tool_button(struct wl_listener *listener, void *data);
void handle_tablet_tool_tip(struct wl_listener *listener, void *data);

struct Tablet {
	struct wlr_tablet_v2_tablet *tablet_v2;
	struct wl_listener destroy;
	struct wlr_input_device *device;
	struct wl_list link;
};

struct TabletTool {
	struct wlr_tablet_v2_tablet_tool *tool_v2;
	struct Tablet *tablet;
	struct wlr_surface *curr_surface;
	struct wl_listener destroy;
	struct wl_listener surface_destroy;
	struct wl_listener set_cursor;
	double tilt_x, tilt_y;
};

struct TabletPad {
	struct wlr_tablet_v2_tablet_pad *pad_v2;
	struct wlr_input_device *device;
	struct Tablet *tablet;
	struct wl_listener tablet_destroy;
	struct wl_listener attach;
	struct wl_listener destroy;
	struct wl_list link;
};

void attach_tablet_pad(struct TabletPad *tablet_pad, struct Tablet *tablet);
void tablet_tool_motion(struct TabletTool *tool, bool change_x, bool change_y,
						double x, double y, double dx, double dy);

#endif
