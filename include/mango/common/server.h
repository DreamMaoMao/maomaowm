#ifndef __MANGO_SERVER_H__
#define __MANGO_SERVER_H__ 1

#include "mango/mango.h"

/* Tag masks */
#define TAG0_MASK (1U << 31)
#define TAGMASK (server.tagmask)

struct MangoServer {
	/* Display server / rendering core */
	struct wl_display *display;
	struct wl_event_loop *event_loop;
	struct wlr_backend *backend;
	struct wlr_backend *headless_backend;
	struct wlr_scene *scene;
	struct wlr_scene_tree *layers[NUM_LAYERS];
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;

	/* wlroots globals */
	struct wlr_xdg_shell *xdg_shell;
	struct wlr_xdg_decoration_manager_v1 *decoration_manager;
	struct wl_list clients;		/* tiling order */
	struct wl_list focus_stack; /* focus order */
	struct wl_list fadeout_clients;
	struct wl_list fadeout_layers;
	struct wlr_idle_notifier_v1 *idle_notifier;
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit_manager;
	struct wlr_layer_shell_v1 *layer_shell;
	struct wlr_output_manager_v1 *output_manager;
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_manager;
	struct wlr_keyboard_shortcuts_inhibit_manager_v1
		*keyboard_shortcuts_inhibit;
	struct wlr_virtual_pointer_manager_v1 *virtual_pointer_manager;
	struct wlr_output_power_manager_v1 *power_manager;
	struct wlr_ext_image_copy_capture_manager_v1
		*ext_image_copy_capture_manager;
	struct wlr_pointer_gestures_v1 *pointer_gestures;
	struct wlr_drm_lease_v1_manager *drm_lease_manager;
	struct mango_print_status_manager *print_status_manager;

	/* Pointer / cursor */
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_manager;
	struct wlr_session *session;

	/* Session lock */
	struct wlr_scene_rect *root_bg;
	struct wlr_session_lock_manager_v1 *session_lock_manager;
	struct wlr_scene_rect *locked_bg;
	struct wlr_session_lock_v1 *current_lock;
	struct wlr_scene_tree *drag_icon;
	struct wlr_cursor_shape_manager_v1 *cursor_shape_manager;
	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;
	struct wlr_pointer_constraint_v1 *active_constraint;

	/* External protocol managers */
	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;
	struct wlr_tearing_control_manager_v1 *tearing_control;
	struct wl_listener tearing_new_object_listener;
	struct wlr_input_method_manager_v2 *input_method_manager;
	struct wlr_text_input_manager_v3 *text_input_manager;
	struct mango_input_method_relay *input_method_relay;
	struct wlr_ext_workspace_manager_v1 *ext_workspace_manager;
	struct wl_listener ext_workspace_commit_listener;

	/* Keyboard / seat */
	struct wlr_seat *seat;
	KeyboardGroup *keyboard_group;
	struct wlr_keyboard *last_active_keyboard; /* last keyboard to emit a key;
												  used to query layout */
	struct wl_list input_devices;
	struct wl_list standalone_keyboards; /* standalone keyboards list */
	struct wl_list virtual_keyboards;	 /* virtual keyboard list */
	struct wl_list keyboard_shortcut_inhibitors;
	uint32_t cursor_mode;

	/* Touch / tablet */
	struct wlr_tablet_manager_v2 *tablet_manager;
	struct wl_list tablets;
	struct wl_list tablet_pads;
	struct wl_listener tablet_tool_axis_listener;
	struct wl_listener tablet_tool_button_listener;
	struct wl_listener tablet_tool_proximity_listener;
	struct wl_listener tablet_tool_tip_listener;
	struct wl_list touch_points;
	struct wl_listener cursor_touch_down_listener;
	struct wl_listener cursor_touch_up_listener;
	struct wl_listener cursor_touch_cancel_listener;
	struct wl_listener cursor_touch_motion_listener;
	struct wl_listener cursor_touch_frame_listener;

	/* Drag / resize */
	Client *grab_client, *drop_client;
	int32_t resize_corner;
	int32_t grab_offset_x, grab_offset_y;			  /* client-relative */
	int32_t drag_begin_cursor_x, drag_begin_cursor_y; /* client-relative */
	bool start_drag_window;
	int32_t last_apply_drag_time;

