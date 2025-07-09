/*
 * See LICENSE file for copyright and license details.
 */
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "wlr/util/box.h"
#include <getopt.h>
#include <libinput.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
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
#include <wordexp.h>
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <X11/Xlib.h>
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>
#endif
#include "common/util.h"

/* macros */
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define GEZERO(A) ((A) >= 0 ? (A) : 0)
#define CLEANMASK(mask) (mask & ~WLR_MODIFIER_CAPS)
#define ISTILED(A)                                                             \
	(!(A)->isfloating && !(A)->isminied && !(A)->iskilling &&                  \
	 !client_should_ignore_focus(A) && !(A)->isunglobal &&                     \
	 !(A)->animation.tagouting && !(A)->ismaxmizescreen && !(A)->isfullscreen)
#define VISIBLEON(C, M)                                                        \
	((M) && (C)->mon == (M) && ((C)->tags & (M)->tagset[(M)->seltags]))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define END(A) ((A) + LENGTH(A))
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define LISTEN(E, L, H) wl_signal_add((E), ((L)->notify = (H), (L)))
#define ISFULLSCREEN(A)                                                        \
	((A)->isfullscreen || (A)->ismaxmizescreen ||                              \
	 (A)->overview_ismaxmizescreenbak || (A)->overview_isfullscreenbak)
#define LISTEN_STATIC(E, H)                                                    \
	do {                                                                       \
		struct wl_listener *_l = ecalloc(1, sizeof(*_l));                      \
		_l->notify = (H);                                                      \
		wl_signal_add((E), _l);                                                \
	} while (0)

#define APPLY_INT_PROP(prop, cond)                                             \
	if (r->prop cond)                                                          \
	c->prop = r->prop

#define APPLY_FLOAT_PROP(prop, cond)                                           \
	if (r->prop cond)                                                          \
	c->prop = r->prop

#define BAKED_POINTS_COUNT 256

/* enums */
enum { VERTICAL, HORIZONTAL };
enum { SWIPE_UP, SWIPE_DOWN, SWIPE_LEFT, SWIPE_RIGHT };
enum { CurNormal, CurPressed, CurMove, CurResize }; /* cursor */
enum { XDGShell, LayerShell, X11 };					/* client types */
enum { AxisUp, AxisDown, AxisLeft, AxisRight };		// 滚轮滚动的方向
enum {
	LyrBg,
	LyrBottom,
	LyrTile,
	LyrFloat,
	LyrTop,
	LyrFadeOut,
	LyrFSorOverTop,
	LyrOverlay,
	LyrIMPopup, // text-input layer
	LyrBlock,
	NUM_LAYERS
}; /* scene layers */
#ifdef XWAYLAND
enum {
	NetWMWindowTypeDialog,
	NetWMWindowTypeSplash,
	NetWMWindowTypeToolbar,
	NetWMWindowTypeUtility,
	NetLast
}; /* EWMH atoms */
#endif
enum { UP, DOWN, LEFT, RIGHT, UNDIR }; /* smartmovewin */
enum { NONE, OPEN, MOVE, CLOSE, TAG };

struct dvec2 {
	double x, y;
};

struct ivec2 {
	int x, y, width, height;
};

typedef struct {
	int i;
	int i2;
	float f;
	float f2;
	char *v;
	char *v2;
	char *v3;
	unsigned int ui;
	unsigned int ui2;
} Arg;

typedef struct {
	unsigned int mod;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg arg;
} Button; // 鼠标按键

typedef struct {
	unsigned int mod;
	unsigned int dir;
	void (*func)(const Arg *);
	const Arg arg;
} Axis;

struct dwl_animation {
	bool should_animate;
	bool running;
	bool tagining;
	bool tagouted;
	bool tagouting;
	bool begin_fade_in;
	bool from_rule;
	unsigned int total_frames;
	unsigned int passed_frames;
	unsigned int duration;
	struct wlr_box initial;
	struct wlr_box current;
	int action;
};

typedef struct Pertag Pertag;
typedef struct Monitor Monitor;
struct wlr_foreign_toplevel_handle_v1;

typedef struct {
	float width_scale;
	float height_scale;
	int width;
	int height;
	double percent;
	float opacity;
	bool should_scale;
} animationScale;

typedef struct Client Client;
struct Client {
	/* Must keep these three elements in this order */
	unsigned int type; /* XDGShell or X11* */
	struct wlr_box geom, pending, oldgeom, animainit_geom, overview_backup_geom,
		current; /* layout-relative, includes border */
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border[4]; /* top, bottom, left, right */
	struct wlr_scene_tree *scene_surface;
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
#endif
	unsigned int bw;
	unsigned int tags, oldtags, mini_restore_tag;
	bool dirty;
	unsigned int configure_serial;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;
	int isfloating, isurgent, isfullscreen, isfakefullscreen,
		need_float_size_reduce, isminied, isoverlay;
	int ismaxmizescreen;
	int overview_backup_bw;
	int fullscreen_backup_x, fullscreen_backup_y, fullscreen_backup_w,
		fullscreen_backup_h;
	int overview_isfullscreenbak, overview_ismaxmizescreenbak,
		overview_isfloatingbak;

	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener foreign_activate_request;
	struct wl_listener foreign_fullscreen_request;
	struct wl_listener foreign_close_request;
	struct wl_listener foreign_destroy;
	struct wl_listener set_decoration_mode;
	struct wl_listener destroy_decoration;

	const char *animation_type_open;
	const char *animation_type_close;
	int is_in_scratchpad;
	int is_scratchpad_show;
	int isglobal;
	int isnoborder;
	int isopensilent;
	int iskilling;
	int isnamedscratchpad;
	struct wlr_box bounds;
	bool is_open_animation;
	bool is_restoring_from_ov;
	float scroller_proportion;
	bool need_output_flush;
	struct dwl_animation animation;
	int isterm, noswallow;
	pid_t pid;
	Client *swallowing, *swallowedby;
	bool is_clip_to_hide;
	bool drag_to_tile;
	bool fake_no_border;
	int nofadein;
	int nofadeout;
	int no_force_center;
	int isunglobal;
	float focused_opacity;
	float unfocused_opacity;
	char oldmonname[128];
	int scratchpad_width, scratchpad_height;
};

typedef struct {
	struct wl_list link;
	struct wl_resource *resource;
	Monitor *mon;
} DwlIpcOutput;

typedef struct {
	unsigned int mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	struct wlr_keyboard_group *wlr_group;

	int nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	unsigned int mods;			 /* invalid if nsyms == 0 */
	unsigned int keycode;
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
} KeyboardGroup;

typedef struct {
	struct wl_list link;
	struct wlr_keyboard *wlr_keyboard;

	int nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	unsigned int mods;			 /* invalid if nsyms == 0 */
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
} Keyboard;

typedef struct {
	/* Must keep these three elements in this order */
	unsigned int type; /* LayerShell */
	struct wlr_box geom, current, pending, animainit_geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	struct wl_list fadeout_link;
	int mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;

	struct dwl_animation animation;
	bool dirty;
	int noanim;
	bool need_output_flush;
} LayerSurface;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
	const char *name;
} Layout;

struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_box m;		  /* monitor area, layout-relative */
	struct wlr_box w;		  /* window area, layout-relative */
	struct wl_list layers[4]; /* LayerSurface::link */
	const Layout *lt;
	unsigned int seltags;
	unsigned int tagset[2];
	double mfact;
	int nmaster;

	struct wl_list dwl_ipc_outputs;
	int gappih; /* horizontal gap between windows */
	int gappiv; /* vertical gap between windows */
	int gappoh; /* horizontal outer gaps */
	int gappov; /* vertical outer gaps */
	Pertag *pertag;
	Client *sel, *prevsel;
	int isoverview;
	int is_in_hotarea;
	int gamma_lut_changed;
	int asleep;
	unsigned int visible_clients;
	unsigned int visible_tiling_clients;
	char last_surface_ws_name[256];
};

typedef struct {
	const char *name;
	float mfact;
	int nmaster;
	float scale;
	const Layout *lt;
	enum wl_output_transform rr;
	int x, y;
} MonitorRule;

typedef struct {
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
} PointerConstraint;

typedef struct {
	const char *id;
	const char *title;
	unsigned int tags;
	int isfloating;
	int isfullscreen;
	float scroller_proportion;
	const char *animation_type_open;
	const char *animation_type_close;
	int isnoborder;
	int monitor;
	unsigned int width;
	unsigned int height;
} Rule;

typedef struct {
	struct wlr_scene_tree *scene;

	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
} SessionLock;

/* function declarations */
static void applybounds(
	Client *c,
	struct wlr_box *bbox); // 设置边界规则,能让一些窗口拥有比较适合的大小
static void applyrules(Client *c); // 窗口规则应用,应用config.h中定义的窗口规则
static void
arrange(Monitor *m,
		bool want_animation); // 布局函数,让窗口俺平铺规则移动和重置大小
static void arrangelayer(Monitor *m, struct wl_list *list,
						 struct wlr_box *usable_area, int exclusive);
static void arrangelayers(Monitor *m);
static char *get_autostart_path(char *, unsigned int); // 自启动命令执行
static void axisnotify(struct wl_listener *listener,
					   void *data); // 滚轮事件处理
static void buttonpress(struct wl_listener *listener,
						void *data); // 鼠标按键事件处理
static int ongesture(struct wlr_pointer_swipe_end_event *event);
static void swipe_begin(struct wl_listener *listener, void *data);
static void swipe_update(struct wl_listener *listener, void *data);
static void swipe_end(struct wl_listener *listener, void *data);
static void pinch_begin(struct wl_listener *listener, void *data);
static void pinch_update(struct wl_listener *listener, void *data);
static void pinch_end(struct wl_listener *listener, void *data);
static void hold_begin(struct wl_listener *listener, void *data);
static void hold_end(struct wl_listener *listener, void *data);
static void checkidleinhibitor(struct wlr_surface *exclude);
static void cleanup(void); // 退出清理
static void cleanupkeyboard(struct wl_listener *listener,
							void *data);						  // 退出清理
static void cleanupmon(struct wl_listener *listener, void *data); // 退出清理
static void closemon(Monitor *m);
static void cleanuplisteners(void);
static void toggle_hotarea(int x_root, int y_root); // 触发热区
static void maplayersurfacenotify(struct wl_listener *listener, void *data);
static void commitlayersurfacenotify(struct wl_listener *listener, void *data);
static void commitnotify(struct wl_listener *listener, void *data);
static void createdecoration(struct wl_listener *listener, void *data);
static void createidleinhibitor(struct wl_listener *listener, void *data);
static void createkeyboard(struct wlr_keyboard *keyboard);
static void requestmonstate(struct wl_listener *listener, void *data);
static void createlayersurface(struct wl_listener *listener, void *data);
static void createlocksurface(struct wl_listener *listener, void *data);
static void createmon(struct wl_listener *listener, void *data);
static void createnotify(struct wl_listener *listener, void *data);
static void createpointer(struct wlr_pointer *pointer);
static void createpointerconstraint(struct wl_listener *listener, void *data);
static void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
static void commitpopup(struct wl_listener *listener, void *data);
static void createpopup(struct wl_listener *listener, void *data);
static void cursorframe(struct wl_listener *listener, void *data);
static void cursorwarptohint(void);
static void destroydecoration(struct wl_listener *listener, void *data);
static void destroydragicon(struct wl_listener *listener, void *data);
static void destroyidleinhibitor(struct wl_listener *listener, void *data);
static void destroylayersurfacenotify(struct wl_listener *listener, void *data);
static void destroylock(SessionLock *lock, int unlocked);
static void destroylocksurface(struct wl_listener *listener, void *data);
static void destroynotify(struct wl_listener *listener, void *data);
static void destroypointerconstraint(struct wl_listener *listener, void *data);
static void destroysessionlock(struct wl_listener *listener, void *data);
static void destroykeyboardgroup(struct wl_listener *listener, void *data);
static Monitor *dirtomon(enum wlr_direction dir);
static void setcursorshape(struct wl_listener *listener, void *data);

static void focusclient(Client *c, int lift);

static void setborder_color(Client *c);
static Client *focustop(Monitor *m);
static void fullscreennotify(struct wl_listener *listener, void *data);
static void gpureset(struct wl_listener *listener, void *data);

static int keyrepeat(void *data);

static void inputdevice(struct wl_listener *listener, void *data);
static int keybinding(unsigned int mods, xkb_keysym_t sym,
					  unsigned int keycode);
static void keypress(struct wl_listener *listener, void *data);
static void keypressmod(struct wl_listener *listener, void *data);
static bool keypressglobal(struct wlr_surface *last_surface,
						   struct wlr_keyboard *keyboard,
						   struct wlr_keyboard_key_event *event,
						   unsigned int mods, xkb_keysym_t keysym,
						   unsigned int keycode);
static void locksession(struct wl_listener *listener, void *data);
static void mapnotify(struct wl_listener *listener, void *data);
static void maximizenotify(struct wl_listener *listener, void *data);
static void minimizenotify(struct wl_listener *listener, void *data);
static void motionabsolute(struct wl_listener *listener, void *data);
static void motionnotify(unsigned int time, struct wlr_input_device *device,
						 double sx, double sy, double sx_unaccel,
						 double sy_unaccel);
static void motionrelative(struct wl_listener *listener, void *data);

static void reset_foreign_tolevel(Client *c);
static void remove_foreign_topleve(Client *c);
static void add_foreign_topleve(Client *c);
static void exchange_two_client(Client *c1, Client *c2);
static void outputmgrapply(struct wl_listener *listener, void *data);
static void outputmgrapplyortest(struct wlr_output_configuration_v1 *config,
								 int test);
static void outputmgrtest(struct wl_listener *listener, void *data);
static void pointerfocus(Client *c, struct wlr_surface *surface, double sx,
						 double sy, unsigned int time);
static void printstatus(void);
static void quitsignal(int signo);
static void powermgrsetmode(struct wl_listener *listener, void *data);
static void rendermon(struct wl_listener *listener, void *data);
static void requestdecorationmode(struct wl_listener *listener, void *data);
static void requeststartdrag(struct wl_listener *listener, void *data);
static void resize(Client *c, struct wlr_box geo, int interact);
static void run(char *startup_cmd);
static void setcursor(struct wl_listener *listener, void *data);
static void setfloating(Client *c, int floating);
static void setfakefullscreen(Client *c, int fakefullscreen);
static void setfullscreen(Client *c, int fullscreen);
static void setmaxmizescreen(Client *c, int maxmizescreen);
static void reset_maxmizescreen_size(Client *c);
static void setgaps(int oh, int ov, int ih, int iv);

static void setmon(Client *c, Monitor *m, unsigned int newtags, bool focus);
static void setpsel(struct wl_listener *listener, void *data);
static void setsel(struct wl_listener *listener, void *data);
static void setup(void);
static void startdrag(struct wl_listener *listener, void *data);

static void unlocksession(struct wl_listener *listener, void *data);
static void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
static void unmapnotify(struct wl_listener *listener, void *data);
static void updatemons(struct wl_listener *listener, void *data);
static void updatetitle(struct wl_listener *listener, void *data);
static void urgent(struct wl_listener *listener, void *data);
static void view(const Arg *arg, bool want_animation);

static void handlesig(int signo);

static void virtualkeyboard(struct wl_listener *listener, void *data);
static void virtualpointer(struct wl_listener *listener, void *data);
static void warp_cursor(const Client *c);
static Monitor *xytomon(double x, double y);
static void xytonode(double x, double y, struct wlr_surface **psurface,
					 Client **pc, LayerSurface **pl, double *nx, double *ny);
static void clear_fullscreen_flag(Client *c);
static pid_t getparentprocess(pid_t p);
static int isdescprocess(pid_t p, pid_t c);
static Client *termforwin(Client *w);
static void swallow(Client *c, Client *w);

static void warp_cursor_to_selmon(Monitor *m);
unsigned int want_restore_fullscreen(Client *target_client);
static void overview_restore(Client *c, const Arg *arg);
static void overview_backup(Client *c);
static int applyrulesgeom(Client *c);
static void set_minized(Client *c);

static void show_scratchpad(Client *c);
static void show_hide_client(Client *c);
static void tag_client(const Arg *arg, Client *target_client);

static struct wlr_box setclient_coordinate_center(Client *c,
												  struct wlr_box geom,
												  int offsetx, int offsety);
static unsigned int get_tags_first_tag(unsigned int tags);

static void client_commit(Client *c);
static void layer_commit(LayerSurface *l);
static void apply_border(Client *c);
static void client_set_opacity(Client *c, double opacity);
static void init_baked_points(void);
static void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int sx,
									   int sy, void *data);

static Client *direction_select(const Arg *arg);
static void view_in_mon(const Arg *arg, bool want_animation, Monitor *m);

static void buffer_set_effect(Client *c, animationScale scale_data);
static void snap_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer,
										   int sx, int sy, void *data);
static void client_set_pending_state(Client *c);
static void layer_set_pending_state(LayerSurface *l);
static void set_rect_size(struct wlr_scene_rect *rect, int width, int height);
static Client *center_select(Monitor *m);
static void handlecursoractivity(void);
static int hidecursor(void *data);
static bool check_hit_no_border(Client *c);
static void reset_keyboard_layout(void);
static void client_update_oldmonname_record(Client *c, Monitor *m);
static void pending_kill_client(Client *c);
static void set_layer_open_animaiton(LayerSurface *l, struct wlr_box geo);
static void init_fadeout_layers(LayerSurface *l);
static void layer_actual_size(LayerSurface *l, unsigned int *width,
							  unsigned int *height);
static void get_layer_target_geometry(LayerSurface *l,
									  struct wlr_box *target_box);
static void scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int sx,
									  int sy, void *data);
static double find_animation_curve_at(double t, int type);

static void apply_opacity_to_rect_nodes(Client *c, struct wlr_scene_node *node,
										double animation_passed);
static double output_frame_duration_ms();
static struct wlr_scene_tree *
wlr_scene_tree_snapshot(struct wlr_scene_node *node,
						struct wlr_scene_tree *parent);
static bool is_scroller_layout(Monitor *m);

#include "data/static_keymap.h"
#include "dispatch/dispatch.h"
#include "layout/layout.h"

/* variables */
static const char broken[] = "broken";
static pid_t child_pid = -1;
static int locked;
static unsigned int locked_mods = 0;
static void *exclusive_focus;
static struct wl_display *dpy;
static struct wl_event_loop *event_loop;
static struct wlr_relative_pointer_manager_v1 *pointer_manager;
static struct wlr_backend *backend;
static struct wlr_backend *headless_backend;
static struct wlr_scene *scene;
static struct wlr_scene_tree *layers[NUM_LAYERS];
static struct wlr_renderer *drw;
static struct wlr_allocator *alloc;
static struct wlr_compositor *compositor;

static struct wlr_xdg_shell *xdg_shell;
static struct wlr_xdg_activation_v1 *activation;
static struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
static struct wl_list clients; /* tiling order */
static struct wl_list fstack;  /* focus order */
static struct wl_list fadeout_clients;
static struct wl_list fadeout_layers;
static struct wlr_idle_notifier_v1 *idle_notifier;
static struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
static struct wlr_layer_shell_v1 *layer_shell;
static struct wlr_output_manager_v1 *output_mgr;
static struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
static struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
static struct wlr_output_power_manager_v1 *power_mgr;
static struct wlr_pointer_gestures_v1 *pointer_gestures;

static struct wlr_cursor *cursor;
static struct wlr_xcursor_manager *cursor_mgr;
static struct wlr_session *session;

static struct wlr_scene_rect *root_bg;
static struct wlr_session_lock_manager_v1 *session_lock_mgr;
static struct wlr_scene_rect *locked_bg;
static struct wlr_session_lock_v1 *cur_lock;
static const int layermap[] = {LyrBg, LyrBottom, LyrTop, LyrOverlay};
static struct wlr_scene_tree *drag_icon;
static struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
static struct wlr_pointer_constraints_v1 *pointer_constraints;
static struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
static struct wlr_pointer_constraint_v1 *active_constraint;

static struct wlr_seat *seat;
static KeyboardGroup *kb_group;
static struct wl_list keyboards;
static unsigned int cursor_mode;
static Client *grabc;
static int grabcx, grabcy; /* client-relative */

static struct wlr_output_layout *output_layout;
static struct wlr_box sgeom;
static struct wl_list mons;
static Monitor *selmon;

static int enablegaps = 1; /* enables gaps, used by togglegaps */
static int axis_apply_time = 0;
static int axis_apply_dir = 0;
static int scroller_focus_lock = 0;

static unsigned int swipe_fingers = 0;
static double swipe_dx = 0;
static double swipe_dy = 0;

bool render_border = true;

struct dvec2 *baked_points_move;
struct dvec2 *baked_points_open;
struct dvec2 *baked_points_tag;
struct dvec2 *baked_points_close;

static struct wl_event_source *hide_source;
static bool cursor_hidden = false;
static struct {
	enum wp_cursor_shape_device_v1_shape shape;
	struct wlr_surface *surface;
	int hotspot_x;
	int hotspot_y;
} last_cursor;

#include "client/client.h"
#include "config/preset.h"

struct Pertag {
	unsigned int curtag, prevtag;	 /* current and previous tag */
	int nmasters[LENGTH(tags) + 1];	 /* number of windows in master area */
	float mfacts[LENGTH(tags) + 1];	 /* mfacts per tag */
	float smfacts[LENGTH(tags) + 1]; /* smfacts per tag */
	const Layout
		*ltidxs[LENGTH(tags) + 1]; /* matrix of tags and layouts indexes  */
};

static struct wl_listener cursor_axis = {.notify = axisnotify};
static struct wl_listener cursor_button = {.notify = buttonpress};
static struct wl_listener cursor_frame = {.notify = cursorframe};
static struct wl_listener cursor_motion = {.notify = motionrelative};
static struct wl_listener cursor_motion_absolute = {.notify = motionabsolute};
static struct wl_listener gpu_reset = {.notify = gpureset};
static struct wl_listener layout_change = {.notify = updatemons};
static struct wl_listener new_idle_inhibitor = {.notify = createidleinhibitor};
static struct wl_listener new_input_device = {.notify = inputdevice};
static struct wl_listener new_virtual_keyboard = {.notify = virtualkeyboard};
static struct wl_listener new_virtual_pointer = {.notify = virtualpointer};
static struct wl_listener new_pointer_constraint = {
	.notify = createpointerconstraint};
static struct wl_listener new_output = {.notify = createmon};
static struct wl_listener new_xdg_toplevel = {.notify = createnotify};
static struct wl_listener new_xdg_popup = {.notify = createpopup};
static struct wl_listener new_xdg_decoration = {.notify = createdecoration};
static struct wl_listener new_layer_surface = {.notify = createlayersurface};
static struct wl_listener output_mgr_apply = {.notify = outputmgrapply};
static struct wl_listener output_mgr_test = {.notify = outputmgrtest};
static struct wl_listener output_power_mgr_set_mode = {.notify =
														   powermgrsetmode};
static struct wl_listener request_activate = {.notify = urgent};
static struct wl_listener request_cursor = {.notify = setcursor};
static struct wl_listener request_set_psel = {.notify = setpsel};
static struct wl_listener request_set_sel = {.notify = setsel};
static struct wl_listener request_set_cursor_shape = {.notify = setcursorshape};
static struct wl_listener request_start_drag = {.notify = requeststartdrag};
static struct wl_listener start_drag = {.notify = startdrag};
static struct wl_listener new_session_lock = {.notify = locksession};

#ifdef XWAYLAND
static void activatex11(struct wl_listener *listener, void *data);
static void configurex11(struct wl_listener *listener, void *data);
static void createnotifyx11(struct wl_listener *listener, void *data);
static void dissociatex11(struct wl_listener *listener, void *data);
static void associatex11(struct wl_listener *listener, void *data);
static void sethints(struct wl_listener *listener, void *data);
static void xwaylandready(struct wl_listener *listener, void *data);
static void setgeometrynotify(struct wl_listener *listener, void *data);
static struct wl_listener new_xwayland_surface = {.notify = createnotifyx11};
static struct wl_listener xwayland_ready = {.notify = xwaylandready};
static struct wlr_xwayland *xwayland;
#endif

#include "animation/client.h"
#include "animation/common.h"
#include "animation/layer.h"
#include "config/parse_config.h"
#include "ext-protocol/all.h"
#include "layout/horizontal.h"
#include "layout/vertical.h"

void client_change_mon(Client *c, Monitor *m) {
	setmon(c, m, c->tags, true);
	reset_foreign_tolevel(c);
	if (c->isfloating) {
		c->oldgeom = c->geom = setclient_coordinate_center(c, c->geom, 0, 0);
	}
}

bool check_hit_no_border(Client *c) {
	int i;
	bool hit_no_border = false;
	if (!render_border) {
		hit_no_border = true;
	}

	for (i = 0; i < config.tag_rules_count; i++) {
		if (c->tags & (1 << (config.tag_rules[i].id - 1)) &&
			config.tag_rules[i].no_render_border) {
			hit_no_border = true;
		}
	}

	if (no_border_when_single && c && c->mon && c->mon->visible_clients == 1) {
		hit_no_border = true;
	}
	return hit_no_border;
}

void applybounds(Client *c, struct wlr_box *bbox) {
	/* set minimum possible */
	c->geom.width = MAX(1 + 2 * (int)c->bw, c->geom.width);
	c->geom.height = MAX(1 + 2 * (int)c->bw, c->geom.height);

	if (c->geom.x >= bbox->x + bbox->width)
		c->geom.x = bbox->x + bbox->width - c->geom.width;
	if (c->geom.y >= bbox->y + bbox->height)
		c->geom.y = bbox->y + bbox->height - c->geom.height;
	if (c->geom.x + c->geom.width <= bbox->x)
		c->geom.x = bbox->x;
	if (c->geom.y + c->geom.height <= bbox->y)
		c->geom.y = bbox->y;
}

/*清除全屏标志,还原全屏时清0的border*/
void clear_fullscreen_flag(Client *c) {
	if (c->isfullscreen || c->ismaxmizescreen) {
		c->isfullscreen = 0;
		c->isfloating = 0;
		c->ismaxmizescreen = 0;
		c->bw = c->isnoborder ? 0 : borderpx;
		setfullscreen(c, false);
	}
}

void toggleoverlay(const Arg *arg) {
	if (!selmon->sel || !selmon->sel->mon || selmon->sel->isfullscreen) {
		return;
	}

	selmon->sel->isoverlay ^= 1;

	if (selmon->sel->isoverlay) {
		wlr_scene_node_reparent(&selmon->sel->scene->node, layers[LyrOverlay]);
		wlr_scene_node_raise_to_top(&selmon->sel->scene->node);
	} else if (client_should_overtop(selmon->sel) && selmon->sel->isfloating) {
		wlr_scene_node_reparent(&selmon->sel->scene->node,
								layers[LyrFSorOverTop]);
	} else {
		wlr_scene_node_reparent(
			&selmon->sel->scene->node,
			layers[selmon->sel->isfloating ? LyrFloat : LyrTile]);
	}
	setborder_color(selmon->sel);
}

