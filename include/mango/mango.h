#ifndef __MANGO_H__
#define __MANGO_H__ 1

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_color_management_v1.h>
#include <wlr/types/wlr_color_representation_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_fixes.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_switch.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <fcntl.h>
#include <getopt.h>
#include <libinput.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <X11/Xlib.h>
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>
#endif
#include "mango/draw/text-node.h"

/* macros */
#define MANGO_MAX(A, B) ((A) > (B) ? (A) : (B))
#define MANGO_MIN(A, B) ((A) < (B) ? (A) : (B))
#define GEZERO(A) ((A) >= 0 ? (A) : 0)
#define CLEANMASK(mask) (mask & ~WLR_MODIFIER_CAPS)
#define INSIDEMON(A)                                                           \
	(A->geom.x >= A->mon->m.x && A->geom.y >= A->mon->m.y &&                   \
	 A->geom.x + A->geom.width <= A->mon->m.x + A->mon->m.width &&             \
	 A->geom.y + A->geom.height <= A->mon->m.y + A->mon->m.height)
#define GEOMINSIDEMON(A, M)                                                    \
	(A->x >= M->m.x && A->y >= M->m.y &&                                       \
	 A->x + A->width <= M->m.x + M->m.width &&                                 \
	 A->y + A->height <= M->m.y + M->m.height)
#define ISTILED(A)                                                             \
	(A && !(A)->isfloating && !(A)->isminimized && !(A)->iskilling &&          \
	 !(A)->ismaximizescreen && !(A)->isfullscreen && !(A)->isunglobal)
#define ISNORMAL(A)                                                            \
	(A && !(A)->isminimized && !(A)->iskilling && !(A)->isunglobal)
#define ISSCROLLTILED(A)                                                       \
	(A && !(A)->isfloating && !(A)->isminimized && !(A)->iskilling &&          \
	 !(A)->isunglobal)
#define ISFAKETILED(A)                                                         \
	(A && !(A)->isfloating && !(A)->isminimized && !(A)->iskilling &&          \
	 !(A)->isunglobal)
#define VISIBLEON(C, M)                                                        \
	((C) && (M) && (C)->mon == (M) && !(C)->is_logic_hide &&                   \
	 !(C)->isminimized &&                                                      \
	 (((C)->tags & (M)->tagset[(M)->seltags] || (C)->isglobal ||               \
	   (C)->isunglobal)))

#define TAGMATCH(C, M)                                                         \
	((C) && (M) && (C)->mon == (M) && !(C)->is_logic_hide &&                   \
	 !(C)->isminimized && (((C)->tags & (M)->tagset[(M)->seltags])))

#define ISMODEKEYCODE(KEY)                                                     \
	((KEY) == 133 || (KEY) == 37 || (KEY) == 64 || (KEY) == 50 ||              \
	 (KEY) == 134 || (KEY) == 105 || (KEY) == 108 || (KEY) == 62)

#define LENGTH(X) (sizeof X / sizeof X[0])
#define END(A) ((A) + LENGTH(A))
#define LISTEN(E, L, H) wl_signal_add((E), ((L)->notify = (H), (L)))

#define ISFULLSCREEN(A)                                                        \
	((A)->isfullscreen || (A)->ismaximizescreen ||                             \
	 (A)->overview_ismaximizescreenbak || (A)->overview_isfullscreenbak)
#define LISTEN_STATIC(E, H)                                                    \
	do {                                                                       \
		struct wl_listener *_l = ecalloc(1, sizeof(*_l));                      \
		_l->notify = (H);                                                      \
		wl_signal_add((E), _l);                                                \
	} while (0)

#define APPLY_INT_PROP(obj, rule, prop)                                        \
	if (rule->prop >= 0)                                                       \
	obj->prop = rule->prop

#define APPLY_FLOAT_PROP(obj, rule, prop)                                      \
	if (rule->prop > 0.0f)                                                     \
	obj->prop = rule->prop

#define APPLY_STRING_PROP(obj, rule, prop)                                     \
	if (rule->prop != NULL)                                                    \
	obj->prop = rule->prop

#define BAKED_POINTS_COUNT 256

#define IPC_WATCH_ARRANGGE                                                     \
	IPC_WATCH_MONITOR | IPC_WATCH_CLIENT | IPC_WATCH_TAGS |                    \
		IPC_WATCH_ALL_MONITORS | IPC_WATCH_ALL_TAGS | IPC_WATCH_ALL_CLIENTS |  \
		IPC_WATCH_LAST_OPEN_SURFACE | IPC_WATCH_FOCUSING_CLIENT