	/* Outputs / monitors */
	struct wlr_output_layout *output_layout;
	struct wlr_box scene_geometry;
	struct wl_list monitors;
	Monitor *selected_monitor;
	struct wlr_scene_output_layout *scene_layout;

	/* Layout / gestures */
	int32_t enable_gaps; /* enables gaps; used by toggle_gaps */
	int32_t axis_apply_time;
	int32_t axis_apply_dir;
	int32_t scroller_focus_lock;

	uint32_t swipe_fingers;
	double swipe_dx;
	double swipe_dy;

	bool render_border;

	/* Other runtime state */
	uint32_t chvt_backup_tag;
	bool allow_frame_scheduling;
	char chvt_backup_monitor_name[32];
	bool cursor_hidden;
	bool tag_combo;
	char cli_config_path[1024];
	int active_capture_count;
	bool cli_debug_log;
	uint32_t last_hold_keycode;
	uint32_t tagmask; /* default 9 tags; updated with config.tag_num */
	uint32_t next_client_id;

	/* Session lock state */
	int32_t session_locked;
	uint32_t locked_modifiers;
	void *exclusive_focus;
	pid_t child_pid;

	/* Pre-baked animation curves */
	struct dvec2 *baked_points_move;
	struct dvec2 *baked_points_open;
	struct dvec2 *baked_points_tag;
	struct dvec2 *baked_points_close;
	struct dvec2 *baked_points_focus;
	struct dvec2 *baked_points_opafadein;
	struct dvec2 *baked_points_opafadeout;

	/* Cursor hiding / input keep-alive */
	struct wl_event_source *hide_cursor_source;
	struct wl_event_source *keep_idle_inhibit_source;

	/* Key mode / IPC print-status signal */
	KeyMode key_mode;
	struct wl_signal print_status_signal;

	/* wlroots event listeners */
	struct wl_listener print_status_listener;
	struct wl_listener cursor_axis_listener;
	struct wl_listener cursor_button_listener;
	struct wl_listener cursor_frame_listener;
	struct wl_listener cursor_motion_listener;
	struct wl_listener cursor_motion_absolute_listener;
	struct wl_listener gpu_reset_listener;
	struct wl_listener layout_change_listener;
	struct wl_listener new_idle_inhibitor_listener;
	struct wl_listener new_input_device_listener;
	struct wl_listener new_virtual_keyboard_listener;
	struct wl_listener new_virtual_pointer_listener;
	struct wl_listener new_pointer_constraint_listener;
	struct wl_listener new_output_listener;
	struct wl_listener new_xdg_toplevel_listener;
	struct wl_listener new_xdg_popup_listener;
	struct wl_listener new_xdg_decoration_listener;
	struct wl_listener new_layer_surface_listener;
	struct wl_listener output_manager_apply_listener;
	struct wl_listener output_manager_test_listener;
	struct wl_listener output_power_manager_set_mode_listener;
	struct wl_listener ext_image_copy_capture_manager_new_session_listener;
	struct wl_listener request_cursor_listener;
	struct wl_listener request_set_psel_listener;
	struct wl_listener request_set_sel_listener;
	struct wl_listener request_set_cursor_shape_listener;
	struct wl_listener request_start_drag_listener;
	struct wl_listener start_drag_listener;
	struct wl_listener new_session_lock_listener;
	struct wl_listener drm_lease_request_listener;
	struct wl_listener keyboard_shortcuts_inhibit_new_inhibitor_listener;
	struct wl_listener last_cursor_surface_destroy_listener;

	/* External image capture */
	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1
		*ext_foreign_toplevel_image_capture_source_manager;
	struct wl_listener new_foreign_toplevel_capture_listener;
	struct wlr_ext_foreign_toplevel_list_v1 *foreign_toplevel_list;

#ifdef XWAYLAND
	struct wlr_xwayland *xwayland;
	struct wl_event_source *sync_keymap;
	struct wl_listener new_xwayland_surface_listener;
	struct wl_listener xwayland_ready_listener;
#endif
};

extern struct MangoServer server;

#endif