void minized(const Arg *arg) {
	if (selmon->sel && !selmon->sel->isminied) {
		set_minized(selmon->sel);
	}
}

void restore_minized(const Arg *arg) {
	Client *c;
	if (selmon && selmon->sel && selmon->sel->is_in_scratchpad &&
		selmon->sel->is_scratchpad_show) {
		selmon->sel->isminied = 0;
		selmon->sel->is_scratchpad_show = 0;
		selmon->sel->is_in_scratchpad = 0;
		selmon->sel->isnamedscratchpad = 0;
		setborder_color(selmon->sel);
		return;
	}

	wl_list_for_each(c, &clients, link) {
		if (c->isminied) {
			show_hide_client(c);
			c->is_scratchpad_show = 0;
			c->is_in_scratchpad = 0;
			c->isnamedscratchpad = 0;
			setborder_color(c);
			break;
		}
	}
}

void show_scratchpad(Client *c) {
	c->is_scratchpad_show = 1;
	if (c->isfullscreen || c->ismaxmizescreen) {
		c->isfullscreen = 0; // 清除窗口全屏标志
		c->ismaxmizescreen = 0;
		c->bw = c->isnoborder ? 0 : borderpx;
	}

	/* return if fullscreen */
	if (!c->isfloating) {
		setfloating(c, 1);
		c->geom.width = c->scratchpad_width
							? c->scratchpad_width
							: c->mon->w.width * scratchpad_width_ratio;
		c->geom.height = c->scratchpad_height
							 ? c->scratchpad_height
							 : c->mon->w.height * scratchpad_height_ratio;
		// 重新计算居中的坐标
		c->oldgeom = c->geom = c->animainit_geom = c->animation.current =
			setclient_coordinate_center(c, c->geom, 0, 0);
		resize(c, c->geom, 0);
	}

	c->oldtags = selmon->tagset[selmon->seltags];
	wl_list_remove(&c->link);					  // 从原来位置移除
	wl_list_insert(clients.prev->next, &c->link); // 插入开头
	show_hide_client(c);
	setborder_color(c);
}

pid_t getparentprocess(pid_t p) {
	unsigned int v = 0;

	FILE *f;
	char buf[256];
	snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

	if (!(f = fopen(buf, "r")))
		return 0;

	// 检查fscanf返回值，确保成功读取了1个参数
	if (fscanf(f, "%*u %*s %*c %u", &v) != 1) {
		fclose(f);
		return 0;
	}

	fclose(f);

	return (pid_t)v;
}

int isdescprocess(pid_t p, pid_t c) {
	while (p != c && c != 0)
		c = getparentprocess(c);

	return (int)c;
}

Client *termforwin(Client *w) {
	Client *c;

	if (!w->pid || w->isterm || w->noswallow)
		return NULL;

	wl_list_for_each(c, &fstack, flink) {
		if (c->isterm && !c->swallowing && c->pid &&
			isdescprocess(c->pid, w->pid)) {
			return c;
		}
	}

	return NULL;
}

void swallow(Client *c, Client *w) {
	c->bw = w->bw;
	c->isfloating = w->isfloating;
	c->isurgent = w->isurgent;
	c->isfullscreen = w->isfullscreen;
	c->ismaxmizescreen = w->ismaxmizescreen;
	c->isminied = w->isminied;
	c->is_in_scratchpad = w->is_in_scratchpad;
	c->is_scratchpad_show = w->is_scratchpad_show;
	c->tags = w->tags;
	c->geom = w->geom;
	c->scroller_proportion = w->scroller_proportion;
	wl_list_insert(&w->link, &c->link);
	wl_list_insert(&w->flink, &c->flink);

	if (w->foreign_toplevel)
		remove_foreign_topleve(w);

	wlr_scene_node_set_enabled(&w->scene->node, false);
	wlr_scene_node_set_enabled(&c->scene->node, true);

	if (!c->foreign_toplevel && c->mon)
		add_foreign_toplevel(c);

	if (c->isminied && c->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,
													 false);
		wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel, true);
	}
}

bool switch_scratchpad_client_state(Client *c) {
	if (c->is_in_scratchpad && c->is_scratchpad_show &&
		(selmon->tagset[selmon->seltags] & c->tags) == 0) {
		unsigned int target =
			get_tags_first_tag(selmon->tagset[selmon->seltags]);
		tag_client(&(Arg){.ui = target}, c);
		return true;
	} else if (c->is_in_scratchpad && c->is_scratchpad_show &&
			   (selmon->tagset[selmon->seltags] & c->tags) != 0) {
		set_minized(c);
		return true;
	} else if (c && c->is_in_scratchpad && !c->is_scratchpad_show) {
		show_scratchpad(c);
		return true;
	}

	return false;
}

void toggle_render_border(const Arg *arg) {
	render_border = !render_border;
	arrange(selmon, false);
}

Client *get_client_by_id_or_title(const char *arg_id, const char *arg_title) {
	Client *target_client = NULL;
	const char *appid, *title;
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != selmon) {
			continue;
		}

		if (!(appid = client_get_appid(c)))
			appid = broken;
		if (!(title = client_get_title(c)))
			title = broken;

		if (arg_id && strncmp(arg_id, "none", 4) == 0)
			arg_id = NULL;

		if (arg_title && strncmp(arg_title, "none", 4) == 0)
			arg_title = NULL;

		if ((arg_title && regex_match(arg_title, title) && !arg_id) ||
			(arg_id && regex_match(arg_id, appid) && !arg_title) ||
			(arg_id && regex_match(arg_id, appid) && arg_title &&
			 regex_match(arg_title, title))) {
			target_client = c;
			break;
		}
	}
	return target_client;
}

void apply_named_scratchpad(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != selmon) {
			continue;
		}
		if (single_scratchpad && c->is_in_scratchpad && c->is_scratchpad_show &&
			c != target_client) {
			set_minized(c);
		}
	}

	if (!target_client->is_in_scratchpad) {
		set_minized(target_client);
		switch_scratchpad_client_state(target_client);
	} else
		switch_scratchpad_client_state(target_client);
}

void toggle_named_scratchpad(const Arg *arg) {
	Client *target_client = NULL;
	char *arg_id = arg->v;
	char *arg_title = arg->v2;

	target_client = get_client_by_id_or_title(arg_id, arg_title);

	if (!target_client && arg->v3) {
		Arg arg_spawn = {.v = arg->v3};
		spawn(&arg_spawn);
		return;
	}

	target_client->isnamedscratchpad = 1;
	target_client->scratchpad_width = arg->ui;
	target_client->scratchpad_height = arg->ui2;

	apply_named_scratchpad(target_client);
}

void toggle_scratchpad(const Arg *arg) {
	Client *c;
	bool hit = false;
	Client *tmp = NULL;
	wl_list_for_each_safe(c, tmp, &clients, link) {
		if (c->mon != selmon) {
			continue;
		}

		if (single_scratchpad && c->isnamedscratchpad && !c->isminied) {
			set_minized(c);
			continue;
		}

		if (c->isnamedscratchpad)
			continue;

		if (hit)
			continue;

		hit = switch_scratchpad_client_state(c);
	}
}

void gpureset(struct wl_listener *listener, void *data) {
	struct wlr_renderer *old_drw = drw;
	struct wlr_allocator *old_alloc = alloc;
	struct Monitor *m;

	wlr_log(WLR_DEBUG, "gpu reset");

	if (!(drw = wlr_renderer_autocreate(backend)))
		die("couldn't recreate renderer");

	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't recreate allocator");

	wl_list_remove(&gpu_reset.link);
	wl_signal_add(&drw->events.lost, &gpu_reset);

	wlr_compositor_set_renderer(compositor, drw);

	wl_list_for_each(m, &mons, link) {
		wlr_output_init_render(m->wlr_output, alloc, drw);
	}

	wlr_allocator_destroy(old_alloc);
	wlr_renderer_destroy(old_drw);
}

void handlesig(int signo) {
	if (signo == SIGCHLD)
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
	else if (signo == SIGINT || signo == SIGTERM)
		quit(NULL);
}

void toggle_hotarea(int x_root, int y_root) {
	// 左下角热区坐标计算,兼容多显示屏
	Arg arg = {0};

	// 在刚启动的时候,selmon为NULL,但鼠标可能已经处于热区,
	// 必须判断避免奔溃
	if (!selmon)
		return;

	if (grabc)
		return;

	unsigned hx = selmon->m.x + hotarea_size;
	unsigned hy = selmon->m.y + selmon->m.height - hotarea_size;

	if (enable_hotarea == 1 && selmon->is_in_hotarea == 0 && y_root > hy &&
		x_root < hx && x_root >= selmon->m.x &&
		y_root <= (selmon->m.y + selmon->m.height)) {
		toggleoverview(&arg);
		selmon->is_in_hotarea = 1;
	} else if (enable_hotarea == 1 && selmon->is_in_hotarea == 1 &&
			   (y_root <= hy || x_root >= hx || x_root < selmon->m.x ||
				y_root > (selmon->m.y + selmon->m.height))) {
		selmon->is_in_hotarea = 0;
	}
}

struct wlr_box // 计算客户端居中坐标
setclient_coordinate_center(Client *c, struct wlr_box geom, int offsetx,
							int offsety) {
	struct wlr_box tempbox;
	int offset = 0;
	int len = 0;
	Monitor *m = c->mon ? c->mon : selmon;

	unsigned int cbw = check_hit_no_border(c) ? c->bw : 0;

	if (!c->no_force_center) {
		tempbox.x = m->w.x + (m->w.width - geom.width) / 2;
		tempbox.y = m->w.y + (m->w.height - geom.height) / 2;
	} else {
		tempbox.x = geom.x;
		tempbox.y = geom.y;
	}

	tempbox.width = geom.width;
	tempbox.height = geom.height;

	if (offsetx != 0) {
		len = (m->w.width - c->geom.width - 2 * m->gappoh) / 2;
		offset = len * (offsetx / 100.0);
		tempbox.x += offset;

		// 限制窗口在屏幕内
		if (tempbox.x < m->m.x) {
			tempbox.x = m->m.x - cbw;
		}
		if (tempbox.x + tempbox.width > m->m.x + m->m.width) {
			tempbox.x = m->m.x + m->m.width - tempbox.width + cbw;
		}
	}
	if (offsety != 0) {
		len = (m->w.height - c->geom.height - 2 * m->gappov) / 2;
		offset = len * (offsety / 100.0);
		tempbox.y += offset;

		// 限制窗口在屏幕内
		if (tempbox.y < m->m.y) {
			tempbox.y = m->m.y - cbw;
		}
		if (tempbox.y + tempbox.height > m->m.y + m->m.height) {
			tempbox.y = m->m.y + m->m.height - tempbox.height + cbw;
		}
	}

	return tempbox;
}

/* Helper: Check if rule matches client */
static bool is_window_rule_matches(const ConfigWinRule *r, const char *appid,
								   const char *title) {
	return (r->title && regex_match(r->title, title) && !r->id) ||
		   (r->id && regex_match(r->id, appid) && !r->title) ||
		   (r->id && regex_match(r->id, appid) && r->title &&
			regex_match(r->title, title));
}

static void apply_rule_properties(Client *c, const ConfigWinRule *r) {
	APPLY_INT_PROP(isterm, >= 0);
	APPLY_INT_PROP(noswallow, >= 0);
	APPLY_INT_PROP(nofadein, >= 0);
	APPLY_INT_PROP(nofadeout, >= 0);
	APPLY_INT_PROP(no_force_center, >= 0);
	APPLY_INT_PROP(isfloating, >= 0);
	APPLY_INT_PROP(isfullscreen, >= 0);
	APPLY_INT_PROP(isnoborder, >= 0);
	APPLY_INT_PROP(isopensilent, >= 0);
	APPLY_INT_PROP(isnamedscratchpad, >= 0);
	APPLY_INT_PROP(isglobal, >= 0);
	APPLY_INT_PROP(isoverlay, >= 0);
	APPLY_INT_PROP(isunglobal, >= 0);
	APPLY_INT_PROP(scratchpad_width, >= 0);
	APPLY_INT_PROP(scratchpad_height, >= 0);

	APPLY_FLOAT_PROP(scroller_proportion, > 0.0f);
	APPLY_FLOAT_PROP(focused_opacity, > 0.0f);
	APPLY_FLOAT_PROP(unfocused_opacity, > 0.0f);

	if (r->animation_type_open)
		c->animation_type_open = r->animation_type_open;
	if (r->animation_type_close)
		c->animation_type_close = r->animation_type_close;
}

int applyrulesgeom(Client *c) {
	/* rule matching */
	const char *appid, *title;
	ConfigWinRule *r;
	int hit = 0;
	int ji;

	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (ji = 0; ji < config.window_rules_count; ji++) {
		if (config.window_rules_count < 1)
			break;
		r = &config.window_rules[ji];

		if (!is_window_rule_matches(r, appid, title))
			continue;

		c->geom.width = r->width > 0 ? r->width : c->geom.width;
		c->geom.height = r->height > 0 ? r->height : c->geom.height;

		// 重新计算居中的坐标
		if (r->offsetx != 0 || r->offsety != 0 || r->width > 0 || r->height > 0)
			c->geom =
				setclient_coordinate_center(c, c->geom, r->offsetx, r->offsety);
		if (r->height > 0 || r->width > 0 || r->offsetx != 0 ||
			r->offsety != 0) {
			hit = 1;
		} else {
			hit = 0;
		}
	}
	return hit;
}

void applyrules(Client *c) {
	/* rule matching */
	const char *appid, *title;
	unsigned int i, newtags = 0;
	unsigned int mon_idx;
	const ConfigWinRule *r;
	Monitor *mon = selmon, *m;
	Client *fc;
	bool hit_rule_pos = false;

	c->isfloating = client_is_float_type(c);
	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (i = 0; i < config.window_rules_count; i++) {

		r = &config.window_rules[i];

		// rule matching
		if (!is_window_rule_matches(r, appid, title))
			continue;

		// set general properties
		apply_rule_properties(c, r);

		// set tags
		newtags |= (r->tags > 0) ? r->tags : 0;

		// set monitor of client
		mon_idx = 0;
		wl_list_for_each(m, &mons, link) {
			if (r->monitor == mon_idx++)
				mon = m;
		}

		// set geometry of floating client
		if (c->isfloating) {
			if (r->width > 0)
				c->geom.width = r->width;
			if (r->height > 0)
				c->geom.height = r->height;

			if (r->offsetx || r->offsety || r->width > 0 || r->height > 0) {
				hit_rule_pos = true;
				c->oldgeom = c->geom = setclient_coordinate_center(
					c, c->geom, r->offsetx, r->offsety);
			}
		}
	}

	// if no geom rule hit and is normal winodw, use the center pos and record
	// the hit size
	if (!hit_rule_pos &&
		(!client_is_x11(c) || !client_should_ignore_focus(c))) {
		c->oldgeom = c->geom = setclient_coordinate_center(c, c->geom, 0, 0);
	}

	/*-----------------------apply rule action-------------------------*/

	// rule action only apply after map not apply in the init commit
	if (!client_surface(c)->mapped)
		return;

	// apply swallow rule
	c->pid = client_get_pid(c);
	if (!c->noswallow && !c->isfloating && !client_is_float_type(c) &&
		!c->surface.xdg->initial_commit) {
		Client *p = termforwin(c);
		if (p) {
			c->swallowedby = p;
			p->swallowing = c;
			wl_list_remove(&c->link);
			wl_list_remove(&c->flink);
			swallow(c, p);
			wl_list_remove(&p->link);
			wl_list_remove(&p->flink);
			mon = p->mon;
			newtags = p->tags;
		}
	}

	// set mon and floating, fullscreen and scratchpad state

	if (c->isnamedscratchpad) {
		c->isfloating = 1;
	}

	int fullscreen_state_backup = c->isfullscreen;
	setmon(c, mon, newtags, !c->isopensilent);

	if (!c->isopensilent && c->mon &&
		!(c->tags & (1 << (c->mon->pertag->curtag - 1)))) {
		c->animation.from_rule = true;
		view(&(Arg){.ui = c->tags}, true);
	}

	setfullscreen(c, fullscreen_state_backup);

	/*
	if there is a new non-floating window in the current tag, the fullscreen
	window in the current tag will exit fullscreen and participate in tiling
	*/
	wl_list_for_each(fc, &clients,
					 link) if (fc && fc != c && c->tags & fc->tags &&
							   ISFULLSCREEN(fc) && !c->isfloating) {
		clear_fullscreen_flag(fc);
		arrange(c->mon, false);
	}

	// apply named scratchpad rule
	if (c->isnamedscratchpad) {
		apply_named_scratchpad(c);
	}

	// apply overlay rule
	if (c->isoverlay) {
		wlr_scene_node_reparent(&selmon->sel->scene->node, layers[LyrOverlay]);
		wlr_scene_node_raise_to_top(&selmon->sel->scene->node);
	}

	// update border color
	setborder_color(c);
}

bool is_scroller_layout(Monitor *m) {
	if (strcmp(m->pertag->ltidxs[m->pertag->curtag]->name, "scroller") == 0)
		return true;
	if (strcmp(m->pertag->ltidxs[m->pertag->curtag]->name,
			   "vertical_scroller") == 0)
		return true;
	return false;
}

void // 17
arrange(Monitor *m, bool want_animation) {
	Client *c;

	if (!m)
		return;

	if (!m->wlr_output->enabled)
		return;

	m->visible_clients = 0;
	m->visible_tiling_clients = 0;
	wl_list_for_each(c, &clients, link) {
		if (c->iskilling)
			continue;

		if (c->mon == m && (c->isglobal || c->isunglobal)) {
			c->tags = m->tagset[m->seltags];
			if (selmon->sel == NULL)
				focusclient(c, 0);
		}

		if (c->mon == m) {
			if (VISIBLEON(c, m)) {

				if (!client_is_unmanaged(c) && !client_should_ignore_focus(c)) {
					m->visible_clients++;
				}

				if (ISTILED(c)) {
					m->visible_tiling_clients++;
				}

				if (!c->is_clip_to_hide || !ISTILED(c) ||
					!is_scroller_layout(c->mon)) {
					c->is_clip_to_hide = false;
					wlr_scene_node_set_enabled(&c->scene->node, true);
				}
				client_set_suspended(c, false);
				if (!c->animation.from_rule && want_animation &&
					m->pertag->prevtag != 0 && m->pertag->curtag != 0 &&
					animations) {
					c->animation.tagining = true;
					if (m->pertag->curtag > m->pertag->prevtag) {
						if (c->animation.running) {
							c->animainit_geom.x = c->animation.current.x;
							c->animainit_geom.y = c->animation.current.y;
						} else {
							c->animainit_geom.x =
								tag_animation_direction == VERTICAL
									? c->animation.current.x
									: c->mon->m.x + c->mon->m.width;
							c->animainit_geom.y =
								tag_animation_direction == VERTICAL
									? c->mon->m.y + c->mon->m.height
									: c->animation.current.y;
						}

					} else {
						if (c->animation.running) {
							c->animainit_geom.x = c->animation.current.x;
							c->animainit_geom.y = c->animation.current.y;
						} else {
							c->animainit_geom.x =
								tag_animation_direction == VERTICAL
									? c->animation.current.x
									: m->m.x - c->geom.width;
							c->animainit_geom.y =
								tag_animation_direction == VERTICAL
									? m->m.y - c->geom.height
									: c->animation.current.y;
						}
					}
				} else {
					c->animainit_geom.x = c->animation.current.x;
					c->animainit_geom.y = c->animation.current.y;
				}

				c->animation.from_rule = false;
				c->animation.tagouting = false;
				c->animation.tagouted = false;
				resize(c, c->geom, 0);

			} else {
				if ((c->tags & (1 << (m->pertag->prevtag - 1))) &&
					m->pertag->prevtag != 0 && m->pertag->curtag != 0 &&
					animations) {
					c->animation.tagouting = true;
					c->animation.tagining = false;
					if (m->pertag->curtag > m->pertag->prevtag) {
						c->pending = c->geom;
						c->pending.x = tag_animation_direction == VERTICAL
										   ? c->animation.current.x
										   : c->mon->m.x - c->geom.width;
						c->pending.y = tag_animation_direction == VERTICAL
										   ? c->mon->m.y - c->geom.height
										   : c->animation.current.y;

						resize(c, c->geom, 0);
					} else {
						c->pending = c->geom;
						c->pending.x = tag_animation_direction == VERTICAL
										   ? c->animation.current.x
										   : c->mon->m.x + c->mon->m.width;
						c->pending.y = tag_animation_direction == VERTICAL
										   ? c->mon->m.y + c->mon->m.height
										   : c->animation.current.y;
						resize(c, c->geom, 0);
					}
				} else {
					wlr_scene_node_set_enabled(&c->scene->node, false);
					client_set_suspended(c, true);
				}
			}
		}

		if (c->mon == m && c->ismaxmizescreen && !c->animation.tagouted &&
			!c->animation.tagouting && VISIBLEON(c, m)) {
			reset_maxmizescreen_size(c);
		}
	}

	if (m->isoverview) {
		overviewlayout.arrange(m);
	} else if (m && m->pertag->ltidxs[m->pertag->curtag]->arrange) {
		m->pertag->ltidxs[m->pertag->curtag]->arrange(m);
	}

	motionnotify(0, NULL, 0, 0, 0, 0);
	checkidleinhibitor(NULL);
}

void arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area,
				  int exclusive) {
	LayerSurface *l;
	struct wlr_box full_area = m->m;

	wl_list_for_each(l, list, link) {
		struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;

		if (exclusive != (layer_surface->current.exclusive_zone > 0) ||
			!layer_surface->initialized)
			continue;

		wlr_scene_layer_surface_v1_configure(l->scene_layer, &full_area,
											 usable_area);
		wlr_scene_node_set_position(&l->popups->node, l->scene->node.x,
									l->scene->node.y);
	}
}

Client *center_select(Monitor *m) {
	Client *c = NULL;
	Client *target_c = NULL;
	long int mini_distance = -1;
	int dirx, diry;
	long int distance;
	wl_list_for_each(c, &clients, link) {
		if (c && VISIBLEON(c, m) && client_surface(c)->mapped &&
			!c->isfloating && !client_is_unmanaged(c)) {
			dirx = c->geom.x + c->geom.width / 2 - (m->w.x + m->w.width / 2);
			diry = c->geom.y + c->geom.height / 2 - (m->w.y + m->w.height / 2);
			distance = dirx * dirx + diry * diry;
			if (distance < mini_distance || mini_distance == -1) {
				mini_distance = distance;
				target_c = c;
			}
		}
	}
	return target_c;
}

void apply_window_snap(Client *c) {
	int snap_up = 99999, snap_down = 99999, snap_left = 99999,
		snap_right = 99999;
	int snap_up_temp = 0, snap_down_temp = 0, snap_left_temp = 0,
		snap_right_temp = 0;
	int snap_up_screen = 0, snap_down_screen = 0, snap_left_screen = 0,
		snap_right_screen = 0;
	int snap_up_mon = 0, snap_down_mon = 0, snap_left_mon = 0,
		snap_right_mon = 0;

	unsigned int cbw = !render_border || c->fake_no_border ? borderpx : 0;
	unsigned int tcbw;
	unsigned int cx, cy, cw, ch, tcx, tcy, tcw, tch;
	cx = c->geom.x + cbw;
	cy = c->geom.y + cbw;
	cw = c->geom.width - 2 * cbw;
	ch = c->geom.height - 2 * cbw;

	Client *tc;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (!c->isfloating || !enable_floating_snap)
		return;

	wl_list_for_each(tc, &clients, link) {
		if (tc && tc->isfloating && !tc->iskilling &&
			client_surface(tc)->mapped && VISIBLEON(tc, c->mon)) {

			tcbw = !render_border || tc->fake_no_border ? borderpx : 0;
			tcx = tc->geom.x + tcbw;
			tcy = tc->geom.y + tcbw;
			tcw = tc->geom.width - 2 * tcbw;
			tch = tc->geom.height - 2 * tcbw;

			snap_left_temp = cx - tcx - tcw;
			snap_right_temp = tcx - cx - cw;
			snap_up_temp = cy - tcy - tch;
			snap_down_temp = tcy - cy - ch;

			if (snap_left_temp < snap_left && snap_left_temp >= 0) {
				snap_left = snap_left_temp;
			}
			if (snap_right_temp < snap_right && snap_right_temp >= 0) {
				snap_right = snap_right_temp;
			}
			if (snap_up_temp < snap_up && snap_up_temp >= 0) {
				snap_up = snap_up_temp;
			}
			if (snap_down_temp < snap_down && snap_down_temp >= 0) {
				snap_down = snap_down_temp;
			}
		}
	}

	snap_left_mon = cx - c->mon->m.x;
	snap_right_mon = c->mon->m.x + c->mon->m.width - cx - cw;
	snap_up_mon = cy - c->mon->m.y;
	snap_down_mon = c->mon->m.y + c->mon->m.height - cy - ch;

	if (snap_up_mon >= 0 && snap_up_mon < snap_up)
		snap_up = snap_up_mon;
	if (snap_down_mon >= 0 && snap_down_mon < snap_down)
		snap_down = snap_down_mon;
	if (snap_left_mon >= 0 && snap_left_mon < snap_left)
		snap_left = snap_left_mon;
	if (snap_right_mon >= 0 && snap_right_mon < snap_right)
		snap_right = snap_right_mon;

	snap_left_screen = cx - c->mon->w.x;
	snap_right_screen = c->mon->w.x + c->mon->w.width - cx - cw;
	snap_up_screen = cy - c->mon->w.y;
	snap_down_screen = c->mon->w.y + c->mon->w.height - cy - ch;

	if (snap_up_screen >= 0 && snap_up_screen < snap_up)
		snap_up = snap_up_screen;
	if (snap_down_screen >= 0 && snap_down_screen < snap_down)
		snap_down = snap_down_screen;
	if (snap_left_screen >= 0 && snap_left_screen < snap_left)
		snap_left = snap_left_screen;
	if (snap_right_screen >= 0 && snap_right_screen < snap_right)
		snap_right = snap_right_screen;

	if (snap_left < snap_right && snap_left < snap_distance) {
		c->geom.x = c->geom.x - snap_left;
	}

	if (snap_right <= snap_left && snap_right < snap_distance) {
		c->geom.x = c->geom.x + snap_right;
	}

	if (snap_up < snap_down && snap_up < snap_distance) {
		c->geom.y = c->geom.y - snap_up;
	}

	if (snap_down <= snap_up && snap_down < snap_distance) {
		c->geom.y = c->geom.y + snap_down;
	}

	c->oldgeom = c->geom;
	resize(c, c->geom, 0);
}