/* enums */
enum { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

enum { VERTICAL, HORIZONTAL };
enum { SWIPE_UP, SWIPE_DOWN, SWIPE_LEFT, SWIPE_RIGHT };
enum { CurNormal, CurPressed, CurMove, CurResize }; /* cursor */
enum {
	XDGShell,
	LayerShell,
	X11,
	Snapshot,
	XdgPopup,
	XdgImPopup,
	GroupBar
}; /* client types */
enum { AxisUp, AxisDown, AxisLeft, AxisRight }; // Scroll wheel direction

#ifdef XWAYLAND
enum {
	NetWMWindowTypeDialog,
	NetWMWindowTypeSplash,
	NetWMWindowTypeToolbar,
	NetWMWindowTypeUtility,
	NetLast
}; /* EWMH atoms */
#endif

enum {
	LyrBg,
	LyrBottom,
	LyrTile,
	LyrMaximize,
	LyrTop,
	LyrSpecialDim,
	LyrSpecialTile,
	LyrSpecialMaximize,
	LyrSpecialTop,
	LyrFadeOut,
	LyrOverlay,
	LyrIMPopup,
	LyrBlock,
	NUM_LAYERS
};

enum { UP, DOWN, LEFT, RIGHT, UNDIR }; /* smartmovewin */
enum { NONE, OPEN, MOVE, CLOSE, TAG, FOCUS, OPAFADEIN, OPAFADEOUT, OVERVIEW };
enum { UNFOLD, FOLD, INVALIDFOLD };
enum { PREV, NEXT };
enum { SW_CURRENT_TAG, SW_ALL_TAG, SW_ALL_MON }; /* Switcher candidate scope */
enum { STATE_UNSPECIFIED = 0, STATE_ENABLED, STATE_DISABLED };
enum { FORCE, UNFORCE };

enum tearing_mode {
	TEARING_DISABLED = 0,
	TEARING_ENABLED,
	TEARING_FULLSCREEN_ONLY,
};

enum seat_config_shortcuts_inhibit {
	SHORTCUTS_INHIBIT_DISABLE,
	SHORTCUTS_INHIBIT_ENABLE,
};

enum ipc_watch_type {
	IPC_WATCH_NONE = 0,
	IPC_WATCH_MONITOR = 1 << 0,
	IPC_WATCH_CLIENT = 1 << 1,
	IPC_WATCH_TAGS = 1 << 2,
	IPC_WATCH_ALL_MONITORS = 1 << 3,
	IPC_WATCH_ALL_TAGS = 1 << 4,
	IPC_WATCH_ALL_CLIENTS = 1 << 5,
	IPC_WATCH_KEYMODE = 1 << 6,
	IPC_WATCH_KB_LAYOUT = 1 << 7,
	IPC_WATCH_LAST_OPEN_SURFACE = 1 << 8,
	IPC_WATCH_FOCUSING_CLIENT = 1 << 9,
	IPC_WATCH_DEVICE = 1 << 10,
};

typedef struct Pertag Pertag;
typedef struct Monitor Monitor;
typedef struct Client Client;

struct dvec2 {
	double x, y;
};

struct ivec2 {
	int32_t x, y, width, height;
};

typedef struct {
	int32_t i;
	int32_t i2;
	float f;
	float f2;
	char *v;
	char *v2;
	char *v3;
	uint32_t ui;
	uint32_t ui2;
	Client *tc;
} Arg;

typedef struct {
	char mode[28];
	bool isdefault;
} KeyMode;

typedef struct {
	struct wl_list link;
	struct wlr_input_device *wlr_device;
	struct libinput_device *libinput_device;
	struct wl_listener destroy_listener;
	void *device_data;
	bool standalone; /* Keyboard matched by a devicerule; independent from the
						default keyboard group. */
	struct wl_listener
		key_watch; /* Records the device that emitted the last key event. */
} InputDevice;

typedef struct {
	struct wl_list link;
	struct wlr_switch *wlr_switch;
	struct wl_listener toggle;
	InputDevice *input_dev;
} Switch;

struct mango_animation {
	bool should_animate;
	bool running;
	bool tagining;
	bool tagouted;
	bool tagouting;
	bool begin_fade_in;
	bool tag_from_rule;
	bool overining;
	bool overview_enter_anim_set;
	uint32_t time_started;
	uint32_t duration;
	struct wlr_box initial;
	struct wlr_box current;
	int32_t action;
};

struct mango_opacity_animation {
	bool running;
	float current_opacity;
	float target_opacity;
	float initial_opacity;
	uint32_t time_started;
	uint32_t duration;
	float current_border_color[4];
	float target_border_color[4];
	float initial_border_color[4];
};

typedef struct {
	float width_scale;
	float height_scale;
	int32_t width;
	int32_t height;
	bool should_scale;
} BufferData;

// Overview card surface node: each surface (including subsurfaces) maps to a
// scene_surface node in the card tree; sx/sy are its coordinates relative to
// the root surface.
struct ov_card_surface {
	Client *c;
	struct wlr_surface *surface;
	struct wlr_scene_surface *scene_surface;
	struct wlr_scene_buffer *buffer;
	int sx, sy; /* Coordinates relative to the root surface */
	bool is_root;
	struct wl_list link;
	struct wl_listener commit; /* Recomputes scaling after commit (commit resets
								  dest/source). */
	struct wl_listener
		destroy; /* Removes the node when the surface is destroyed. */
};

struct Client {
	/* Must keep these three elements in this order */
	uint32_t type; // must at first in struct
	struct wlr_box geom, pending, float_geom, animainit_geom,
		overview_backup_geom, current,
		drag_begin_geom; /* layout-relative, includes border */
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border[4]; /* top, bottom, left, right */
	struct wlr_scene_rect *shield;
	struct wlr_scene_rect *droparea;
	struct wlr_scene_rect *splitindicator[4];
	struct wlr_scene_tree *scene_surface;
	struct wlr_scene_tree *image_capture_tree;
	struct wlr_scene *image_capture_scene;
	struct wlr_ext_image_capture_source_v1 *image_capture_source;
	struct wlr_scene_surface *image_capture_scene_surface;
	struct wlr_scene_tree *overview_scene_surface;
	MangoJumpLabel *jump_label_node;
	MangoGroupBar *group_bar;
	struct wl_list link;
	struct wl_list flink;
	struct wl_list fadeout_link;
	union {
		struct wlr_xdg_surface *xdg;
		struct wlr_xwayland_surface *xwayland;
	} surface;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener maximize;
	struct wl_listener minimize;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener set_title;
	struct wl_listener fullscreen;
#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener configure;
	struct wl_listener set_hints;
	struct wl_listener set_geometry;
	struct wl_listener commmitx11;
	struct wlr_scene_buffer *xwl_root_buffer;
	float xwayland_scale;	 /* X11 coordinate scale relative to logical
								coordinates. */
	struct wlr_box xwl_clip; /* Most recent logical clip area of the XWayland
								root surface. */
	bool xwl_clip_active;	 /* Whether source_box clipping is active. */
							 /*
							  * X11 configure deduplication: until the client acks, surface->current is
							  * not updated,						  so repeated arrange calls resend
							  * identical configures and						  force clients to re-render/
							  * re-upload. Record the most recently requested						  physical size/position here
							  * and skip						  configure if it has not changed.
							  */
	int32_t xwl_req_x, xwl_req_y, xwl_req_w, xwl_req_h;
	bool xwl_req_valid;
#endif
	uint32_t bw;
	uint32_t tags, oldtags, mini_restore_tag;
	bool dirty;
	uint32_t configure_serial;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;
	int32_t isfloating, isurgent, isfullscreen, isfakefullscreen,
		need_float_size_reduce, isminimized, isoverlay, isnosizehint,
		ignore_maximize, ignore_minimize, idleinhibit_when_focus,
		vrr_only_fullscreen, force_render, activation_bypass;
	int32_t ismaximizescreen;
	int32_t overview_backup_bw;
	int32_t fullscreen_backup_x, fullscreen_backup_y, fullscreen_backup_w,
		fullscreen_backup_h;
	int32_t overview_isfullscreenbak, overview_ismaximizescreenbak,
		overview_isfloatingbak;

