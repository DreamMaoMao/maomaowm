#ifndef __MANGO_COMMON_TYPES_H__
#define __MANGO_COMMON_TYPES_H__ 1

/*
 * Cross-module opaque type handles.
 *
 * Only forward declarations live here. Every header that merely passes these
 * types around by pointer can include this file; the actual definitions live
 * in their owning module headers (manage/client.h, input/keyboard.h, ...).
 */
typedef struct MangoServer MangoServer;
typedef struct Arg Arg;
typedef struct Client Client;
typedef struct Monitor Monitor;
typedef struct Pertag Pertag;
typedef struct InputDevice InputDevice;
typedef struct Switch Switch;
typedef struct KeyboardGroup KeyboardGroup;
typedef struct KeyboardShortcutsInhibitor KeyboardShortcutsInhibitor;
typedef struct LayerSurface LayerSurface;
typedef struct Popup Popup;
typedef struct Layout Layout;
typedef struct DwindleNode DwindleNode;
typedef struct TagScrollerState TagScrollerState;
typedef struct ScrollerStackNode ScrollerStackNode;
typedef struct SessionLock SessionLock;
typedef struct PointerConstraint PointerConstraint;
typedef struct SnapshotMetadata SnapshotMetadata;
typedef struct LastCursor LastCursor;
typedef struct MangoJumpLabel MangoJumpLabel;
typedef struct MangoGroupBar MangoGroupBar;
typedef struct MangoCloseButton MangoCloseButton;

/*
 * Forward declarations for libwayland / wlroots / scenefx structs referenced by
 * module headers. Translation units that need the actual definitions include
 * the corresponding headers themselves.
 */
struct wl_client;
struct wl_display;
struct wl_event_loop;
struct wl_event_source;
struct wl_listener;
struct wl_resource;
struct wl_signal;

struct wlr_allocator;
struct wlr_backend;
struct wlr_box;
struct wlr_buffer;
struct wlr_color_transform;
struct wlr_compositor;
struct wlr_cursor;
struct wlr_cursor_shape_manager_v1;
struct wlr_drm_lease_v1_manager;
struct wlr_ext_foreign_toplevel_handle_v1;
struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1;
struct wlr_ext_foreign_toplevel_list_v1;
struct wlr_ext_image_capture_source_v1;
struct wlr_ext_image_copy_capture_manager_v1;
struct wlr_ext_workspace_group_handle_v1;
struct wlr_ext_workspace_handle_v1;
struct wlr_ext_workspace_manager_v1;
struct wlr_foreign_toplevel_handle_v1;
struct wlr_foreign_toplevel_manager_v1;
struct wlr_idle_inhibit_manager_v1;
struct wlr_idle_notifier_v1;
struct wlr_input_device;
struct wlr_input_method_keyboard_grab_v2;
struct wlr_input_method_manager_v2;
struct wlr_input_method_v2;
struct wlr_input_popup_surface_v2;
struct wlr_keyboard;
struct wlr_keyboard_group;
struct wlr_keyboard_key_event;
struct wlr_keyboard_modifiers;
struct wlr_keyboard_shortcuts_inhibit_manager_v1;
struct wlr_keyboard_shortcuts_inhibitor_v1;
struct wlr_layer_shell_v1;
struct wlr_layer_surface_v1;
struct wlr_output;
struct wlr_output_configuration_v1;
struct wlr_output_layout;
struct wlr_output_manager_v1;
struct wlr_output_mode;
struct wlr_output_power_manager_v1;
struct wlr_output_state;
struct wlr_pointer;
struct wlr_pointer_button_event;
struct wlr_pointer_constraint_v1;
struct wlr_pointer_constraints_v1;
struct wlr_pointer_gestures_v1;
struct wlr_pointer_swipe_end_event;
struct wlr_relative_pointer_manager_v1;
struct wlr_renderer;
struct wlr_scene;
struct wlr_scene_blur;
struct wlr_scene_buffer;
struct wlr_scene_layer_surface_v1;
struct wlr_scene_node;
struct wlr_scene_optimized_blur;
struct wlr_scene_output;
struct wlr_scene_output_layout;
struct wlr_scene_rect;
struct wlr_scene_shadow;
struct wlr_scene_surface;
struct wlr_scene_tree;
struct wlr_seat;
struct wlr_session;
struct wlr_session_lock_manager_v1;
struct wlr_session_lock_surface_v1;
struct wlr_session_lock_v1;
struct wlr_surface;
struct wlr_switch;
struct wlr_tablet_manager_v2;
struct wlr_tablet_v2_tablet;
struct wlr_tablet_v2_tablet_pad;
struct wlr_tablet_v2_tablet_tool;
struct wlr_tearing_control_manager_v1;
struct wlr_tearing_control_v1;
struct wlr_text_input_manager_v3;
struct wlr_text_input_v3;
struct wlr_touch;
struct wlr_virtual_keyboard_manager_v1;
struct wlr_virtual_pointer_manager_v1;
struct wlr_xcursor_manager;
struct wlr_xdg_activation_token_v1;
struct wlr_xdg_decoration_manager_v1;
struct wlr_xdg_popup;
struct wlr_xdg_shell;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel_decoration_v1;
struct wlr_xwayland;
struct wlr_xwayland_surface;

#endif