Client *find_client_by_direction(Client *tc, const Arg *arg, bool findfloating,
								 bool align) {
	Client *c;
	Client **tempClients = NULL; // 初始化为 NULL
	int last = -1;

	// 第一次遍历，计算客户端数量
	wl_list_for_each(c, &clients, link) {
		if (c && (findfloating || !c->isfloating) && !c->isunglobal &&
			(focus_cross_monitor || c->mon == selmon) &&
			(c->tags & c->mon->tagset[c->mon->seltags])) {
			last++;
		}
	}

	if (last < 0) {
		return NULL; // 没有符合条件的客户端
	}

	// 动态分配内存
	tempClients = malloc((last + 1) * sizeof(Client *));
	if (!tempClients) {
		// 处理内存分配失败的情况
		return NULL;
	}

	// 第二次遍历，填充 tempClients
	last = -1;
	wl_list_for_each(c, &clients, link) {
		if (c && (findfloating || !c->isfloating) && !c->isunglobal &&
			(focus_cross_monitor || c->mon == selmon) &&
			(c->tags & c->mon->tagset[c->mon->seltags])) {
			last++;
			tempClients[last] = c;
		}
	}

	int sel_x = tc->geom.x;
	int sel_y = tc->geom.y;
	long long int distance = LLONG_MAX;
	Client *tempFocusClients = NULL;

	switch (arg->i) {
	case UP:
		for (int _i = 0; _i <= last; _i++) {
			if (tempClients[_i]->geom.y < sel_y &&
				tempClients[_i]->geom.x == sel_x) {
				int dis_x = tempClients[_i]->geom.x - sel_x;
				int dis_y = tempClients[_i]->geom.y - sel_y;
				long long int tmp_distance =
					dis_x * dis_x + dis_y * dis_y; // 计算距离
				if (tmp_distance < distance) {
					distance = tmp_distance;
					tempFocusClients = tempClients[_i];
				}
			}
		}
		if (!tempFocusClients && !align) {
			for (int _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.y < sel_y) {
					int dis_x = tempClients[_i]->geom.x - sel_x;
					int dis_y = tempClients[_i]->geom.y - sel_y;
					long long int tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	case DOWN:
		for (int _i = 0; _i <= last; _i++) {
			if (tempClients[_i]->geom.y > sel_y &&
				tempClients[_i]->geom.x == sel_x) {
				int dis_x = tempClients[_i]->geom.x - sel_x;
				int dis_y = tempClients[_i]->geom.y - sel_y;
				long long int tmp_distance =
					dis_x * dis_x + dis_y * dis_y; // 计算距离
				if (tmp_distance < distance) {
					distance = tmp_distance;
					tempFocusClients = tempClients[_i];
				}
			}
		}
		if (!tempFocusClients && !align) {
			for (int _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.y > sel_y) {
					int dis_x = tempClients[_i]->geom.x - sel_x;
					int dis_y = tempClients[_i]->geom.y - sel_y;
					long long int tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	case LEFT:
		for (int _i = 0; _i <= last; _i++) {
			if (tempClients[_i]->geom.x < sel_x &&
				tempClients[_i]->geom.y == sel_y) {
				int dis_x = tempClients[_i]->geom.x - sel_x;
				int dis_y = tempClients[_i]->geom.y - sel_y;
				long long int tmp_distance =
					dis_x * dis_x + dis_y * dis_y; // 计算距离
				if (tmp_distance < distance) {
					distance = tmp_distance;
					tempFocusClients = tempClients[_i];
				}
			}
		}
		if (!tempFocusClients && !align) {
			for (int _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.x < sel_x) {
					int dis_x = tempClients[_i]->geom.x - sel_x;
					int dis_y = tempClients[_i]->geom.y - sel_y;
					long long int tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	case RIGHT:
		for (int _i = 0; _i <= last; _i++) {
			if (tempClients[_i]->geom.x > sel_x &&
				tempClients[_i]->geom.y == sel_y) {
				int dis_x = tempClients[_i]->geom.x - sel_x;
				int dis_y = tempClients[_i]->geom.y - sel_y;
				long long int tmp_distance =
					dis_x * dis_x + dis_y * dis_y; // 计算距离
				if (tmp_distance < distance) {
					distance = tmp_distance;
					tempFocusClients = tempClients[_i];
				}
			}
		}
		if (!tempFocusClients && !align) {
			for (int _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.x > sel_x) {
					int dis_x = tempClients[_i]->geom.x - sel_x;
					int dis_y = tempClients[_i]->geom.y - sel_y;
					long long int tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	}

	free(tempClients); // 释放内存
	return tempFocusClients;
}

Client *direction_select(const Arg *arg) {

	Client *tc = selmon->sel;

	if (!tc)
		return NULL;

	if (tc && (tc->isfullscreen || tc->ismaxmizescreen)) {
		// 不支持全屏窗口的焦点切换
		return NULL;
	}

	return find_client_by_direction(tc, arg, true, false);
}

void focusdir(const Arg *arg) {
	Client *c;
	c = direction_select(arg);
	if (c) {
		focusclient(c, 1);
		if (warpcursor)
			warp_cursor(c);
	} else {
		if (config.focus_cross_tag) {
			if (arg->i == LEFT || arg->i == UP)
				viewtoleft_have_client(NULL);
			if (arg->i == RIGHT || arg->i == DOWN)
				viewtoright_have_client(NULL);
		}
	}
}

void arrangelayers(Monitor *m) {
	int i;
	struct wlr_box usable_area = m->m;
	LayerSurface *l;
	unsigned int layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};
	if (!m->wlr_output->enabled)
		return;

	/* Arrange exclusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 1);

	if (!wlr_box_equal(&usable_area, &m->w)) {
		m->w = usable_area;
		arrange(m, false);
	}

	/* Arrange non-exlusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 0);

	/* Find topmost keyboard interactive layer, if such a layer exists */
	for (i = 0; i < (int)LENGTH(layers_above_shell); i++) {
		wl_list_for_each_reverse(l, &m->layers[layers_above_shell[i]], link) {
			if (locked ||
				l->layer_surface->current.keyboard_interactive !=
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE ||
				!l->mapped || l == exclusive_focus)
				continue;
			/* Deactivate the focused client. */
			focusclient(NULL, 0);
			exclusive_focus = l;
			client_notify_enter(l->layer_surface->surface,
								wlr_seat_get_keyboard(seat));
			return;
		}
	}
}

char *get_autostart_path(char *autostart_path, unsigned int buf_size) {
	const char *maomaoconfig = getenv("MAOMAOCONFIG");

	if (maomaoconfig && maomaoconfig[0] != '\0') {
		snprintf(autostart_path, buf_size, "%s/autostart.sh", maomaoconfig);
	} else {
		const char *homedir = getenv("HOME");
		if (!homedir) {
			fprintf(stderr, "Error: HOME environment variable not set.\n");
			return NULL;
		}
		snprintf(autostart_path, buf_size, "%s/.config/maomao/autostart.sh",
				 homedir);
	}

	return autostart_path;
}

void // 鼠标滚轮事件
axisnotify(struct wl_listener *listener, void *data) {

	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct wlr_pointer_axis_event *event = data;
	struct wlr_keyboard *keyboard;
	unsigned int mods;
	AxisBinding *a;
	int ji;
	unsigned int adir;
	// IDLE_NOTIFY_ACTIVITY;
	handlecursoractivity();
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
	keyboard = wlr_seat_get_keyboard(seat);

	// 获取当前按键的mask,比如alt+super或者alt+ctrl
	mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

	if (event->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL)
		adir = event->delta > 0 ? AxisDown : AxisUp;
	else
		adir = event->delta > 0 ? AxisRight : AxisLeft;

	for (ji = 0; ji < config.axis_bindings_count; ji++) {
		if (config.axis_bindings_count < 1)
			break;
		a = &config.axis_bindings[ji];
		if (CLEANMASK(mods) == CLEANMASK(a->mod) && // 按键一致
			adir == a->dir && a->func) { // 滚轮方向判断一致且处理函数存在
			if (event->time_msec - axis_apply_time > axis_bind_apply_timeout ||
				axis_apply_dir * event->delta < 0) {
				a->func(&a->arg);
				axis_apply_time = event->time_msec;
				axis_apply_dir = event->delta > 0 ? 1 : -1;
				return; // 如果成功匹配就不把这个滚轮事件传送给客户端了
			} else {
				axis_apply_dir = event->delta > 0 ? 1 : -1;
				axis_apply_time = event->time_msec;
				return;
			}
		}
	}

	/* TODO: allow usage of scroll whell for mousebindings, it can be
	 * implemented checking the event's orientation and the delta of the event
	 */
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(seat, // 滚轮事件发送给客户端也就是窗口
								 event->time_msec, event->orientation,
								 event->delta, event->delta_discrete,
								 event->source, event->relative_direction);
}

int ongesture(struct wlr_pointer_swipe_end_event *event) {
	struct wlr_keyboard *keyboard;
	unsigned int mods;
	const GestureBinding *g;
	unsigned int motion;
	unsigned int adx = (int)round(fabs(swipe_dx));
	unsigned int ady = (int)round(fabs(swipe_dy));
	int handled = 0;
	int ji;

	if (event->cancelled) {
		return handled;
	}

	// Require absolute distance movement beyond a small thresh-hold
	if (adx * adx + ady * ady < swipe_min_threshold * swipe_min_threshold) {
		return handled;
	}

	if (adx > ady) {
		motion = swipe_dx < 0 ? SWIPE_LEFT : SWIPE_RIGHT;
	} else {
		motion = swipe_dy < 0 ? SWIPE_UP : SWIPE_DOWN;
	}

	keyboard = wlr_seat_get_keyboard(seat);
	mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

	for (ji = 0; ji < config.gesture_bindings_count; ji++) {
		if (config.gesture_bindings_count < 1)
			break;
		g = &config.gesture_bindings[ji];
		if (CLEANMASK(mods) == CLEANMASK(g->mod) &&
			swipe_fingers == g->fingers_count && motion == g->motion &&
			g->func) {
			g->func(&g->arg);
			handled = 1;
		}
	}
	return handled;
}

void swipe_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_begin_event *event = data;

	// Forward swipe begin event to client
	wlr_pointer_gestures_v1_send_swipe_begin(pointer_gestures, seat,
											 event->time_msec, event->fingers);
}

void swipe_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_update_event *event = data;

	swipe_fingers = event->fingers;
	// Accumulate swipe distance
	swipe_dx += event->dx;
	swipe_dy += event->dy;

	// Forward swipe update event to client
	wlr_pointer_gestures_v1_send_swipe_update(
		pointer_gestures, seat, event->time_msec, event->dx, event->dy);
}

void swipe_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_end_event *event = data;
	ongesture(event);
	swipe_dx = 0;
	swipe_dy = 0;
	// Forward swipe end event to client
	wlr_pointer_gestures_v1_send_swipe_end(pointer_gestures, seat,
										   event->time_msec, event->cancelled);
}

void pinch_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_begin_event *event = data;

	// Forward pinch begin event to client
	wlr_pointer_gestures_v1_send_pinch_begin(pointer_gestures, seat,
											 event->time_msec, event->fingers);
}

void pinch_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_update_event *event = data;

	// Forward pinch update event to client
	wlr_pointer_gestures_v1_send_pinch_update(
		pointer_gestures, seat, event->time_msec, event->dx, event->dy,
		event->scale, event->rotation);
}

void pinch_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_end_event *event = data;

	// Forward pinch end event to client
	wlr_pointer_gestures_v1_send_pinch_end(pointer_gestures, seat,
										   event->time_msec, event->cancelled);
}

void hold_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_begin_event *event = data;

	// Forward hold begin event to client
	wlr_pointer_gestures_v1_send_hold_begin(pointer_gestures, seat,
											event->time_msec, event->fingers);
}

void hold_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_end_event *event = data;

	// Forward hold end event to client
	wlr_pointer_gestures_v1_send_hold_end(pointer_gestures, seat,
										  event->time_msec, event->cancelled);
}

void place_drag_tile_client(Client *c) {
	Client *tc = NULL;
	Client *closest_client = NULL;
	long min_distant = LONG_MAX;
	long temp_distant;
	int x, y;

	wl_list_for_each(tc, &clients, link) {
		if (tc != c && ISTILED(tc) && VISIBLEON(tc, c->mon)) {
			x = tc->geom.x + (int)(tc->geom.width / 2) - cursor->x;
			y = tc->geom.y + (int)(tc->geom.height / 2) - cursor->y;
			temp_distant = x * x + y * y;
			if (temp_distant < min_distant) {
				min_distant = temp_distant;
				closest_client = tc;
			}
		}
	}
	if (closest_client && closest_client->link.prev != &c->link) {
		wl_list_remove(&c->link);
		c->link.next = &closest_client->link;
		c->link.prev = closest_client->link.prev;
		closest_client->link.prev->next = &c->link;
		closest_client->link.prev = &c->link;
	} else if (closest_client) {
		exchange_two_client(c, closest_client);
	}
	setfloating(c, 0);
}

void // 鼠标按键事件
buttonpress(struct wl_listener *listener, void *data) {
	struct wlr_pointer_button_event *event = data;
	struct wlr_keyboard *keyboard;
	unsigned int mods;
	Client *c;
	LayerSurface *l;
	struct wlr_surface *surface;
	Client *tmpc;
	int ji;
	const MouseBinding *b;
	struct wlr_surface *old_pointer_focus_surface =
		seat->pointer_state.focused_surface;

	handlecursoractivity();
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	switch (event->state) {
	case WL_POINTER_BUTTON_STATE_PRESSED:
		cursor_mode = CurPressed;
		selmon = xytomon(cursor->x, cursor->y);
		if (locked)
			break;

		xytonode(cursor->x, cursor->y, &surface, NULL, NULL, NULL, NULL);
		if (toplevel_from_wlr_surface(surface, &c, &l) >= 0) {
			if (c && (!client_is_unmanaged(c) || client_wants_focus(c)))
				focusclient(c, 1);

			if (surface != old_pointer_focus_surface) {
				wlr_seat_pointer_notify_clear_focus(seat);
				motionnotify(0, NULL, 0, 0, 0, 0);
			}

			// 聚焦按需要交互焦点的layer，但注意不能抢占独占焦点的layer
			if (l && !exclusive_focus &&
				l->layer_surface->current.keyboard_interactive ==
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
				focusclient(NULL, 0);
				client_notify_enter(l->layer_surface->surface,
									wlr_seat_get_keyboard(seat));
			}
		}

		keyboard = wlr_seat_get_keyboard(seat);
		mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
		for (ji = 0; ji < config.mouse_bindings_count; ji++) {
			if (config.mouse_bindings_count < 1)
				break;
			b = &config.mouse_bindings[ji];
			if (CLEANMASK(mods) == CLEANMASK(b->mod) &&
				event->button == b->button && b->func &&
				(selmon->isoverview == 1 || b->button == BTN_MIDDLE) && c) {
				b->func(&b->arg);
				return;
			} else if (CLEANMASK(mods) == CLEANMASK(b->mod) &&
					   event->button == b->button && b->func &&
					   CLEANMASK(b->mod) != 0) {
				b->func(&b->arg);
				return;
			}
		}
		break;
	case WL_POINTER_BUTTON_STATE_RELEASED:
		/* If you released any buttons, we exit interactive move/resize mode. */
		if (!locked && cursor_mode != CurNormal && cursor_mode != CurPressed) {
			cursor_mode = CurNormal;
			/* Clear the pointer focus, this way if the cursor is over a surface
			 * we will send an enter event after which the client will provide
			 * us a cursor surface */
			wlr_seat_pointer_clear_focus(seat);
			motionnotify(0, NULL, 0, 0, 0, 0);
			/* Drop the window off on its new monitor */
			if (grabc == selmon->sel) {
				selmon->sel = NULL;
			}
			selmon = xytomon(cursor->x, cursor->y);
			client_update_oldmonname_record(grabc, selmon);
			setmon(grabc, selmon, 0, true);
			reset_foreign_tolevel(grabc);
			selmon->prevsel = selmon->sel;
			selmon->sel = grabc;
			tmpc = grabc;
			grabc = NULL;
			if (tmpc->drag_to_tile && drag_tile_to_tile) {
				place_drag_tile_client(tmpc);
			} else {
				apply_window_snap(tmpc);
			}
			tmpc->drag_to_tile = false;
			return;
		} else {
			cursor_mode = CurNormal;
		}
		break;
	}
	/* If the event wasn't handled by the compositor, notify the client with
	 * pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(seat, event->time_msec, event->button,
								   event->state);
}

void chvt(const Arg *arg) { wlr_session_change_vt(session, arg->ui); }

void checkidleinhibitor(struct wlr_surface *exclude) {
	int inhibited = 0, unused_lx, unused_ly;
	struct wlr_idle_inhibitor_v1 *inhibitor;
	wl_list_for_each(inhibitor, &idle_inhibit_mgr->inhibitors, link) {
		struct wlr_surface *surface =
			wlr_surface_get_root_surface(inhibitor->surface);
		struct wlr_scene_tree *tree = surface->data;
		if (exclude != surface &&
			(inhibit_regardless_of_visibility ||
			 (!tree ||
			  wlr_scene_node_coords(&tree->node, &unused_lx, &unused_ly)))) {
			inhibited = 1;
			break;
		}
	}

	wlr_idle_notifier_v1_set_inhibited(idle_notifier, inhibited);
}

void setcursorshape(struct wl_listener *listener, void *data) {
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided cursor shape. */
	if (event->seat_client == seat->pointer_state.focused_client) {
		last_cursor.shape = event->shape;
		last_cursor.surface = NULL;
		if (!cursor_hidden)
			wlr_cursor_set_xcursor(cursor, cursor_mgr,
								   wlr_cursor_shape_v1_name(event->shape));
	}
}

void cleanuplisteners(void) {
	wl_list_remove(&cursor_axis.link);
	wl_list_remove(&cursor_button.link);
	wl_list_remove(&cursor_frame.link);
	wl_list_remove(&cursor_motion.link);
	wl_list_remove(&cursor_motion_absolute.link);
	wl_list_remove(&gpu_reset.link);
	wl_list_remove(&new_idle_inhibitor.link);
	wl_list_remove(&layout_change.link);
	wl_list_remove(&new_input_device.link);
	wl_list_remove(&new_virtual_keyboard.link);
	wl_list_remove(&new_virtual_pointer.link);
	wl_list_remove(&new_pointer_constraint.link);
	wl_list_remove(&new_output.link);
	wl_list_remove(&new_xdg_toplevel.link);
	wl_list_remove(&new_xdg_decoration.link);
	wl_list_remove(&new_xdg_popup.link);
	wl_list_remove(&new_layer_surface.link);
	wl_list_remove(&output_mgr_apply.link);
	wl_list_remove(&output_mgr_test.link);
	wl_list_remove(&output_power_mgr_set_mode.link);
	wl_list_remove(&request_activate.link);
	wl_list_remove(&request_cursor.link);
	wl_list_remove(&request_set_psel.link);
	wl_list_remove(&request_set_sel.link);
	wl_list_remove(&request_set_cursor_shape.link);
	wl_list_remove(&request_start_drag.link);
	wl_list_remove(&start_drag.link);
	wl_list_remove(&new_session_lock.link);
#ifdef XWAYLAND
	wl_list_remove(&new_xwayland_surface.link);
	wl_list_remove(&xwayland_ready.link);
#endif
}

void cleanup(void) {
	cleanuplisteners();
#ifdef XWAYLAND
	wlr_xwayland_destroy(xwayland);
	xwayland = NULL;
#endif

	wl_display_destroy_clients(dpy);
	if (child_pid > 0) {
		kill(-child_pid, SIGTERM);
		waitpid(child_pid, NULL, 0);
	}
	wlr_xcursor_manager_destroy(cursor_mgr);

	destroykeyboardgroup(&kb_group->destroy, NULL);

	dwl_im_relay_finish(dwl_input_method_relay);

	/* If it's not destroyed manually it will cause a use-after-free of
	 * wlr_seat. Destroy it until it's fixed in the wlroots side */
	wlr_backend_destroy(backend);

	wl_display_destroy(dpy);
	/* Destroy after the wayland display (when the monitors are already
	   destroyed) to avoid destroying them with an invalid scene output. */
	wlr_scene_node_destroy(&scene->tree.node);
}

void // 17
cleanupkeyboard(struct wl_listener *listener, void *data) {
	Keyboard *kb = wl_container_of(listener, kb, destroy);

	wl_event_source_remove(kb->key_repeat_source);
	wl_list_remove(&kb->link);
	wl_list_remove(&kb->modifiers.link);
	wl_list_remove(&kb->key.link);
	wl_list_remove(&kb->destroy.link);
	free(kb);
}

void cleanupmon(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, destroy);
	LayerSurface *l, *tmp;
	unsigned int i;

	/* m->layers[i] are intentionally not unlinked */
	for (i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmp, &m->layers[i], link)
			wlr_layer_surface_v1_destroy(l->layer_surface);
	}

	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->frame.link);
	wl_list_remove(&m->link);
	wl_list_remove(&m->request_state.link);
	if (m->lock_surface)
		destroylocksurface(&m->destroy_lock_surface, NULL);
	m->wlr_output->data = NULL;
	wlr_output_layout_remove(output_layout, m->wlr_output);
	wlr_scene_output_destroy(m->scene_output);

	closemon(m);
	free(m);
}

void closemon(Monitor *m) {
	/* update selmon if needed and
	 * move closed monitor's clients to the focused one */
	Client *c;
	int i = 0, nmons = wl_list_length(&mons);
	if (!nmons) {
		selmon = NULL;
	} else if (m == selmon) {
		do /* don't switch to disabled mons */
			selmon = wl_container_of(mons.next, selmon, link);
		while (!selmon->wlr_output->enabled && i++ < nmons);

		if (!selmon->wlr_output->enabled)
			selmon = NULL;
	}

	wl_list_for_each(c, &clients, link) {
		if (c->mon == m) {

			if (selmon == NULL) {
				remove_foreign_topleve(c);
				c->mon = NULL;
			} else {
				client_change_mon(c, selmon);
			}
			// record the oldmonname which is used to restore
			if (c->oldmonname[0] == '\0') {
				client_update_oldmonname_record(c, m);
			}
		}
	}
	if (selmon) {
		focusclient(focustop(selmon), 1);
		printstatus();
	}
}

void maplayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, map);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	int ji;

	l->mapped = 1;

	if (!l->mon)
		return;
	strncpy(l->mon->last_surface_ws_name, layer_surface->namespace,
			sizeof(l->mon->last_surface_ws_name) - 1); // 最多拷贝255个字符
	l->mon->last_surface_ws_name[sizeof(l->mon->last_surface_ws_name) - 1] =
		'\0'; // 确保字符串以null结尾

	// 初始化几何位置
	get_layer_target_geometry(l, &l->geom);

	l->noanim = 0;
	l->dirty = false;
	l->need_output_flush = true;

	// 应用layer规则
	for (ji = 0; ji < config.layer_rules_count; ji++) {
		if (config.layer_rules_count < 1)
			break;
		if (strcmp(config.layer_rules[ji].layer_name,
				   l->layer_surface->namespace) == 0) {
			if (config.layer_rules[ji].noanim > 0) {
				l->noanim = 1;
			}
		}
	}

	// 初始化动画
	if (animations && layer_animations && !l->noanim) {
		l->animation.action = OPEN;
		layer_set_pending_state(l);
	}
	// 刷新布局，让窗口能感应到exclude_zone变化以及设置独占表面
	arrangelayers(l->mon);

	// 按需交互layer需要像正常窗口一样抢占非独占layer的焦点
	if (!exclusive_focus &&
		l->layer_surface->current.keyboard_interactive ==
			ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
		focusclient(NULL, 0);
		client_notify_enter(l->layer_surface->surface,
							wlr_seat_get_keyboard(seat));
	}
}

void commitlayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, surface_commit);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	struct wlr_scene_tree *scene_layer =
		layers[layermap[layer_surface->current.layer]];
	struct wlr_layer_surface_v1_state old_state;
	struct wlr_box box;

	if (l->layer_surface->initial_commit) {
		client_set_scale(layer_surface->surface, l->mon->wlr_output->scale);

		/* Temporarily set the layer's current state to pending
		 * so that we can easily arrange it */
		old_state = l->layer_surface->current;
		l->layer_surface->current = l->layer_surface->pending;
		arrangelayers(l->mon);
		l->layer_surface->current = old_state;

		return;
	}

	get_layer_target_geometry(l, &box);

	if (animations && layer_animations && !l->noanim && l->mapped &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND &&
		!wlr_box_equal(&box, &l->geom)) {

		l->geom.x = box.x;
		l->geom.y = box.y;
		l->geom.width = box.width;
		l->geom.height = box.height;
		l->animation.action = MOVE;
		l->need_output_flush = true;
		layer_set_pending_state(l);
	}

	if (layer_surface == exclusive_focus &&
		layer_surface->current.keyboard_interactive !=
			ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
		exclusive_focus = NULL;

	if (layer_surface->current.committed == 0 &&
		l->mapped == layer_surface->surface->mapped)
		return;
	l->mapped = layer_surface->surface->mapped;

	if (scene_layer != l->scene->node.parent) {
		wlr_scene_node_reparent(&l->scene->node, scene_layer);
		wl_list_remove(&l->link);
		wl_list_insert(&l->mon->layers[layer_surface->current.layer], &l->link);
		wlr_scene_node_reparent(
			&l->popups->node,
			(layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP
				 ? layers[LyrTop]
				 : scene_layer));
	}

	arrangelayers(l->mon);
}

void commitnotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, commit);

	if (!c->surface.xdg->initialized)
		return;

	if (c->surface.xdg->initial_commit) {
		// xdg client will first enter this before mapnotify
		applyrules(c);
		if (c->mon) {
			client_set_scale(client_surface(c), c->mon->wlr_output->scale);
		}
		setmon(c, NULL, 0,
			   true); /* Make sure to reapply rules in mapnotify() */
		wlr_xdg_toplevel_set_wm_capabilities(
			c->surface.xdg->toplevel,
			WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
		wlr_xdg_toplevel_set_size(c->surface.xdg->toplevel, 0, 0);
		client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
								WLR_EDGE_RIGHT);
		if (c->decoration)
			requestdecorationmode(&c->set_decoration_mode, c->decoration);
		return;
	}

	if (!c || c->iskilling || c->animation.tagouting || c->animation.tagouted ||
		c->animation.tagining)
		return;

	if (c == grabc)
		return;

	if (client_is_unmanaged(c))
		return;

	unsigned int serial = c->surface.xdg->current.configure_serial;
	if (!c->dirty || serial < c->configure_serial)
		return;

	struct wlr_box geometry;
	client_get_geometry(c, &geometry);

	if (geometry.width == c->geom.width - 2 * c->bw &&
		geometry.height == c->geom.height - 2 * c->bw) {
		c->dirty = false;
		return;
	}

	if (geometry.width == c->animation.current.width - 2 * c->bw &&
		geometry.height == c->animation.current.height - 2 * c->bw) {
		return;
	}

	resize(c, c->geom, 0);
}

