#ifndef __COMMON_GLOBALS_H__
#define __COMMON_GLOBALS_H__ 1

#include <time.h>
#include <wayland-util.h>
#include "mango/mango.h"
#include "mango/config/parse_config.h"

// Macros
#define TAG0_MASK (1U << 31)
/* PERTAG_SLOTS / PERTAG_ALL_TAGS_IDX 定义于 mango.h（Pertag 结构体前） */

/* Global variables */
extern const char broken[];
extern pid_t child_pid;
extern int32_t locked;
extern uint32_t locked_mods;
extern void *exclusive_focus;
extern struct wl_display *dpy;
extern struct wl_event_loop *event_loop;
extern struct wlr_backend *backend;
extern struct wlr_backend *headless_backend;
extern struct wlr_scene *scene;
extern struct wlr_scene_tree *layers[NUM_LAYERS];
extern struct wlr_renderer *drw;
extern struct wlr_allocator *alloc;
extern struct wlr_compositor *compositor;

extern struct wlr_xdg_shell *xdg_shell;
extern struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
extern struct wl_list clients; /* tiling order */
extern struct wl_list fstack;  /* focus order */
extern struct wl_list fadeout_clients;
extern struct wl_list fadeout_layers;
extern struct wlr_idle_notifier_v1 *idle_notifier;
extern struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
extern struct wlr_layer_shell_v1 *layer_shell;
extern struct wlr_output_manager_v1 *output_mgr;
extern struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
extern struct wlr_keyboard_shortcuts_inhibit_manager_v1
	*keyboard_shortcuts_inhibit;
extern struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
extern struct wlr_output_power_manager_v1 *power_mgr;
extern struct wlr_ext_image_copy_capture_manager_v1 *ext_image_copy_capture_mgr;
extern struct wlr_pointer_gestures_v1 *pointer_gestures;
extern struct wlr_drm_lease_v1_manager *drm_lease_manager;
extern struct mango_print_status_manager *print_status_manager;

extern struct wlr_cursor *cursor;
extern struct wlr_xcursor_manager *cursor_mgr;
extern struct wlr_session *session;

extern struct wlr_scene_rect *root_bg;
extern struct wlr_session_lock_manager_v1 *session_lock_mgr;
extern struct wlr_scene_rect *locked_bg;
extern struct wlr_session_lock_v1 *cur_lock;
extern const int32_t layermap[];
extern struct wlr_scene_tree *drag_icon;
extern struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
extern struct wlr_pointer_constraints_v1 *pointer_constraints;
extern struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
extern struct wlr_pointer_constraint_v1 *active_constraint;

extern struct wlr_seat *seat;
extern KeyboardGroup *kb_group;
extern struct wlr_keyboard
	*last_active_keyboard; /* 最后按键的键盘，get keyboardlayout 用 */
extern struct wl_list inputdevices;
extern struct wl_list standalone_keyboards; /* 独立键盘链表 */
extern struct wl_list virtual_keyboards; /* 虚拟键盘组链表 */
extern struct wl_list keyboard_shortcut_inhibitors;
extern uint32_t cursor_mode;
extern Client *grabc, *dropc;
extern int32_t rzcorner;
extern int32_t grabcx, grabcy; /* client-relative */

extern struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1
	*ext_foreign_toplevel_image_capture_source_manager_v1;
extern struct wl_listener new_foreign_toplevel_capture_request;
extern struct wlr_ext_foreign_toplevel_list_v1 *foreign_toplevel_list;

extern int32_t drag_begin_cursorx, drag_begin_cursory; /* client-relative */
extern bool start_drag_window;
extern int32_t last_apply_drap_time;

extern struct wlr_output_layout *output_layout;
extern struct wlr_box sgeom;
extern struct wl_list mons;
extern Monitor *selmon;
extern struct wlr_scene_output_layout *scene_layout;

extern int32_t enablegaps; /* enables gaps, used by togglegaps */
extern int32_t axis_apply_time;
extern int32_t axis_apply_dir;
extern int32_t scroller_focus_lock;