	struct wlr_scene_tree *ov_card_tree; /* Overview card tree (root surface
											plus all subsurface nodes). */
	struct wl_list ov_card_surfaces;	 /* struct ov_card_surface list */

	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener foreign_activate_request;
	struct wl_listener foreign_fullscreen_request;
	struct wl_listener foreign_close_request;
	struct wl_listener foreign_destroy;
	struct wl_listener foreign_minimize_request;
	struct wl_listener foreign_maximize_request;
	struct wl_listener set_decoration_mode;
	struct wl_listener destroy_decoration;

	const char *animation_type_open;
	const char *animation_type_close;
	int32_t is_in_scratchpad;
	int32_t iscustomsize;
	int32_t iscustompos;
	int32_t iscustom_scroller_proportion;
	int32_t iscustom_scroller_proportion_single;
	int32_t is_scratchpad_show;
	int32_t isglobal;
	int32_t isnoborder;
	int32_t isnoanimation;
	int32_t isopensilent;
	int32_t istagsilent;
	int32_t iskilling;
	int32_t isnamedscratchpad;
	int32_t shield_when_capture;
	bool is_pending_open_animation;
	bool is_restoring_from_ov;
	float scroller_proportion;
	float stack_proportion;
	float old_stack_proportion;
	bool need_output_flush;
	struct mango_animation animation;
	struct mango_opacity_animation opacity_animation;
	int32_t isterm, noswallow;
	int32_t allow_csd;
	int32_t force_fakemaximize;
	int32_t force_tiled_state;
	pid_t pid;
	Client *swallowdby, *swallowing;
	bool is_clip_to_hide;
	bool drag_to_tile;
	bool scratchpad_switching_mon;
	bool fake_no_border;
	int32_t nofocus;
	int32_t nofadein;
	int32_t nofadeout;
	int32_t no_force_center;
	int32_t isunglobal;
	float focused_opacity;
	float unfocused_opacity;
	char oldmonname[128];
	struct wlr_ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel;
	double master_mfact_per, master_inner_per, stack_inner_per;
	double old_master_mfact_per, old_master_inner_per, old_stack_inner_per;
	double old_scroller_pproportion;
	bool ismaster;
	bool old_ismaster;
	bool cursor_in_upper_half, cursor_in_left_half;
	bool isleftstack;
	int32_t tearing_hint;
	int32_t force_tearing;
	int32_t allow_shortcuts_inhibit;
	float scroller_proportion_single;
	bool isfocusing;
	char jump_char;
	bool enable_drop_area_draw;
	int32_t drop_direction;
	struct wlr_box drag_tile_float_backup_geom;
	float grid_col_per;
	float grid_row_per;
	float old_grid_col_per;
	float old_grid_row_per;
	int32_t grid_col_idx;
	int32_t grid_row_idx;
	uint32_t id;
	Client *group_prev;
	Client *group_next;
	bool isgroupfocusing;
	bool is_logic_hide;
};

typedef struct {
	struct wlr_keyboard_group *wlr_group;
	struct wlr_keyboard *keyboard; /* The keyboard actually in use (group or
									  standalone keyboard). */
	struct wlr_keyboard *virtual_keyboard;
	struct wlr_keyboard
		*prev_seat_keyboard; /* Keyboard in effect before taking over the seat;
								restored on destroy. */

	int32_t nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	uint32_t mods;				 /* invalid if nsyms == 0 */
	uint32_t keycode;
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;

	uint32_t layout_index;
	struct wl_list link; /* standalone_keyboards */
} KeyboardGroup;

typedef struct {
	struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor;
	struct wl_listener destroy;
	struct wl_list link;
} KeyboardShortcutsInhibitor;

typedef struct {
	/* Must keep these three elements in this order */
	uint32_t type; // must at first in struct
	struct wlr_box geom, current, pending, animainit_geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_rect *shield;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	struct wl_list fadeout_link;
	int32_t mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;

	struct mango_animation animation;
	bool dirty;
	int32_t noanim;
	char *animation_type_open;
	char *animation_type_close;
	bool shield_when_capture;
	bool need_output_flush;
	bool being_unmapped;
} LayerSurface;

typedef struct {
	uint32_t type; // must at first in struct
	struct wlr_xdg_popup *wlr_popup;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener reposition;
} Popup;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
	const char *name;
	uint32_t id;
} Layout;

struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_output_state pending;
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wl_event_source *skip_frame_timeout;
	struct wlr_box m;		  /* monitor area, layout-relative */
	struct wlr_box w;		  /* window area, layout-relative */
	struct wl_list layers[4]; /* LayerSurface::link */
	uint32_t seltags;
	uint32_t tagset[2];
	bool skiping_frame;
	uint32_t resizing_count_pending;
	uint32_t resizing_count_current;

	int32_t gappih; /* horizontal gap between windows */
	int32_t gappiv; /* vertical gap between windows */
	int32_t gappoh; /* horizontal outer gaps */
	int32_t gappov; /* vertical outer gaps */
	// special workspace gaps, per monitor
	int32_t special_gappih;
	int32_t special_gappiv;
	int32_t special_gappoh;
	int32_t special_gappov;
	Pertag *pertag;
	uint32_t ovbk_current_tagset;
	uint32_t ovbk_prev_tagset;
	Client *sel, *prevsel;
	int32_t isoverview;
	int32_t is_jump_mode;
	int32_t is_in_hotarea;
	int32_t ov_normal_mode; /* Uses the normal grid layout when entering via the
							   hot area. */
	int32_t ov_tab_layout;	/* Uses the centered tab layout when entering via
							   overcircle. */
	int32_t only_sleep;
	bool special_empty_view; // user intentionally opened the empty special view
	uint32_t visible_clients;
	uint32_t visible_tiling_clients;
	uint32_t visible_scroll_tiling_clients;
	uint32_t visible_fake_tiling_clients;
	uint32_t hide_clients;
	struct wlr_scene_rect *special_dim_rect;
	char last_open_surface[256];
	struct wlr_ext_workspace_group_handle_v1 *ext_group;
	bool iscleanuping;
	int8_t carousel_anim_dir;
	bool vrr_global_enable;
	bool is_vrr_enabling;
	bool hdr_enable;
	bool prefer_disable;
	bool is_hdr_enabling;
	// Mastering display metadata, in cd/m². 0 = unset, see output_enable_hdr().
	float hdr_min_lum;
	float hdr_max_lum;
	float hdr_max_avg_lum;
	// Bypass the EDID-derived capability checks (DisplayID-only panels).
	bool hdr_force;
	struct wlr_color_transform
		*icc_transform; /* ICC transform loaded from the ICC file. */
	char icc_path[PATH_MAX];
};