void destroydecoration(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, destroy_decoration);

	wl_list_remove(&c->destroy_decoration.link);
	wl_list_remove(&c->set_decoration_mode.link);
}

void commitpopup(struct wl_listener *listener, void *data) {
	struct wlr_surface *surface = data;
	struct wlr_xdg_popup *popup = wlr_xdg_popup_try_from_wlr_surface(surface);
	LayerSurface *l = NULL;
	Client *c = NULL;
	struct wlr_box box;
	int type = -1;

	if (!popup->base->initial_commit)
		return;

	type = toplevel_from_wlr_surface(popup->base->surface, &c, &l);
	if (!popup->parent || type < 0)
		return;
	popup->base->surface->data =
		wlr_scene_xdg_surface_create(popup->parent->data, popup->base);
	if ((l && !l->mon) || (c && !c->mon)) {
		wlr_xdg_popup_destroy(popup);
		return;
	}
	box = type == LayerShell ? l->mon->m : c->mon->w;
	box.x -= (type == LayerShell ? l->scene->node.x : c->geom.x);
	box.y -= (type == LayerShell ? l->scene->node.y : c->geom.y);
	wlr_xdg_popup_unconstrain_from_box(popup, &box);
	wl_list_remove(&listener->link);
	free(listener);
}

void createdecoration(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;
	Client *c = deco->toplevel->base->data;
	c->decoration = deco;

	LISTEN(&deco->events.request_mode, &c->set_decoration_mode,
		   requestdecorationmode);
	LISTEN(&deco->events.destroy, &c->destroy_decoration, destroydecoration);

	requestdecorationmode(&c->set_decoration_mode, deco);
}

void createidleinhibitor(struct wl_listener *listener, void *data) {
	struct wlr_idle_inhibitor_v1 *idle_inhibitor = data;
	LISTEN_STATIC(&idle_inhibitor->events.destroy, destroyidleinhibitor);

	checkidleinhibitor(NULL);
}

void createkeyboard(struct wlr_keyboard *keyboard) {

	/* Set the keymap to match the group keymap */
	wlr_keyboard_set_keymap(keyboard, kb_group->wlr_group->keyboard.keymap);

	wlr_keyboard_notify_modifiers(keyboard, 0, 0, locked_mods, 0);

	/* Add the new keyboard to the group */
	wlr_keyboard_group_add_keyboard(kb_group->wlr_group, keyboard);
}

KeyboardGroup *createkeyboardgroup(void) {
	KeyboardGroup *group = ecalloc(1, sizeof(*group));
	struct xkb_context *context;
	struct xkb_keymap *keymap;

	group->wlr_group = wlr_keyboard_group_create();
	group->wlr_group->data = group;

	/* Prepare an XKB keymap and assign it to the keyboard group. */
	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!(keymap = xkb_keymap_new_from_names(context, &xkb_rules,
											 XKB_KEYMAP_COMPILE_NO_FLAGS)))
		die("failed to compile keymap");

	wlr_keyboard_set_keymap(&group->wlr_group->keyboard, keymap);

	if (numlockon) {
		xkb_mod_index_t mod_index =
			xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_NUM);
		if (mod_index != XKB_MOD_INVALID)
			locked_mods |= (unsigned int)1 << mod_index;
	}

	if (capslock) {
		xkb_mod_index_t mod_index =
			xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
		if (mod_index != XKB_MOD_INVALID)
			locked_mods |= (unsigned int)1 << mod_index;
	}

	if (locked_mods)
		wlr_keyboard_notify_modifiers(&group->wlr_group->keyboard, 0, 0,
									  locked_mods, 0);

	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	wlr_keyboard_set_repeat_info(&group->wlr_group->keyboard, repeat_rate,
								 repeat_delay);

	/* Set up listeners for keyboard events */
	LISTEN(&group->wlr_group->keyboard.events.key, &group->key, keypress);
	LISTEN(&group->wlr_group->keyboard.events.modifiers, &group->modifiers,
		   keypressmod);

	group->key_repeat_source =
		wl_event_loop_add_timer(event_loop, keyrepeat, group);

	/* A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same wlr_keyboard_group, which provides a single wlr_keyboard interface
	 * for all of them. Set this combined wlr_keyboard as the seat keyboard.
	 */
	wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
	return group;
}

void createlayersurface(struct wl_listener *listener, void *data) {
	struct wlr_layer_surface_v1 *layer_surface = data;
	LayerSurface *l;
	struct wlr_surface *surface = layer_surface->surface;
	struct wlr_scene_tree *scene_layer =
		layers[layermap[layer_surface->pending.layer]];

	if (!layer_surface->output &&
		!(layer_surface->output = selmon ? selmon->wlr_output : NULL)) {
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	l = layer_surface->data = ecalloc(1, sizeof(*l));
	l->type = LayerShell;
	LISTEN(&surface->events.map, &l->map, maplayersurfacenotify);
	LISTEN(&surface->events.commit, &l->surface_commit,
		   commitlayersurfacenotify);
	LISTEN(&surface->events.unmap, &l->unmap, unmaplayersurfacenotify);
	LISTEN(&layer_surface->events.destroy, &l->destroy,
		   destroylayersurfacenotify);

	l->layer_surface = layer_surface;
	l->mon = layer_surface->output->data;
	l->scene_layer =
		wlr_scene_layer_surface_v1_create(scene_layer, layer_surface);
	l->scene = l->scene_layer->tree;
	l->popups = surface->data = wlr_scene_tree_create(
		layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP
			? layers[LyrTop]
			: scene_layer);
	l->scene->node.data = l->popups->node.data = l;

	wl_list_insert(&l->mon->layers[layer_surface->pending.layer], &l->link);
	wlr_surface_send_enter(surface, layer_surface->output);
}

void createlocksurface(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	Monitor *m = lock_surface->output->data;
	struct wlr_scene_tree *scene_tree = lock_surface->surface->data =
		wlr_scene_subsurface_tree_create(lock->scene, lock_surface->surface);
	m->lock_surface = lock_surface;

	wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
	wlr_session_lock_surface_v1_configure(lock_surface, m->m.width,
										  m->m.height);

	LISTEN(&lock_surface->events.destroy, &m->destroy_lock_surface,
		   destroylocksurface);

	if (m == selmon)
		client_notify_enter(lock_surface->surface, wlr_seat_get_keyboard(seat));
}

void createmon(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new output (aka a display or
	 * monitor) becomes available. */
	struct wlr_output *wlr_output = data;
	const ConfigMonitorRule *r;
	unsigned int i;
	int ji, jk;
	struct wlr_output_state state;
	Monitor *m;

	if (!wlr_output_init_render(wlr_output, alloc, drw))
		return;

	m = wlr_output->data = ecalloc(1, sizeof(*m));
	m->wlr_output = wlr_output;

	wl_list_init(&m->dwl_ipc_outputs);

	for (i = 0; i < LENGTH(m->layers); i++)
		wl_list_init(&m->layers[i]);

	wlr_output_state_init(&state);
	/* Initialize monitor state using configured rules */
	m->gappih = gappih;
	m->gappiv = gappiv;
	m->gappoh = gappoh;
	m->gappov = gappov;
	m->isoverview = 0;
	m->sel = NULL;
	m->is_in_hotarea = 0;
	m->tagset[0] = m->tagset[1] = 1;
	float scale = 1;
	m->mfact = default_mfact;
	m->nmaster = default_nmaster;
	enum wl_output_transform rr = WL_OUTPUT_TRANSFORM_NORMAL;
	wlr_output_state_set_scale(&state, scale);
	wlr_output_state_set_transform(&state, rr);

	m->lt = &layouts[0];
	for (ji = 0; ji < config.monitor_rules_count; ji++) {
		if (config.monitor_rules_count < 1)
			break;

		r = &config.monitor_rules[ji];
		if (!r->name || regex_match(r->name, wlr_output->name)) {
			m->mfact = r->mfact;
			m->nmaster = r->nmaster;
			m->m.x = r->x;
			m->m.y = r->y;
			if (r->layout) {
				for (jk = 0; jk < LENGTH(layouts); jk++) {
					if (strcmp(layouts[jk].name, r->layout) == 0) {
						m->lt = &layouts[jk];
					}
				}
			}
			scale = r->scale;
			rr = r->rr;
			wlr_output_state_set_scale(&state, r->scale);
			wlr_output_state_set_transform(&state, r->rr);
			break;
		}
	}

	/* The mode is a tuple of (width, height, refresh rate), and each
	 * monitor supports only a specific set of modes. We just pick the
	 * monitor's preferred mode; a more sophisticated compositor would let
	 * the user configure it. */
	wlr_output_state_set_mode(&state, wlr_output_preferred_mode(wlr_output));

	/* Set up event listeners */
	LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
	LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
	LISTEN(&wlr_output->events.request_state, &m->request_state,
		   requestmonstate);

	wlr_output_state_set_enabled(&state, 1);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	wl_list_insert(&mons, &m->link);
	m->pertag = calloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;
		m->pertag->smfacts[i] = default_smfact;
		m->pertag->ltidxs[i] = m->lt;
	}

	// apply tag rule
	for (i = 1; i <= config.tag_rules_count; i++) {
		for (jk = 0; jk < LENGTH(layouts); jk++) {
			if (config.tag_rules_count > 0 &&
				strcmp(layouts[jk].name, config.tag_rules[i - 1].layout_name) ==
					0) {
				m->pertag->ltidxs[config.tag_rules[i - 1].id] = &layouts[jk];
			}
		}
	}

	printstatus();

	/* The xdg-protocol specifies:
	 *
	 * If the fullscreened surface is not opaque, the compositor must make
	 * sure that other screen content not part of the same surface tree (made
	 * up of subsurfaces, popups or similarly coupled surfaces) are not
	 * visible below the fullscreened surface.
	 *
	 */

	/* Adds this to the output layout in the order it was configured.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	m->scene_output = wlr_scene_output_create(scene, wlr_output);
	if (m->m.x == -1 && m->m.y == -1)
		wlr_output_layout_add_auto(output_layout, wlr_output);
	else
		wlr_output_layout_add(output_layout, wlr_output, m->m.x, m->m.y);
}

void // fix for 0.5
createnotify(struct wl_listener *listener, void *data) {
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup,
	 * or when wlr_layer_shell receives a new popup from a layer.
	 * If you want to do something tricky with popups you should check if
	 * its parent is wlr_xdg_shell or wlr_layer_shell */
	struct wlr_xdg_toplevel *toplevel = data;
	Client *c = NULL;

	/* Allocate a Client for this surface */
	c = toplevel->base->data = ecalloc(1, sizeof(*c));
	c->surface.xdg = toplevel->base;
	c->bw = borderpx;

	LISTEN(&toplevel->base->surface->events.commit, &c->commit, commitnotify);
	LISTEN(&toplevel->base->surface->events.map, &c->map, mapnotify);
	LISTEN(&toplevel->base->surface->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&toplevel->events.destroy, &c->destroy, destroynotify);
	LISTEN(&toplevel->events.request_fullscreen, &c->fullscreen,
		   fullscreennotify);
	LISTEN(&toplevel->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&toplevel->events.request_minimize, &c->minimize, minimizenotify);
	LISTEN(&toplevel->events.set_title, &c->set_title, updatetitle);
}

void createpointer(struct wlr_pointer *pointer) {
	struct libinput_device *device;
	if (wlr_input_device_is_libinput(&pointer->base) &&
		(device = wlr_libinput_get_device_handle(&pointer->base))) {

		if (libinput_device_config_tap_get_finger_count(device) &&
			disable_trackpad) {
			return;
		}

		if (libinput_device_config_tap_get_finger_count(device)) {
			libinput_device_config_tap_set_enabled(device, tap_to_click);
			libinput_device_config_tap_set_drag_enabled(device, tap_and_drag);
			libinput_device_config_tap_set_drag_lock_enabled(device, drag_lock);
			libinput_device_config_tap_set_button_map(device, button_map);
			libinput_device_config_scroll_set_natural_scroll_enabled(
				device, trackpad_natural_scrolling);
		} else {
			libinput_device_config_scroll_set_natural_scroll_enabled(
				device, mouse_natural_scrolling);
		}

		if (libinput_device_config_dwt_is_available(device))
			libinput_device_config_dwt_set_enabled(device,
												   disable_while_typing);

		if (libinput_device_config_left_handed_is_available(device))
			libinput_device_config_left_handed_set(device, left_handed);

		if (libinput_device_config_middle_emulation_is_available(device))
			libinput_device_config_middle_emulation_set_enabled(
				device, middle_button_emulation);

		if (libinput_device_config_scroll_get_methods(device) !=
			LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
			libinput_device_config_scroll_set_method(device, scroll_method);
		if (libinput_device_config_scroll_get_methods(device) ==
			LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN)
			libinput_device_config_scroll_set_button(device, scroll_button);

		if (libinput_device_config_click_get_methods(device) !=
			LIBINPUT_CONFIG_CLICK_METHOD_NONE)
			libinput_device_config_click_set_method(device, click_method);

		if (libinput_device_config_send_events_get_modes(device))
			libinput_device_config_send_events_set_mode(device,
														send_events_mode);

		if (libinput_device_config_accel_is_available(device)) {
			libinput_device_config_accel_set_profile(device, accel_profile);
			libinput_device_config_accel_set_speed(device, accel_speed);
		}
	}

	wlr_cursor_attach_input_device(cursor, &pointer->base);
}

void createpointerconstraint(struct wl_listener *listener, void *data) {
	PointerConstraint *pointer_constraint =
		ecalloc(1, sizeof(*pointer_constraint));
	pointer_constraint->constraint = data;
	LISTEN(&pointer_constraint->constraint->events.destroy,
		   &pointer_constraint->destroy, destroypointerconstraint);
}

void createpopup(struct wl_listener *listener, void *data) {
	/* This event is raised when a client (either xdg-shell or layer-shell)
	 * creates a new popup. */
	struct wlr_xdg_popup *popup = data;
	LISTEN_STATIC(&popup->base->surface->events.commit, commitpopup);
}

void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint) {
	if (active_constraint == constraint)
		return;

	if (active_constraint)
		wlr_pointer_constraint_v1_send_deactivated(active_constraint);

	active_constraint = constraint;
	wlr_pointer_constraint_v1_send_activated(constraint);
}

void cursorframe(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(seat);
}

void cursorwarptohint(void) {
	Client *c = NULL;
	double sx = active_constraint->current.cursor_hint.x;
	double sy = active_constraint->current.cursor_hint.y;

	toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
	if (c && active_constraint->current.cursor_hint.enabled) {
		wlr_cursor_warp(cursor, NULL, sx + c->geom.x + c->bw,
						sy + c->geom.y + c->bw);
		wlr_seat_pointer_warp(active_constraint->seat, sx, sy);
	}
}

void defaultgaps(const Arg *arg) { setgaps(gappoh, gappov, gappih, gappiv); }

void destroydragicon(struct wl_listener *listener, void *data) {
	/* Focus enter isn't sent during drag, so refocus the focused node. */
	focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
	wl_list_remove(&listener->link);
	free(listener);
}

void destroyidleinhibitor(struct wl_listener *listener, void *data) {
	/* `data` is the wlr_surface of the idle inhibitor being destroyed,
	 * at this point the idle inhibitor is still in the list of the manager */
	checkidleinhibitor(wlr_surface_get_root_surface(data));
	wl_list_remove(&listener->link);
	free(listener);
}

void destroylayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, destroy);

	wl_list_remove(&l->link);
	wl_list_remove(&l->destroy.link);
	wl_list_remove(&l->map.link);
	wl_list_remove(&l->unmap.link);
	wl_list_remove(&l->surface_commit.link);
	wlr_scene_node_destroy(&l->scene->node);
	wlr_scene_node_destroy(&l->popups->node);
	free(l);
}

void destroylock(SessionLock *lock, int unlock) {
	wlr_seat_keyboard_notify_clear_focus(seat);
	if ((locked = !unlock))
		goto destroy;

	wlr_scene_node_set_enabled(&locked_bg->node, false);

	focusclient(focustop(selmon), 0);
	motionnotify(0, NULL, 0, 0, 0, 0);

destroy:
	wl_list_remove(&lock->new_surface.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->destroy.link);

	wlr_scene_node_destroy(&lock->scene->node);
	cur_lock = NULL;
	free(lock);
}

void destroylocksurface(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, destroy_lock_surface);
	struct wlr_session_lock_surface_v1 *surface,
		*lock_surface = m->lock_surface;

	m->lock_surface = NULL;
	wl_list_remove(&m->destroy_lock_surface.link);

	if (lock_surface->surface != seat->keyboard_state.focused_surface)
		return;

	if (locked && cur_lock && !wl_list_empty(&cur_lock->surfaces)) {
		surface = wl_container_of(cur_lock->surfaces.next, surface, link);
		client_notify_enter(surface->surface, wlr_seat_get_keyboard(seat));
	} else if (!locked) {
		focusclient(focustop(selmon), 1);
	} else {
		wlr_seat_keyboard_clear_focus(seat);
	}
}

void // 0.7 custom
destroynotify(struct wl_listener *listener, void *data) {
	/* Called when the xdg_toplevel is destroyed. */
	Client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->set_title.link);
	wl_list_remove(&c->fullscreen.link);
	wl_list_remove(&c->maximize.link);
	wl_list_remove(&c->minimize.link);
#ifdef XWAYLAND
	if (c->type != XDGShell) {
		wl_list_remove(&c->activate.link);
		wl_list_remove(&c->associate.link);
		wl_list_remove(&c->configure.link);
		wl_list_remove(&c->dissociate.link);
		wl_list_remove(&c->set_hints.link);
	} else
#endif
	{
		wl_list_remove(&c->commit.link);
		wl_list_remove(&c->map.link);
		wl_list_remove(&c->unmap.link);
	}
	free(c);
}

void destroypointerconstraint(struct wl_listener *listener, void *data) {
	PointerConstraint *pointer_constraint =
		wl_container_of(listener, pointer_constraint, destroy);

	if (active_constraint == pointer_constraint->constraint) {
		cursorwarptohint();
		active_constraint = NULL;
	}

	wl_list_remove(&pointer_constraint->destroy.link);
	free(pointer_constraint);
}

void destroysessionlock(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, destroy);
	destroylock(lock, 0);
}

void destroykeyboardgroup(struct wl_listener *listener, void *data) {
	KeyboardGroup *group = wl_container_of(listener, group, destroy);
	wl_event_source_remove(group->key_repeat_source);
	wl_list_remove(&group->key.link);
	wl_list_remove(&group->modifiers.link);
	wl_list_remove(&group->destroy.link);
	wlr_keyboard_group_destroy(group->wlr_group);
	free(group);
}

Monitor *dirtomon(enum wlr_direction dir) {
	struct wlr_output *next;
	if (!wlr_output_layout_get(output_layout, selmon->wlr_output))
		return selmon;
	if ((next = wlr_output_layout_adjacent_output(output_layout, 1 << dir,
												  selmon->wlr_output,
												  selmon->m.x, selmon->m.y)))
		return next->data;
	if ((next = wlr_output_layout_farthest_output(
			 output_layout,
			 dir ^ (WLR_DIRECTION_LEFT | WLR_DIRECTION_RIGHT |
					WLR_DIRECTION_UP | WLR_DIRECTION_DOWN),
			 selmon->wlr_output, selmon->m.x, selmon->m.y)))
		return next->data;
	return selmon;
}

void focusclient(Client *c, int lift) {
	struct wlr_surface *old_keyboard_focus_surface =
		seat->keyboard_state.focused_surface;

	if (locked)
		return;

	if (c && c->iskilling)
		return;

	if (c && !client_surface(c)->mapped)
		return;

	if (c && c->animation.tagouting && !c->animation.tagouting)
		return;

	if (c && client_should_ignore_focus(c)) {
		return;
	}

	/* Raise client in stacking order if requested */
	if (c && lift)
		wlr_scene_node_raise_to_top(&c->scene->node); // 将视图提升到顶层

	if (c && client_surface(c) == old_keyboard_focus_surface && selmon &&
		selmon->sel)
		return;

	if (selmon && selmon->sel && selmon->sel != c &&
		selmon->sel->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_set_activated(
			selmon->sel->foreign_toplevel, false);
	}

	if (c && !c->iskilling && !client_is_unmanaged(c) && c->mon) {

		selmon = c->mon;
		selmon->prevsel = selmon->sel;
		selmon->sel = c;

		// decide whether need to re-arrange
		if (c && selmon->prevsel && !selmon->prevsel->isfloating &&
			(selmon->prevsel->tags & selmon->tagset[selmon->seltags]) &&
			(c->tags & selmon->tagset[selmon->seltags]) && !c->isfloating &&
			!c->isfullscreen && is_scroller_layout(selmon)) {
			arrange(selmon, false);
		} else if (selmon->prevsel) {
			selmon->prevsel = NULL;
		}

		// change focus link position
		wl_list_remove(&c->flink);
		wl_list_insert(&fstack, &c->flink);

		// change border color
		c->isurgent = 0;
		setborder_color(c);
	}

	if (c && !c->iskilling && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);

	/* Deactivate old client if focus is changing */
	if (old_keyboard_focus_surface &&
		(!c || client_surface(c) != old_keyboard_focus_surface)) {
		/* If an exclusive_focus layer is focused, don't focus or activate the
		 * client, but only update its position in fstack to render its border
		 * with focuscolor and focus it after the exclusive_focus layer is
		 * closed. */
		Client *w = NULL;
		LayerSurface *l = NULL;
		int type =
			toplevel_from_wlr_surface(old_keyboard_focus_surface, &w, &l);
		if (type == LayerShell && l->scene->node.enabled &&
			l->layer_surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP &&
			l == exclusive_focus) {
			return;
		} else if (w && w == exclusive_focus && client_wants_focus(w)) {
			return;
			/* Don't deactivate old_keyboard_focus_surface client if the new one
			 * wants focus, as this causes issues with winecfg and probably
			 * other clients */
		} else if (w && !client_is_unmanaged(w) &&
				   (!c || !client_wants_focus(c))) {
			setborder_color(w);

			client_activate_surface(old_keyboard_focus_surface, 0);
		}
	}
	printstatus();

	if (!c) {
		/* With no client, all we have left is to clear focus */
		if (selmon && selmon->sel)
			selmon->sel =
				NULL; // 这个很关键,因为很多地方用到当前窗口做计算,不重置成NULL就会到处有野指针

		// clear text input focus state
		dwl_im_relay_set_focus(dwl_input_method_relay, NULL);
		wlr_seat_keyboard_notify_clear_focus(seat);
		return;
	}

	/* Change cursor surface */
	motionnotify(0, NULL, 0, 0, 0, 0);

	/* Have a client, so focus its top-level wlr_surface */
	client_notify_enter(client_surface(c), wlr_seat_get_keyboard(seat));

	// set text input focus
	dwl_im_relay_set_focus(dwl_input_method_relay, client_surface(c));
	/* Activate the new client */
	client_activate_surface(client_surface(c), 1);
}

void focusmon(const Arg *arg) {
	Client *c;
	Monitor *m = NULL;
	int i = 0, nmons = wl_list_length(&mons);
	if (nmons && arg->i != UNDIR) {
		do /* don't switch to disabled mons */
			selmon = dirtomon(arg->i);
		while (!selmon->wlr_output->enabled && i++ < nmons);
	} else if (arg->v) {
		wl_list_for_each(m, &mons, link) {
			if (!m->wlr_output->enabled) {
				continue;
			}
			if (regex_match(arg->v, m->wlr_output->name)) {
				selmon = m;
				break;
			}
		}
	} else {
		return;
	}
	warp_cursor_to_selmon(selmon);
	c = focustop(selmon);
	if (!c) {
		selmon->sel = NULL;
		wlr_seat_pointer_notify_clear_focus(seat);
		wlr_seat_keyboard_notify_clear_focus(seat);
	} else
		focusclient(c, 1);
}

void // 17
focusstack(const Arg *arg) {
	/* Focus the next or previous client (in tiling order) on selmon */
	Client *c, *sel = focustop(selmon);
	if (!sel || sel->isfullscreen)
		return;
	if (arg->i > 0) {
		wl_list_for_each(c, &sel->link, link) {
			if (&c->link == &clients || c->isunglobal)
				continue; /* wrap past the sentinel node */
			if (VISIBLEON(c, selmon))
				break; /* found it */
		}
	} else {
		wl_list_for_each_reverse(c, &sel->link, link) {
			if (&c->link == &clients)
				continue; /* wrap past the sentinel node */
			if (VISIBLEON(c, selmon) || c->isunglobal)
				break; /* found it */
		}
	}
	/* If only one client is visible on selmon, then c == sel */
	focusclient(c, 1);
	if (warpcursor)
		warp_cursor(c);
}

/* We probably should change the name of this, it sounds like
 * will focus the topmost client of this mon, when actually will
 * only return that client */
Client * // 0.5
focustop(Monitor *m) {
	Client *c;
	wl_list_for_each(c, &fstack, flink) {
		if (c->iskilling || c->isunglobal)
			continue;
		if (VISIBLEON(c, m))
			return c;
	}
	return NULL;
}

void // 0.6
fullscreennotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, fullscreen);

	if (!c || c->iskilling)
		return;

	setfullscreen(c, client_wants_fullscreen(c));
}

void incnmaster(const Arg *arg) {
	if (!arg || !selmon)
		return;
	selmon->pertag->nmasters[selmon->pertag->curtag] =
		MAX(selmon->pertag->nmasters[selmon->pertag->curtag] + arg->i, 0);
	arrange(selmon, false);
}

void incgaps(const Arg *arg) {
	setgaps(selmon->gappoh + arg->i, selmon->gappov + arg->i,
			selmon->gappih + arg->i, selmon->gappiv + arg->i);
}

void incigaps(const Arg *arg) {
	setgaps(selmon->gappoh, selmon->gappov, selmon->gappih + arg->i,
			selmon->gappiv + arg->i);
}

void incogaps(const Arg *arg) {
	setgaps(selmon->gappoh + arg->i, selmon->gappov + arg->i, selmon->gappih,
			selmon->gappiv);
}

void incihgaps(const Arg *arg) {
	setgaps(selmon->gappoh, selmon->gappov, selmon->gappih + arg->i,
			selmon->gappiv);
}

