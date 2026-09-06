#include "mango/common/server.h"

#include "mango/ipc/ipc.h"
#include "mango/ext-protocol/tearing.h"
#include "mango/ext-protocol/ext-workspace.h"
#include "mango/input/pointer.h"
#include "mango/input/device.h"
#include "mango/input/keyboard.h"
#include "mango/input/tablet.h"
#include "mango/input/touch.h"
#include "mango/manage/client.h"
#include "mango/manage/layer.h"
#include "mango/manage/misc.h"
#include "mango/manage/monitor.h"

struct MangoServer server = {
	.child_pid = -1,

	.enable_gaps = 1, /* enables gaps, used by togglegaps */
	.allow_frame_scheduling = true,
	.render_border = true,
	.tagmask =
		((1u << 9) - 1), /* default 9 tags; updated with config.tag_num */

	.key_mode =
		{
			.mode = {'d', 'e', 'f', 'a', 'u', 'l', 't', '\0'},
			.isdefault = true,
		},

	.print_status_listener = {.notify = handle_print_status},
	.cursor_axis_listener = {.notify = handle_cursor_axis},
	.cursor_button_listener = {.notify = handle_cursor_button},
	.cursor_frame_listener = {.notify = handle_cursor_frame},
	.cursor_motion_listener = {.notify = handle_cursor_motion},
	.cursor_motion_absolute_listener = {.notify =
											handle_cursor_motion_absolute},
	.gpu_reset_listener = {.notify = handle_renderer_lost},
	.layout_change_listener = {.notify = handle_output_layout_change},
	.new_idle_inhibitor_listener = {.notify = handle_new_idle_inhibitor},
	.new_input_device_listener = {.notify = handle_new_input_device},
	.new_virtual_keyboard_listener = {.notify = handle_new_virtual_keyboard},
	.new_virtual_pointer_listener = {.notify = handle_new_virtual_pointer},
	.new_pointer_constraint_listener = {.notify =
											handle_new_pointer_constraint},
	.new_output_listener = {.notify = handle_new_output},
	.new_xdg_toplevel_listener = {.notify = handle_new_xdg_toplevel},
	.new_xdg_popup_listener = {.notify = handle_new_xdg_popup},
	.new_xdg_decoration_listener = {.notify = handle_new_xdg_decoration},
	.new_layer_surface_listener = {.notify = handle_new_layer_surface},
	.output_manager_apply_listener = {.notify = handle_output_manager_apply},
	.output_manager_test_listener = {.notify = handle_output_manager_test},
	.output_power_manager_set_mode_listener =
		{.notify = handle_output_power_manager_set_mode},
	.tearing_new_object_listener = {.notify = handle_tearing_new_object},
	.ext_workspace_commit_listener = {.notify = handle_ext_commit},
	.tablet_tool_axis_listener = {.notify = handle_tablet_tool_axis},
	.tablet_tool_button_listener = {.notify = handle_tablet_tool_button},
	.tablet_tool_proximity_listener = {.notify = handle_tablet_tool_proximity},
	.tablet_tool_tip_listener = {.notify = handle_tablet_tool_tip},
	.cursor_touch_down_listener = {.notify = handle_cursor_touch_down},
	.cursor_touch_up_listener = {.notify = handle_cursor_touch_up},
	.cursor_touch_cancel_listener = {.notify = handle_cursor_touch_cancel},
	.cursor_touch_motion_listener = {.notify = handle_cursor_touch_motion},
	.cursor_touch_frame_listener = {.notify = handle_cursor_touch_frame},
	.ext_image_copy_capture_manager_new_session_listener =
		{.notify = handle_ext_image_copy_capture_new_session},
	.request_cursor_listener = {.notify = handle_request_set_cursor},
	.request_set_psel_listener = {.notify =
									  handle_request_set_primary_selection},
	.request_set_sel_listener = {.notify = handle_request_set_selection},
	.request_set_cursor_shape_listener = {.notify =
											  handle_request_set_cursor_shape},
	.request_start_drag_listener = {.notify = handle_request_start_drag},
	.start_drag_listener = {.notify = handle_start_drag},
	.new_session_lock_listener = {.notify = handle_new_session_lock},
	.drm_lease_request_listener = {.notify = handle_drm_lease_request},
	.keyboard_shortcuts_inhibit_new_inhibitor_listener =
		{.notify = handle_keyboard_shortcuts_inhibit_new_inhibitor},
	.last_cursor_surface_destroy_listener =
		{.notify = handle_last_cursor_surface_destroy},

#ifdef XWAYLAND
	.new_xwayland_surface_listener = {.notify = handle_new_xwayland_surface},
	.xwayland_ready_listener = {.notify = handle_xwayland_ready},
#endif
};
