#include "mango/common/globals.h"
#include "mango/ipc/ipc.h"
#include "mango/input/pointer.h"
#include "mango/input/device.h"
#include "mango/input/keyboard.h"
#include "mango/manage/client.h"
#include "mango/manage/layer.h"
#include "mango/manage/misc.h"
#include "mango/manage/monitor.h"

const char broken[] = "broken";
pid_t child_pid = -1;
int32_t locked;
uint32_t locked_mods = 0;
void *exclusive_focus;
struct wl_display *dpy;
struct wl_event_loop *event_loop;
struct wlr_backend *backend;
struct wlr_backend *headless_backend;
struct wlr_scene *scene;
struct wlr_scene_tree *layers[NUM_LAYERS];
struct wlr_renderer *drw;
struct wlr_allocator *alloc;
struct wlr_compositor *compositor;

struct wlr_xdg_shell *xdg_shell;
struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
struct wl_list clients; /* tiling order */
struct wl_list fstack;	/* focus order */
struct wl_list fadeout_clients;
struct wl_list fadeout_layers;
struct wlr_idle_notifier_v1 *idle_notifier;
struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
struct wlr_layer_shell_v1 *layer_shell;
struct wlr_output_manager_v1 *output_mgr;
struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
struct wlr_keyboard_shortcuts_inhibit_manager_v1 *keyboard_shortcuts_inhibit;
struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
struct wlr_output_power_manager_v1 *power_mgr;
struct wlr_ext_image_copy_capture_manager_v1 *ext_image_copy_capture_mgr;
struct wlr_pointer_gestures_v1 *pointer_gestures;
struct wlr_drm_lease_v1_manager *drm_lease_manager;
struct mango_print_status_manager *print_status_manager;

struct wlr_cursor *cursor;
struct wlr_xcursor_manager *cursor_mgr;
struct wlr_session *session;

struct wlr_scene_rect *root_bg;
struct wlr_session_lock_manager_v1 *session_lock_mgr;
struct wlr_scene_rect *locked_bg;
struct wlr_session_lock_v1 *cur_lock;
const int32_t layermap[] = {LyrBg, LyrBottom, LyrTop, LyrOverlay};
struct wlr_scene_tree *drag_icon;
struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
struct wlr_pointer_constraints_v1 *pointer_constraints;
struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
struct wlr_pointer_constraint_v1 *active_constraint;

struct wlr_seat *seat;
KeyboardGroup *kb_group;
struct wlr_keyboard
	*last_active_keyboard; /* 最后按键的键盘，get keyboardlayout 用 */
struct wl_list inputdevices;
struct wl_list standalone_keyboards; /* 独立键盘链表 */
struct wl_list virtual_keyboards;	 /* 虚拟键盘组链表 */
struct wl_list keyboard_shortcut_inhibitors;
uint32_t cursor_mode;
Client *grabc, *dropc;
int32_t rzcorner;
int32_t grabcx, grabcy; /* client-relative */

struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1
	*ext_foreign_toplevel_image_capture_source_manager_v1;
struct wl_listener new_foreign_toplevel_capture_request;
struct wlr_ext_foreign_toplevel_list_v1 *foreign_toplevel_list;

int32_t drag_begin_cursorx, drag_begin_cursory; /* client-relative */
bool start_drag_window = false;
int32_t last_apply_drap_time = 0;

struct wlr_output_layout *output_layout;
struct wlr_box sgeom;
struct wl_list mons;
Monitor *selmon;
struct wlr_scene_output_layout *scene_layout;

int32_t enablegaps = 1; /* enables gaps, used by togglegaps */
int32_t axis_apply_time = 0;
int32_t axis_apply_dir = 0;
int32_t scroller_focus_lock = 0;

uint32_t swipe_fingers = 0;
double swipe_dx = 0;
double swipe_dy = 0;

bool render_border = true;

uint32_t chvt_backup_tag = 0;
bool allow_frame_scheduling = true;
char chvt_backup_selmon[32] = {0};

struct dvec2 *baked_points_move;
struct dvec2 *baked_points_open;
struct dvec2 *baked_points_tag;
struct dvec2 *baked_points_close;
struct dvec2 *baked_points_focus;
struct dvec2 *baked_points_opafadein;
struct dvec2 *baked_points_opafadeout;