void incivgaps(const Arg *arg) {
	setgaps(selmon->gappoh, selmon->gappov, selmon->gappih,
			selmon->gappiv + arg->i);
}

void incohgaps(const Arg *arg) {
	setgaps(selmon->gappoh + arg->i, selmon->gappov, selmon->gappih,
			selmon->gappiv);
}

void incovgaps(const Arg *arg) {
	setgaps(selmon->gappoh, selmon->gappov + arg->i, selmon->gappih,
			selmon->gappiv);
}

void requestmonstate(struct wl_listener *listener, void *data) {
	struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(event->output, event->state);
	updatemons(NULL, NULL);
}

void inputdevice(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct wlr_input_device *device = data;
	unsigned int caps;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		createkeyboard(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_POINTER:
		createpointer(wlr_pointer_from_input_device(device));
		break;
	default:
		/* TODO handle other input device types */
		break;
	}

	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In dwl we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	/* TODO do we actually require a cursor? */
	caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&kb_group->wlr_group->devices))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(seat, caps);
}

int keyrepeat(void *data) {
	KeyboardGroup *group = data;
	int i;
	if (!group->nsyms || group->wlr_group->keyboard.repeat_info.rate <= 0)
		return 0;

	wl_event_source_timer_update(
		group->key_repeat_source,
		1000 / group->wlr_group->keyboard.repeat_info.rate);

	for (i = 0; i < group->nsyms; i++)
		keybinding(group->mods, group->keysyms[i], group->keycode);

	return 0;
}

int // 17
keybinding(unsigned int mods, xkb_keysym_t sym, unsigned int keycode) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 */
	int handled = 0;
	const KeyBinding *k;
	int ji;
	for (ji = 0; ji < config.key_bindings_count; ji++) {
		if (config.key_bindings_count < 1)
			break;
		k = &config.key_bindings[ji];
		if (CLEANMASK(mods) == CLEANMASK(k->mod) &&
			((k->keysymcode.type == KEY_TYPE_SYM &&
			  normalize_keysym(sym) ==
				  normalize_keysym(k->keysymcode.keysym)) ||
			 (k->keysymcode.type == KEY_TYPE_CODE &&
			  keycode == k->keysymcode.keycode)) &&
			k->func) {
			k->func(&k->arg);
			handled = 1;
		}
	}
	return handled;
}

bool keypressglobal(struct wlr_surface *last_surface,
					struct wlr_keyboard *keyboard,
					struct wlr_keyboard_key_event *event, unsigned int mods,
					xkb_keysym_t keysym, unsigned int keycode) {
	Client *c = NULL, *lastc = focustop(selmon);
	unsigned int keycodes[32] = {0};
	int reset = false;
	const char *appid = NULL;
	const char *title = NULL;
	int ji;
	const ConfigWinRule *r;

	for (ji = 0; ji < config.window_rules_count; ji++) {
		if (config.window_rules_count < 1)
			break;
		r = &config.window_rules[ji];

		if (!r->globalkeybinding.mod ||
			(!r->globalkeybinding.keysymcode.keysym &&
			 !r->globalkeybinding.keysymcode.keycode))
			continue;

		/* match key only (case insensitive) ignoring mods */
		if (((r->globalkeybinding.keysymcode.type == KEY_TYPE_SYM &&
			  r->globalkeybinding.keysymcode.keysym == keysym) ||
			 (r->globalkeybinding.keysymcode.type == KEY_TYPE_CODE &&
			  r->globalkeybinding.keysymcode.keycode == keycode)) &&
			r->globalkeybinding.mod == mods) {
			wl_list_for_each(c, &clients, link) {
				if (c && c != lastc) {
					appid = client_get_appid(c);
					title = client_get_title(c);

					if ((r->title && regex_match(r->title, title) && !r->id) ||
						(r->id && regex_match(r->id, appid) && !r->title) ||
						(r->id && regex_match(r->id, appid) && r->title &&
						 regex_match(r->title, title))) {
						reset = true;
						wlr_seat_keyboard_enter(seat, client_surface(c),
												keycodes, 0,
												&keyboard->modifiers);
						wlr_seat_keyboard_send_key(seat, event->time_msec,
												   event->keycode,
												   event->state);
						goto done;
					}
				}
			}
		}
	}

done:
	if (reset)
		wlr_seat_keyboard_enter(seat, last_surface, keycodes, 0,
								&keyboard->modifiers);
	return reset;
}

void keypress(struct wl_listener *listener, void *data) {
	int i;
	/* This event is raised when a key is pressed or released. */
	KeyboardGroup *group = wl_container_of(listener, group, key);
	struct wlr_keyboard_key_event *event = data;

	struct wlr_surface *last_surface = seat->keyboard_state.focused_surface;
	struct wlr_xdg_surface *xdg_surface =
		last_surface ? wlr_xdg_surface_try_from_wlr_surface(last_surface)
					 : NULL;
	int pass = 0;
	bool hit_global = false;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface =
		last_surface ? wlr_xwayland_surface_try_from_wlr_surface(last_surface)
					 : NULL;
#endif

	/* Translate libinput keycode -> xkbcommon */
	unsigned int keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(group->wlr_group->keyboard.xkb_state,
									   keycode, &syms);

	int handled = 0;
	unsigned int mods = wlr_keyboard_get_modifiers(&group->wlr_group->keyboard);

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	// ov tab mode detect moe key release
	if (ov_tab_mode && !locked &&
		event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
		(keycode == 133 || keycode == 37 || keycode == 64 || keycode == 50 ||
		 keycode == 134 || keycode == 105 || keycode == 108 || keycode == 62) &&
		selmon && selmon->sel) {
		if (selmon->isoverview && selmon->sel) {
			toggleoverview(&(Arg){.i = -1});
		}
	}

	/* On _press_ if there is no active screen locker,
	 * attempt to process a compositor keybinding. */
	if (!locked && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (i = 0; i < nsyms; i++)
			handled = keybinding(mods, syms[i], keycode) || handled;
	}

	if (handled && group->wlr_group->keyboard.repeat_info.delay > 0) {
		group->mods = mods;
		group->keysyms = syms;
		group->keycode = keycode;
		group->nsyms = nsyms;
		wl_event_source_timer_update(
			group->key_repeat_source,
			group->wlr_group->keyboard.repeat_info.delay);
	} else {
		group->nsyms = 0;
		wl_event_source_timer_update(group->key_repeat_source, 0);
	}

	if (handled)
		return;

	/* don't pass when popup is focused
	 * this is better than having popups (like fuzzel or wmenu) closing while
	 * typing in a passed keybind */
	pass = (xdg_surface && xdg_surface->role != WLR_XDG_SURFACE_ROLE_POPUP) ||
		   !last_surface
#ifdef XWAYLAND
		   || xsurface
#endif
		;
	/* passed keys don't get repeated */
	if (pass && syms)
		hit_global = keypressglobal(last_surface, &group->wlr_group->keyboard,
									event, mods, syms[0], keycode);

	if (hit_global) {
		return;
	}
	if (!dwl_im_keyboard_grab_forward_key(group, event)) {
		wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
		/* Pass unhandled keycodes along to the client. */
		wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
									 event->state);
	}
}

void keypressmod(struct wl_listener *listener, void *data) {
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	KeyboardGroup *group = wl_container_of(listener, group, modifiers);

	if (!dwl_im_keyboard_grab_forward_modifiers(group)) {
		wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
		/* Send modifiers to the client. */
		wlr_seat_keyboard_notify_modifiers(
			seat, &group->wlr_group->keyboard.modifiers);
	}
}

void pending_kill_client(Client *c) {
	// c->iskilling = 1; //不可以提前标记已经杀掉，因为有些客户端可能拒绝
	client_send_close(c);
}

void focuslast(const Arg *arg) {

	Client *c, *prev = NULL;
	wl_list_for_each(c, &fstack, flink) {
		if (c->iskilling || c->isminied || c->isunglobal)
			continue;
		if (c)
			break;
	}

	struct wl_list *prev_node = c->flink.next;
	prev = wl_container_of(prev_node, prev, flink);

	unsigned int target;
	if (prev) {
		if (prev->mon != selmon) {
			selmon = prev->mon;
			warp_cursor_to_selmon(selmon);
		}
		target = get_tags_first_tag(prev->tags);
		view(&(Arg){.ui = target}, true);
		focusclient(prev, 1);
	}
}

void killclient(const Arg *arg) {
	Client *c;
	c = selmon->sel;
	if (c) {
		pending_kill_client(c);
	}
}

void locksession(struct wl_listener *listener, void *data) {
	struct wlr_session_lock_v1 *session_lock = data;
	SessionLock *lock;
	wlr_scene_node_set_enabled(&locked_bg->node, true);
	if (cur_lock) {
		wlr_session_lock_v1_destroy(session_lock);
		return;
	}
	lock = session_lock->data = ecalloc(1, sizeof(*lock));
	focusclient(NULL, 0);

	lock->scene = wlr_scene_tree_create(layers[LyrBlock]);
	cur_lock = lock->lock = session_lock;
	locked = 1;

	LISTEN(&session_lock->events.new_surface, &lock->new_surface,
		   createlocksurface);
	LISTEN(&session_lock->events.destroy, &lock->destroy, destroysessionlock);
	LISTEN(&session_lock->events.unlock, &lock->unlock, unlocksession);

	wlr_session_lock_v1_send_locked(session_lock);
}

void init_client_properties(Client *c) {
	c->ismaxmizescreen = 0;
	c->isfullscreen = 0;
	c->need_float_size_reduce = 0;
	c->iskilling = 0;
	c->isglobal = 0;
	c->isminied = 0;
	c->isoverlay = 0;
	c->isunglobal = 0;
	c->is_in_scratchpad = 0;
	c->isnamedscratchpad = 0;
	c->is_scratchpad_show = 0;
	c->need_float_size_reduce = 0;
	c->is_clip_to_hide = 0;
	c->is_restoring_from_ov = 0;
	c->isurgent = 0;
	c->need_output_flush = 0;
	c->scroller_proportion = scroller_default_proportion;
	c->is_open_animation = true;
	c->drag_to_tile = false;
	c->fake_no_border = false;
	c->focused_opacity = focused_opacity;
	c->unfocused_opacity = unfocused_opacity;
	c->nofadein = 0;
	c->nofadeout = 0;
	c->no_force_center = 0;
	c->scratchpad_width = 0;
	c->scratchpad_height = 0;
}

void // old fix to 0.5
mapnotify(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	Client *p = NULL;
	Client *c = wl_container_of(listener, c, map);
	int i;
	/* Create scene tree for this client and its border */
	c->scene = client_surface(c)->data = wlr_scene_tree_create(layers[LyrTile]);
	wlr_scene_node_set_enabled(&c->scene->node, c->type != XDGShell);
	c->scene_surface =
		c->type == XDGShell
			? wlr_scene_xdg_surface_create(c->scene, c->surface.xdg)
			: wlr_scene_subsurface_tree_create(c->scene, client_surface(c));
	c->scene->node.data = c->scene_surface->node.data = c;

	client_get_geometry(c, &c->geom);

	init_client_properties(c);

	// set special window properties
	if (client_is_unmanaged(c) || client_should_ignore_focus(c)) {
		c->bw = 0;
		c->isnoborder = 1;
	} else {
		c->bw = borderpx;
	}

	if (client_should_global(c)) {
		c->isunglobal = 1;
	}

	// init client geom
	c->geom.width += 2 * c->bw;
	c->geom.height += 2 * c->bw;

	/* Handle unmanaged clients first so we can return prior create borders */
	if (client_is_unmanaged(c)) {
		/* Unmanaged clients always are floating */
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFSorOverTop]);
		wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
		if (client_wants_focus(c)) {
			focusclient(c, 1);
			exclusive_focus = c;
		}
#ifdef XWAYLAND
		if (client_is_x11(c)) {
			LISTEN(&c->surface.xwayland->events.set_geometry, &c->set_geometry,
				   setgeometrynotify);
		}
#endif
		return;
	}

	for (i = 0; i < 4; i++) {
		c->border[i] = wlr_scene_rect_create(
			c->scene, 0, 0, c->isurgent ? urgentcolor : bordercolor);
		c->border[i]->node.data = c;
	}

	/* Initialize client geometry with room for border */
	client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
							WLR_EDGE_RIGHT);

	if (new_is_master && selmon && !is_scroller_layout(selmon))
		// tile at the top
		wl_list_insert(&clients, &c->link); // 新窗口是master,头部入栈
	else if (selmon && is_scroller_layout(selmon) && center_select(selmon)) {
		Client *at_client = center_select(selmon);
		at_client->link.next->prev = &c->link;
		c->link.prev = &at_client->link;
		c->link.next = at_client->link.next;
		at_client->link.next = &c->link;
	} else
		wl_list_insert(clients.prev, &c->link); // 尾部入栈
	wl_list_insert(&fstack, &c->flink);

	/* Set initial monitor, tags, floating status, and focus:
	 * we always consider floating, clients that have parent and thus
	 * we set the same tags and monitor than its parent, if not
	 * try to apply rules for them */
	if ((p = client_get_parent(c))) {
		c->isfloating = 1;
		setmon(c, p->mon, p->tags, true);
	} else {
		applyrules(c);
	}

	// make sure the animation is open type
	c->is_open_animation = true;
	resize(c, c->geom, 0);
	printstatus();
}

void // 0.5 custom
maximizenotify(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on
	 * client-side decorations. dwl doesn't support maximization, but
	 * to conform to xdg-shell protocol we still must send a configure.
	 * Since xdg-shell protocol v5 we should ignore request of unsupported
	 * capabilities, just schedule a empty configure when the client uses <5
	 * protocol version
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply. */
	// Client *c = wl_container_of(listener, c, maximize);
	// if (wl_resource_get_version(c->surface.xdg->toplevel->resource)
	// 		< XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
	// 	wlr_xdg_surface_schedule_configure(c->surface.xdg);
	// togglemaxmizescreen(&(Arg){0});
	Client *c = wl_container_of(listener, c, maximize);

	if (!c || !c->mon || c->iskilling)
		return;

	if (c->ismaxmizescreen || c->isfullscreen)
		setmaxmizescreen(c, 0);
	else
		setmaxmizescreen(c, 1);
}

void set_minized(Client *c) {

	if (!c || !c->mon)
		return;

	c->isglobal = 0;
	c->oldtags = c->mon->tagset[c->mon->seltags];
	c->mini_restore_tag = c->tags;
	c->tags = 0;
	c->isminied = 1;
	c->is_in_scratchpad = 1;
	c->is_scratchpad_show = 0;
	focusclient(focustop(selmon), 1);
	arrange(c->mon, false);
	wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, false);
	wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel, true);
	wl_list_remove(&c->link);				// 从原来位置移除
	wl_list_insert(clients.prev, &c->link); // 插入尾部
}

void // 0.5 custom
minimizenotify(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on
	 * client-side decorations. dwl doesn't support maximization, but
	 * to conform to xdg-shell protocol we still must send a configure.
	 * Since xdg-shell protocol v5 we should ignore request of unsupported
	 * capabilities, just schedule a empty configure when the client uses <5
	 * protocol version
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply. */
	// Client *c = wl_container_of(listener, c, maximize);
	// if (wl_resource_get_version(c->surface.xdg->toplevel->resource)
	// 		< XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
	// 	wlr_xdg_surface_schedule_configure(c->surface.xdg);
	// togglemaxmizescreen(&(Arg){0});
	Client *c = wl_container_of(listener, c, minimize);

	if (!c || !c->mon || c->iskilling || c->isminied)
		return;

	set_minized(c);
}

void motionabsolute(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an _absolute_
	 * motion event, from 0..1 on each axis. This happens, for example, when
	 * wlroots is running under a Wayland window rather than KMS+DRM, and you
	 * move the mouse over the window. You could enter the window from any edge,
	 * so we have to warp the mouse there. There is also some hardware which
	 * emits these events. */
	struct wlr_pointer_motion_absolute_event *event = data;
	double lx, ly, dx, dy;

	if (!event->time_msec) /* this is 0 with virtual pointers */
		wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x,
								 event->y);

	wlr_cursor_absolute_to_layout_coords(cursor, &event->pointer->base,
										 event->x, event->y, &lx, &ly);
	dx = lx - cursor->x;
	dy = ly - cursor->y;
	motionnotify(event->time_msec, &event->pointer->base, dx, dy, dx, dy);
}

void motionnotify(unsigned int time, struct wlr_input_device *device, double dx,
				  double dy, double dx_unaccel, double dy_unaccel) {
	double sx = 0, sy = 0, sx_confined, sy_confined;
	Client *c = NULL, *w = NULL;
	LayerSurface *l = NULL;
	struct wlr_surface *surface = NULL;
	struct wlr_pointer_constraint_v1 *constraint;
	bool should_lock = false;

	/* Find the client under the pointer and send the event along. */
	xytonode(cursor->x, cursor->y, &surface, &c, NULL, &sx, &sy);

	if (cursor_mode == CurPressed && !seat->drag &&
		surface != seat->pointer_state.focused_surface &&
		toplevel_from_wlr_surface(seat->pointer_state.focused_surface, &w,
								  &l) >= 0) {
		c = w;
		surface = seat->pointer_state.focused_surface;
		sx = cursor->x - (l ? l->scene->node.x : w->geom.x);
		sy = cursor->y - (l ? l->scene->node.y : w->geom.y);
	}

	/* time is 0 in internal calls meant to restore pointer focus. */
	if (time) {
		wlr_relative_pointer_manager_v1_send_relative_motion(
			relative_pointer_mgr, seat, (uint64_t)time * 1000, dx, dy,
			dx_unaccel, dy_unaccel);

		wl_list_for_each(constraint, &pointer_constraints->constraints, link)
			cursorconstrain(constraint);

		if (active_constraint && cursor_mode != CurResize &&
			cursor_mode != CurMove) {
			toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
			if (c && active_constraint->surface ==
						 seat->pointer_state.focused_surface) {
				sx = cursor->x - c->geom.x - c->bw;
				sy = cursor->y - c->geom.y - c->bw;
				if (wlr_region_confine(&active_constraint->region, sx, sy,
									   sx + dx, sy + dy, &sx_confined,
									   &sy_confined)) {
					dx = sx_confined - sx;
					dy = sy_confined - sy;
				}

				if (active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
					return;
			}
		}

		wlr_cursor_move(cursor, device, dx, dy);
		handlecursoractivity();
		wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

		/* Update selmon (even while dragging a window) */
		if (sloppyfocus)
			selmon = xytomon(cursor->x, cursor->y);
	}

	/* Update drag icon's position */
	wlr_scene_node_set_position(&drag_icon->node, (int)round(cursor->x),
								(int)round(cursor->y));

	/* If we are currently grabbing the mouse, handle and return */
	if (cursor_mode == CurMove) {
		/* Move the grabbed client to the new position. */
		grabc->oldgeom = (struct wlr_box){.x = (int)round(cursor->x) - grabcx,
										  .y = (int)round(cursor->y) - grabcy,
										  .width = grabc->geom.width,
										  .height = grabc->geom.height};
		resize(grabc, grabc->oldgeom, 1);
		return;
	} else if (cursor_mode == CurResize) {
		grabc->oldgeom =
			(struct wlr_box){.x = grabc->geom.x,
							 .y = grabc->geom.y,
							 .width = (int)round(cursor->x) - grabc->geom.x,
							 .height = (int)round(cursor->y) - grabc->geom.y};
		resize(grabc, grabc->oldgeom, 1);
		return;
	}

	/* If there's no client surface under the cursor, set the cursor image to a
	 * default. This is what makes the cursor image appear when you move it
	 * off of a client or over its border. */
	if (!surface && !seat->drag && !cursor_hidden)
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

	if (c && c->mon && !c->animation.running &&
		(!(c->geom.x + c->geom.width > c->mon->m.x + c->mon->m.width ||
		   c->geom.x < c->mon->m.x ||
		   c->geom.y + c->geom.height > c->mon->m.y + c->mon->m.height ||
		   c->geom.y < c->mon->m.y) ||
		 !ISTILED(c))) {
		scroller_focus_lock = 0;
	}

	should_lock = false;
	if (!scroller_focus_lock ||
		!(c && c->mon &&
		  (c->geom.x + c->geom.width > c->mon->m.x + c->mon->m.width ||
		   c->geom.x < c->mon->m.x ||
		   c->geom.y + c->geom.height > c->mon->m.y + c->mon->m.height ||
		   c->geom.y < c->mon->m.y))) {
		if (c && c->mon && is_scroller_layout(c->mon) &&
			(c->geom.x + c->geom.width > c->mon->m.x + c->mon->m.width ||
			 c->geom.x < c->mon->m.x ||
			 c->geom.y + c->geom.height > c->mon->m.y + c->mon->m.height ||
			 c->geom.y < c->mon->m.y)) {
			should_lock = true;
		}
		pointerfocus(c, surface, sx, sy, time);

		if (should_lock && c && c->mon && ISTILED(c) && c == c->mon->sel) {
			scroller_focus_lock = 1;
		}
	}
}

void motionrelative(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor automatically
	 * handles constraining the motion to the output layout, as well as any
	 * special configuration applied for the specific input device which
	 * generated the event. You can pass NULL for the device if you want to move
	 * the cursor around without any input. */
	motionnotify(event->time_msec, &event->pointer->base, event->delta_x,
				 event->delta_y, event->unaccel_dx, event->unaccel_dy);
	toggle_hotarea(cursor->x, cursor->y);
}

void // 17
moveresize(const Arg *arg) {
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	xytonode(cursor->x, cursor->y, NULL, &grabc, NULL, NULL, NULL);
	if (!grabc || client_is_unmanaged(grabc) || grabc->isfullscreen)
		return;

	/* Float the window and tell motionnotify to grab it */
	if (grabc->isfloating == 0) {
		grabc->drag_to_tile = true;
		setfloating(grabc, 1);
	}

	switch (cursor_mode = arg->ui) {
	case CurMove:
		grabcx = cursor->x - grabc->geom.x;
		grabcy = cursor->y - grabc->geom.y;
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "all-scroll");
		break;
	case CurResize:
		/* Doesn't work for X11 output - the next absolute motion event
		 * returns the cursor to where it started */
		wlr_cursor_warp_closest(cursor, NULL, grabc->geom.x + grabc->geom.width,
								grabc->geom.y + grabc->geom.height);
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "bottom_right_corner");
		break;
	}
}

void outputmgrapply(struct wl_listener *listener, void *data) {
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 0);
}

void // 0.7 custom
outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test) {
	/*
	 * Called when a client such as wlr-randr requests a change in output
	 * configuration. This is only one way that the layout can be changed,
	 * so any Monitor information should be updated by updatemons() after an
	 * output_layout.change event, not here.
	 */
	struct wlr_output_configuration_head_v1 *config_head;
	int ok = 1;

	wl_list_for_each(config_head, &config->heads, link) {
		struct wlr_output *wlr_output = config_head->state.output;
		Monitor *m = wlr_output->data;
		struct wlr_output_state state;

		/* Ensure displays previously disabled by wlr-output-power-management-v1
		 * are properly handled*/
		m->asleep = 0;

		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state, config_head->state.enabled);
		if (!config_head->state.enabled)
			goto apply_or_test;

		if (config_head->state.mode)
			wlr_output_state_set_mode(&state, config_head->state.mode);
		else
			wlr_output_state_set_custom_mode(
				&state, config_head->state.custom_mode.width,
				config_head->state.custom_mode.height,
				config_head->state.custom_mode.refresh);

		wlr_output_state_set_transform(&state, config_head->state.transform);
		wlr_output_state_set_scale(&state, config_head->state.scale);
		wlr_output_state_set_adaptive_sync_enabled(
			&state, config_head->state.adaptive_sync_enabled);

	apply_or_test:
		ok &= test ? wlr_output_test_state(wlr_output, &state)
				   : wlr_output_commit_state(wlr_output, &state);

		/* Don't move monitors if position wouldn't change, this to avoid
		 * wlroots marking the output as manually configured.
		 * wlr_output_layout_add does not like disabled outputs */
		if (!test && wlr_output->enabled &&
			(m->m.x != config_head->state.x || m->m.y != config_head->state.y))
			wlr_output_layout_add(output_layout, wlr_output,
								  config_head->state.x, config_head->state.y);

		wlr_output_state_finish(&state);
	}

	if (ok)
		wlr_output_configuration_v1_send_succeeded(config);
	else
		wlr_output_configuration_v1_send_failed(config);
	wlr_output_configuration_v1_destroy(config);

	/* https://codeberg.org/dwl/dwl/issues/577 */
	updatemons(NULL, NULL);
}

void outputmgrtest(struct wl_listener *listener, void *data) {
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 1);
}

void pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
				  unsigned int time) {
	struct timespec now;

	if (surface != seat->pointer_state.focused_surface && sloppyfocus && time &&
		c && !client_is_unmanaged(c))
		focusclient(c, 0);

	/* If surface is NULL, clear pointer focus */
	if (!surface) {
		wlr_seat_pointer_notify_clear_focus(seat);
		return;
	}

	if (!time) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		time = now.tv_sec * 1000 + now.tv_nsec / 1000000;
	}

	/* Let the client know that the mouse cursor has entered one
	 * of its surfaces, and make keyboard focus follow if desired.
	 * wlroots makes this a no-op if surface is already focused */
	wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
	wlr_seat_pointer_notify_motion(seat, time, sx, sy);
}

void // 17
printstatus(void) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		dwl_ipc_output_printstatus(m); // 更新waybar上tag的状态 这里很关键
	}
}

void powermgrsetmode(struct wl_listener *listener, void *data) {
	struct wlr_output_power_v1_set_mode_event *event = data;
	struct wlr_output_state state = {0};
	Monitor *m = event->output->data;

	if (!m)
		return;

	m->gamma_lut_changed = 1; /* Reapply gamma LUT when re-enabling the ouput */
	wlr_output_state_set_enabled(&state, event->mode);
	wlr_output_commit_state(m->wlr_output, &state);

	m->asleep = !event->mode;
	updatemons(NULL, NULL);
}

void // 0.5 custom
quit(const Arg *arg) {
	wl_display_terminate(dpy);
}

void quitsignal(int signo) { quit(NULL); }

void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int sx, int sy,
								void *data) {
	wlr_scene_buffer_set_opacity(buffer, *(double *)data);
}

void client_set_opacity(Client *c, double opacity) {
	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   scene_buffer_apply_opacity, &opacity);
}