extern uint32_t swipe_fingers;
extern double swipe_dx;
extern double swipe_dy;

extern bool render_border;

extern uint32_t chvt_backup_tag;
extern bool allow_frame_scheduling;
extern char chvt_backup_selmon[32];

extern struct dvec2 *baked_points_move;
extern struct dvec2 *baked_points_open;
extern struct dvec2 *baked_points_tag;
extern struct dvec2 *baked_points_close;
extern struct dvec2 *baked_points_focus;
extern struct dvec2 *baked_points_opafadein;
extern struct dvec2 *baked_points_opafadeout;

extern struct wl_event_source *hide_cursor_source;
extern struct wl_event_source *keep_idle_inhibit_source;
extern bool cursor_hidden;
extern bool tag_combo;
extern char cli_config_path[1024];
extern int active_capture_count;
extern bool cli_debug_log;
extern uint32_t last_hold_keycode;

extern KeyMode keymode;

extern char *env_vars[];
extern struct wl_signal mango_print_status;

extern struct wl_listener print_status_listener;
extern struct wl_listener cursor_axis;
extern struct wl_listener cursor_button;
extern struct wl_listener cursor_frame;
extern struct wl_listener cursor_motion;
extern struct wl_listener cursor_motion_absolute;
extern struct wl_listener gpu_reset;
extern struct wl_listener layout_change;
extern struct wl_listener new_idle_inhibitor;
extern struct wl_listener new_input_device;
extern struct wl_listener new_virtual_keyboard;
extern struct wl_listener new_virtual_pointer;
extern struct wl_listener new_pointer_constraint;
extern struct wl_listener new_output;
extern struct wl_listener new_xdg_toplevel;
extern struct wl_listener new_xdg_popup;
extern struct wl_listener new_xdg_decoration;
extern struct wl_listener new_layer_surface;
extern struct wl_listener output_mgr_apply;
extern struct wl_listener output_mgr_test;
extern struct wl_listener output_power_mgr_set_mode;
extern struct wl_listener ext_image_copy_capture_mgr_new_session;
extern struct wl_listener request_cursor;
extern struct wl_listener request_set_psel;
extern struct wl_listener request_set_sel;
extern struct wl_listener request_set_cursor_shape;
extern struct wl_listener request_start_drag;
extern struct wl_listener start_drag;
extern struct wl_listener new_session_lock;
extern struct wl_listener drm_lease_request;
extern struct wl_listener keyboard_shortcuts_inhibit_new_inhibitor;
extern struct wl_listener last_cursor_surface_destroy_listener;

#ifdef XWAYLAND
float xwayland_client_scale(Client *c);
float xwayland_preferred_scale(Client *c);
void xwayland_apply_scale(Client *c);
void xwayland_logical_to_x11(struct wlr_box *box, float scale);
void xwayland_x11_to_logical(struct wlr_box *box, float scale);
bool xwayland_scene_buffer_point_accepts_input(struct wlr_scene_buffer *buffer,
											   double *sx, double *sy);
void fix_xwayland_coordinate(struct wlr_box *geom);
int32_t synckeymap(void *data);
void activatex11(struct wl_listener *listener, void *data);
void configurex11(struct wl_listener *listener, void *data);
void createnotifyx11(struct wl_listener *listener, void *data);
void dissociatex11(struct wl_listener *listener, void *data);
void commitx11(struct wl_listener *listener, void *data);
void associatex11(struct wl_listener *listener, void *data);
void sethints(struct wl_listener *listener, void *data);
void xwaylandready(struct wl_listener *listener, void *data);
void setgeometrynotify(struct wl_listener *listener, void *data);
extern struct wl_listener new_xwayland_surface;
extern struct wl_listener xwayland_ready;
extern struct wlr_xwayland *xwayland;
extern struct wl_event_source *sync_keymap;
#endif // XWAYLAND

// config variable defined here
extern Config config;

#endif