typedef struct {
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
} PointerConstraint;

typedef struct {
	struct wlr_scene_tree *scene;

	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
} SessionLock;

struct capture_session_tracker {
	struct wl_listener session_destroy;
	struct wlr_ext_image_copy_capture_session_v1 *session;
};

typedef struct DwindleNode DwindleNode;
struct DwindleNode {
	bool is_split;
	bool split_h;
	bool split_locked;
	bool custom_leaf_split_h;
	float ratio;
	float drag_init_ratio;
	int32_t container_x;
	int32_t container_y;
	int32_t container_w;
	int32_t container_h;
	DwindleNode *parent;
	DwindleNode *first;
	DwindleNode *second;
	Client *client;
};

struct ScrollerStackNode {
	Client *client;
	float scroller_proportion;
	float stack_proportion;
	float scroller_proportion_single;

	struct ScrollerStackNode *next_in_stack;
	struct ScrollerStackNode *prev_in_stack;
	struct ScrollerStackNode *all_next;
};

struct TagScrollerState {
	struct ScrollerStackNode
		*all_first; /* Singly linked list head for all nodes. */
	int count;
};

typedef struct {
	uint32_t type; // must at first in struct
	int32_t orig_width;
	int32_t orig_height;
	bool is_subsurface;
	struct wl_listener destroy;
} SnapshotMetadata;

// Skipping functions
// TODO Rearrenging correctly

struct LastCursor {
	enum wp_cursor_shape_device_v1_shape shape;
	struct wlr_surface *surface;
	int32_t hotspot_x;
	int32_t hotspot_y;
};

#include "mango/config/preset.h"

#ifndef PERTAG_SLOTS
#define PERTAG_SLOTS (tag_num_MAX + 2)
#define PERTAG_ALL_TAGS_IDX (tag_num_MAX + 1)
#endif

struct Pertag {
	uint32_t curtag, prevtag;
	int32_t nmasters[PERTAG_SLOTS];
	float mfacts[PERTAG_SLOTS];
	int32_t no_hide[PERTAG_SLOTS];
	int32_t no_render_border[PERTAG_SLOTS];
	int32_t open_as_floating[PERTAG_SLOTS];
	float scroller_default_proportion[PERTAG_SLOTS];
	float scroller_default_proportion_single[PERTAG_SLOTS];
	int32_t scroller_ignore_proportion_single[PERTAG_SLOTS];
	struct DwindleNode *dwindle_root[PERTAG_SLOTS];
	const Layout *ltidxs[PERTAG_SLOTS];
	struct TagScrollerState *scroller_state[PERTAG_SLOTS];
};

// Functions declaration of mango.c

void handle_new_xdg_toplevel(struct wl_listener *listener, void *data);

void request_fresh_all_monitors(void);

void handle_signal(int32_t signo);

void cleanup_listeners();

void cleanup();

void handle_quit_signal(int32_t signo);

void setup();

void update_scroller_state(Monitor *m);

void set_activation_env();

void run(char *startup_cmd, int readiness_fd);

int32_t main(int32_t argc, char *argv[]);

#endif