void rendermon(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, frame);
	Client *c, *tmp;
	struct wlr_output_state pending = {0};
	LayerSurface *l, *tmpl;
	int i;
	struct wl_list *layer_list;

	struct timespec now;
	bool need_more_frames = false;

	for (i = 0; i < LENGTH(m->layers); i++) {
		layer_list = &m->layers[i];
		// Draw frames for all layer
		wl_list_for_each_safe(l, tmpl, layer_list, link) {
			need_more_frames = layer_draw_frame(l) || need_more_frames;
		}
	}

	// Draw frames for all clients
	wl_list_for_each(c, &clients, link) {
		need_more_frames = client_draw_frame(c) || need_more_frames;
	}

	wl_list_for_each_safe(c, tmp, &fadeout_clients, fadeout_link) {
		need_more_frames = client_draw_fadeout_frame(c) || need_more_frames;
	}

	wl_list_for_each_safe(l, tmpl, &fadeout_layers, fadeout_link) {
		need_more_frames = layer_draw_fadeout_frame(l) || need_more_frames;
	}

	wlr_scene_output_commit(m->scene_output, NULL);

	// Send frame done notification
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(m->scene_output, &now);

	// // Clean up pending state
	wlr_output_state_finish(&pending);

	if (need_more_frames) {
		wlr_output_schedule_frame(m->wlr_output);
	}
}

void requestdecorationmode(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_decoration_mode);
	if (c->surface.xdg->initialized)
		wlr_xdg_toplevel_decoration_v1_set_mode(
			c->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void requeststartdrag(struct wl_listener *listener, void *data) {
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(seat, event->origin,
											  event->serial))
		wlr_seat_start_pointer_drag(seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}

void setborder_color(Client *c) {
	if (!c || !c->mon)
		return;
	if (c->isurgent) {
		client_set_border_color(c, urgentcolor);
		return;
	}
	if (c->is_in_scratchpad && selmon && c == selmon->sel) {
		client_set_border_color(c, scratchpadcolor);
	} else if (c->isglobal && selmon && c == selmon->sel) {
		client_set_border_color(c, globalcolor);
	} else if (c->isoverlay && selmon && c == selmon->sel) {
		client_set_border_color(c, overlaycolor);
	} else if (c->ismaxmizescreen && selmon && c == selmon->sel) {
		client_set_border_color(c, maxmizescreencolor);
	} else if (selmon && c == selmon->sel) {
		client_set_border_color(c, focuscolor);
	} else {
		client_set_border_color(c, bordercolor);
	}
}

void exchange_two_client(Client *c1, Client *c2) {
	if (c1 == NULL || c2 == NULL || c1->mon != c2->mon) {
		return;
	}

	struct wl_list *tmp1_prev = c1->link.prev;
	struct wl_list *tmp2_prev = c2->link.prev;
	struct wl_list *tmp1_next = c1->link.next;
	struct wl_list *tmp2_next = c2->link.next;

	// wl_list
	// 是双向链表,其中clients是头部节点,它的下一个节点是第一个客户端的链表节点
	// 最后一个客户端的链表节点的下一个节点也指向clients,但clients本身不是客户端的链表节点
	// 客户端遍历从clients的下一个节点开始,到检测到客户端节点的下一个是clients结束

	// 当c1和c2为相邻节点时
	if (c1->link.next == &c2->link) {
		c1->link.next = c2->link.next;
		c1->link.prev = &c2->link;
		c2->link.next = &c1->link;
		c2->link.prev = tmp1_prev;
		tmp1_prev->next = &c2->link;
		tmp2_next->prev = &c1->link;
	} else if (c2->link.next == &c1->link) {
		c2->link.next = c1->link.next;
		c2->link.prev = &c1->link;
		c1->link.next = &c2->link;
		c1->link.prev = tmp2_prev;
		tmp2_prev->next = &c1->link;
		tmp1_next->prev = &c2->link;
	} else { // 不为相邻节点
		c2->link.next = tmp1_next;
		c2->link.prev = tmp1_prev;
		c1->link.next = tmp2_next;
		c1->link.prev = tmp2_prev;

		tmp1_prev->next = &c2->link;
		tmp1_next->prev = &c2->link;
		tmp2_prev->next = &c1->link;
		tmp2_next->prev = &c1->link;
	}

	arrange(c1->mon, false);
	focusclient(c1, 0);
}

void exchange_client(const Arg *arg) {
	Client *c = selmon->sel;
	if (!c || c->isfloating || c->isfullscreen || c->ismaxmizescreen)
		return;
	exchange_two_client(c, direction_select(arg));
}

void // 17
run(char *startup_cmd) {
	char autostart_temp_path[1024];
	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(dpy);
	if (!socket)
		die("startup: display_add_socket_auto");
	setenv("WAYLAND_DISPLAY", socket, 1);

	/* Start the backend. This will enumerate outputs and inputs, become the DRM
	 * master, etc */
	if (!wlr_backend_start(backend))
		die("startup: backend_start");

	/* Now that the socket exists and the backend is started, run the startup
	 * command */
	if (!startup_cmd)
		startup_cmd = get_autostart_path(autostart_temp_path,
										 sizeof(autostart_temp_path));
	if (startup_cmd) {
		int piperw[2];
		if (pipe(piperw) < 0)
			die("startup: pipe:");
		if ((child_pid = fork()) < 0)
			die("startup: fork:");
		if (child_pid == 0) {
			setsid();
			dup2(piperw[0], STDIN_FILENO);
			close(piperw[0]);
			close(piperw[1]);
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, NULL);
			die("startup: execl:");
		}
		dup2(piperw[1], STDOUT_FILENO);
		close(piperw[1]);
		close(piperw[0]);
	}

	/* Mark stdout as non-blocking to avoid people who does not close stdin
	 * nor consumes it in their startup script getting dwl frozen */
	if (fd_set_nonblock(STDOUT_FILENO) < 0)
		close(STDOUT_FILENO);

	printstatus();

	/* At this point the outputs are initialized, choose initial selmon based on
	 * cursor position, and set default cursor image */
	selmon = xytomon(cursor->x, cursor->y);

	/* TODO hack to get cursor to display in its initial location (100, 100)
	 * instead of (0, 0) and then jumping. still may not be fully
	 * initialized, as the image/coordinates are not transformed for the
	 * monitor when displayed here */
	wlr_cursor_warp_closest(cursor, NULL, cursor->x, cursor->y);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "left_ptr");
	handlecursoractivity();

	run_exec();
	run_exec_once();

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */

	wl_display_run(dpy);
}

void setcursor(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	/* If we're "grabbing" the cursor, don't use the client's image, we will
	 * restore it after "grabbing" sending a leave event, followed by a enter
	 * event, which will result in the client requesting set the cursor surface
	 */
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided surface as the cursor image. It will set the
	 * hardware cursor on the output that it's currently on and continue to
	 * do so as the cursor moves between outputs. */
	if (event->seat_client == seat->pointer_state.focused_client) {
		last_cursor.shape = 0;
		last_cursor.surface = event->surface;
		last_cursor.hotspot_x = event->hotspot_x;
		last_cursor.hotspot_y = event->hotspot_y;
		if (!cursor_hidden)
			wlr_cursor_set_surface(cursor, event->surface, event->hotspot_x,
								   event->hotspot_y);
	}
}

void // 0.5
setfloating(Client *c, int floating) {

	Client *fc;
	int hit;
	struct wlr_box target_box, backup_box;
	c->isfloating = floating;
	bool window_size_outofrange = false;

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (c->isoverlay) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
	} else if (client_should_overtop(c) && c->isfloating) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFSorOverTop]);
	} else {
		wlr_scene_node_reparent(&c->scene->node,
								layers[c->isfloating ? LyrFloat : LyrTile]);
	}

	target_box = c->geom;

	if (floating == 1 && c != grabc) {

		if (c->isfullscreen || c->ismaxmizescreen) {
			c->isfullscreen = 0; // 清除窗口全屏标志
			c->ismaxmizescreen = 0;
			c->bw = c->isnoborder ? 0 : borderpx;
		}

		// 重新计算居中的坐标
		if (!client_is_x11(c) || !client_should_ignore_focus(c))
			target_box = setclient_coordinate_center(c, target_box, 0, 0);
		backup_box = c->geom;
		hit = applyrulesgeom(c);
		target_box = hit == 1 ? c->geom : target_box;
		c->geom = backup_box;

		// restore to the memeroy geom
		if (c->oldgeom.width > 0 && c->oldgeom.height > 0) {
			if (c->mon && c->oldgeom.width >= c->mon->w.width - gappoh) {
				c->oldgeom.width = c->mon->w.width * 0.9;
				window_size_outofrange = true;
			}
			if (c->mon && c->oldgeom.height >= c->mon->w.height - gappov) {
				c->oldgeom.height = c->mon->w.height * 0.9;
				window_size_outofrange = true;
			}
			if (window_size_outofrange) {
				c->oldgeom = setclient_coordinate_center(c, c->oldgeom, 0, 0);
			}
			resize(c, c->oldgeom, 0);
		} else {
			resize(c, target_box, 0);
		}

		c->need_float_size_reduce = 0;
	} else if (c->isfloating && c == grabc) {
		c->need_float_size_reduce = 0;
	} else {
		c->need_float_size_reduce = 1;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		// 让当前tag中的全屏窗口退出全屏参与平铺
		wl_list_for_each(fc, &clients, link) if (fc && fc != c &&
												 c->tags & fc->tags &&
												 ISFULLSCREEN(fc)) {
			clear_fullscreen_flag(fc);
		}
	}

	arrange(c->mon, false);
	setborder_color(c);
	printstatus();
}

void reset_maxmizescreen_size(Client *c) {
	c->geom.x = c->mon->w.x + gappov;
	c->geom.y = c->mon->w.y + gappoh;
	c->geom.width = c->mon->w.width - 2 * gappov;
	c->geom.height = c->mon->w.height - 2 * gappov;
	resize(c, c->geom, 0);
}

void setmaxmizescreen(Client *c, int maxmizescreen) {
	struct wlr_box maxmizescreen_box;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	c->ismaxmizescreen = maxmizescreen;

	wlr_scene_node_reparent(&c->scene->node, layers[maxmizescreen	? LyrTile
													: c->isfloating ? LyrFloat
																	: LyrTile]);

	if (maxmizescreen) {
		if (c->isfloating)
			c->oldgeom = c->geom;
		if (selmon->isoverview) {
			Arg arg = {0};
			toggleoverview(&arg);
		}

		maxmizescreen_box.x = c->mon->w.x + gappov;
		maxmizescreen_box.y = c->mon->w.y + gappoh;
		maxmizescreen_box.width = c->mon->w.width - 2 * gappov;
		maxmizescreen_box.height = c->mon->w.height - 2 * gappov;
		wlr_scene_node_raise_to_top(&c->scene->node); // 将视图提升到顶层
		resize(c, maxmizescreen_box, 0);
		c->ismaxmizescreen = 1;
	} else {
		c->bw = c->isnoborder ? 0 : borderpx;
		c->ismaxmizescreen = 0;
		c->isfullscreen = 0;
		setfullscreen(c, false);
		if (c->isfloating)
			setfloating(c, 1);
		arrange(c->mon, false);
	}
}

void setfakefullscreen(Client *c, int fakefullscreen) {
	c->isfakefullscreen = fakefullscreen;
	if (!c->mon)
		return;
	if (c->isfullscreen)
		setfullscreen(c, 0);
	client_set_fullscreen(c, fakefullscreen);
}

void setfullscreen(Client *c, int fullscreen) // 用自定义全屏代理自带全屏
{
	c->isfullscreen = fullscreen;

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	client_set_fullscreen(c, fullscreen);

	if (c->isoverlay) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
	} else if (client_should_overtop(c) && c->isfloating) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFSorOverTop]);
	} else {
		wlr_scene_node_reparent(&c->scene->node,
								layers[fullscreen	   ? LyrFSorOverTop
									   : c->isfloating ? LyrFloat
													   : LyrTile]);
	}

	if (fullscreen) {
		if (c->isfloating)
			c->oldgeom = c->geom;
		if (selmon->isoverview) {
			Arg arg = {0};
			toggleoverview(&arg);
		}

		c->bw = 0;
		wlr_scene_node_raise_to_top(&c->scene->node); // 将视图提升到顶层
		resize(c, c->mon->m, 1);
		c->isfullscreen = 1;
		// c->isfloating = 0;
	} else {
		c->bw = c->isnoborder ? 0 : borderpx;
		c->isfullscreen = 0;
		c->isfullscreen = 0;
		c->ismaxmizescreen = 0;
		if (c->isfloating)
			setfloating(c, 1);
		arrange(c->mon, false);
	}
}

void setgaps(int oh, int ov, int ih, int iv) {
	selmon->gappoh = MAX(oh, 0);
	selmon->gappov = MAX(ov, 0);
	selmon->gappih = MAX(ih, 0);
	selmon->gappiv = MAX(iv, 0);
	arrange(selmon, false);
}

void // 17
setlayout(const Arg *arg) {
	int jk;
	for (jk = 0; jk < LENGTH(layouts); jk++) {
		if (strcmp(layouts[jk].name, arg->v) == 0) {
			selmon->pertag->ltidxs[selmon->pertag->curtag] = &layouts[jk];

			arrange(selmon, false);
			printstatus();
			return;
		}
	}
}

char *get_layout_abbr(const char *full_name) {
	// 1. 尝试在映射表中查找
	for (int i = 0; layout_mappings[i].full_name != NULL; i++) {
		if (strcmp(full_name, layout_mappings[i].full_name) == 0) {
			return strdup(layout_mappings[i].abbr);
		}
	}

	// 2. 尝试从名称中提取并转换为小写
	const char *open = strrchr(full_name, '(');
	const char *close = strrchr(full_name, ')');
	if (open && close && close > open) {
		unsigned int len = close - open - 1;
		if (len > 0 && len <= 4) {
			char *abbr = malloc(len + 1);
			if (abbr) {
				// 提取并转换为小写
				for (unsigned int j = 0; j < len; j++) {
					abbr[j] = tolower(open[j + 1]);
				}
				abbr[len] = '\0';
				return abbr;
			}
		}
	}

	// 3. 提取前2-3个字母并转换为小写
	char *abbr = malloc(4);
	if (abbr) {
		unsigned int j = 0;
		for (unsigned int i = 0; full_name[i] != '\0' && j < 3; i++) {
			if (isalpha(full_name[i])) {
				abbr[j++] = tolower(full_name[i]);
			}
		}
		abbr[j] = '\0';

		// 确保至少2个字符
		if (j >= 2)
			return abbr;
		free(abbr);
	}

	// 4. 回退方案：使用首字母小写
	char *fallback = malloc(3);
	if (fallback) {
		fallback[0] = tolower(full_name[0]);
		fallback[1] = full_name[1] ? tolower(full_name[1]) : '\0';
		fallback[2] = '\0';
		return fallback;
	}

	// 5. 最终回退：返回 "xx"
	return strdup("xx");
}

void reset_keyboard_layout(void) {
	if (!kb_group || !kb_group->wlr_group || !seat) {
		wlr_log(WLR_ERROR, "Invalid keyboard group or seat");
		return;
	}

	struct wlr_keyboard *keyboard = &kb_group->wlr_group->keyboard;
	if (!keyboard || !keyboard->keymap) {
		wlr_log(WLR_ERROR, "Invalid keyboard or keymap");
		return;
	}

	// Get current layout
	xkb_layout_index_t current = xkb_state_serialize_layout(
		keyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	const int num_layouts = xkb_keymap_num_layouts(keyboard->keymap);
	if (num_layouts < 1) {
		wlr_log(WLR_INFO, "No layouts available");
		return;
	}

	// Create context
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context) {
		wlr_log(WLR_ERROR, "Failed to create XKB context");
		return;
	}

	// Get layout abbreviations
	char **layout_ids = calloc(num_layouts, sizeof(char *));
	if (!layout_ids) {
		wlr_log(WLR_ERROR, "Failed to allocate layout IDs");
		goto cleanup_context;
	}

	for (int i = 0; i < num_layouts; i++) {
		layout_ids[i] =
			get_layout_abbr(xkb_keymap_layout_get_name(keyboard->keymap, i));
		if (!layout_ids[i]) {
			wlr_log(WLR_ERROR, "Failed to get layout abbreviation");
			goto cleanup_layouts;
		}
	}

	// Keep the same rules but just reapply them
	struct xkb_rule_names rules = xkb_rules;

	// Create new keymap with current rules
	struct xkb_keymap *new_keymap =
		xkb_keymap_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!new_keymap) {
		wlr_log(WLR_ERROR, "Failed to create keymap for layouts: %s",
				rules.layout);
		goto cleanup_layouts;
	}

	// Apply the same keymap (this will reset the layout state)
	unsigned int depressed = keyboard->modifiers.depressed;
	unsigned int latched = keyboard->modifiers.latched;
	unsigned int locked = keyboard->modifiers.locked;

	wlr_keyboard_set_keymap(keyboard, new_keymap);

	wlr_keyboard_notify_modifiers(keyboard, depressed, latched, locked, 0);
	keyboard->modifiers.group = current; // Keep the same layout index

	// Update seat
	wlr_seat_set_keyboard(seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(seat, &keyboard->modifiers);

	// Cleanup
	xkb_keymap_unref(new_keymap);

cleanup_layouts:
	for (int i = 0; i < num_layouts; i++) {
		free(layout_ids[i]);
	}
	free(layout_ids);

cleanup_context:
	xkb_context_unref(context);
}

void switch_keyboard_layout(const Arg *arg) {
	if (!kb_group || !kb_group->wlr_group || !seat) {
		wlr_log(WLR_ERROR, "Invalid keyboard group or seat");
		return;
	}

	struct wlr_keyboard *keyboard = &kb_group->wlr_group->keyboard;
	if (!keyboard || !keyboard->keymap) {
		wlr_log(WLR_ERROR, "Invalid keyboard or keymap");
		return;
	}

	// 1. 获取当前布局和计算下一个布局
	xkb_layout_index_t current = xkb_state_serialize_layout(
		keyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	const int num_layouts = xkb_keymap_num_layouts(keyboard->keymap);
	if (num_layouts < 2) {
		wlr_log(WLR_INFO, "Only one layout available");
		return;
	}
	xkb_layout_index_t next = (current + 1) % num_layouts;

	// 2. 创建上下文
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context) {
		wlr_log(WLR_ERROR, "Failed to create XKB context");
		return;
	}

	// 3. 分配并获取布局缩写
	char **layout_ids = calloc(num_layouts, sizeof(char *));
	if (!layout_ids) {
		wlr_log(WLR_ERROR, "Failed to allocate layout IDs");
		goto cleanup_context;
	}

	for (int i = 0; i < num_layouts; i++) {
		layout_ids[i] =
			get_layout_abbr(xkb_keymap_layout_get_name(keyboard->keymap, i));
		if (!layout_ids[i]) {
			wlr_log(WLR_ERROR, "Failed to get layout abbreviation");
			goto cleanup_layouts;
		}
	}

	// 4. 直接修改 rules.layout（保持原有逻辑）
	struct xkb_rule_names rules = xkb_rules;
	char *layout_buf = (char *)rules.layout; // 假设这是可修改的

	// 清空原有内容（安全方式）
	unsigned int layout_buf_size = strlen(layout_buf) + 1;
	memset(layout_buf, 0, layout_buf_size);

	// 构建新的布局字符串
	for (int i = 0; i < num_layouts; i++) {
		const char *layout = layout_ids[(next + i) % num_layouts];

		if (i > 0) {
			strncat(layout_buf, ",", layout_buf_size - strlen(layout_buf) - 1);
		}

		if (strchr(layout, ',')) {
			// 处理包含逗号的布局名
			char *quoted = malloc(strlen(layout) + 3);
			if (quoted) {
				snprintf(quoted, strlen(layout) + 3, "\"%s\"", layout);
				strncat(layout_buf, quoted,
						layout_buf_size - strlen(layout_buf) - 1);
				free(quoted);
			}
		} else {
			strncat(layout_buf, layout,
					layout_buf_size - strlen(layout_buf) - 1);
		}
	}

	// 5. 创建新 keymap
	struct xkb_keymap *new_keymap =
		xkb_keymap_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!new_keymap) {
		wlr_log(WLR_ERROR, "Failed to create keymap for layouts: %s",
				rules.layout);
		goto cleanup_layouts;
	}

	// 6. 应用新 keymap
	unsigned int depressed = keyboard->modifiers.depressed;
	unsigned int latched = keyboard->modifiers.latched;
	unsigned int locked = keyboard->modifiers.locked;

	wlr_keyboard_set_keymap(keyboard, new_keymap);
	wlr_keyboard_notify_modifiers(keyboard, depressed, latched, locked, 0);
	keyboard->modifiers.group = 0;

	// 7. 更新 seat
	wlr_seat_set_keyboard(seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(seat, &keyboard->modifiers);

	// 8. 清理资源
	xkb_keymap_unref(new_keymap);

cleanup_layouts:
	for (int i = 0; i < num_layouts; i++) {
		free(layout_ids[i]);
	}
	free(layout_ids);

cleanup_context:
	xkb_context_unref(context);
}

void switch_layout(const Arg *arg) {

	int jk, ji;
	char *target_layout_name = NULL;
	unsigned int len;

	if (config.circle_layout_count != 0) {
		for (jk = 0; jk < config.circle_layout_count; jk++) {

			len = MAX(
				strlen(config.circle_layout[jk]),
				strlen(selmon->pertag->ltidxs[selmon->pertag->curtag]->name));

			if (strncmp(config.circle_layout[jk],
						selmon->pertag->ltidxs[selmon->pertag->curtag]->name,
						len) == 0) {
				target_layout_name = jk == config.circle_layout_count - 1
										 ? config.circle_layout[0]
										 : config.circle_layout[jk + 1];
				break;
			}
		}

		if (!target_layout_name) {
			target_layout_name = config.circle_layout[0];
		}

		for (ji = 0; ji < LENGTH(layouts); ji++) {
			len = MAX(strlen(layouts[ji].name), strlen(target_layout_name));
			if (strncmp(layouts[ji].name, target_layout_name, len) == 0) {
				selmon->pertag->ltidxs[selmon->pertag->curtag] = &layouts[ji];
				break;
			}
		}

		arrange(selmon, false);
		printstatus();
		return;
	}

	for (jk = 0; jk < LENGTH(layouts); jk++) {
		if (strcmp(layouts[jk].name,
				   selmon->pertag->ltidxs[selmon->pertag->curtag]->name) == 0) {
			selmon->pertag->ltidxs[selmon->pertag->curtag] =
				jk == LENGTH(layouts) - 1 ? &layouts[0] : &layouts[jk + 1];
			arrange(selmon, false);
			printstatus();
			return;
		}
	}
}

/* arg > 1.0 will set mfact absolutely */
void // 17
setmfact(const Arg *arg) {
	float f;

	if (!arg || !selmon ||
		!selmon->pertag->ltidxs[selmon->pertag->curtag]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->pertag->mfacts[selmon->pertag->curtag]
					 : arg->f - 1.0;
	if (f < 0.1 || f > 0.9)
		return;
	// selmon->mfact = f;
	selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	arrange(selmon, false);
}

void setsmfact(const Arg *arg) {
	float f;

	if (!arg || !selmon ||
		!selmon->pertag->ltidxs[selmon->pertag->curtag]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->pertag->smfacts[selmon->pertag->curtag]
					 : arg->f - 1.0;
	if (f < 0.1 || f > 0.9)
		return;
	// selmon->mfact = f;
	selmon->pertag->smfacts[selmon->pertag->curtag] = f;
	arrange(selmon, false);
}

void setmon(Client *c, Monitor *m, unsigned int newtags, bool focus) {
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;

	if (oldmon && oldmon->sel == c) {
		oldmon->sel = NULL;
	}

	if (oldmon && oldmon->prevsel == c) {
		oldmon->prevsel = NULL;
	}

	c->mon = m;

	/* Scene graph sends surface leave/enter events on move and resize */
	if (oldmon)
		arrange(oldmon, false);
	if (m) {
		/* Make sure window actually overlaps with the monitor */
		resize(c, c->geom, 0);
		c->tags =
			newtags ? newtags
					: m->tagset[m->seltags]; /* assign tags of target monitor */
		setfloating(c, c->isfloating);
		setfullscreen(c, c->isfullscreen); /* This will call arrange(c->mon) */
	}
	if (focus)
		focusclient(focustop(selmon), 1);

	if (!c->foreign_toplevel && c->mon) {
		add_foreign_toplevel(c);
		if (selmon->sel && selmon->sel->foreign_toplevel)
			wlr_foreign_toplevel_handle_v1_set_activated(
				selmon->sel->foreign_toplevel, false);
		if (c->foreign_toplevel)
			wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,
														 true);
	}
}

void setpsel(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the
	 * selection, usually when the user copies something. wlroots allows
	 * compositors to ignore such requests if they so choose, but in dwl we
	 * always honor
	 */
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

void setsel(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the
	 * selection, usually when the user copies something. wlroots allows
	 * compositors to ignore such requests if they so choose, but in dwl we
	 * always honor
	 */
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat, event->source, event->serial);
}

// 获取tags中最前面的tag的tagmask
unsigned int get_tags_first_tag(unsigned int source_tags) {
	unsigned int i, tag;
	tag = 0;

	if (!source_tags) {
		return selmon->pertag->curtag;
	}

	for (i = 0; !(tag & 1) && source_tags != 0 && i < LENGTH(tags); i++) {
		tag = source_tags >> i;
	}

	if (i == 1) {
		return 1;
	} else if (i > 9) {
		return 1 << 8;
	} else {
		return 1 << (i - 1);
	}
}

void show_hide_client(Client *c) {
	unsigned int target = get_tags_first_tag(c->oldtags);
	tag_client(&(Arg){.ui = target}, c);
	// c->tags = c->oldtags;
	c->isminied = 0;
	wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel, false);
	focusclient(c, 1);
	wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);
}

void create_output(struct wlr_backend *backend, void *data) {
	bool *done = data;
	if (*done) {
		return;
	}

	if (wlr_backend_is_wl(backend)) {
		wlr_wl_output_create(backend);
		*done = true;
	} else if (wlr_backend_is_headless(backend)) {
		wlr_headless_add_output(backend, 1920, 1080);
		*done = true;
	}
#if WLR_HAS_X11_BACKEND
	else if (wlr_backend_is_x11(backend)) {
		wlr_x11_output_create(backend);
		*done = true;
	}
#endif
}

/**
 * This command is intended for developer use only.
 */
void create_virtual_output(const Arg *arg) {

	if (!wlr_backend_is_multi(backend)) {
		wlr_log(WLR_ERROR, "Expected a multi backend");
		return;
	}

	bool done = false;
	wlr_multi_for_each_backend(backend, create_output, &done);

	if (!done) {
		wlr_log(WLR_ERROR, "Failed to create virtual output");
		return;
	}

	wlr_log(WLR_INFO, "Virtual output created");
	return;
}