struct wl_event_source *hide_cursor_source;
struct wl_event_source *keep_idle_inhibit_source;
bool cursor_hidden = false;
bool tag_combo = false;
char cli_config_path[1024] = {0};
int active_capture_count = 0;
bool cli_debug_log = false;
uint32_t last_hold_keycode = 0;

KeyMode keymode = {
	.mode = {'d', 'e', 'f', 'a', 'u', 'l', 't', '\0'},
	.isdefault = true,
};

char *env_vars[] = {"DISPLAY",
					"WAYLAND_DISPLAY",
					"XDG_CURRENT_DESKTOP",
					"XDG_SESSION_TYPE",
					"XCURSOR_THEME",
					"XCURSOR_SIZE",
					"MANGO_INSTANCE_SIGNATURE",
					NULL};
struct wl_signal mango_print_status;

struct wl_listener print_status_listener = {.notify = handle_print_status};
struct wl_listener cursor_axis = {.notify = axisnotify};
struct wl_listener cursor_button = {.notify = buttonpress};
struct wl_listener cursor_frame = {.notify = cursorframe};
struct wl_listener cursor_motion = {.notify = motionrelative};
struct wl_listener cursor_motion_absolute = {.notify = motionabsolute};
struct wl_listener gpu_reset = {.notify = gpureset};
struct wl_listener layout_change = {.notify = updatemons};
struct wl_listener new_idle_inhibitor = {.notify = createidleinhibitor};
struct wl_listener new_input_device = {.notify = inputdevice};
struct wl_listener new_virtual_keyboard = {.notify = virtualkeyboard};
struct wl_listener new_virtual_pointer = {.notify = virtualpointer};
struct wl_listener new_pointer_constraint = {.notify = createpointerconstraint};
struct wl_listener new_output = {.notify = createmon};
struct wl_listener new_xdg_toplevel = {.notify = createnotify};
struct wl_listener new_xdg_popup = {.notify = createpopup};
struct wl_listener new_xdg_decoration = {.notify = createdecoration};
struct wl_listener new_layer_surface = {.notify = createlayersurface};
struct wl_listener output_mgr_apply = {.notify = outputmgrapply};
struct wl_listener output_mgr_test = {.notify = outputmgrtest};
struct wl_listener output_power_mgr_set_mode = {.notify = powermgrsetmode};
struct wl_listener ext_image_copy_capture_mgr_new_session = {
	.notify = handle_iamge_copy_capture_new_session};
struct wl_listener request_cursor = {.notify = setcursor};
struct wl_listener request_set_psel = {.notify = setpsel};
struct wl_listener request_set_sel = {.notify = setsel};
struct wl_listener request_set_cursor_shape = {.notify = setcursorshape};
struct wl_listener request_start_drag = {.notify = requeststartdrag};
struct wl_listener start_drag = {.notify = startdrag};
struct wl_listener new_session_lock = {.notify = locksession};
struct wl_listener drm_lease_request = {.notify = requestdrmlease};
struct wl_listener keyboard_shortcuts_inhibit_new_inhibitor = {
	.notify = handle_keyboard_shortcuts_inhibit_new_inhibitor};
struct wl_listener last_cursor_surface_destroy_listener = {
	.notify = last_cursor_surface_destroy};

#ifdef XWAYLAND
struct wl_listener new_xwayland_surface = {.notify = createnotifyx11};
struct wl_listener xwayland_ready = {.notify = xwaylandready};
struct wlr_xwayland *xwayland;
struct wl_event_source *sync_keymap;
#endif // XWAYLAND

// config variable defined here
Config config;
uint32_t tagmask = ((1u << 9) - 1); /* 默认 9 个 tag，随 config.tag_num 更新 */

/* config/preset.h */
const char *tags[] = {
	"1",  "2",	"3",  "4",	"5",  "6",	"7",  "8",	"9",  "10", "11",
	"12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22",
	"23", "24", "25", "26", "27", "28", "29", "30", "31",
};

const struct xkb_rule_names xkb_fallback_rules = {
	.layout = "us",
	.variant = NULL,
	.model = NULL,
	.rules = NULL,
	.options = NULL,
};

/* config/parse_config.h */
const char default_jump_labels[] = "HJKLASDFGQWERTYUIOPZXCVBNM";
char **file_paths = NULL;
int file_paths_count = 0;
int current_file_index = -1;

/* manage/client.h */
uint32_t next_client_id = 0;