void destroy_all_virtual_output(const Arg *arg) {

	if (!wlr_backend_is_multi(backend)) {
		wlr_log(WLR_ERROR, "Expected a multi backend");
		return;
	}

	Monitor *m, *tmp;
	wl_list_for_each_safe(m, tmp, &mons, link) {
		if (wlr_output_is_headless(m->wlr_output)) {
			// if(selmon == m)
			//   selmon = NULL;
			wlr_output_destroy(m->wlr_output);
			wlr_log(WLR_INFO, "Virtual output destroyed");
		}
	}
}

void setup(void) {

	setenv("XCURSOR_SIZE", "24", 1);
	setenv("XDG_CURRENT_DESKTOP", "maomao", 1);

	parse_config();
	init_baked_points();

	int drm_fd, i, sig[] = {SIGCHLD, SIGINT, SIGTERM, SIGPIPE};
	struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handlesig};
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < LENGTH(sig); i++)
		sigaction(sig[i], &sa, NULL);

	wlr_log_init(log_level, NULL);

	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	dpy = wl_display_create();
	event_loop = wl_display_get_event_loop(dpy);
	pointer_manager = wlr_relative_pointer_manager_v1_create(dpy);
	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. The NULL argument here optionally allows you
	 * to pass in a custom renderer if wlr_renderer doesn't meet your needs. The
	 * backend uses the renderer, for example, to fall back to software cursors
	 * if the backend does not support hardware cursors (some older GPUs
	 * don't). */
	if (!(backend = wlr_backend_autocreate(event_loop, &session)))
		die("couldn't create backend");

	headless_backend = wlr_headless_backend_create(event_loop);
	if (!headless_backend) {
		wlr_log(WLR_ERROR, "Failed to create secondary headless backend");
	} else {
		wlr_multi_backend_add(backend, headless_backend);
	}

	/* Initialize the scene graph used to lay out windows */
	scene = wlr_scene_create();
	root_bg = wlr_scene_rect_create(&scene->tree, 0, 0, rootcolor);
	for (i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);
	drag_icon = wlr_scene_tree_create(&scene->tree);
	wlr_scene_node_place_below(&drag_icon->node, &layers[LyrBlock]->node);

	/* Create a renderer with the default implementation */
	if (!(drw = wlr_renderer_autocreate(backend)))
		die("couldn't recreate renderer");

	wl_signal_add(&drw->events.lost, &gpu_reset);

	/* Create shm, drm and linux_dmabuf interfaces by ourselves.
	 * The simplest way is call:
	 *      wlr_renderer_init_wl_display(drw);
	 * but we need to create manually the linux_dmabuf interface to integrate it
	 * with wlr_scene. */
	wlr_renderer_init_wl_shm(drw, dpy);

	if (wlr_renderer_get_texture_formats(drw, WLR_BUFFER_CAP_DMABUF)) {
		wlr_drm_create(dpy, drw);
		wlr_scene_set_linux_dmabuf_v1(
			scene, wlr_linux_dmabuf_v1_create_with_renderer(dpy, 5, drw));
	}

	if (syncobj_enable && (drm_fd = wlr_renderer_get_drm_fd(drw)) >= 0 &&
		drw->features.timeline && backend->features.timeline)
		wlr_linux_drm_syncobj_manager_v1_create(dpy, 1, drm_fd);

	/* Create a default allocator */
	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't create allocator");

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device manager
	 * handles the clipboard. Each of these wlroots interfaces has room for you
	 * to dig your fingers in and play with their behavior if you want. Note
	 * that the clients cannot set the selection directly without compositor
	 * approval, see the setsel() function. */
	compositor = wlr_compositor_create(dpy, 6, drw);
	wlr_export_dmabuf_manager_v1_create(dpy);
	wlr_screencopy_manager_v1_create(dpy);
	wlr_ext_image_copy_capture_manager_v1_create(dpy, 1);
	wlr_ext_output_image_capture_source_manager_v1_create(dpy, 1);
	wlr_data_control_manager_v1_create(dpy);
	wlr_data_device_manager_create(dpy);
	wlr_primary_selection_v1_device_manager_create(dpy);
	wlr_viewporter_create(dpy);
	wlr_single_pixel_buffer_manager_v1_create(dpy);
	wlr_fractional_scale_manager_v1_create(dpy, 1);
	wlr_presentation_create(dpy, backend, 2);
	wlr_subcompositor_create(dpy);
	wlr_alpha_modifier_v1_create(dpy);
	wlr_ext_data_control_manager_v1_create(dpy, 1);

	/* Initializes the interface used to implement urgency hints */
	activation = wlr_xdg_activation_v1_create(dpy);
	wl_signal_add(&activation->events.request_activate, &request_activate);

	wlr_scene_set_gamma_control_manager_v1(
		scene, wlr_gamma_control_manager_v1_create(dpy));

	power_mgr = wlr_output_power_manager_v1_create(dpy);
	wl_signal_add(&power_mgr->events.set_mode, &output_power_mgr_set_mode);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	output_layout = wlr_output_layout_create(dpy);
	wl_signal_add(&output_layout->events.change, &layout_change);
	wlr_xdg_output_manager_v1_create(dpy, output_layout);

	/* Configure a listener to be notified when new outputs are available on the
	 * backend. */
	wl_list_init(&mons);
	wl_signal_add(&backend->events.new_output, &new_output);

	/* Set up our client lists and the xdg-shell. The xdg-shell is a
	 * Wayland protocol which is used for application windows. For more
	 * detail on shells, refer to the article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	wl_list_init(&clients);
	wl_list_init(&fstack);
	wl_list_init(&fadeout_clients);
	wl_list_init(&fadeout_layers);

	idle_notifier = wlr_idle_notifier_v1_create(dpy);

	idle_inhibit_mgr = wlr_idle_inhibit_v1_create(dpy);
	wl_signal_add(&idle_inhibit_mgr->events.new_inhibitor, &new_idle_inhibitor);

	layer_shell = wlr_layer_shell_v1_create(dpy, 4);
	wl_signal_add(&layer_shell->events.new_surface, &new_layer_surface);

	xdg_shell = wlr_xdg_shell_create(dpy, 6);
	wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);
	wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

	session_lock_mgr = wlr_session_lock_manager_v1_create(dpy);
	wl_signal_add(&session_lock_mgr->events.new_lock, &new_session_lock);

	locked_bg =
		wlr_scene_rect_create(layers[LyrBlock], sgeom.width, sgeom.height,
							  (float[4]){0.1, 0.1, 0.1, 1.0});
	wlr_scene_node_set_enabled(&locked_bg->node, false);

	/* Use decoration protocols to negotiate server-side decorations */
	wlr_server_decoration_manager_set_default_mode(
		wlr_server_decoration_manager_create(dpy),
		WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(dpy);
	wl_signal_add(&xdg_decoration_mgr->events.new_toplevel_decoration,
				  &new_xdg_decoration);

	pointer_constraints = wlr_pointer_constraints_v1_create(dpy);
	wl_signal_add(&pointer_constraints->events.new_constraint,
				  &new_pointer_constraint);

	relative_pointer_mgr = wlr_relative_pointer_manager_v1_create(dpy);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(cursor, output_layout);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that cursor
	 * images are available at all scale factors on the screen (necessary for
	 * HiDPI support). Scaled cursors will be loaded with each output. */
	// cursor_mgr = wlr_xcursor_manager_create(cursor_theme, 24);
	cursor_mgr = wlr_xcursor_manager_create(config.cursor_theme, cursor_size);

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move around
	 * when the pointer moves. However, we can attach input devices to it, and
	 * it will generate aggregate events for all of them. In these events, we
	 * can choose how we want to process them, forwarding them to clients and
	 * moving the cursor around. More detail on this process is described in my
	 * input handling blog post:
	 *
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	 *
	 * And more comments are sprinkled throughout the notify functions above.
	 */
	wl_signal_add(&cursor->events.motion, &cursor_motion);
	wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);
	wl_signal_add(&cursor->events.button, &cursor_button);
	wl_signal_add(&cursor->events.axis, &cursor_axis);
	wl_signal_add(&cursor->events.frame, &cursor_frame);

	// 这两句代码会造成obs窗口里的鼠标光标消失,不知道注释有什么影响
	cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(dpy, 1);
	wl_signal_add(&cursor_shape_mgr->events.request_set_shape,
				  &request_set_cursor_shape);
	hide_source = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
										  hidecursor, cursor);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	wl_list_init(&keyboards);
	wl_signal_add(&backend->events.new_input, &new_input_device);
	virtual_keyboard_mgr = wlr_virtual_keyboard_manager_v1_create(dpy);
	wl_signal_add(&virtual_keyboard_mgr->events.new_virtual_keyboard,
				  &new_virtual_keyboard);
	virtual_pointer_mgr = wlr_virtual_pointer_manager_v1_create(dpy);
	wl_signal_add(&virtual_pointer_mgr->events.new_virtual_pointer,
				  &new_virtual_pointer);

	pointer_gestures = wlr_pointer_gestures_v1_create(dpy);
	LISTEN_STATIC(&cursor->events.swipe_begin, swipe_begin);
	LISTEN_STATIC(&cursor->events.swipe_update, swipe_update);
	LISTEN_STATIC(&cursor->events.swipe_end, swipe_end);
	LISTEN_STATIC(&cursor->events.pinch_begin, pinch_begin);
	LISTEN_STATIC(&cursor->events.pinch_update, pinch_update);
	LISTEN_STATIC(&cursor->events.pinch_end, pinch_end);
	LISTEN_STATIC(&cursor->events.hold_begin, hold_begin);
	LISTEN_STATIC(&cursor->events.hold_end, hold_end);

	seat = wlr_seat_create(dpy, "seat0");
	wl_signal_add(&seat->events.request_set_cursor, &request_cursor);
	wl_signal_add(&seat->events.request_set_selection, &request_set_sel);
	wl_signal_add(&seat->events.request_set_primary_selection,
				  &request_set_psel);
	wl_signal_add(&seat->events.request_start_drag, &request_start_drag);
	wl_signal_add(&seat->events.start_drag, &start_drag);

	kb_group = createkeyboardgroup();
	wl_list_init(&kb_group->destroy.link);

	output_mgr = wlr_output_manager_v1_create(dpy);
	wl_signal_add(&output_mgr->events.apply, &output_mgr_apply);
	wl_signal_add(&output_mgr->events.test, &output_mgr_test);

	/* create text_input-, and input_method-protocol relevant globals */
	input_method_manager = wlr_input_method_manager_v2_create(dpy);
	text_input_manager = wlr_text_input_manager_v3_create(dpy);

	dwl_input_method_relay = calloc(1, sizeof(*dwl_input_method_relay));
	dwl_input_method_relay = dwl_im_relay_create();

	wl_global_create(dpy, &zdwl_ipc_manager_v2_interface, 2, NULL,
					 dwl_ipc_manager_bind);

	// 创建顶层管理句柄
	foreign_toplevel_manager = wlr_foreign_toplevel_manager_v1_create(dpy);
	struct wlr_xdg_foreign_registry *foreign_registry =
		wlr_xdg_foreign_registry_create(dpy);
	wlr_xdg_foreign_v1_create(dpy, foreign_registry);
	wlr_xdg_foreign_v2_create(dpy, foreign_registry);
#ifdef XWAYLAND
	/*
	 * Initialise the XWayland X server.
	 * It will be started when the first X client is started.
	 */
	xwayland = wlr_xwayland_create(dpy, compositor, !xwayland_persistence);
	if (xwayland) {
		wl_signal_add(&xwayland->events.ready, &xwayland_ready);
		wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);

		setenv("DISPLAY", xwayland->display_name, 1);
	} else {
		fprintf(stderr,
				"failed to setup XWayland X server, continuing without it\n");
	}
#endif
}

void spawn(const Arg *arg) {

	if (!arg->v)
		return;

	if (fork() == 0) {
		// 1. 忽略可能导致 coredump 的信号
		signal(SIGSEGV, SIG_IGN);
		signal(SIGABRT, SIG_IGN);
		signal(SIGILL, SIG_IGN);

		dup2(STDERR_FILENO, STDOUT_FILENO);
		setsid();

		// 2. 解析参数
		char *argv[64];
		int argc = 0;
		char *token = strtok((char *)arg->v, " ");
		while (token != NULL && argc < 63) {
			wordexp_t p;
			if (wordexp(token, &p, 0) == 0) {
				argv[argc++] = p.we_wordv[0];
			} else {
				argv[argc++] = token;
			}
			token = strtok(NULL, " ");
		}
		argv[argc] = NULL;

		// 3. 执行命令
		execvp(argv[0], argv);

		// 4. execvp 失败时：打印错误并直接退出（避免 coredump）
		wlr_log(WLR_ERROR, "dwl: execvp '%s' failed: %s\n", argv[0],
				strerror(errno));
		_exit(EXIT_FAILURE); // 使用 _exit 避免缓冲区刷新等操作
	}
}

void spawn_on_empty(const Arg *arg) {
	bool is_empty = true;
	Client *c;

	wl_list_for_each(c, &clients, link) {
		if (arg->ui & c->tags && c->mon == selmon) {
			is_empty = false;
			break;
		}
	}
	if (!is_empty) {
		view(arg, true);
		return;
	} else {
		view(arg, true);
		spawn(arg);
	}
}

void startdrag(struct wl_listener *listener, void *data) {
	struct wlr_drag *drag = data;
	if (!drag->icon)
		return;

	drag->icon->data = &wlr_scene_drag_icon_create(drag_icon, drag->icon)->node;
	LISTEN_STATIC(&drag->icon->events.destroy, destroydragicon);
}

void tag_client(const Arg *arg, Client *target_client) {
	Client *fc;
	if (target_client && arg->ui & TAGMASK) {
		target_client->tags = arg->ui & TAGMASK;
		wl_list_for_each(fc, &clients, link) {
			if (fc && fc != target_client && target_client->tags & fc->tags &&
				ISFULLSCREEN(fc) && !target_client->isfloating) {
				clear_fullscreen_flag(fc);
			}
		}
		view(&(Arg){.ui = arg->ui}, false);

	} else {
		view(arg, false);
	}

	focusclient(target_client, 1);
	printstatus();
}

void tag(const Arg *arg) {
	Client *target_client = selmon->sel;
	tag_client(arg, target_client);
}

void tagsilent(const Arg *arg) {
	Client *fc;
	Client *target_client = selmon->sel;
	target_client->tags = arg->ui & TAGMASK;
	wl_list_for_each(fc, &clients, link) {
		if (fc && fc != target_client && target_client->tags & fc->tags &&
			ISFULLSCREEN(fc) && !target_client->isfloating) {
			clear_fullscreen_flag(fc);
		}
	}
	focusclient(focustop(selmon), 1);
	arrange(target_client->mon, false);
}

void client_update_oldmonname_record(Client *c, Monitor *m) {
	if (!c || c->iskilling || !client_surface(c)->mapped)
		return;
	memset(c->oldmonname, 0, sizeof(c->oldmonname));
	strncpy(c->oldmonname, m->wlr_output->name, sizeof(c->oldmonname) - 1);
	c->oldmonname[sizeof(c->oldmonname) - 1] = '\0';
}

void tagmon(const Arg *arg) {
	Monitor *m;
	Client *c = focustop(selmon);

	if (!c)
		return;

	m = dirtomon(arg->i);

	if (!m || !m->wlr_output->enabled || m == c->mon)
		return;

	unsigned int newtags = arg->ui ? c->tags : 0;
	unsigned int target;
	if (c == selmon->sel) {
		selmon->sel = NULL;
	}

	setmon(c, m, newtags, true);
	client_update_oldmonname_record(c, m);

	reset_foreign_tolevel(c);
	// 重新计算居中的坐标
	if (c->isfloating) {
		c->geom.width =
			(int)(c->geom.width * c->mon->w.width / selmon->w.width);
		c->geom.height =
			(int)(c->geom.height * c->mon->w.height / selmon->w.height);
		selmon = c->mon;
		c->geom = setclient_coordinate_center(c, c->geom, 0, 0);
		target = get_tags_first_tag(c->tags);
		view(&(Arg){.ui = target}, true);
		focusclient(c, 1);
		c->oldgeom = c->geom;
		resize(c, c->geom, 1);
	} else {
		selmon = c->mon;
		target = get_tags_first_tag(c->tags);
		view(&(Arg){.ui = target}, true);
		focusclient(c, 1);
		arrange(selmon, false);
	}
	warp_cursor_to_selmon(c->mon);
}

void overview(Monitor *m) { grid(m); }

// 目标窗口有其他窗口和它同个tag就返回0
unsigned int want_restore_fullscreen(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {
		if (c && c != target_client && c->tags == target_client->tags &&
			c == selmon->sel) {
			return 0;
		}
	}

	return 1;
}

// 普通视图切换到overview时保存窗口的旧状态
void overview_backup(Client *c) {
	c->overview_isfloatingbak = c->isfloating;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->overview_ismaxmizescreenbak = c->ismaxmizescreen;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->animation.tagining = false;
	c->animation.tagouted = false;
	c->animation.tagouting = false;
	c->overview_backup_geom = c->geom;
	c->overview_backup_bw = c->bw;
	if (c->isfloating) {
		c->isfloating = 0;
	}
	if (c->isfullscreen || c->ismaxmizescreen) {
		c->isfullscreen = 0; // 清除窗口全屏标志
		c->ismaxmizescreen = 0;
	}
	c->bw = c->isnoborder ? 0 : borderpx;
}

// overview切回到普通视图还原窗口的状态
void overview_restore(Client *c, const Arg *arg) {
	c->isfloating = c->overview_isfloatingbak;
	c->isfullscreen = c->overview_isfullscreenbak;
	c->ismaxmizescreen = c->overview_ismaxmizescreenbak;
	c->overview_isfloatingbak = 0;
	c->overview_isfullscreenbak = 0;
	c->overview_ismaxmizescreenbak = 0;
	c->geom = c->overview_backup_geom;
	c->bw = c->overview_backup_bw;
	c->animation.tagining = false;
	c->is_restoring_from_ov = (arg->ui & c->tags & TAGMASK) == 0 ? true : false;

	if (c->isfloating) {
		// XRaiseWindow(dpy, c->win); // 提升悬浮窗口到顶层
		resize(c, c->overview_backup_geom, 0);
	} else if (c->isfullscreen || c->ismaxmizescreen) {
		if (want_restore_fullscreen(
				c)) { // 如果同tag有其他窗口,且其他窗口是将要聚焦的,那么不恢复该窗口的全屏状态
			resize(c, c->overview_backup_geom, 0);
		} else {
			c->isfullscreen = 0;
			c->ismaxmizescreen = 0;
			setfullscreen(c, false);
		}
	} else {
		if (c->is_restoring_from_ov) {
			c->is_restoring_from_ov = false;
			resize(c, c->overview_backup_geom, 0);
		}
	}

	if (c->bw == 0 &&
		!c->isfullscreen) { // 如果是在ov模式中创建的窗口,没有bw记录
		c->bw = c->isnoborder ? 0 : borderpx;
	}
}

void switch_proportion_preset(const Arg *arg) {
	float target_proportion = 0;

	if (config.scroller_proportion_preset_count == 0) {
		return;
	}

	if (selmon->sel) {

		for (int i = 0; i < config.scroller_proportion_preset_count; i++) {
			if (config.scroller_proportion_preset[i] ==
				selmon->sel->scroller_proportion) {
				if (i == config.scroller_proportion_preset_count - 1) {
					target_proportion = config.scroller_proportion_preset[0];
					break;
				} else {
					target_proportion =
						config.scroller_proportion_preset[i + 1];
					break;
				}
			}
		}

		if (target_proportion == 0) {
			target_proportion = config.scroller_proportion_preset[0];
		}

		unsigned int max_client_width =
			selmon->w.width - 2 * scroller_structs - gappih;
		selmon->sel->scroller_proportion = target_proportion;
		selmon->sel->geom.width = max_client_width * target_proportion;
		// resize(selmon->sel, selmon->sel->geom, 0);
		arrange(selmon, false);
	}
}

void set_proportion(const Arg *arg) {
	if (selmon->sel) {
		unsigned int max_client_width =
			selmon->w.width - 2 * scroller_structs - gappih;
		selmon->sel->scroller_proportion = arg->f;
		selmon->sel->geom.width = max_client_width * arg->f;
		// resize(selmon->sel, selmon->sel->geom, 0);
		arrange(selmon, false);
	}
}

void handlecursoractivity(void) {
	wl_event_source_timer_update(hide_source, cursor_hide_timeout * 1000);

	if (!cursor_hidden)
		return;

	cursor_hidden = false;

	if (last_cursor.shape)
		wlr_cursor_set_xcursor(cursor, cursor_mgr,
							   wlr_cursor_shape_v1_name(last_cursor.shape));
	else
		wlr_cursor_set_surface(cursor, last_cursor.surface,
							   last_cursor.hotspot_x, last_cursor.hotspot_y);
}

int hidecursor(void *data) {
	wlr_cursor_unset_image(cursor);
	cursor_hidden = true;
	return 1;
}

void increase_proportion(const Arg *arg) {
	if (selmon->sel) {
		unsigned int max_client_width =
			selmon->w.width - 2 * scroller_structs - gappih;
		selmon->sel->scroller_proportion =
			MIN(MAX(arg->f + selmon->sel->scroller_proportion, 0.1), 1.0);
		selmon->sel->geom.width = max_client_width * arg->f;
		arrange(selmon, false);
	}
}

// 显示所有tag 或 跳转到聚焦窗口的tag
void toggleoverview(const Arg *arg) {

	Client *c;

	if (selmon->isoverview && ov_tab_mode && arg->i != -1 && selmon->sel) {
		focusstack(&(Arg){.i = 1});
		return;
	}

	selmon->isoverview ^= 1;
	unsigned int target;
	unsigned int visible_client_number = 0;

	if (selmon->isoverview) {
		wl_list_for_each(c, &clients,
						 link) if (c && c->mon == selmon &&
								   !client_is_unmanaged(c) &&
								   !client_should_ignore_focus(c) &&
								   !c->isminied && !c->isunglobal) {
			visible_client_number++;
		}
		if (visible_client_number > 0) {
			target = ~0;
		} else {
			selmon->isoverview ^= 1;
			return;
		}
	} else if (!selmon->isoverview && selmon->sel) {
		target = get_tags_first_tag(selmon->sel->tags);
	} else if (!selmon->isoverview && !selmon->sel) {
		target = (1 << (selmon->pertag->prevtag - 1));
		view(&(Arg){.ui = target}, false);
		return;
	}

	// 正常视图到overview,退出所有窗口的浮动和全屏状态参与平铺,
	// overview到正常视图,还原之前退出的浮动和全屏窗口状态
	if (selmon->isoverview) {
		wl_list_for_each(c, &clients, link) {
			if (c && !client_is_unmanaged(c) &&
				!client_should_ignore_focus(c) && !c->isunglobal)
				overview_backup(c);
		}
	} else {
		wl_list_for_each(c, &clients, link) {
			if (c && !c->iskilling && !client_is_unmanaged(c) &&
				!c->isunglobal && !client_should_ignore_focus(c) &&
				client_surface(c)->mapped)
				overview_restore(c, &(Arg){.ui = target});
		}
	}

	view(&(Arg){.ui = target}, false);

	if (ov_tab_mode && selmon->isoverview && selmon->sel) {
		focusstack(&(Arg){.i = 1});
	}
}

void togglefloating(const Arg *arg) {
	Client *sel = focustop(selmon);

	if (!sel)
		return;

	if ((sel->isfullscreen || sel->ismaxmizescreen)) {
		sel->isfloating = 1;
	} else {
		sel->isfloating = !sel->isfloating;
	}

	setfloating(sel, sel->isfloating);
}

void togglefakefullscreen(const Arg *arg) {
	Client *sel = focustop(selmon);
	if (sel)
		setfakefullscreen(sel, !sel->isfakefullscreen);
}

void togglefullscreen(const Arg *arg) {
	Client *sel = focustop(selmon);
	if (!sel)
		return;

	// if(sel->isfloating)
	// 	setfloating(sel, 0);

	if (sel->isfullscreen || sel->ismaxmizescreen)
		setfullscreen(sel, 0);
	else
		setfullscreen(sel, 1);

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
	sel->isnamedscratchpad = 0;
}

void togglemaxmizescreen(const Arg *arg) {
	Client *sel = focustop(selmon);
	if (!sel)
		return;

	// if(sel->isfloating)
	// 	setfloating(sel, 0);

	if (sel->isfullscreen || sel->ismaxmizescreen)
		setmaxmizescreen(sel, 0);
	else
		setmaxmizescreen(sel, 1);

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
	sel->isnamedscratchpad = 0;
}

void togglegaps(const Arg *arg) {
	enablegaps ^= 1;
	arrange(selmon, false);
}

void toggletag(const Arg *arg) {
	unsigned int newtags;
	Client *sel = focustop(selmon);
	if (!sel)
		return;
	newtags = sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		sel->tags = newtags;
		focusclient(focustop(selmon), 1);
		arrange(selmon, false);
	}
	printstatus();
}

void toggleview(const Arg *arg) {
	unsigned int newtagset =
		selmon ? selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK) : 0;

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focusclient(focustop(selmon), 1);
		arrange(selmon, false);
	}
	printstatus();
}

void unlocksession(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, unlock);
	destroylock(lock, 1);
}

void unmaplayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, unmap);

	l->mapped = 0;

	init_fadeout_layers(l);

	wlr_scene_node_set_enabled(&l->scene->node, false);
	if (l == exclusive_focus)
		exclusive_focus = NULL;
	if (l->layer_surface->output && (l->mon = l->layer_surface->output->data))
		arrangelayers(l->mon);
	if (l->layer_surface->surface == seat->keyboard_state.focused_surface)
		focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void unmapnotify(struct wl_listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown. */
	Client *c = wl_container_of(listener, c, unmap);
	Monitor *m;
	c->iskilling = 1;

	if (animations && !c->is_clip_to_hide && !c->isminied &&
		(!c->mon || VISIBLEON(c, c->mon)))
		init_fadeout_client(c);

	if (c->swallowedby) {
		c->swallowedby->mon = c->mon;
		swallow(c->swallowedby, c);
	}

	if (c == grabc) {
		cursor_mode = CurNormal;
		grabc = NULL;
	}

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		if (c == m->sel) {
			m->sel = NULL;
		}
		if (c == m->prevsel) {
			m->prevsel = NULL;
		}
	}

	if (c->mon && c->mon == selmon) {
		Client *nextfocus = focustop(selmon);

		if (nextfocus) {
			focusclient(nextfocus, 0);
		}

		if (!nextfocus && selmon->isoverview) {
			Arg arg = {0};
			toggleoverview(&arg);
		}
	}

	if (client_is_unmanaged(c)) {
#ifdef XWAYLAND
		if (client_is_x11(c)) {
			wl_list_remove(&c->set_geometry.link);
		}
#endif
		if (c == exclusive_focus)
			exclusive_focus = NULL;
		if (client_surface(c) == seat->keyboard_state.focused_surface)
			focusclient(focustop(selmon), 1);
	} else {
		if (!c->swallowing)
			wl_list_remove(&c->link);
		setmon(c, NULL, 0, true);
		if (!c->swallowing)
			wl_list_remove(&c->flink);
	}

	if (c->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
		c->foreign_toplevel = NULL;
	}

	if (c->swallowedby) {
		setfullscreen(c->swallowedby, c->isfullscreen);
		setmaxmizescreen(c->swallowedby, c->ismaxmizescreen);
		c->swallowedby->swallowing = NULL;
		c->swallowedby = NULL;
	}

	if (c->swallowing) {
		c->swallowing->swallowedby = NULL;
		c->swallowing = NULL;
	}

	wlr_scene_node_destroy(&c->scene->node);
	printstatus();
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void updatemons(struct wl_listener *listener, void *data) {
	/*
	 * Called whenever the output layout changes: adding or removing a
	 * monitor, changing an output's mode or position, etc. This is where
	 * the change officially happens and we update geometry, window
	 * positions, focus, and the stored configuration in wlroots'
	 * output-manager implementation.
	 */
	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	Client *c;
	struct wlr_output_configuration_head_v1 *config_head;
	Monitor *m;
	int mon_pos_offsetx, mon_pos_offsety, oldx, oldy;

	/* First remove from the layout the disabled monitors */
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled || m->asleep)
			continue;
		config_head =
			wlr_output_configuration_head_v1_create(config, m->wlr_output);
		config_head->state.enabled = 0;
		/* Remove this output from the layout to avoid cursor enter inside it */
		wlr_output_layout_remove(output_layout, m->wlr_output);
		closemon(m);
		m->m = m->w = (struct wlr_box){0};
	}
	/* Insert outputs that need to */
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled &&
			!wlr_output_layout_get(output_layout, m->wlr_output))
			wlr_output_layout_add_auto(output_layout, m->wlr_output);
	}

	/* Now that we update the output layout we can get its box */
	wlr_output_layout_get_box(output_layout, NULL, &sgeom);

	wlr_scene_node_set_position(&root_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(root_bg, sgeom.width, sgeom.height);

	/* Make sure the clients are hidden when dwl is locked */
	wlr_scene_node_set_position(&locked_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(locked_bg, sgeom.width, sgeom.height);

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled)
			continue;
		config_head =
			wlr_output_configuration_head_v1_create(config, m->wlr_output);

		oldx = m->m.x;
		oldy = m->m.y;
		/* Get the effective monitor geometry to use for surfaces */
		wlr_output_layout_get_box(output_layout, m->wlr_output, &m->m);
		m->w = m->m;
		mon_pos_offsetx = m->m.x - oldx;
		mon_pos_offsety = m->m.y - oldy;

		wl_list_for_each(c, &clients, link) {
			// floating window position auto adjust the change of monitor
			// position
			if (c->isfloating && c->mon == m) {
				c->geom.x += mon_pos_offsetx;
				c->geom.y += mon_pos_offsety;
				c->oldgeom = c->geom;
				resize(c, c->geom, 1);
			}

			// restore window to old monitor
			if (c->mon && c->mon != m && client_surface(c)->mapped &&
				strcmp(c->oldmonname, m->wlr_output->name) == 0) {
				client_change_mon(c, m);
			}
		}

		/*
		 must put it under the floating window position adjustment,
		 Otherwise, incorrect floating window calculations will occur here.
		 */
		wlr_scene_output_set_position(m->scene_output, m->m.x, m->m.y);

		if (m->lock_surface) {
			struct wlr_scene_tree *scene_tree = m->lock_surface->surface->data;
			wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
			wlr_session_lock_surface_v1_configure(m->lock_surface, m->m.width,
												  m->m.height);
		}

		/* Calculate the effective monitor geometry to use for clients */
		arrangelayers(m);
		/* Don't move clients to the left output when plugging monitors */
		arrange(m, false);
		/* make sure fullscreen clients have the right size */
		if ((c = focustop(m)) && c->isfullscreen)
			resize(c, m->m, 0);

		/* Try to re-set the gamma LUT when updating monitors,
		 * it's only really needed when enabling a disabled output, but meh. */
		m->gamma_lut_changed = 1;

		config_head->state.x = m->m.x;
		config_head->state.y = m->m.y;

		if (!selmon)
			selmon = m;
	}

	if (selmon && selmon->wlr_output->enabled) {
		wl_list_for_each(c, &clients, link) {
			if (!c->mon && client_surface(c)->mapped) {
				client_change_mon(c, selmon);
			}
		}
		focusclient(focustop(selmon), 1);
		if (selmon->lock_surface) {
			client_notify_enter(selmon->lock_surface->surface,
								wlr_seat_get_keyboard(seat));
			client_activate_surface(selmon->lock_surface->surface, 1);
		}
	}

	/* FIXME: figure out why the cursor image is at 0,0 after turning all
	 * the monitors on.
	 * Move the cursor image where it used to be. It does not generate a
	 * wl_pointer.motion event for the clients, it's only the image what it's
	 * at the wrong position after all. */
	wlr_cursor_move(cursor, NULL, 0, 0);

	wlr_output_manager_v1_set_configuration(output_mgr, config);
}

void updatetitle(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_title);

	if (!c || c->iskilling)
		return;

	const char *title;
	title = client_get_title(c);
	if (title && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel, title);
	if (c == focustop(c->mon))
		printstatus();
}

void // 17 fix to 0.5
urgent(struct wl_listener *listener, void *data) {
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	Client *c = NULL;
	toplevel_from_wlr_surface(event->surface, &c, NULL);

	if (!c || !c->foreign_toplevel)
		return;

	if (focus_on_activate && c != selmon->sel) {
		view(&(Arg){.ui = c->tags}, true);
		focusclient(c, 1);
	} else if (c != focustop(selmon)) {
		if (client_surface(c)->mapped)
			client_set_border_color(c, urgentcolor);
		c->isurgent = 1;
		printstatus();
	}
}

void bind_to_view(const Arg *arg) { view(arg, true); }

void view_in_mon(const Arg *arg, bool want_animation, Monitor *m) {
	unsigned int i, tmptag;

	if (!m || (arg->ui != ~0 && m->isoverview)) {
		return;
	}

	if (arg->ui == 0)
		return;

	if ((m->tagset[m->seltags] & arg->ui & TAGMASK) != 0) {
		want_animation = false;
	}

	m->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK) {
		m->tagset[m->seltags] = arg->ui & TAGMASK;
		tmptag = m->pertag->curtag;

		if (arg->ui == ~0)
			m->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i) && i < LENGTH(tags) && arg->ui != 0;
				 i++)
				;
			m->pertag->curtag = i >= LENGTH(tags) ? LENGTH(tags) : i + 1;
		}

		m->pertag->prevtag =
			tmptag == m->pertag->curtag ? m->pertag->prevtag : tmptag;
	} else {
		tmptag = m->pertag->prevtag;
		m->pertag->prevtag = m->pertag->curtag;
		m->pertag->curtag = tmptag;
	}

	focusclient(focustop(m), 1);
	arrange(m, want_animation);
	printstatus();
}

void view(const Arg *arg, bool want_animation) {
	view_in_mon(arg, want_animation, selmon);
}

void viewtoleft(const Arg *arg) {
	unsigned int tmptag;
	unsigned int target = selmon->tagset[selmon->seltags];

	if (selmon->isoverview || selmon->pertag->curtag == 0) {
		return;
	}

	target >>= 1;

	if (target == 0) {
		return;
	}

	if (!selmon || (target) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (target) {
		selmon->tagset[selmon->seltags] = target;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = selmon->pertag->curtag - 1;
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	focusclient(focustop(selmon), 1);
	arrange(selmon, true);
	printstatus();
}

void viewtoright_have_client(const Arg *arg) {
	unsigned int tmptag;
	Client *c;
	unsigned int found = 0;
	unsigned int n = 1;
	unsigned int target = selmon->tagset[selmon->seltags];

	if (selmon->isoverview || selmon->pertag->curtag == 0) {
		return;
	}

	for (target <<= 1; target & TAGMASK && n <= LENGTH(tags);
		 target <<= 1, n++) {
		wl_list_for_each(c, &clients, link) {
			if (c->mon == selmon && target & c->tags) {
				found = 1;
				break;
			}
		}
		if (found) {
			break;
		}
	}

	if (!(target & TAGMASK)) {
		return;
	}

	if (!selmon || (target) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */

	int new_tag = selmon->pertag->curtag + n;
	if (new_tag > LENGTH(tags))
		new_tag = LENGTH(tags);

	if (target) {
		selmon->tagset[selmon->seltags] = target;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = new_tag;
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	focusclient(focustop(selmon), 1);
	arrange(selmon, true);
	printstatus();
}

void viewtoright(const Arg *arg) {
	if (selmon->isoverview || selmon->pertag->curtag == 0) {
		return;
	}
	unsigned int tmptag;
	unsigned int target = selmon->tagset[selmon->seltags];
	target <<= 1;

	if (!selmon || (target) == selmon->tagset[selmon->seltags])
		return;
	if (!(target & TAGMASK)) {
		return;
	}
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (target) {
		selmon->tagset[selmon->seltags] = target;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = selmon->pertag->curtag + 1;
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	focusclient(focustop(selmon), 1);
	arrange(selmon, true);
	printstatus();
}

void viewtoleft_have_client(const Arg *arg) {
	unsigned int tmptag;
	Client *c;
	unsigned int found = 0;
	unsigned int n = 1;
	unsigned int target = selmon->tagset[selmon->seltags];

	if (selmon->isoverview || selmon->pertag->curtag == 0) {
		return;
	}

	for (target >>= 1; target > 0 && n <= LENGTH(tags); target >>= 1, n++) {
		wl_list_for_each(c, &clients, link) {
			if (c->mon == selmon && target & c->tags) {
				found = 1;
				break;
			}
		}
		if (found) {
			break;
		}
	}

	if (target == 0) {
		return;
	}

	if (!selmon || (target) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */

	int new_tag = selmon->pertag->curtag - n;
	if (new_tag < 1)
		new_tag = 1;

	if (target) {
		selmon->tagset[selmon->seltags] = target;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = new_tag;
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	focusclient(focustop(selmon), 1);
	arrange(selmon, true);
	printstatus();
}

void tagtoleft(const Arg *arg) {
	if (selmon->sel != NULL &&
		__builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1 &&
		selmon->tagset[selmon->seltags] > 1) {
		tag(&(Arg){.ui = selmon->tagset[selmon->seltags] >> 1});
	}
}

void tagtoright(const Arg *arg) {
	if (selmon->sel != NULL &&
		__builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1 &&
		selmon->tagset[selmon->seltags] & (TAGMASK >> 1)) {
		tag(&(Arg){.ui = selmon->tagset[selmon->seltags] << 1});
	}
}

void virtualkeyboard(struct wl_listener *listener, void *data) {
	struct wlr_virtual_keyboard_v1 *kb = data;
	/* virtual keyboards shouldn't share keyboard group */
	KeyboardGroup *group = createkeyboardgroup();
	/* Set the keymap to match the group keymap */
	wlr_keyboard_set_keymap(&kb->keyboard, group->wlr_group->keyboard.keymap);
	LISTEN(&kb->keyboard.base.events.destroy, &group->destroy,
		   destroykeyboardgroup);

	/* Add the new keyboard to the group */
	wlr_keyboard_group_add_keyboard(group->wlr_group, &kb->keyboard);
}

void warp_cursor(const Client *c) {
	if (cursor->x < c->geom.x || cursor->x > c->geom.x + c->geom.width ||
		cursor->y < c->geom.y || cursor->y > c->geom.y + c->geom.height) {
		wlr_cursor_warp_closest(cursor, NULL, c->geom.x + c->geom.width / 2.0,
								c->geom.y + c->geom.height / 2.0);
		motionnotify(0, NULL, 0, 0, 0, 0);
	}
}

void warp_cursor_to_selmon(Monitor *m) {

	wlr_cursor_warp_closest(cursor, NULL, m->w.x + m->w.width / 2.0,
							m->w.y + m->w.height / 2.0);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
	handlecursoractivity();
}

void virtualpointer(struct wl_listener *listener, void *data) {
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	struct wlr_input_device *device = &event->new_pointer->pointer.base;

	wlr_cursor_attach_input_device(cursor, device);
	if (event->suggested_output)
		wlr_cursor_map_input_to_output(cursor, device, event->suggested_output);

	handlecursoractivity();
}

Monitor *xytomon(double x, double y) {
	struct wlr_output *o = wlr_output_layout_output_at(output_layout, x, y);
	return o ? o->data : NULL;
}

void xytonode(double x, double y, struct wlr_surface **psurface, Client **pc,
			  LayerSurface **pl, double *nx, double *ny) {
	struct wlr_scene_node *node, *pnode;
	struct wlr_surface *surface = NULL;
	Client *c = NULL;
	LayerSurface *l = NULL;
	int layer;

	for (layer = NUM_LAYERS - 1; !surface && layer >= 0; layer--) {

		// ignore text-input layer
		if (layer == LyrIMPopup)
			continue;

		if (layer == LyrFadeOut)
			continue;

		if (!(node = wlr_scene_node_at(&layers[layer]->node, x, y, nx, ny)))
			continue;

		if (node->type == WLR_SCENE_NODE_BUFFER)
			surface = wlr_scene_surface_try_from_buffer(
						  wlr_scene_buffer_from_node(node))
						  ->surface;
		/* Walk the tree to find a node that knows the client */
		for (pnode = node; pnode && !c; pnode = &pnode->parent->node)
			c = pnode->data;
		if (c && c->type == LayerShell) {
			c = NULL;
			l = pnode->data;
		}
	}

	if (psurface)
		*psurface = surface;
	if (pc)
		*pc = c;
	if (pl)
		*pl = l;
}

void toggleglobal(const Arg *arg) {
	if (!selmon->sel)
		return;
	if (selmon->sel->is_in_scratchpad) {
		selmon->sel->is_in_scratchpad = 0;
		selmon->sel->is_scratchpad_show = 0;
		selmon->sel->isnamedscratchpad = 0;
	}
	selmon->sel->isglobal ^= 1;
	//   selmon->sel->tags =
	//       selmon->sel->isglobal ? TAGMASK : selmon->tagset[selmon->seltags];
	//   focustop(selmon);
	setborder_color(selmon->sel);
}

void zoom(const Arg *arg) {
	Client *c, *sel = focustop(selmon);

	if (!sel || !selmon ||
		!selmon->pertag->ltidxs[selmon->pertag->curtag]->arrange ||
		sel->isfloating)
		return;

	/* Search for the first tiled window that is not sel, marking sel as
	 * NULL if we pass it along the way */
	wl_list_for_each(c, &clients,
					 link) if (VISIBLEON(c, selmon) && !c->isfloating) {
		if (c != sel)
			break;
		sel = NULL;
	}

	/* Return if no other tiled window was found */
	if (&c->link == &clients)
		return;

	/* If we passed sel, move c to the front; otherwise, move sel to the
	 * front */
	if (!sel)
		sel = c;
	wl_list_remove(&sel->link);
	wl_list_insert(&clients, &sel->link);

	focusclient(sel, 1);
	arrange(selmon, false);
}

void resizewin(const Arg *arg) {
	Client *c;
	c = selmon->sel;
	if (!c || c->isfullscreen)
		return;
	if (!c->isfloating)
		togglefloating(NULL);

	switch (arg->ui) {
	case NUM_TYPE_MINUS:
		c->geom.width -= arg->i;
		break;
	case NUM_TYPE_PLUS:
		c->geom.width += arg->i;
		break;
	default:
		c->geom.width = arg->i;
		break;
	}

	switch (arg->ui2) {
	case NUM_TYPE_MINUS:
		c->geom.height -= arg->i2;
		break;
	case NUM_TYPE_PLUS:
		c->geom.height += arg->i2;
		break;
	default:
		c->geom.height = arg->i2;
		break;
	}

	c->oldgeom = c->geom;
	resize(c, c->geom, 0);
}

void movewin(const Arg *arg) {
	Client *c;
	c = selmon->sel;
	if (!c || c->isfullscreen)
		return;
	if (!c->isfloating)
		togglefloating(NULL);

	switch (arg->ui) {
	case NUM_TYPE_MINUS:
		c->geom.x -= arg->i;
		break;
	case NUM_TYPE_PLUS:
		c->geom.x += arg->i;
		break;
	default:
		c->geom.x = arg->i;
		break;
	}

	switch (arg->ui2) {
	case NUM_TYPE_MINUS:
		c->geom.y -= arg->i2;
		break;
	case NUM_TYPE_PLUS:
		c->geom.y += arg->i2;
		break;
	default:
		c->geom.y = arg->i2;
		break;
	}

	c->oldgeom = c->geom;
	resize(c, c->geom, 0);
}

void smartmovewin(const Arg *arg) {
	Client *c, *tc;
	int nx, ny;
	int buttom, top, left, right, tar;
	c = selmon->sel;
	if (!c || c->isfullscreen)
		return;
	if (!c->isfloating)
		setfloating(selmon->sel, true);
	nx = c->geom.x;
	ny = c->geom.y;

	switch (arg->i) {
	case UP:
		tar = -99999;
		top = c->geom.y;
		ny -= c->mon->w.height / 4;

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.x + c->geom.width < tc->geom.x ||
				c->geom.x > tc->geom.x + tc->geom.width)
				continue;
			buttom = tc->geom.y + tc->geom.height + gappiv;
			if (top > buttom && ny < buttom) {
				tar = MAX(tar, buttom);
			};
		}

		ny = tar == -99999 ? ny : tar;
		ny = MAX(ny, c->mon->w.y + c->mon->gappov);
		break;
	case DOWN:
		tar = 99999;
		buttom = c->geom.y + c->geom.height;
		ny += c->mon->w.height / 4;

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.x + c->geom.width < tc->geom.x ||
				c->geom.x > tc->geom.x + tc->geom.width)
				continue;
			top = tc->geom.y - gappiv;
			if (buttom < top && (ny + c->geom.height) > top) {
				tar = MIN(tar, top - c->geom.height);
			};
		}
		ny = tar == 99999 ? ny : tar;
		ny = MIN(ny, c->mon->w.y + c->mon->w.height - c->geom.height -
						 c->mon->gappov);
		break;
	case LEFT:
		tar = -99999;
		left = c->geom.x;
		nx -= c->mon->w.width / 6;

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.y + c->geom.height < tc->geom.y ||
				c->geom.y > tc->geom.y + tc->geom.height)
				continue;
			right = tc->geom.x + tc->geom.width + gappih;
			if (left > right && nx < right) {
				tar = MAX(tar, right);
			};
		}

		nx = tar == -99999 ? nx : tar;
		nx = MAX(nx, c->mon->w.x + c->mon->gappoh);
		break;
	case RIGHT:
		tar = 99999;
		right = c->geom.x + c->geom.width;
		nx += c->mon->w.width / 6;
		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.y + c->geom.height < tc->geom.y ||
				c->geom.y > tc->geom.y + tc->geom.height)
				continue;
			left = tc->geom.x - gappih;
			if (right < left && (nx + c->geom.width) > left) {
				tar = MIN(tar, left - c->geom.width);
			};
		}
		nx = tar == 99999 ? nx : tar;
		nx = MIN(nx, c->mon->w.x + c->mon->w.width - c->geom.width -
						 c->mon->gappoh);
		break;
	}

	c->oldgeom = (struct wlr_box){
		.x = nx, .y = ny, .width = c->geom.width, .height = c->geom.height};

	resize(c, c->oldgeom, 1);
}

void smartresizewin(const Arg *arg) {
	Client *c, *tc;
	int nw, nh;
	int buttom, top, left, right, tar;
	c = selmon->sel;
	if (!c || c->isfullscreen)
		return;
	if (!c->isfloating)
		setfloating(c, true);
	nw = c->geom.width;
	nh = c->geom.height;

	switch (arg->i) {
	case UP:
		nh -= selmon->w.height / 8;
		nh = MAX(nh, selmon->w.height / 10);
		break;
	case DOWN:
		tar = -99999;
		buttom = c->geom.y + c->geom.height;
		nh += selmon->w.height / 8;

		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.x + c->geom.width < tc->geom.x ||
				c->geom.x > tc->geom.x + tc->geom.width)
				continue;
			top = tc->geom.y - gappiv;
			if (buttom < top && (nh + c->geom.y) > top) {
				tar = MAX(tar, top - c->geom.y);
			};
		}
		nh = tar == -99999 ? nh : tar;
		if (c->geom.y + nh + gappov > selmon->w.y + selmon->w.height)
			nh = selmon->w.y + selmon->w.height - c->geom.y - gappov;
		break;
	case LEFT:
		nw -= selmon->w.width / 16;
		nw = MAX(nw, selmon->w.width / 10);
		break;
	case RIGHT:
		tar = 99999;
		right = c->geom.x + c->geom.width;
		nw += selmon->w.width / 16;
		wl_list_for_each(tc, &clients, link) {
			if (!VISIBLEON(tc, selmon) || !tc->isfloating || tc == c)
				continue;
			if (c->geom.y + c->geom.height < tc->geom.y ||
				c->geom.y > tc->geom.y + tc->geom.height)
				continue;
			left = tc->geom.x - gappih;
			if (right < left && (nw + c->geom.x) > left) {
				tar = MIN(tar, left - c->geom.x);
			};
		}

		nw = tar == 99999 ? nw : tar;
		if (c->geom.x + nw + gappoh > selmon->w.x + selmon->w.width)
			nw = selmon->w.x + selmon->w.width - c->geom.x - gappoh;
		break;
	}

	c->oldgeom = (struct wlr_box){
		.x = c->geom.x, .y = c->geom.y, .width = nw, .height = nh};

	resize(c, c->oldgeom, 1);
}

#ifdef XWAYLAND
void activatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, activate);
	bool need_arrange = false;

	if (!c || c->iskilling || !c->foreign_toplevel || client_is_unmanaged(c))
		return;

	if (c && c->swallowing)
		return;

	if (c->isminied) {
		c->isminied = 0;
		c->tags = c->mini_restore_tag;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel,
													 false);
		setborder_color(c);
		if (VISIBLEON(c, c->mon)) {
			need_arrange = true;
		}
	}

	if (focus_on_activate && c != selmon->sel) {
		view(&(Arg){.ui = c->tags}, true);
		wlr_xwayland_surface_activate(c->surface.xwayland, 1);
		focusclient(c, 1);
		need_arrange = false;
	} else if (c != focustop(selmon)) {
		c->isurgent = 1;
		if (client_surface(c)->mapped)
			client_set_border_color(c, urgentcolor);
	}

	if (need_arrange) {
		arrange(c->mon, false);
	}

	printstatus();
}

void configurex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	if (!client_surface(c) || !client_surface(c)->mapped) {
		wlr_xwayland_surface_configure(c->surface.xwayland, event->x, event->y,
									   event->width, event->height);
		return;
	}
	if (client_is_unmanaged(c)) {
		wlr_scene_node_set_position(&c->scene->node, event->x, event->y);
		wlr_xwayland_surface_configure(c->surface.xwayland, event->x, event->y,
									   event->width, event->height);
		return;
	}
	if ((c->isfloating && c != grabc) ||
		!c->mon->pertag->ltidxs[c->mon->pertag->curtag]->arrange) {
		resize(c,
			   (struct wlr_box){.x = event->x - c->bw,
								.y = event->y - c->bw,
								.width = event->width + c->bw * 2,
								.height = event->height + c->bw * 2},
			   0);
	} else {
		arrange(c->mon, false);
	}
}

void createnotifyx11(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = data;
	Client *c;

	/* Allocate a Client for this surface */
	c = xsurface->data = ecalloc(1, sizeof(*c));
	c->surface.xwayland = xsurface;
	c->type = X11;
	/* Listen to the various events it can emit */
	LISTEN(&xsurface->events.associate, &c->associate, associatex11);
	LISTEN(&xsurface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xsurface->events.dissociate, &c->dissociate, dissociatex11);
	LISTEN(&xsurface->events.request_activate, &c->activate, activatex11);
	LISTEN(&xsurface->events.request_configure, &c->configure, configurex11);
	LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen,
		   fullscreennotify);
	LISTEN(&xsurface->events.set_hints, &c->set_hints, sethints);
	LISTEN(&xsurface->events.set_title, &c->set_title, updatetitle);
	LISTEN(&xsurface->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&xsurface->events.request_minimize, &c->minimize, minimizenotify);
}

void associatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, associate);

	LISTEN(&client_surface(c)->events.map, &c->map, mapnotify);
	LISTEN(&client_surface(c)->events.unmap, &c->unmap, unmapnotify);
}

void dissociatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, dissociate);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
}

void sethints(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_hints);
	struct wlr_surface *surface = client_surface(c);
	if (c == focustop(selmon) || !c || !c->surface.xwayland->hints)
		return;

	c->isurgent = xcb_icccm_wm_hints_get_urgency(c->surface.xwayland->hints);
	printstatus();

	if (c->isurgent && surface && surface->mapped)
		client_set_border_color(c, urgentcolor);
}

void xwaylandready(struct wl_listener *listener, void *data) {
	struct wlr_xcursor *xcursor;

	/* assign the one and only seat */
	wlr_xwayland_set_seat(xwayland, seat);

	/* Set the default XWayland cursor to match the rest of dwl. */
	if ((xcursor = wlr_xcursor_manager_get_xcursor(cursor_mgr, "default", 1)))
		wlr_xwayland_set_cursor(
			xwayland, xcursor->images[0]->buffer, xcursor->images[0]->width * 4,
			xcursor->images[0]->width, xcursor->images[0]->height,
			xcursor->images[0]->hotspot_x, xcursor->images[0]->hotspot_y);
}

static void setgeometrynotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_geometry);

	wlr_scene_node_set_position(&c->scene->node, c->surface.xwayland->x,
								c->surface.xwayland->y);
	motionnotify(0, NULL, 0, 0, 0, 0);
}
#endif

int main(int argc, char *argv[]) {
	char *startup_cmd = NULL;
	int c;

	while ((c = getopt(argc, argv, "s:hdv")) != -1) {
		if (c == 's')
			startup_cmd = optarg;
		else if (c == 'd')
			log_level = WLR_DEBUG;
		else if (c == 'v')
			die("maomao " VERSION);
		else
			goto usage;
	}
	if (optind < argc)
		goto usage;

	/* Wayland requires XDG_RUNTIME_DIR for creating its communications socket
	 */
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");
	setup();
	run(startup_cmd);
	cleanup();
	return EXIT_SUCCESS;

usage:
	die("Usage: %s [-v] [-d] [-s startup command]", argv[0]);
}
