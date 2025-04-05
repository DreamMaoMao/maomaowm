/*
 * See LICENSE file for copyright and license details.
 */
#include <execinfo.h>
#include <getopt.h>
#include <libinput.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/util/region.h>
#include <wordexp.h>
// #include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
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
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <X11/Xlib.h>
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>
#endif

#include "dwl-ipc-unstable-v2-protocol.h"
#include "util.h"
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

/* macros */
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define GEZERO(A) ((A) >= 0 ? (A) : 0)
#define ISTILED(A) (!(A)->isfloating && !(A)->isminied && !(A)->iskilling && !(A)->isfloating && !(A)->ismaxmizescreen && !(A)->isfullscreen)
#define CLEANMASK(mask) (mask & ~WLR_MODIFIER_CAPS)
#define VISIBLEON(C, M)                                                        \
  ((M) && (C)->mon == (M) && ((C)->tags & (M)->tagset[(M)->seltags]))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define END(A) ((A) + LENGTH(A))
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define LISTEN(E, L, H) wl_signal_add((E), ((L)->notify = (H), (L)))
#define ISFULLSCREEN(A)                                                        \
  ((A)->isfullscreen || (A)->ismaxmizescreen ||                                \
   (A)->overview_ismaxmizescreenbak || (A)->overview_isfullscreenbak)
#define LISTEN_STATIC(E, H)                                                    \
  do {                                                                         \
    static struct wl_listener _l = {.notify = (H)};                            \
    wl_signal_add((E), &_l);                                                   \
  } while (0)

/* enums */
/* enums */
enum { SWIPE_UP,SWIPE_DOWN, SWIPE_LEFT, SWIPE_RIGHT };
enum { CurNormal, CurPressed, CurMove, CurResize }; /* cursor */
enum { XDGShell, LayerShell, X11 };                 /* client types */
enum { AxisUp, AxisDown, AxisLeft, AxisRight };     // 滚轮滚动的方向
enum {
  LyrBg,
  LyrBottom,
  LyrTile,
  LyrFloat,
  LyrFS,
  LyrTop,
  LyrOverlay,
#ifdef IM
  LyrIMPopup,
#endif
  LyrFadeOut,
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
enum { UP, DOWN, LEFT, RIGHT, UNDIR }; /* movewin */
enum { NONE, OPEN, MOVE, CLOSE, TAG };

typedef struct {
  int i;
  float f;
  char *v;
  unsigned int ui;
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
  uint32_t total_frames;
  uint32_t passed_frames;
  uint32_t duration;
  struct wlr_box initial;
  struct wlr_box current;
  int action;
};

typedef struct Pertag Pertag;
typedef struct Monitor Monitor;
typedef struct Client Client;

typedef struct {
  float width_scale;
  float height_scale;
  int width;
  int height;
  Monitor *m;
} animationScale;

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
  struct wlr_box prev; /* layout-relative, includes border */
#ifdef XWAYLAND
  struct wl_listener activate;
  struct wl_listener associate;
  struct wl_listener dissociate;
  struct wl_listener configure;
  struct wl_listener set_hints;
#endif
  unsigned int bw;
  unsigned int tags, oldtags;
  bool dirty;
  uint32_t configure_serial;
  struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;
  int isfloating, isurgent, isfullscreen, need_float_size_reduce, isminied;
  int ismaxmizescreen;
  int overview_backup_bw;
  int fullscreen_backup_x, fullscreen_backup_y, fullscreen_backup_w,
      fullscreen_backup_h;
  int overview_isfullscreenbak, overview_ismaxmizescreenbak,
      overview_isfloatingbak;
  uint32_t resize; /* configure serial of a pending resize */

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
  struct wlr_box bounds;
  bool resizing;
  bool is_open_animation;
  bool is_restoring_from_ov;
  float scroller_proportion;
  bool need_output_flush;
  struct dwl_animation animation;
  bool is_fadeout_client;
  int isterm, noswallow;
  // struct wl_event_source *timer_tick;
  pid_t pid;
  Client *swallowing, *swallowedby;
  bool is_clip_to_hide;
  bool need_scale_first_frame;
};


typedef struct {
  struct wl_list link;
  struct wl_resource *resource;
  Monitor *mon;
} DwlIpcOutput;

typedef struct {
  uint32_t mod;
  xkb_keysym_t keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  struct wl_list link;
  struct wlr_keyboard *wlr_keyboard;

  int nsyms;
  const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
  uint32_t mods;               /* invalid if nsyms == 0 */
  struct wl_event_source *key_repeat_source;

  struct wl_listener modifiers;
  struct wl_listener key;
  struct wl_listener destroy;
} Keyboard;

typedef struct {
  /* Must keep these three elements in this order */
  unsigned int type; /* LayerShell */
  struct wlr_box geom;
  Monitor *mon;
  struct wlr_scene_tree *scene;
  struct wlr_scene_tree *popups;
  struct wlr_scene_layer_surface_v1 *scene_layer;
  struct wl_list link;
  int mapped;
  struct wlr_layer_surface_v1 *layer_surface;

  struct wl_listener destroy;
  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener surface_commit;
} LayerSurface;

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *, unsigned int, unsigned int);
  const char *name;
} Layout;

struct Monitor {
  struct wl_list link;
  struct wlr_output *wlr_output;
  struct wlr_scene_output *scene_output;
  // struct wlr_scene_rect *fullscreen_bg; /* See createmon() for info */
  struct wl_listener frame;
  struct wl_listener destroy;
  struct wl_listener request_state;
  struct wl_listener destroy_lock_surface;
  struct wlr_session_lock_surface_v1 *lock_surface;
  struct wlr_box m;         /* monitor area, layout-relative */
  struct wlr_box w;         /* window area, layout-relative */
  struct wl_list layers[4]; /* LayerSurface::link */
  const Layout *lt;
  unsigned int seltags;
  uint32_t tagset[2];
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
static void logtofile(const char *fmt, ...); // 日志函数
static void lognumtofile(float num);         // 日志函数
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
static void autostartexec(void); // 自启动命令执行
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
                            void *data);                          // 退出清理
static void cleanupmon(struct wl_listener *listener, void *data); // 退出清理
static void closemon(Monitor *m);                                 // 退出清理
static void toggle_hotarea(int x_root, int y_root);               // 触发热区
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
static void destroysessionmgr(struct wl_listener *listener, void *data);
static Monitor *dirtomon(enum wlr_direction dir);
static void setcursorshape(struct wl_listener *listener, void *data);
static void dwl_ipc_manager_bind(struct wl_client *client, void *data,
                                 uint32_t version, uint32_t id);
static void dwl_ipc_manager_destroy(struct wl_resource *resource);
static void dwl_ipc_manager_get_output(struct wl_client *client,
                                       struct wl_resource *resource,
                                       uint32_t id, struct wl_resource *output);
static void dwl_ipc_manager_release(struct wl_client *client,
                                    struct wl_resource *resource);
static void dwl_ipc_output_destroy(struct wl_resource *resource);
static void powermgrsetmode(struct wl_listener *listener, void *data);
static void dwl_ipc_output_printstatus(Monitor *monitor);
static void dwl_ipc_output_printstatus_to(DwlIpcOutput *ipc_output);
static void dwl_ipc_output_set_client_tags(struct wl_client *client,
                                           struct wl_resource *resource,
                                           uint32_t and_tags,
                                           uint32_t xor_tags);
static void dwl_ipc_output_set_layout(struct wl_client *client,
                                      struct wl_resource *resource,
                                      uint32_t index);
static void dwl_ipc_output_set_tags(struct wl_client *client,
                                    struct wl_resource *resource,
                                    uint32_t tagmask, uint32_t toggle_tagset);
static void dwl_ipc_output_release(struct wl_client *client,
                                   struct wl_resource *resource);
static void dwl_ipc_output_quit(struct wl_client *client,struct wl_resource *resource);

static void focusclient(Client *c, int lift);

static void setborder_color(Client *c);
static Client *focustop(Monitor *m);
static void fullscreennotify(struct wl_listener *listener, void *data);

static int keyrepeat(void *data);

static void inputdevice(struct wl_listener *listener, void *data);
static int keybinding(uint32_t mods, xkb_keysym_t sym);
static void keypress(struct wl_listener *listener, void *data);
static void keypressmod(struct wl_listener *listener, void *data);
static bool keypressglobal(struct wlr_surface *last_surface, struct wlr_keyboard *keyboard, struct wlr_keyboard_key_event *event, uint32_t mods, xkb_keysym_t keysym);
static void locksession(struct wl_listener *listener, void *data);
static void maplayersurfacenotify(struct wl_listener *listener, void *data);
static void mapnotify(struct wl_listener *listener, void *data);
static void maximizenotify(struct wl_listener *listener, void *data);
static void minimizenotify(struct wl_listener *listener, void *data);
static void monocle(Monitor *m, unsigned int gappo, unsigned int gappi);
static void motionabsolute(struct wl_listener *listener, void *data);
static void motionnotify(uint32_t time, struct wlr_input_device *device,
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
                         double sy, uint32_t time);
static void printstatus(void);
static void quitsignal(int signo);
static void rendermon(struct wl_listener *listener, void *data);
static void requestdecorationmode(struct wl_listener *listener, void *data);
static void requeststartdrag(struct wl_listener *listener, void *data);
static void resize(Client *c, struct wlr_box geo, int interact);
static void run(char *startup_cmd);
static void setcursor(struct wl_listener *listener, void *data);
static void setfloating(Client *c, int floating);
static void setfullscreen(Client *c, int fullscreen);
static void setmaxmizescreen(Client *c, int maxmizescreen);
static void setgaps(int oh, int ov, int ih, int iv);

static void setmon(Client *c, Monitor *m, unsigned int newtags, bool focus);
static void setpsel(struct wl_listener *listener, void *data);
static void setsel(struct wl_listener *listener, void *data);
static void setup(void);
static void sigchld(int unused);
static void startdrag(struct wl_listener *listener, void *data);

static void setgamma(struct wl_listener *listener, void *data);
static void tile(Monitor *m, unsigned int gappo, unsigned int uappi);
static void overview(Monitor *m, unsigned int gappo, unsigned int gappi);
static void grid(Monitor *m, unsigned int gappo, unsigned int uappi);
static void scroller(Monitor *m, unsigned int gappo, unsigned int uappi);
static void dwindle(Monitor *mon, unsigned int gappo, unsigned int gappi);
static void spiral(Monitor *mon, unsigned int gappo, unsigned int gappi);
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

static void warp_cursor_to_selmon(const Monitor *m);
unsigned int want_restore_fullscreen(Client *target_client);
static void overview_restore(Client *c, const Arg *arg);
static void overview_backup(Client *c);
static int applyrulesgeom(Client *c);
static void set_minized(Client *c);

static void show_scratchpad(Client *c);
static void show_hide_client(Client *c);
static void tag_client(const Arg *arg, Client *target_client);

static void handle_foreign_activate_request(struct wl_listener *listener,
                                            void *data);
static void handle_foreign_fullscreen_request(struct wl_listener *listener,
                                              void *data);
static void handle_foreign_close_request(struct wl_listener *listener,
                                         void *data);
static void handle_foreign_destroy(struct wl_listener *listener, void *data);

static struct wlr_box setclient_coordinate_center(struct wlr_box geom);
static unsigned int get_tags_first_tag(unsigned int tags);

void client_commit(Client *c);
void apply_border(Client *c, struct wlr_box clip_box, int offsetx, int offsety);
void client_set_opacity(Client *c, double opacity);
void init_baked_points(void);
void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int sx, int sy,
                                void *data);
void view_in_mon(const Arg *arg, bool want_animation, Monitor *m);

Client *direction_select(const Arg *arg);
void bind_to_view(const Arg *arg);
void toggletag(const Arg *arg);
void toggleview(const Arg *arg);
void tag(const Arg *arg);
void incihgaps(const Arg *arg);
void incivgaps(const Arg *arg);
void incogaps(const Arg *arg);
void incohgaps(const Arg *arg);
void incovgaps(const Arg *arg);
void incigaps(const Arg *arg);
void defaultgaps(const Arg *arg);
void buffer_set_size(Client *c, animationScale scale_data);
void snap_scene_buffer_apply_size(struct wlr_scene_buffer *buffer, int sx,
                                  int sy, void *data);
void client_set_pending_state(Client *c);
// int timer_tick_action(void *data);

#include "dispatch.h"

/* variables */
static const char broken[] = "broken";
// static const char *cursor_image = "left_ptr";
static pid_t child_pid = -1;
static int locked;
static void *exclusive_focus;
static struct wl_display *dpy;
static struct wlr_relative_pointer_manager_v1 *pointer_manager;
static struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;
static struct wlr_backend *backend;
static struct wlr_scene *scene;
static struct wlr_scene_tree *layers[NUM_LAYERS];
static struct wlr_renderer *drw;
static struct wlr_allocator *alloc;
static struct wlr_compositor *compositor;

static struct wlr_gamma_control_manager_v1 *gamma_control_mgr;
static struct wlr_xdg_shell *xdg_shell;
static struct wlr_xdg_activation_v1 *activation;
static struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
static struct wl_list clients; /* tiling order */
static struct wl_list fstack;  /* focus order */
static struct wl_list fadeout_clients;
// static struct wlr_idle *idle;
static struct wlr_idle_notifier_v1 *idle_notifier;
static struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
// static struct wlr_input_inhibit_manager *input_inhibit_mgr;
static struct wlr_layer_shell_v1 *layer_shell;
static struct wlr_output_manager_v1 *output_mgr;
static struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
static struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
static struct wlr_output_power_manager_v1 *power_mgr;
static struct wlr_pointer_gestures_v1 *pointer_gestures;

static struct wlr_cursor *cursor;
static struct wlr_xcursor_manager *cursor_mgr;
static struct wlr_session *session;

static struct wlr_session_lock_manager_v1 *session_lock_mgr;
static struct wlr_scene_rect *locked_bg;
static struct wlr_session_lock_v1 *cur_lock;
static const int layermap[] = {LyrBg, LyrBottom, LyrTop, LyrOverlay};
static struct wlr_scene_tree *drag_icon;
static struct wlr_cursor_shape_manager_v1
    *cursor_shape_mgr; // 这个跟steup obs影响对应
static struct wlr_pointer_constraints_v1 *pointer_constraints;
static struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
static struct wlr_pointer_constraint_v1 *active_constraint;

static struct wlr_seat *seat;
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

static uint32_t swipe_fingers = 0;
static double swipe_dx = 0;
static double swipe_dy = 0;

/* global event handlers */
static struct zdwl_ipc_manager_v2_interface dwl_manager_implementation = {
    .release = dwl_ipc_manager_release,
    .get_output = dwl_ipc_manager_get_output};
static struct zdwl_ipc_output_v2_interface dwl_output_implementation = {
    .release = dwl_ipc_output_release,
    .set_tags = dwl_ipc_output_set_tags,
    .set_layout = dwl_ipc_output_set_layout,
    .set_client_tags = dwl_ipc_output_set_client_tags};
static struct wl_listener lock_listener = {.notify = locksession};

#ifdef XWAYLAND
static void activatex11(struct wl_listener *listener, void *data);
static void configurex11(struct wl_listener *listener, void *data);
static void createnotifyx11(struct wl_listener *listener, void *data);
static void dissociatex11(struct wl_listener *listener, void *data);
static void associatex11(struct wl_listener *listener, void *data);
static Atom getatom(xcb_connection_t *xc, const char *name);
static void sethints(struct wl_listener *listener, void *data);
static void xwaylandready(struct wl_listener *listener, void *data);
void free_config(void);
// static struct wl_listener new_xwayland_surface = {.notify = createnotifyx11};
// static struct wl_listener xwayland_ready = {.notify = xwaylandready};
static struct wlr_xwayland *xwayland;
static Atom netatom[NetLast];
#endif

/* configuration, allows nested code to access above variables */
#include "preset_config.h"

/* attempt to encapsulate suck into one file */
#include "client.h"
#ifdef IM
#include "IM.h"
#endif

struct NumTags {
  char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

struct Pertag {
  unsigned int curtag, prevtag;    /* current and previous tag */
  int nmasters[LENGTH(tags) + 1];  /* number of windows in master area */
  float mfacts[LENGTH(tags) + 1];  /* mfacts per tag */
  float smfacts[LENGTH(tags) + 1]; /* smfacts per tag */
  const Layout
      *ltidxs[LENGTH(tags) + 1]; /* matrix of tags and layouts indexes  */
};

static pid_t *autostart_pids;
static size_t autostart_len;

#include "parse_config.h"

struct vec2 {
  double x, y;
};

struct uvec2 {
  int x, y;
};

#define BAKED_POINTS_COUNT 256

struct vec2 *baked_points_move;
struct vec2 *baked_points_open;
struct vec2 *baked_points_tag;
struct vec2 *baked_points_close;

struct vec2 calculate_animation_curve_at(double t, int type) {
  struct vec2 point;
  double *animation_curve;
  if (type == MOVE) {
    animation_curve = animation_curve_move;
  } else if (type == OPEN) {
    animation_curve = animation_curve_open;
  } else if (type == TAG) {
    animation_curve = animation_curve_tag;
  } else if (type == CLOSE) {
    animation_curve = animation_curve_close;
  }

  point.x = 3 * t * (1 - t) * (1 - t) * animation_curve[0] +
            3 * t * t * (1 - t) * animation_curve[2] + t * t * t;

  point.y = 3 * t * (1 - t) * (1 - t) * animation_curve[1] +
            3 * t * t * (1 - t) * animation_curve[3] + t * t * t;

  return point;
}

void init_baked_points(void) {
  baked_points_move = calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_move));
  baked_points_open = calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_open));
  baked_points_tag = calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_tag));
  baked_points_close = calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_close));

  for (size_t i = 0; i < BAKED_POINTS_COUNT; i++) {
    baked_points_move[i] = calculate_animation_curve_at(
        (double)i / (BAKED_POINTS_COUNT - 1), MOVE);
  }
  for (size_t i = 0; i < BAKED_POINTS_COUNT; i++) {
    baked_points_open[i] = calculate_animation_curve_at(
        (double)i / (BAKED_POINTS_COUNT - 1), OPEN);
  }
  for (size_t i = 0; i < BAKED_POINTS_COUNT; i++) {
    baked_points_tag[i] =
        calculate_animation_curve_at((double)i / (BAKED_POINTS_COUNT - 1), TAG);
  }
  for (size_t i = 0; i < BAKED_POINTS_COUNT; i++) {
    baked_points_close[i] =
        calculate_animation_curve_at((double)i / (BAKED_POINTS_COUNT - 1), CLOSE);
  }
}

double find_animation_curve_at(double t, int type) {
  size_t down = 0;
  size_t up = BAKED_POINTS_COUNT - 1;

  size_t middle = (up + down) / 2;
  struct vec2 *baked_points;
  if (type == MOVE) {
    baked_points = baked_points_move;
  } else if (type == OPEN) {
    baked_points = baked_points_open;
  } else if (type == TAG) {
    baked_points = baked_points_tag;
  } else if (type == CLOSE) {
    baked_points = baked_points_close;
  }

  while (up - down != 1) {
    if (baked_points[middle].x <= t) {
      down = middle;
    } else {
      up = middle;
    }
    middle = (up + down) / 2;
  }
  return baked_points[up].y;
}

// 有 bug,只是让上面那根透明了
void apply_opacity_to_rect_nodes(Client *c, struct wlr_scene_node *node,
                                 double animation_passed) {
  int offsetx = 0;
  int offsety = 0;
  if (node->type == WLR_SCENE_NODE_RECT) {
    struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
    // Assuming the rect has a color field and we can modify it
    rect->color[0] = (1 - animation_passed) * rect->color[0];
    rect->color[1] = (1 - animation_passed) * rect->color[1];
    rect->color[2] = (1 - animation_passed) * rect->color[2];
    rect->color[3] = (1 - animation_passed) * rect->color[3];
    wlr_scene_rect_set_color(rect, rect->color);

    // TODO: 判断当前窗口是否在屏幕外，如果在屏幕外就不要绘制
    // 划出的border剪切屏幕之外的，这里底部bttome可以了，左右的还不不对
    offsetx = c->geom.width -  c->animation.current.width;
    offsety = c->geom.height -  c->animation.current.height;
    if(node->y > c->geom.y + c->geom.height/2 ){
      wlr_scene_node_set_position(node,c->geom.x,c->geom.y + c->geom.height - offsety);
      wlr_scene_rect_set_size(rect, c->animation.current.width, c->bw); // down
    } else if(node->y < c->geom.y + c->geom.height/2 && rect->width > rect->height) {
      wlr_scene_node_set_position(node,c->geom.x,c->geom.y);
      wlr_scene_rect_set_size(rect, c->animation.current.width, c->bw); // up      
    } else if(node->x < c->geom.x + c->geom.width/2 && rect->width < rect->height) {
      wlr_scene_rect_set_size(rect,c->bw,c->animation.current.height); // left
    } else {
      wlr_scene_node_set_position(node,c->geom.x + c->geom.width - offsetx,c->geom.y);
      wlr_scene_rect_set_size(rect,c->bw,c->animation.current.height); // right
    }
  }

  // If the node is a tree, recursively traverse its children
  if (node->type == WLR_SCENE_NODE_TREE) {
    struct wlr_scene_tree *scene_tree = wlr_scene_tree_from_node(node);
    struct wlr_scene_node *child;
    wl_list_for_each(child, &scene_tree->children, link) {
      apply_opacity_to_rect_nodes(c, child, animation_passed);
    }
  }
}

void fadeout_client_animation_next_tick(Client *c) {
  if (!c)
    return;

  animationScale scale_data;

  double animation_passed =
      (double)c->animation.passed_frames / c->animation.total_frames;
  int type = c->animation.action = c->animation.action;
  double factor = find_animation_curve_at(animation_passed, type);
  uint32_t width = c->animation.initial.width +
                   (c->current.width - c->animation.initial.width) * factor;
  uint32_t height = c->animation.initial.height +
                    (c->current.height - c->animation.initial.height) * factor;

  uint32_t x =
      c->animation.initial.x + (c->current.x - c->animation.initial.x) * factor;
  uint32_t y =
      c->animation.initial.y + (c->current.y - c->animation.initial.y) * factor;

  wlr_scene_node_set_position(&c->scene->node, x, y);

  c->animation.current = (struct wlr_box){
      .x = x,
      .y = y,
      .width = width,
      .height = height,
  };

  double opacity = MAX(fadeout_begin_opacity - animation_passed, 0);

  wlr_scene_node_for_each_buffer(&c->scene->node, scene_buffer_apply_opacity,
                                 &opacity);

  apply_opacity_to_rect_nodes(c, &c->scene->node, animation_passed);

  if ((c->animation_type_close && strcmp(c->animation_type_close, "zoom") == 0) ||
      (!c->animation_type_close && strcmp(animation_type_close, "zoom") == 0)) {

    scale_data.width = width;
    scale_data.height = height;
    scale_data.width_scale = animation_passed;
    scale_data.height_scale = animation_passed;

    wlr_scene_node_for_each_buffer(&c->scene->node,
                                   snap_scene_buffer_apply_size, &scale_data);
  }

  if (animation_passed == 1.0) {
    wl_list_remove(&c->fadeout_link);
    wlr_scene_node_destroy(&c->scene->node);
    free(c);
    c = NULL;
  } else {
    c->animation.passed_frames++;
  }
}

void client_animation_next_tick(Client *c) {
  double animation_passed =
      (double)c->animation.passed_frames / c->animation.total_frames;

  int type = c->animation.action == NONE ? MOVE : c->animation.action;
  double factor = find_animation_curve_at(animation_passed, type);

  Client *pointer_c = NULL;
  double sx = 0, sy = 0;
  struct wlr_surface *surface = NULL;

  uint32_t width = c->animation.initial.width +
                   (c->current.width - c->animation.initial.width) * factor;
  uint32_t height = c->animation.initial.height +
                    (c->current.height - c->animation.initial.height) * factor;

  uint32_t x =
      c->animation.initial.x + (c->current.x - c->animation.initial.x) * factor;
  uint32_t y =
      c->animation.initial.y + (c->current.y - c->animation.initial.y) * factor;

  wlr_scene_node_set_position(&c->scene->node, x, y);
  c->animation.current = (struct wlr_box){
      .x = x,
      .y = y,
      .width = width,
      .height = height,
  };

  if (!c->iskilling && (c->is_open_animation || c->animation.begin_fade_in) &&
      animation_fade_in) {
    c->animation.begin_fade_in = true;
    client_set_opacity(c, MIN(animation_passed + fadein_begin_opacity, 1.0));
  }

  c->is_open_animation = false;

  if (animation_passed == 1.0) {
    if (c->animation.begin_fade_in) {
      c->animation.begin_fade_in = false;
    }

    c->animation.running = false;

    if (c->animation.tagining) {
      c->animation.tagining = false;
    }

    if (c->animation.tagouting) {
      c->animation.tagouting = false;
      wlr_scene_node_set_enabled(&c->scene->node, false);
      client_set_suspended(c, true);
      c->animation.tagouted = true;
      c->animation.current = c->geom;
    }

    xytonode(cursor->x, cursor->y, NULL, &pointer_c, NULL, &sx, &sy);

    surface = pointer_c && pointer_c == c ? client_surface(pointer_c) : NULL;
    if (surface && pointer_c == selmon->sel) {
      wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
    }

    // end flush in next frame, not the current frame
    c->need_output_flush = false;
  } else {
    c->animation.passed_frames++;
  }
}

void client_actual_size(Client *c, uint32_t *width, uint32_t *height) {
  *width = c->animation.current.width;

  *height = c->animation.current.height;
}

void set_rect_size(struct wlr_scene_rect *rect, int width, int height) {
  wlr_scene_rect_set_size(rect, GEZERO(width), GEZERO(height));
}

void apply_border(Client *c, struct wlr_box clip_box, int offsetx,
                  int offsety) {

  if (c->iskilling || !client_surface(c)->mapped)
    return;

  wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
  set_rect_size(c->border[0], clip_box.width, c->bw);
  set_rect_size(c->border[1], clip_box.width, c->bw);
  set_rect_size(c->border[2], c->bw, clip_box.height - 2 * c->bw);
  set_rect_size(c->border[3], c->bw, clip_box.height - 2 * c->bw);
  wlr_scene_node_set_position(&c->border[0]->node, 0, 0);
  wlr_scene_node_set_position(&c->border[2]->node, 0, c->bw);
  wlr_scene_node_set_position(&c->border[1]->node, 0, clip_box.height - c->bw);
  wlr_scene_node_set_position(&c->border[3]->node, clip_box.width - c->bw,
                              c->bw);

  if (ISTILED(c) || c->animation.tagining || c->animation.tagouted || c->animation.tagouting) {
    if (c->animation.current.x < c->mon->m.x) {
      set_rect_size(c->border[2], 0, 0);
    } else if (c->animation.current.x + c->animation.current.width >
               c->mon->m.x + c->mon->m.width) {
      set_rect_size(c->border[3], 0, 0);
    } else if (c->animation.current.y < c->mon->m.y) {
      set_rect_size(c->border[0], 0, 0);
    } else if (c->animation.current.y + c->animation.current.height >
               c->mon->m.y + c->mon->m.height) {
      set_rect_size(c->border[1], 0, 0);
    }
  }

  wlr_scene_node_set_position(&c->border[0]->node, offsetx, offsety);
  wlr_scene_node_set_position(&c->border[2]->node, offsetx, c->bw + offsety);
  wlr_scene_node_set_position(&c->border[1]->node, offsetx,
                              clip_box.height - c->bw + offsety);
  wlr_scene_node_set_position(
      &c->border[3]->node, clip_box.width - c->bw + offsetx, c->bw + offsety);
}

struct uvec2 clip_to_hide(Client *c, struct wlr_box *clip_box) {
  int offsetx=0;
  int offsety=0;
  struct uvec2 offset;
  offset.x = 0;
  offset.y =0;

  if(!ISTILED(c) && !c->animation.tagining && !c->animation.tagouted && !c->animation.tagouting) return offset;

  // // make tagout tagin animations not visible in other monitors
  if (ISTILED(c) || c->animation.tagining || c->animation.tagouted || c->animation.tagouting) {
    if (c->animation.current.x <= c->mon->m.x) {
      offsetx = c->mon->m.x - c->animation.current.x - c->bw;
      clip_box->x = clip_box->x + offsetx;
      clip_box->width = clip_box->width - offsetx;
    } else if (c->animation.current.x + c->animation.current.width >=
               c->mon->m.x + c->mon->m.width) {
      clip_box->width = clip_box->width -
                       (c->animation.current.x + c->animation.current.width -
                        c->mon->m.x - c->mon->m.width) + c->bw;
    }

    if (c->animation.current.y <= c->mon->m.y) {
      offsety = c->mon->m.y - c->animation.current.y - c->bw;
      clip_box->y = clip_box->y + offsety;
      clip_box->height = clip_box->height - offsety;
    } else if (c->animation.current.y + c->animation.current.height >=
               c->mon->m.y + c->mon->m.height) {
      clip_box->height = clip_box->height -
                        (c->animation.current.y + c->animation.current.height -
                         c->mon->m.y - c->mon->m.height) + c->bw;
    }
  }

  offset.x = offsetx;
  offset.y = offsety;

  if((clip_box->width <= 0 || clip_box->height <= 0) && ISTILED(c)) {
    c->is_clip_to_hide = true;
    wlr_scene_node_set_enabled(&c->scene->node, false);
  } else if(c->is_clip_to_hide && VISIBLEON(c, c->mon)) {
    c->is_clip_to_hide = false;
    wlr_scene_node_set_enabled(&c->scene->node, true);
  }

  return offset;
}

void apply_buffer_scale(Client *c, struct wlr_box clip_box ) {
  animationScale scale_data;
  scale_data.width = clip_box.width - 2 * c->bw;
  scale_data.height = clip_box.height - 2 * c->bw;
  scale_data.m = c->mon;
  scale_data.width_scale = (float)clip_box.width / c->current.width;
  scale_data.height_scale = (float)clip_box.height / c->current.height;
  buffer_set_size(c, scale_data);
}

void client_apply_clip(Client *c) {

  if (c->iskilling || !client_surface(c)->mapped)
    return;
  struct wlr_box clip_box;
  struct uvec2 offset;

  if (!animations) {
    c->animation.running = false;
    c->need_output_flush = false;
    c->animainit_geom = c->current = c->pending = c->animation.current =
        c->geom;
    client_get_clip(c, &clip_box);
    offset = clip_to_hide(c, &clip_box);
    apply_border(c, clip_box, offset.x, offset.y);
    wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip_box);
    /* apply_buffer_scale(c, clip_box); */
    return;
  }

  uint32_t width, height;
  client_actual_size(c, &width, &height);

  struct wlr_box geometry;
  client_get_geometry(c, &geometry);
  clip_box = (struct wlr_box){
      .x = geometry.x,
      .y = geometry.y,
      .width = width,
      .height = height,
  };

  if (client_is_x11(c)) {
    clip_box.x = 0;
    clip_box.y = 0;
  }

  offset = clip_to_hide(c, &clip_box);

  wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip_box);
  apply_border(c, clip_box, offset.x, offset.y);
  apply_buffer_scale(c,clip_box);
}

bool client_draw_frame(Client *c) {

  if (!c || !client_surface(c)->mapped)
    return false;

  if (!c->need_output_flush)
    return false;

  if (animations && c->animation.running) {
    client_animation_next_tick(c);
    client_apply_clip(c);
  } else {
    wlr_scene_node_set_position(&c->scene->node, c->pending.x, c->pending.y);
    c->animainit_geom = c->animation.initial = c->pending = c->current =
        c->geom;
    client_apply_clip(c);
    c->need_output_flush = false;
  }
  // c->resize = 1;
  return true;
}

bool client_draw_fadeout_frame(Client *c) {
  if (!c)
    return false;

  fadeout_client_animation_next_tick(c);
  return true;
}

void // 0.5
applybounds(Client *c, struct wlr_box *bbox) {
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
    c->bw = borderpx;
    client_set_fullscreen(c, false);
  }
}

void // 升级忽略
setgamma(struct wl_listener *listener, void *data) {
  struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;
  struct wlr_output_state state;
  wlr_output_state_init(&state);
  if (!wlr_gamma_control_v1_apply(event->control, &state)) {
    wlr_output_state_finish(&state);
    return;
  }

  if (!wlr_output_test_state(event->output, &state)) {
    wlr_gamma_control_v1_send_failed_and_destroy(event->control);
    wlr_output_state_finish(&state);
    return;
  }

  wlr_output_commit_state(event->output, &state);
  wlr_output_schedule_frame(event->output);
}

void minized(const Arg *arg) {
  if (selmon->sel && !selmon->sel->isminied) {
    set_minized(selmon->sel);
  }
}

void restore_minized(const Arg *arg) {
  Client *c;

  if(selmon && selmon->sel && selmon->sel->is_scratchpad_show && !selmon->sel->iskilling) {
    show_hide_client(selmon->sel);
    selmon->sel->is_scratchpad_show = 0;
    selmon->sel->is_in_scratchpad = 0;
    setborder_color(selmon->sel);
    return;
  }

  wl_list_for_each(c, &clients, link) {
    if (c->isminied) {
      show_hide_client(c);
      c->is_scratchpad_show = 0;
      c->is_in_scratchpad = 0;
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
    c->bw = borderpx; // 恢复非全屏的border
  }
  /* return if fullscreen */
  if (!c->isfloating) {
    setfloating(c, 1);
    c->geom.width = c->mon->w.width * 0.7;
    c->geom.height = c->mon->w.height * 0.8;
    // 重新计算居中的坐标
    c->geom = setclient_coordinate_center(c->geom);
    resize(c, c->geom, 0);
  }
  c->oldtags = selmon->tagset[selmon->seltags];
  show_hide_client(c);
  setborder_color(c);
}

void remove_foreign_topleve(Client *c) {
  wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
  c->foreign_toplevel = NULL;  
}

void add_foreign_toplevel(Client *c) {
  c->foreign_toplevel =
      wlr_foreign_toplevel_handle_v1_create(foreign_toplevel_manager);
  // 监听来自外部对于窗口的事件请求
  if (c->foreign_toplevel) {
    LISTEN(&(c->foreign_toplevel->events.request_activate),
           &c->foreign_activate_request, handle_foreign_activate_request);
    LISTEN(&(c->foreign_toplevel->events.request_fullscreen),
           &c->foreign_fullscreen_request, handle_foreign_fullscreen_request);
    LISTEN(&(c->foreign_toplevel->events.request_close),
           &c->foreign_close_request, handle_foreign_close_request);
    LISTEN(&(c->foreign_toplevel->events.destroy), &c->foreign_destroy,
           handle_foreign_destroy);
    // 设置外部顶层句柄的id为应用的id
    const char *appid;
    appid = client_get_appid(c);
    if (appid)
      wlr_foreign_toplevel_handle_v1_set_app_id(c->foreign_toplevel, appid);
    // 设置外部顶层句柄的title为应用的title
    const char *title;
    title = client_get_title(c);
    if (title)
      wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel, title);
    // 设置外部顶层句柄的显示监视器为当前监视器
    wlr_foreign_toplevel_handle_v1_output_enter(c->foreign_toplevel,
                                                c->mon->wlr_output);
  }
}

void reset_foreign_tolevel(Client *c) {
 remove_foreign_topleve(c);
 add_foreign_toplevel(c);
}

void toggle_scratchpad(const Arg *arg) {
  Client *c;
  wl_list_for_each(c, &clients, link) {
    if (c->mon != selmon) {
      continue;
    }
    if (c->is_in_scratchpad && c->is_scratchpad_show &&
        (selmon->tagset[selmon->seltags] & c->tags) == 0) {
      unsigned int target = get_tags_first_tag(selmon->tagset[selmon->seltags]);
      tag_client(&(Arg){.ui = target}, c);
      return;
    } else if (c->is_in_scratchpad && c->is_scratchpad_show &&
               (selmon->tagset[selmon->seltags] & c->tags) != 0) {
      set_minized(c);
      return;
    } else if (c && c->is_in_scratchpad && !c->is_scratchpad_show) {
      show_scratchpad(c);
      return;
    }
  }
}

pid_t
getparentprocess(pid_t p)
{
	unsigned int v = 0;

	FILE *f;
	char buf[256];
	snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

	if (!(f = fopen(buf, "r")))
		return 0;

	fscanf(f, "%*u %*s %*c %u", &v);
	fclose(f);

	return (pid_t)v;
}

int
isdescprocess(pid_t p, pid_t c)
{
	while (p != c && c != 0)
		c = getparentprocess(c);

	return (int)c;
}

Client *
termforwin(Client *w)
{
	Client *c;

	if (!w->pid || w->isterm || w->noswallow)
		return NULL;

	wl_list_for_each(c, &fstack, flink)
		if (c->isterm && !c->swallowing && c->pid && isdescprocess(c->pid, w->pid))
			return c;

	return NULL;
}

void
swallow(Client *c, Client *w)
{
	c->bw = w->bw;
	c->isfloating = w->isfloating;
	c->isurgent = w->isurgent;
	c->isfullscreen = w->isfullscreen;
  c->ismaxmizescreen = w->ismaxmizescreen;
	c->tags = w->tags;
	c->geom = w->geom;
  c->scroller_proportion = w->scroller_proportion;
	wl_list_insert(&w->link, &c->link);
	wl_list_insert(&w->flink, &c->flink);

  if(w->foreign_toplevel)
    remove_foreign_topleve(w);

	wlr_scene_node_set_enabled(&w->scene->node, 0);
	wlr_scene_node_set_enabled(&c->scene->node, 1);

  if(!c->foreign_toplevel && c->mon)
    add_foreign_toplevel(c);
}

void // 0.5
handlesig(int signo) {
  if (signo == SIGCHLD) {
#ifdef XWAYLAND
    siginfo_t in;
    /* wlroots expects to reap the XWayland process itself, so we
     * use WNOWAIT to keep the child waitable until we know it's not
     * XWayland.
     */
    while (!waitid(P_ALL, 0, &in, WEXITED | WNOHANG | WNOWAIT) && in.si_pid &&
           (!xwayland || in.si_pid != xwayland->server->pid))
      waitpid(in.si_pid, NULL, 0);
#else
    while (waitpid(-1, NULL, WNOHANG) > 0)
      ;
#endif
  } else if (signo == SIGINT || signo == SIGTERM) {
    quit(NULL);
  }
}

void toggle_hotarea(int x_root, int y_root) {
  // 左下角热区坐标计算,兼容多显示屏
  Arg arg = {0};

  // 在刚启动的时候,selmon为NULL,但鼠标可能已经处于热区,
  // 必须判断避免奔溃
  if (!selmon)
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
setclient_coordinate_center(struct wlr_box geom) {
  struct wlr_box tempbox;
  tempbox.x = selmon->w.x + (selmon->w.width - geom.width) / 2;
  tempbox.y = selmon->w.y + (selmon->w.height - geom.height) / 2;
  tempbox.width = geom.width;
  tempbox.height = geom.height;
  return tempbox;
}

/* function implementations */
void logtofile(const char *fmt, ...) {
  char buf[256];
  char cmd[256];
  va_list ap;
  va_start(ap, fmt);
  vsprintf((char *)buf, fmt, ap);
  va_end(ap);
  unsigned int i = strlen((const char *)buf);

  sprintf(cmd, "echo '%.*s' >> ~/log", i, buf);
  system(cmd);
}

/* function implementations */
void lognumtofile(float num) {
  char cmd[256];
  sprintf(cmd, "echo '%f' >> ~/log", num);
  system(cmd);
}

int // 0.5
applyrulesgeom(Client *c) {
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
    if ((r->title && strstr(title, r->title)) ||
        (r->id && strstr(appid, r->id))) {
      c->geom.width = r->width > 0 ? r->width : c->geom.width;
      c->geom.height = r->height > 0 ? r->height : c->geom.height;
      // 重新计算居中的坐标
      c->geom = setclient_coordinate_center(c->geom);
      hit = r->height > 0 || r->width > 0 ? 1 : 0;
    }
  }
  return hit;
}

void // 17
applyrules(Client *c) {
  /* rule matching */
  const char *appid, *title;
  uint32_t i, newtags = 0;
  int ji;
  const ConfigWinRule *r;
  Monitor *mon = selmon, *m;

  c->isfloating = client_is_float_type(c);
  if (!(appid = client_get_appid(c)))
    appid = broken;
  if (!(title = client_get_title(c)))
    title = broken;

  c->pid = client_get_pid(c);
    
  for (ji = 0; ji < config.window_rules_count; ji++) {
    if (config.window_rules_count < 1)
      break;
    r = &config.window_rules[ji];

    if ((r->title && strstr(title, r->title)) ||
        (r->id && strstr(appid, r->id))) {
      c->isterm     = r->isterm > 0 ? r->isterm : c->isterm;
      c->noswallow  = r->noswallow > 0? r->noswallow : c->noswallow;
      c->isfloating = r->isfloating > 0 ? r->isfloating : c->isfloating;
      c->isfullscreen = r->isfullscreen > 0 ? r->isfullscreen : c->isfullscreen;
      c->animation_type_open =
          r->animation_type_open == NULL ? c->animation_type_open : r->animation_type_open;
      c->animation_type_close =
          r->animation_type_close == NULL ? c->animation_type_close : r->animation_type_close;
      c->scroller_proportion = r->scroller_proportion > 0
                                   ? r->scroller_proportion
                                   : c->scroller_proportion;
      c->isnoborder = r->isnoborder > 0 ? r->isnoborder : c->isnoborder;
      c->isopensilent = r->isopensilent > 0 ? r->isopensilent : c->isopensilent;
      newtags = r->tags > 0 ? r->tags | newtags : newtags;
      i = 0;
      wl_list_for_each(m, &mons, link) if (r->monitor == i++) mon = m;

      if (c->isfloating) {
        c->geom.width = r->width > 0 ? r->width : c->geom.width;
        c->geom.height = r->height > 0 ? r->height : c->geom.height;
        // 重新计算居中的坐标
        c->geom = setclient_coordinate_center(c->geom);
      }
    }
  }

  if (!c->noswallow && !c->isfloating && !client_is_float_type(c)
      && !c->surface.xdg->initial_commit) {
    Client *p = termforwin(c);
    if (p) {
      c->swallowedby = p;
      p->swallowing  = c;
      wl_list_remove(&c->link);
      wl_list_remove(&c->flink);
      swallow(c, p);
      wl_list_remove(&p->link);
      wl_list_remove(&p->flink);
      mon = p->mon;
      newtags = p->tags;
    }
  }

  wlr_scene_node_reparent(&c->scene->node,
                          layers[c->isfloating ? LyrFloat : LyrTile]);

  Client *fc;
  // 如果当前的tag中有新创建的非悬浮窗口,就让当前tag中的全屏窗口退出全屏参与平铺
  wl_list_for_each(fc, &clients, link) if (fc && fc !=c &&
                                           c->tags & fc->tags &&
                                           ISFULLSCREEN(fc) && !c->isfloating) {
    clear_fullscreen_flag(fc);
    arrange(c->mon, false);
  }

  setmon(c, mon, newtags,!c->isopensilent);

  if (!c->isopensilent && !(c->tags & (1 << (selmon->pertag->curtag - 1)))) {
    c->animation.from_rule = true;
    view(&(Arg){.ui = c->tags}, true);
  }
}

void // 17
arrange(Monitor *m, bool want_animation) {
  Client *c;

  if(!m)
    return;

  if (!m->wlr_output->enabled)
    return;

  wl_list_for_each(c, &clients, link) {
    if (c->iskilling)
      continue;

    if (c->mon == m && c->isglobal) {
      c->tags = m->tagset[m->seltags];
      if (selmon->sel == NULL)
        focusclient(c, 0);
    }

    if (c->mon == m) {
      if (VISIBLEON(c, m)) {
        if(!c->is_clip_to_hide || strcmp(c->mon->pertag->ltidxs[c->mon->pertag->curtag]->name,
             "scroller") != 0) {
          wlr_scene_node_set_enabled(&c->scene->node, true);
        }
        client_set_suspended(c, false);
        if (!c->animation.from_rule && want_animation &&
            m->pertag->prevtag != 0 && m->pertag->curtag != 0 && animations) {
          c->animation.tagining = true;
          if (m->pertag->curtag > m->pertag->prevtag) {
            c->animainit_geom.x =
                c->animation.running
                    ? c->animation.current.x
                    : c->mon->m.x + c->mon->m.width;
          } else {
            c->animainit_geom.x = c->animation.running ? c->animation.current.x
                                                       : m->m.x - c->geom.width;
          }
        }

        c->animation.from_rule = false;
        c->animation.tagouting = false;
        c->animation.tagouted = false;
        resize(c, c->geom, 0);

      } else {
        if ((c->tags & (1 << (selmon->pertag->prevtag - 1))) &&
            want_animation && m->pertag->prevtag != 0 &&
            m->pertag->curtag != 0 && animations) {
          c->animation.tagouting = true;
          c->animation.tagining = false;
          if (m->pertag->curtag > m->pertag->prevtag) {
            c->pending = c->geom;
            c->pending.x = c->mon->m.x - c->geom.width;
            resize(c, c->geom, 0);
          } else {
            c->pending = c->geom;
            c->pending.x =
                c->mon->m.x + c->mon->m.width;
            resize(c, c->geom, 0);
          }
        } else {
          wlr_scene_node_set_enabled(&c->scene->node, false);
          client_set_suspended(c, true);
        }
      }
    }
  }

  // 给全屏窗口设置背景为黑色
  // 好像要跟LyrFS图层一起使用，我不用这个图层，所以注释掉
  // wlr_scene_node_set_enabled(&m->fullscreen_bg->node,
  // 		(c = focustop(m)) && c->isfullscreen);

  if (m->isoverview) {
    overviewlayout.arrange(m, 0, 0);
  } else if (m && m->pertag->ltidxs[m->pertag->curtag]->arrange) {
    m->pertag->ltidxs[m->pertag->curtag]->arrange(m, gappoh, 0);
  }

#ifdef IM
  if (input_relay && input_relay->popup)
    input_popup_update(input_relay->popup);
#endif
  motionnotify(0, NULL, 0, 0, 0, 0);
  checkidleinhibitor(NULL);
}

void // 0.5
arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area,
             int exclusive) {
  LayerSurface *l;
  struct wlr_box full_area = m->m;

  wl_list_for_each(l, list, link) {
    struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;

    if (exclusive != (layer_surface->current.exclusive_zone > 0))
      continue;

    wlr_scene_layer_surface_v1_configure(l->scene_layer, &full_area,
                                         usable_area);
    wlr_scene_node_set_position(&l->popups->node, l->scene->node.x,
                                l->scene->node.y);
    l->geom.x = l->scene->node.x;
    l->geom.y = l->scene->node.y;
  }
}

Client *direction_select(const Arg *arg) {
  Client *c;
  Client **tempClients = NULL; // 初始化为 NULL
  Client *tc = selmon->sel;
  int last = -1;

  if (!tc)
    return NULL;

  if (tc && (tc->isfullscreen || tc->ismaxmizescreen)) {
    // 不支持全屏窗口的焦点切换
    return NULL;
  }

  // 第一次遍历，计算客户端数量
  wl_list_for_each(c, &clients, link) {
    if (c && (focus_cross_monitor || c->mon == selmon) && (c->tags & c->mon->tagset[c->mon->seltags])) {
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
    if (c && (focus_cross_monitor || c->mon == selmon) && (c->tags & c->mon->tagset[c->mon->seltags])) {
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
      if (tempClients[_i]->geom.y < sel_y && tempClients[_i]->geom.x == sel_x) {
        int dis_x = tempClients[_i]->geom.x - sel_x;
        int dis_y = tempClients[_i]->geom.y - sel_y;
        long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
        if (tmp_distance < distance) {
          distance = tmp_distance;
          tempFocusClients = tempClients[_i];
        }
      }
    }
    if (!tempFocusClients) {
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
      if (tempClients[_i]->geom.y > sel_y && tempClients[_i]->geom.x == sel_x) {
        int dis_x = tempClients[_i]->geom.x - sel_x;
        int dis_y = tempClients[_i]->geom.y - sel_y;
        long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
        if (tmp_distance < distance) {
          distance = tmp_distance;
          tempFocusClients = tempClients[_i];
        }
      }
    }
    if (!tempFocusClients) {
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
      if (tempClients[_i]->geom.x < sel_x && tempClients[_i]->geom.y == sel_y) {
        int dis_x = tempClients[_i]->geom.x - sel_x;
        int dis_y = tempClients[_i]->geom.y - sel_y;
        long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
        if (tmp_distance < distance) {
          distance = tmp_distance;
          tempFocusClients = tempClients[_i];
        }
      }
    }
    if (!tempFocusClients) {
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
      if (tempClients[_i]->geom.x > sel_x && tempClients[_i]->geom.y == sel_y) {
        int dis_x = tempClients[_i]->geom.x - sel_x;
        int dis_y = tempClients[_i]->geom.y - sel_y;
        long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
        if (tmp_distance < distance) {
          distance = tmp_distance;
          tempFocusClients = tempClients[_i];
        }
      }
    }
    if (!tempFocusClients) {
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

void focusdir(const Arg *arg) {
  Client *c;
  c = direction_select(arg);
  if (c) {
    focusclient(c, 1);
    if (warpcursor)
      warp_cursor(c);
  }
}

void // 0.5
arrangelayers(Monitor *m) {
  int i;
  struct wlr_box usable_area = m->m;
  LayerSurface *l;
  uint32_t layers_above_shell[] = {
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
      if (locked || !l->layer_surface->current.keyboard_interactive ||
          !l->mapped)
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

void autostartexec(void) {
  const char *const *p;
  size_t i = 0;

  const char *maomaoconfig = getenv("MAOMAOCONFIG");
  static const char
      *autostart[4];         // 声明一个全局数组，大小为 5（包括 NULL 结 尾）
  char autostart_path[1024]; // 用于存储脚本的完整路径

  if (maomaoconfig && maomaoconfig[0] != '\0') {
    // 如果 MAOMAOCONFIG 存在且不为空，使用它作为配置文件夹
    snprintf(autostart_path, sizeof(autostart_path), "%s/autostart.sh",
             maomaoconfig);
  } else {
    // 否则使用 HOME 环境变量下的默认路径
    const char *homedir = getenv("HOME");
    if (!homedir) {
      // 如果 HOME 环境变量不存在，无法继续
      fprintf(stderr, "Error: HOME environment variable not set.\n");
      return;
    }
    snprintf(autostart_path, sizeof(autostart_path),
             "%s/.config/maomao/autostart.sh", homedir);
  }

  autostart[0] = "/bin/sh";      // 使用 /bin/sh 执行脚本
  autostart[1] = "-c";           // -c 参数表示从命令行读取脚本
  autostart[2] = autostart_path; // 脚本的完整路径
  autostart[3] = NULL;           // 数组以 NULL 结尾

  /* count entries */
  for (p = autostart; *p; autostart_len++, p++)
    while (*++p)
      ;

  autostart_pids = calloc(autostart_len, sizeof(pid_t));
  for (p = autostart; *p; i++, p++) {
    if ((autostart_pids[i] = fork()) == 0) {
      setsid();
      execvp(*p, (char *const *)p);
      die("dwl: execvp %s:", *p);
    }
    /* skip arguments */
    while (*++p)
      ;
  }
}

void // 鼠标滚轮事件
axisnotify(struct wl_listener *listener, void *data) {

  /* This event is forwarded by the cursor when a pointer emits an axis event,
   * for example when you move the scroll wheel. */
  struct wlr_pointer_axis_event *event = data;
  struct wlr_keyboard *keyboard;
  uint32_t mods;
  AxisBinding *a;
  int ji;
  unsigned int adir;
  // IDLE_NOTIFY_ACTIVITY;
  wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
  keyboard = wlr_seat_get_keyboard(seat);

  // 获取当前按键的mask,比如alt+super或者alt+ctrl
  mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

  if (event->orientation == WLR_AXIS_ORIENTATION_VERTICAL)
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

  /* TODO: allow usage of scroll whell for mousebindings, it can be implemented
   * checking the event's orientation and the delta of the event */
  /* Notify the client with pointer focus of the axis event. */
  wlr_seat_pointer_notify_axis(seat, // 滚轮事件发送给客户端也就是窗口
                               event->time_msec, event->orientation,
                               event->delta, event->delta_discrete,
                               event->source);
}

 
int
ongesture(struct wlr_pointer_swipe_end_event *event)
{
	struct wlr_keyboard *keyboard;
	uint32_t mods;
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
			 swipe_fingers == g->fingers_count &&
			 motion == g->motion && g->func) {
			g->func(&g->arg);
			handled = 1;
		}
	}
	return handled;
}

void
swipe_begin(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_swipe_begin_event *event = data;

	// Forward swipe begin event to client
	wlr_pointer_gestures_v1_send_swipe_begin(
		pointer_gestures, 
		seat,
		event->time_msec,
		event->fingers
	);
}

void
swipe_update(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_swipe_update_event *event = data;

  swipe_fingers = event->fingers;
  // Accumulate swipe distance
  swipe_dx += event->dx;
  swipe_dy += event->dy;

	// Forward swipe update event to client
	wlr_pointer_gestures_v1_send_swipe_update(
		pointer_gestures, 
		seat,
		event->time_msec,
		event->dx,
		event->dy
	);
}

void
swipe_end(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_swipe_end_event *event = data;
  ongesture(event);
  swipe_dx = 0;
  swipe_dy = 0;
	// Forward swipe end event to client
	wlr_pointer_gestures_v1_send_swipe_end(
		pointer_gestures, 
		seat,
		event->time_msec,
		event->cancelled
	);
}

void
pinch_begin(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_pinch_begin_event *event = data;

	// Forward pinch begin event to client
	wlr_pointer_gestures_v1_send_pinch_begin(
		pointer_gestures, 
		seat,
		event->time_msec,
		event->fingers
	);
}

void
pinch_update(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_pinch_update_event *event = data;

	// Forward pinch update event to client
	wlr_pointer_gestures_v1_send_pinch_update(
		pointer_gestures,
		seat,
		event->time_msec,
		event->dx,
		event->dy,
		event->scale,
		event->rotation
	);
}

void
pinch_end(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_pinch_end_event *event = data;

	// Forward pinch end event to client
	wlr_pointer_gestures_v1_send_pinch_end(
		pointer_gestures,
		seat,
		event->time_msec,
		event->cancelled
	);
}

void
hold_begin(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_hold_begin_event *event = data;

	// Forward hold begin event to client
	wlr_pointer_gestures_v1_send_hold_begin(
		pointer_gestures,
		seat,
		event->time_msec,
		event->fingers
	);
}

void
hold_end(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_hold_end_event *event = data;

	// Forward hold end event to client
	wlr_pointer_gestures_v1_send_hold_end(
		pointer_gestures,
		seat,
		event->time_msec,
		event->cancelled
	);
}

void // 鼠标按键事件
buttonpress(struct wl_listener *listener, void *data) {
  struct wlr_pointer_button_event *event = data;
  struct wlr_keyboard *keyboard;
  uint32_t mods;
  Client *c;
  int ji;
  const MouseBinding *b;
  struct wlr_surface *surface;

  struct wlr_surface *old_pointer_focus_surface =
      seat->pointer_state.focused_surface;

  wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

  switch (event->state) {
  case WLR_BUTTON_PRESSED:
    cursor_mode = CurPressed;
    selmon = xytomon(cursor->x, cursor->y);
    if (locked)
      break;

    /* Change focus if the button was _pressed_ over a client */
    xytonode(cursor->x, cursor->y, &surface, &c, NULL, NULL, NULL);
    if (c && (!client_is_unmanaged(c) || client_wants_focus(c)))
      focusclient(c, 1);

    if (surface != old_pointer_focus_surface)
      wlr_seat_pointer_notify_clear_focus(seat);

    keyboard = wlr_seat_get_keyboard(seat);
    mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
    for (ji = 0; ji < config.mouse_bindings_count; ji++) {
      if (config.mouse_bindings_count < 1)
        break;
      b = &config.mouse_bindings[ji];
      if (CLEANMASK(mods) == CLEANMASK(b->mod) && event->button == b->button &&
          b->func && (selmon->isoverview == 1 || b->button == BTN_MIDDLE) &&
          c) {
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
  case WLR_BUTTON_RELEASED:
    /* If you released any buttons, we exit interactive move/resize mode. */
    if (!locked && cursor_mode != CurNormal && cursor_mode != CurPressed) {
      cursor_mode = CurNormal;
      /* Clear the pointer focus, this way if the cursor is over a surface
       * we will send an enter event after which the client will provide us
       * a cursor surface */
      wlr_seat_pointer_clear_focus(seat);
      motionnotify(0, NULL, 0, 0, 0, 0);
      /* Drop the window off on its new monitor */
      selmon = xytomon(cursor->x, cursor->y);
      setmon(grabc, selmon, 0,true);
      grabc = NULL;
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

void // 0.5
chvt(const Arg *arg) {
  wlr_session_change_vt(session, arg->ui);
}

void // 0.5
checkidleinhibitor(struct wlr_surface *exclude) {
  int inhibited = 0, unused_lx, unused_ly;
  struct wlr_idle_inhibitor_v1 *inhibitor;
  wl_list_for_each(inhibitor, &idle_inhibit_mgr->inhibitors, link) {
    struct wlr_surface *surface =
        wlr_surface_get_root_surface(inhibitor->surface);
    struct wlr_scene_tree *tree = surface->data;
    if (exclude != surface &&
        (bypass_surface_visibility ||
         (!tree ||
          wlr_scene_node_coords(&tree->node, &unused_lx, &unused_ly)))) {
      inhibited = 1;
      break;
    }
  }

  wlr_idle_notifier_v1_set_inhibited(idle_notifier, inhibited);
}

void // 0.5
setcursorshape(struct wl_listener *listener, void *data) {
  struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
  if (cursor_mode != CurNormal && cursor_mode != CurPressed)
    return;
  /* This can be sent by any client, so we check to make sure this one is
   * actually has pointer focus first. If so, we can tell the cursor to
   * use the provided cursor shape. */
  if (event->seat_client == seat->pointer_state.focused_client)
    wlr_cursor_set_xcursor(cursor, cursor_mgr,
                           wlr_cursor_shape_v1_name(event->shape));
}

void // 17
cleanup(void) {
#ifdef XWAYLAND
  wlr_xwayland_destroy(xwayland);
#endif
  wl_display_destroy_clients(dpy);
  if (child_pid > 0) {
    kill(child_pid, SIGTERM);
    waitpid(child_pid, NULL, 0);
  }
  wlr_backend_destroy(backend);
  wlr_scene_node_destroy(&scene->tree.node);
  wlr_renderer_destroy(drw);
  wlr_allocator_destroy(alloc);
  wlr_xcursor_manager_destroy(cursor_mgr);
  wlr_cursor_destroy(cursor);
  wlr_output_layout_destroy(output_layout);
  wlr_seat_destroy(seat);
  wl_display_destroy(dpy);
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

void // 0.5
cleanupmon(struct wl_listener *listener, void *data) {
  Monitor *m = wl_container_of(listener, m, destroy);
  LayerSurface *l, *tmp;
  size_t i;

  DwlIpcOutput *ipc_output, *ipc_output_tmp;
  wl_list_for_each_safe(ipc_output, ipc_output_tmp, &m->dwl_ipc_outputs, link)
  	wl_resource_destroy(ipc_output->resource);

  /* m->layers[i] are intentionally not unlinked */
  for (i = 0; i < LENGTH(m->layers); i++) {
    wl_list_for_each_safe(l, tmp, &m->layers[i], link)
        wlr_layer_surface_v1_destroy(l->layer_surface);
  }

  wl_list_remove(&m->destroy.link);
  wl_list_remove(&m->frame.link);
  wl_list_remove(&m->link);
  wl_list_remove(&m->request_state.link);
  m->wlr_output->data = NULL;
  wlr_output_layout_remove(output_layout, m->wlr_output);
  wlr_scene_output_destroy(m->scene_output);

  closemon(m);
  // wlr_scene_node_destroy(&m->fullscreen_bg->node);
  free(m);
}

void closemon(Monitor *m) // 0.5
{
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
  }

  wl_list_for_each(c, &clients, link) {
    if (c->isfloating && c->geom.x > m->m.width)
      resize(c,
             (struct wlr_box){.x = c->geom.x - m->w.width,
                              .y = c->geom.y,
                              .width = c->geom.width,
                              .height = c->geom.height},
             0);
    if (c->mon == m) {
      setmon(c, selmon, c->tags,true);
      reset_foreign_tolevel(c);
    }
  }
  focusclient(focustop(selmon), 1);
  printstatus();
}

void // 0.5
commitlayersurfacenotify(struct wl_listener *listener, void *data) {
  LayerSurface *l = wl_container_of(listener, l, surface_commit);
  struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
  struct wlr_scene_tree *scene_layer =
      layers[layermap[layer_surface->current.layer]];

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

void client_set_pending_state(Client *c) {

  // 判断是否需要动画
  if (!animations) {
    c->animation.should_animate = false;
  } else if (c->isglobal && c->isfloating) {
    c->animation.should_animate = false;
  } else if (animations && c->animation.tagining) {
    c->animation.should_animate = true;
    c->animation.initial = c->animainit_geom;
  } else if (!animations || c == grabc ||
             (!c->is_open_animation &&
              wlr_box_equal(&c->current, &c->pending))) {
    c->animation.should_animate = false;
  } else {
    c->animation.should_animate = true;
    c->animation.initial = c->animainit_geom;
  }

  // 开始动画
  client_commit(c);
  c->dirty = true;
}

double output_frame_duration_ms(Client *c) {
  int32_t refresh_total  = 0;
  Monitor *m;
  wl_list_for_each(m, &mons, link) {
    if (!m->wlr_output->enabled) {
      continue;
    }
    refresh_total += m->wlr_output->refresh;
  }
  return 1000000.0 / refresh_total;
}

void client_commit(Client *c) {
  c->dirty = false;
  c->current = c->pending; // 设置动画的结束位置

  if (c->animation.should_animate) {
    // 设置动画速度
    c->animation.passed_frames = 0;
    c->animation.total_frames =
        c->animation.duration / output_frame_duration_ms(c);

    // 标记动画开始
    c->animation.running = true;
    c->animation.should_animate = false;
  }
  // 请求刷新屏幕
  wlr_output_schedule_frame(c->mon->wlr_output);
}

void commitnotify(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, commit);

  if (!c || c->iskilling || c->animation.tagining || c->animation.tagining ||
      c->animation.tagouted)
    return;

  if (c == grabc)
    return;

  if(!c->dirty)
    return;

	wlr_log(WLR_DEBUG, "app commit event handle:%s",client_get_appid(c));
  resize(c, c->geom, (c->isfloating && !c->isfullscreen));

  uint32_t width, height;
  client_actual_size(c, &width, &height);
  if(width == c->geom.width && height == c->geom.height)
    c->dirty = false;
  // if (c->configure_serial && c->configure_serial <=
  // c->surface.xdg->current.configure_serial) 	c->configure_serial = 0;
}

void // 0.5
destroydecoration(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, destroy_decoration);

  wl_list_remove(&c->destroy_decoration.link);
  wl_list_remove(&c->set_decoration_mode.link);
  motionnotify(0, NULL, 0, 0, 0, 0);
}

void // 0.5
createdecoration(struct wl_listener *listener, void *data) {
  struct wlr_xdg_toplevel_decoration_v1 *deco = data;
  Client *c = deco->toplevel->base->data;
  c->decoration = deco;

  LISTEN(&deco->events.request_mode, &c->set_decoration_mode,
         requestdecorationmode);
  LISTEN(&deco->events.destroy, &c->destroy_decoration, destroydecoration);

  requestdecorationmode(&c->set_decoration_mode, deco);
}

void // 0.5
createidleinhibitor(struct wl_listener *listener, void *data) {
  struct wlr_idle_inhibitor_v1 *idle_inhibitor = data;
  LISTEN_STATIC(&idle_inhibitor->events.destroy, destroyidleinhibitor);

  checkidleinhibitor(NULL);
}

void // 17
createkeyboard(struct wlr_keyboard *keyboard) {
  struct xkb_context *context;
  struct xkb_keymap *keymap;
  Keyboard *kb = keyboard->data = ecalloc(1, sizeof(*kb));
  kb->wlr_keyboard = keyboard;

  /* Prepare an XKB keymap and assign it to the keyboard. */
  context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  keymap = xkb_keymap_new_from_names(context, &xkb_rules,
                                     XKB_KEYMAP_COMPILE_NO_FLAGS);

  wlr_keyboard_set_keymap(keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  wlr_keyboard_set_repeat_info(keyboard, repeat_rate, repeat_delay);

  if (numlockon == 1) {
    uint32_t leds = 0;
    xkb_mod_mask_t locked_mods = 0;
    leds = leds | WLR_LED_NUM_LOCK;
    // 获取numlock所在的位置
    xkb_mod_index_t mod_index = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_NUM);
    if (mod_index != XKB_MOD_INVALID) {
      locked_mods |= (uint32_t)1
                     << mod_index; // 将该位置设置为1,默认为0表示锁住小键盘
    }
    // 设置numlock为on
    xkb_state_update_mask(keyboard->xkb_state, 0, 0, locked_mods, 0, 0, 0);
    wlr_keyboard_led_update(keyboard, leds); // 将表示numlockon的灯打开
  }

  /* Here we set up listeners for keyboard events. */
  LISTEN(&keyboard->events.modifiers, &kb->modifiers, keypressmod);
  LISTEN(&keyboard->events.key, &kb->key, keypress);
  LISTEN(&keyboard->base.events.destroy, &kb->destroy, cleanupkeyboard);

  wlr_seat_set_keyboard(seat, keyboard);

  kb->key_repeat_source =
      wl_event_loop_add_timer(wl_display_get_event_loop(dpy), keyrepeat, kb);

  /* And add the keyboard to our list of keyboards */
  wl_list_insert(&keyboards, &kb->link);
}

void // 0.5
createlayersurface(struct wl_listener *listener, void *data) {
  struct wlr_layer_surface_v1 *layer_surface = data;
  LayerSurface *l;
  struct wlr_surface *surface = layer_surface->surface;
  struct wlr_scene_tree *scene_layer =
      layers[layermap[layer_surface->pending.layer]];
  struct wlr_layer_surface_v1_state old_state;

  if (!layer_surface->output &&
      !(layer_surface->output = selmon ? selmon->wlr_output : NULL)) {
    wlr_layer_surface_v1_destroy(layer_surface);
    return;
  }

  l = layer_surface->data = ecalloc(1, sizeof(*l));
  l->type = LayerShell;
  LISTEN(&surface->events.commit, &l->surface_commit, commitlayersurfacenotify);
  LISTEN(&surface->events.map, &l->map, maplayersurfacenotify);
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

  /* Temporarily set the layer's current state to pending
   * so that we can easily arrange it
   */
  old_state = layer_surface->current;
  layer_surface->current = layer_surface->pending;
  l->mapped = 1;
  arrangelayers(l->mon);
  layer_surface->current = old_state;
}

void // 0.5
createlocksurface(struct wl_listener *listener, void *data) {
  SessionLock *lock = wl_container_of(listener, lock, new_surface);
  struct wlr_session_lock_surface_v1 *lock_surface = data;
  Monitor *m = lock_surface->output->data;
  struct wlr_scene_tree *scene_tree = lock_surface->surface->data =
      wlr_scene_subsurface_tree_create(lock->scene, lock_surface->surface);
  m->lock_surface = lock_surface;

  wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
  wlr_session_lock_surface_v1_configure(lock_surface, m->m.width, m->m.height);

  LISTEN(&lock_surface->events.destroy, &m->destroy_lock_surface,
         destroylocksurface);

  if (m == selmon)
    client_notify_enter(lock_surface->surface, wlr_seat_get_keyboard(seat));
}

void // 17  need fix 0.5
createmon(struct wl_listener *listener, void *data) {
  /* This event is raised by the backend when a new output (aka a display or
   * monitor) becomes available. */
  struct wlr_output *wlr_output = data;
  const ConfigMonitorRule *r;
  size_t i;
  int ji, jk;
  Monitor *m = wlr_output->data = ecalloc(1, sizeof(*m));
  m->wlr_output = wlr_output;

  wl_list_init(&m->dwl_ipc_outputs);
  wlr_output_init_render(wlr_output, alloc, drw);

  /* Initialize monitor state using configured rules */
  for (i = 0; i < LENGTH(m->layers); i++)
    wl_list_init(&m->layers[i]);

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

  m->lt = &layouts[0];

  for (ji = 0; ji < config.monitor_rules_count; ji++) {
    if (config.monitor_rules_count < 1)
      break;

    r = &config.monitor_rules[ji];
    if (!r->name || strstr(wlr_output->name, r->name)) {
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
      break;
    }
  }

  wlr_output_set_scale(wlr_output, scale);
  wlr_xcursor_manager_load(cursor_mgr, scale);
  wlr_output_set_transform(wlr_output, rr);
  /* The mode is a tuple of (width, height, refresh rate), and each
   * monitor supports only a specific set of modes. We just pick the
   * monitor's preferred mode; a more sophisticated compositor would let
   * the user configure it. */
  wlr_output_set_mode(wlr_output, wlr_output_preferred_mode(wlr_output));

  /* Set up event listeners */
  LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
  LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
  LISTEN(&wlr_output->events.request_state, &m->request_state, requestmonstate);

  wlr_output_enable(wlr_output, 1);
  if (!wlr_output_commit(wlr_output))
    return;

  /* Try to enable adaptive sync, note that not all monitors support it.
   * wlr_output_commit() will deactivate it in case it cannot be enabled */
  wlr_output_enable_adaptive_sync(wlr_output, 1);
  wlr_output_commit(wlr_output);

  wl_list_insert(&mons, &m->link);

  m->pertag = calloc(1, sizeof(Pertag));
  m->pertag->curtag = m->pertag->prevtag = 1;

  for (i = 0; i <= LENGTH(tags); i++) {
    m->pertag->nmasters[i] = m->nmaster;
    m->pertag->mfacts[i] = m->mfact;
    m->pertag->smfacts[i] = default_smfact;
    m->pertag->ltidxs[i] = m->lt;

    if (i > 0 && strlen(config.tags[i - 1].layout_name) > 0) {
      for (jk = 0; jk < LENGTH(layouts); jk++) {
        if (strcmp(layouts[jk].name, config.tags[i - 1].layout_name) == 0) {
          m->pertag->ltidxs[i] = &layouts[jk];
        }
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
  /* updatemons() will resize and set correct position */
  // m->fullscreen_bg = wlr_scene_rect_create(layers[LyrFS], 0, 0,
  // fullscreen_bg); wlr_scene_node_set_enabled(&m->fullscreen_bg->node, 0);

  /* Adds this to the output layout in the order it was configured in.
   *
   * The output layout utility automatically adds a wl_output global to the
   * display, which Wayland clients can see to find out information about the
   * output (such as DPI, scale factor, manufacturer, etc).
   */
  m->scene_output = wlr_scene_output_create(scene, wlr_output);
  if (m->m.x < 0 || m->m.y < 0)
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
  struct wlr_xdg_surface *xdg_surface = data;
  Client *c = NULL;
  LayerSurface *l = NULL;

  if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
    struct wlr_xdg_popup *popup = xdg_surface->popup;
    struct wlr_box box;
    if (toplevel_from_wlr_surface(popup->base->surface, &c, &l) < 0)
      return;
    popup->base->surface->data =
        wlr_scene_xdg_surface_create(popup->parent->data, popup->base);
    if ((l && !l->mon) || (c && !c->mon))
      return;
    box = l ? l->mon->m : c->mon->w;
    box.x -= (l ? l->geom.x : c->geom.x);
    box.y -= (l ? l->geom.y : c->geom.y);
    wlr_xdg_popup_unconstrain_from_box(popup, &box);
    return;
  } else if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_NONE)
    return;

  /* Allocate a Client for this surface */
  c = xdg_surface->data = ecalloc(1, sizeof(*c));
  c->surface.xdg = xdg_surface;
  c->bw = borderpx;

  wlr_xdg_toplevel_set_wm_capabilities(
      xdg_surface->toplevel, WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);

  LISTEN(&xdg_surface->surface->events.commit, &c->commit, commitnotify);
  LISTEN(&xdg_surface->surface->events.map, &c->map, mapnotify);
  LISTEN(&xdg_surface->surface->events.unmap, &c->unmap, unmapnotify);
  LISTEN(&xdg_surface->events.destroy, &c->destroy, destroynotify);
  LISTEN(&xdg_surface->toplevel->events.set_title, &c->set_title, updatetitle);
  LISTEN(&xdg_surface->toplevel->events.request_fullscreen, &c->fullscreen,
         fullscreennotify);
  LISTEN(&xdg_surface->toplevel->events.request_maximize, &c->maximize,
         maximizenotify);
  LISTEN(&xdg_surface->toplevel->events.request_minimize, &c->minimize,
         minimizenotify);
}

void // 0.5
createpointer(struct wlr_pointer *pointer) {
  struct libinput_device *device;
  if (wlr_input_device_is_libinput(&pointer->base) &&
      (device = wlr_libinput_get_device_handle(&pointer->base))) {

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
      libinput_device_config_dwt_set_enabled(device, disable_while_typing);

    if (libinput_device_config_left_handed_is_available(device))
      libinput_device_config_left_handed_set(device, left_handed);

    if (libinput_device_config_middle_emulation_is_available(device))
      libinput_device_config_middle_emulation_set_enabled(
          device, middle_button_emulation);

    if (libinput_device_config_scroll_get_methods(device) !=
        LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
      libinput_device_config_scroll_set_method(device, scroll_method);

    if (libinput_device_config_click_get_methods(device) !=
        LIBINPUT_CONFIG_CLICK_METHOD_NONE)
      libinput_device_config_click_set_method(device, click_method);

    if (libinput_device_config_send_events_get_modes(device))
      libinput_device_config_send_events_set_mode(device, send_events_mode);

    if (libinput_device_config_accel_is_available(device)) {
      libinput_device_config_accel_set_profile(device, accel_profile);
      libinput_device_config_accel_set_speed(device, accel_speed);
    }
  }

  wlr_cursor_attach_input_device(cursor, &pointer->base);
}

void // 0.5
createpointerconstraint(struct wl_listener *listener, void *data) {
  PointerConstraint *pointer_constraint =
      ecalloc(1, sizeof(*pointer_constraint));
  pointer_constraint->constraint = data;
  LISTEN(&pointer_constraint->constraint->events.destroy,
         &pointer_constraint->destroy, destroypointerconstraint);
}

void // 0.5
cursorconstrain(struct wlr_pointer_constraint_v1 *constraint) {
  if (active_constraint == constraint)
    return;

  if (active_constraint)
    wlr_pointer_constraint_v1_send_deactivated(active_constraint);

  active_constraint = constraint;
  wlr_pointer_constraint_v1_send_activated(constraint);
}

void // 0.5
cursorframe(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits an frame
   * event. Frame events are sent after regular pointer events to group
   * multiple events together. For instance, two axis events may happen at the
   * same time, in which case a frame event won't be sent in between. */
  /* Notify the client with pointer focus of the frame event. */
  wlr_seat_pointer_notify_frame(seat);
}

void // 0.5
cursorwarptohint(void) {
  Client *c = NULL;
  double sx = active_constraint->current.cursor_hint.x;
  double sy = active_constraint->current.cursor_hint.y;

  toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
  /* TODO: wlroots 0.18:
   * https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4478 */
  if (c && (active_constraint->current.committed &
            WLR_POINTER_CONSTRAINT_V1_STATE_CURSOR_HINT)) {
    wlr_cursor_warp(cursor, NULL, sx + c->geom.x + c->bw,
                    sy + c->geom.y + c->bw);
    wlr_seat_pointer_warp(active_constraint->seat, sx, sy);
  }
}

void defaultgaps(const Arg *arg) { setgaps(gappoh, gappov, gappih, gappiv); }

void // 0.5
destroydragicon(struct wl_listener *listener, void *data) {
  /* Focus enter isn't sent during drag, so refocus the focused node. */
  focusclient(focustop(selmon), 1);
  motionnotify(0, NULL, 0, 0, 0, 0);
}

void // 17
destroyidleinhibitor(struct wl_listener *listener, void *data) {
  /* `data` is the wlr_surface of the idle inhibitor being destroyed,
   * at this point the idle inhibitor is still in the list of the manager */
  checkidleinhibitor(wlr_surface_get_root_surface(data));
}

void // 0.5
destroylayersurfacenotify(struct wl_listener *listener, void *data) {
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

void // 0.5
destroylock(SessionLock *lock, int unlock) {
  wlr_seat_keyboard_notify_clear_focus(seat);
  if ((locked = !unlock))
    goto destroy;

  wlr_scene_node_set_enabled(&locked_bg->node, 0);

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

void // 0.5
destroylocksurface(struct wl_listener *listener, void *data) {
  Monitor *m = wl_container_of(listener, m, destroy_lock_surface);
  struct wlr_session_lock_surface_v1 *surface, *lock_surface = m->lock_surface;

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

void // 0.5
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

void // 0.5
destroypointerconstraint(struct wl_listener *listener, void *data) {
  PointerConstraint *pointer_constraint =
      wl_container_of(listener, pointer_constraint, destroy);

  if (active_constraint == pointer_constraint->constraint) {
    cursorwarptohint();
    active_constraint = NULL;
  }

  wl_list_remove(&pointer_constraint->destroy.link);
  free(pointer_constraint);
}

void // 0.5
destroysessionlock(struct wl_listener *listener, void *data) {
  SessionLock *lock = wl_container_of(listener, lock, destroy);
  destroylock(lock, 0);
  motionnotify(0, NULL, 0, 0, 0, 0);
}

void // 0.5
destroysessionmgr(struct wl_listener *listener, void *data) {
  wl_list_remove(&lock_listener.link);
  wl_list_remove(&listener->link);
}

void // 17
associatex11(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, associate);

  LISTEN(&client_surface(c)->events.map, &c->map, mapnotify);
  LISTEN(&client_surface(c)->events.unmap, &c->unmap, unmapnotify);
}

void // 17
dissociatex11(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, dissociate);
  wl_list_remove(&c->map.link);
  wl_list_remove(&c->unmap.link);
}

Monitor * // 0.5
dirtomon(enum wlr_direction dir) {
  struct wlr_output *next;
  if (!wlr_output_layout_get(output_layout, selmon->wlr_output))
    return selmon;
  if ((next = wlr_output_layout_adjacent_output(
           output_layout, 1 << dir, selmon->wlr_output, selmon->m.x, selmon->m.y)))
    return next->data;
  if ((next = wlr_output_layout_farthest_output(
           output_layout, dir ^ (WLR_DIRECTION_LEFT | WLR_DIRECTION_RIGHT),
           selmon->wlr_output, selmon->m.x, selmon->m.y)))
    return next->data;
  return selmon;
}

void
dwl_ipc_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *manager_resource = wl_resource_create(client, &zdwl_ipc_manager_v2_interface, version, id);
	if (!manager_resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(manager_resource, &dwl_manager_implementation, NULL, dwl_ipc_manager_destroy);

	zdwl_ipc_manager_v2_send_tags(manager_resource, LENGTH(tags));

	for (unsigned int i = 0; i < LENGTH(layouts); i++)
		zdwl_ipc_manager_v2_send_layout(manager_resource, layouts[i].symbol);
}

void
dwl_ipc_manager_destroy(struct wl_resource *resource)
{
	/* No state to destroy */
}

void
dwl_ipc_manager_get_output(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *output)
{
	DwlIpcOutput *ipc_output;
  struct wlr_output *op = wlr_output_from_resource(output);
  if(!op)
    return;
	Monitor *monitor = op->data;
	struct wl_resource *output_resource = wl_resource_create(client, &zdwl_ipc_output_v2_interface, wl_resource_get_version(resource), id);
	if (!output_resource)
		return;

	ipc_output = ecalloc(1, sizeof(*ipc_output));
	ipc_output->resource = output_resource;
	ipc_output->mon = monitor;
	wl_resource_set_implementation(output_resource, &dwl_output_implementation, ipc_output, dwl_ipc_output_destroy);
	wl_list_insert(&monitor->dwl_ipc_outputs, &ipc_output->link);
	dwl_ipc_output_printstatus_to(ipc_output);
}

void
dwl_ipc_manager_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
dwl_ipc_output_destroy(struct wl_resource *resource)
{
	DwlIpcOutput *ipc_output = wl_resource_get_user_data(resource);
	wl_list_remove(&ipc_output->link);
	free(ipc_output);
}

void
dwl_ipc_output_printstatus(Monitor *monitor)
{
	DwlIpcOutput *ipc_output;
	wl_list_for_each(ipc_output, &monitor->dwl_ipc_outputs, link)
		dwl_ipc_output_printstatus_to(ipc_output);
}

void dwl_ipc_output_printstatus_to(DwlIpcOutput *ipc_output) {
  Monitor *monitor = ipc_output->mon;
  Client *c, *focused;
  int tagmask, state, numclients, focused_client, tag;
  const char *title, *appid, *symbol;
  focused = focustop(monitor);
  zdwl_ipc_output_v2_send_active(ipc_output->resource, monitor == selmon);

  // if ((monitor->tagset[monitor->seltags] & TAGMASK) == TAGMASK) {
  //   state = 0;
  //   state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE;
  //   zdwl_ipc_output_v2_send_tag(ipc_output->resource, 888, state, 1, 1);
  // } else {
  //   for (tag = 0; tag < LENGTH(tags); tag++) {
  //     numclients = state = focused_client = 0;
  //     tagmask = 1 << tag;
  //     if ((tagmask & monitor->tagset[monitor->seltags]) != 0)
  //       state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE;

  //     wl_list_for_each(c, &clients, link) {
  //       if (c->iskilling)
  //         continue;
  //       if (c->mon != monitor)
  //         continue;
  //       if (!(c->tags & tagmask))
  //         continue;
  //       if (c == focused)
  //         focused_client = 1;
  //       if (c->isurgent)
  //         state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_URGENT;

  //       numclients++;
  //     }
  //     zdwl_ipc_output_v2_send_tag(ipc_output->resource, tag, state, numclients,
  //                                 focused_client);
  //   }
  // }

  for ( tag = 0 ; tag < LENGTH(tags); tag++) {
      numclients = state = focused_client = 0;
      tagmask = 1 << tag;
      if ((tagmask & monitor->tagset[monitor->seltags]) != 0)
          state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE;
      wl_list_for_each(c, &clients, link) {
          if (c->mon != monitor)
              continue;
          if (!(c->tags & tagmask))
              continue;
          if (c == focused)
              focused_client = 1;
          if (c->isurgent)
              state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_URGENT;
          numclients++;
      }
      zdwl_ipc_output_v2_send_tag(ipc_output->resource, tag, state,
      numclients, focused_client);
  }

  title = focused ? client_get_title(focused) : "";
  appid = focused ? client_get_appid(focused) : "";
  symbol = monitor->pertag->ltidxs[monitor->pertag->curtag]->symbol;

  zdwl_ipc_output_v2_send_layout(
    ipc_output->resource,
    monitor->pertag->ltidxs[monitor->pertag->curtag] - layouts);
  zdwl_ipc_output_v2_send_title(ipc_output->resource, title ? title : broken);
  zdwl_ipc_output_v2_send_appid(ipc_output->resource, appid ? appid : broken);
  zdwl_ipc_output_v2_send_layout_symbol(ipc_output->resource, symbol);
	if (wl_resource_get_version(ipc_output->resource) >= ZDWL_IPC_OUTPUT_V2_FULLSCREEN_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_fullscreen(ipc_output->resource, focused ? focused->isfullscreen : 0);
	}
	if (wl_resource_get_version(ipc_output->resource) >= ZDWL_IPC_OUTPUT_V2_FLOATING_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_floating(ipc_output->resource, focused ? focused->isfloating : 0);
	}
	zdwl_ipc_output_v2_send_frame(ipc_output->resource);
}

void
dwl_ipc_output_set_client_tags(struct wl_client *client, struct wl_resource *resource, uint32_t and_tags, uint32_t xor_tags)
{
	DwlIpcOutput *ipc_output;
	Monitor *monitor;
	Client *selected_client;
	unsigned int newtags = 0;

	ipc_output = wl_resource_get_user_data(resource);
	if (!ipc_output)
		return;

	monitor = ipc_output->mon;
	selected_client = focustop(monitor);
	if (!selected_client)
		return;

	newtags = (selected_client->tags & and_tags) ^ xor_tags;
	if (!newtags)
		return;

	selected_client->tags = newtags;
	if (selmon == monitor)
		focusclient(focustop(monitor), 1);
	arrange(selmon,false);
	printstatus();
}

void dwl_ipc_output_set_layout(struct wl_client *client,
                               struct wl_resource *resource, uint32_t index) {
  DwlIpcOutput *ipc_output;
  Monitor *monitor;

  ipc_output = wl_resource_get_user_data(resource);
  if (!ipc_output)
    return;

  monitor = ipc_output->mon;
  if (index >= LENGTH(layouts))
    index = 0;

  monitor->pertag->ltidxs[monitor->pertag->curtag] = &layouts[index];
  arrange(monitor, false);
  printstatus();
}

void
dwl_ipc_output_quit(struct wl_client *client, struct wl_resource *resource)
{
  quit(&(Arg){0});
}

void
dwl_ipc_output_set_tags(struct wl_client *client, struct wl_resource *resource, uint32_t tagmask, uint32_t toggle_tagset)
{
	DwlIpcOutput *ipc_output;
	Monitor *monitor;
	unsigned int newtags = tagmask & TAGMASK;

	ipc_output = wl_resource_get_user_data(resource);
	if (!ipc_output)
		return;
	monitor = ipc_output->mon;

  view_in_mon(&(Arg){.ui = newtags}, true,monitor);
}

void
dwl_ipc_output_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

void focusclient(Client *c, int lift) {
  struct wlr_surface *old_keyboard_focus_surface =
      seat->keyboard_state.focused_surface;

  if (locked)
    return;

  if (c && !client_surface(c)->mapped)
    return;
  if (c && c->iskilling)
    return;

  if (c && c->animation.tagouting && !c->animation.tagouting)
    return;

  /* Raise client in stacking order if requested */
  if (c && lift)
    wlr_scene_node_raise_to_top(&c->scene->node); // 将视图提升到顶层

  if (c && client_surface(c) == old_keyboard_focus_surface)
    return;

  if (selmon && selmon->sel && selmon->sel->foreign_toplevel) {
    wlr_foreign_toplevel_handle_v1_set_activated(selmon->sel->foreign_toplevel,
                                                 false);
  }

  if(c && !c->iskilling && c->mon) {
    selmon = c->mon;
  }

  if (selmon) {
    selmon->prevsel = selmon->sel;
    selmon->sel = c;
    if (c && selmon->prevsel && !selmon->prevsel->isfloating &&
        selmon->prevsel->tags == c->tags && !c->isfloating &&
        !c->isfullscreen &&
        strcmp(selmon->pertag->ltidxs[selmon->pertag->curtag]->name,
               "scroller") == 0) {
      arrange(selmon, false);
    } else if (selmon->prevsel) {
      selmon->prevsel = NULL;
    }
  }
  if (c && c->foreign_toplevel)
    wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);

  /* Put the new client atop the focus stack and select its monitor */
  if (c && !client_is_unmanaged(c)) {
    wl_list_remove(&c->flink);
    wl_list_insert(&fstack, &c->flink);
    selmon = c->mon;
    c->isurgent = 0;
    client_restack_surface(c);
    setborder_color(c);
    /* Don't change border color if there is an exclusive focus or we are
     * handling a drag operation */
  }

  /* Deactivate old client if focus is changing */
  if (old_keyboard_focus_surface &&
      (!c || client_surface(c) != old_keyboard_focus_surface)) {
    /* If an overlay is focused, don't focus or activate the client,
     * but only update its position in fstack to render its border with
     * focuscolor and focus it after the overlay is closed. */
    Client *w = NULL;
    LayerSurface *l = NULL;
    int type = toplevel_from_wlr_surface(old_keyboard_focus_surface, &w, &l);
    if (type == LayerShell && l->scene->node.enabled &&
        l->layer_surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
      return;
    } else if (w && w == exclusive_focus && client_wants_focus(w)) {
      return;
      /* Don't deactivate old_keyboard_focus_surface client if the new one wants
       * focus, as this causes issues with winecfg and probably other clients */
    } else if (w && !client_is_unmanaged(w) && (!c || !client_wants_focus(c))) {
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
#ifdef IM
    dwl_input_method_relay_set_focus(input_relay, NULL);
#endif
    wlr_seat_keyboard_notify_clear_focus(seat);
    return;
  }

  /* Change cursor surface */
  motionnotify(0, NULL, 0, 0, 0, 0);

  /* Have a client, so focus its top-level wlr_surface */
  client_notify_enter(client_surface(c), wlr_seat_get_keyboard(seat));

#ifdef IM
  struct wlr_keyboard *keyboard;
  keyboard = wlr_seat_get_keyboard(seat);
  uint32_t mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
  if (mods == 0)
    dwl_input_method_relay_set_focus(input_relay, client_surface(c));
#endif
  /* Activate the new client */
  client_activate_surface(client_surface(c), 1);
}

void focusmon(const Arg *arg) {
  Client *c;
  int i = 0, nmons = wl_list_length(&mons);
  if (nmons) {
    do /* don't switch to disabled mons */
      selmon = dirtomon(arg->i);
    while (!selmon->wlr_output->enabled && i++ < nmons);
  }
  warp_cursor_to_selmon(selmon);
  c = focustop(selmon);
  if(!c)
    selmon->sel = NULL;
  else
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
      if (&c->link == &clients)
        continue; /* wrap past the sentinel node */
      if (VISIBLEON(c, selmon))
        break; /* found it */
    }
  } else {
    wl_list_for_each_reverse(c, &sel->link, link) {
      if (&c->link == &clients)
        continue; /* wrap past the sentinel node */
      if (VISIBLEON(c, selmon))
        break; /* found it */
    }
  }
  /* If only one client is visible on selmon, then c == sel */
  focusclient(c, 1);
}

/* We probably should change the name of this, it sounds like
 * will focus the topmost client of this mon, when actually will
 * only return that client */
Client * // 0.5
focustop(Monitor *m) {
  Client *c;
  wl_list_for_each(c, &fstack, flink) {
    if (c->iskilling)
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

void incihgaps(const Arg *arg) {
  setgaps(selmon->gappoh, selmon->gappov, selmon->gappih + arg->i,
          selmon->gappiv);
}

void incivgaps(const Arg *arg) {
  setgaps(selmon->gappoh, selmon->gappov, selmon->gappih,
          selmon->gappiv + arg->i);
}

void incogaps(const Arg *arg) {
  setgaps(selmon->gappoh + arg->i, selmon->gappov + arg->i, selmon->gappih,
          selmon->gappiv);
}

void incohgaps(const Arg *arg) {
  setgaps(selmon->gappoh + arg->i, selmon->gappov, selmon->gappih,
          selmon->gappiv);
}

void requestmonstate(struct wl_listener *listener, void *data) {
  struct wlr_output_event_request_state *event = data;
  wlr_output_commit_state(event->output, event->state);
  updatemons(NULL, NULL);
}

void incovgaps(const Arg *arg) {
  setgaps(selmon->gappoh, selmon->gappov + arg->i, selmon->gappih,
          selmon->gappiv);
}

void // 17
inputdevice(struct wl_listener *listener, void *data) {
  /* This event is raised by the backend when a new input device becomes
   * available. */
  struct wlr_input_device *device = data;
  uint32_t caps;

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
  if (!wl_list_empty(&keyboards))
    caps |= WL_SEAT_CAPABILITY_KEYBOARD;
  wlr_seat_set_capabilities(seat, caps);
}

int keyrepeat(void *data) {
  Keyboard *kb = data;
  int i;
  if (!kb->nsyms || kb->wlr_keyboard->repeat_info.rate <= 0)
    return 0;

  wl_event_source_timer_update(kb->key_repeat_source,
                               1000 / kb->wlr_keyboard->repeat_info.rate);

  for (i = 0; i < kb->nsyms; i++)
    keybinding(kb->mods, kb->keysyms[i]);

  return 0;
}

int // 17
keybinding(uint32_t mods, xkb_keysym_t sym) {
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
    if (CLEANMASK(mods) == CLEANMASK(k->mod) && sym == k->keysym && k->func) {
      k->func(&k->arg);
      handled = 1;
    }
  }
  return handled;
}

bool
keypressglobal(struct wlr_surface *last_surface, struct wlr_keyboard *keyboard, struct wlr_keyboard_key_event *event, uint32_t mods, xkb_keysym_t keysym)
{
	Client *c = NULL, *lastc = focustop(selmon);
	uint32_t keycodes[32] = {0};
	int reset = false;
	const char *appid = NULL;
  const char *title = NULL;
  int appid_len,title_len,ji;
  const ConfigWinRule *r;

  for (ji = 0; ji < config.window_rules_count; ji++) {
    if (config.window_rules_count < 1)
      break;
    r = &config.window_rules[ji];

    if(!r->globalkeybinding.mod || !r->globalkeybinding.keysym )
      continue;


		/* match key only (case insensitive) ignoring mods */
		if (r->globalkeybinding.keysym  == keysym && r->globalkeybinding.mod == mods) {
			wl_list_for_each(c, &clients, link) {
				if (c && c != lastc) {
					appid = client_get_appid(c);
          title = client_get_title(c);
          if (appid && r->id) {
            appid_len = strlen(appid);
            if(strncmp(appid, r->id, appid_len) == 0) {
              reset = true;
              wlr_seat_keyboard_enter(seat, client_surface(c), keycodes, 0, &keyboard->modifiers);
              wlr_seat_keyboard_send_key(seat, event->time_msec, event->keycode, event->state);
              goto done;              
            }
          }
          
          if (title && r->title) {
            title_len = strlen(title);
            if(strncmp(title, r->title, title_len) == 0) {
              reset = true;
              wlr_seat_keyboard_enter(seat, client_surface(c), keycodes, 0, &keyboard->modifiers);  
              wlr_seat_keyboard_send_key(seat, event->time_msec, event->keycode, event->state);
              goto done;              
            }
          }
				}
			}
		}
	}

done:
	if (reset)
		wlr_seat_keyboard_enter(seat, last_surface, keycodes, 0, &keyboard->modifiers);
  return reset;
}

void // 17
keypress(struct wl_listener *listener, void *data) {
  int i;
  /* This event is raised when a key is pressed or released. */
  Keyboard *kb = wl_container_of(listener, kb, key);
  struct wlr_keyboard_key_event *event = data;

	struct wlr_surface *last_surface = seat->keyboard_state.focused_surface;
	struct wlr_xdg_surface *xdg_surface = last_surface ? wlr_xdg_surface_try_from_wlr_surface(last_surface) : NULL;
	int pass = 0;
  bool hit_global = false;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface = last_surface ? wlr_xwayland_surface_try_from_wlr_surface(last_surface) : NULL;
#endif

  /* Translate libinput keycode -> xkbcommon */
  uint32_t keycode = event->keycode + 8;
  /* Get a list of keysyms based on the keymap for this keyboard */
  const xkb_keysym_t *syms;
  int nsyms =
      xkb_state_key_get_syms(kb->wlr_keyboard->xkb_state, keycode, &syms);

  int handled = 0;
  uint32_t mods = wlr_keyboard_get_modifiers(kb->wlr_keyboard);

  wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

  // ov tab mode detect moe key release
  if (ov_tab_mode && !locked &&
      event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
      (keycode == 133 || keycode == 37 || keycode == 64 || keycode == 50 ||
       keycode == 134 || keycode == 105 || keycode == 108 || keycode == 62) &&
      selmon->sel) {
    if (selmon->isoverview && selmon->sel) {
      toggleoverview(&(Arg){.i = -1});
    }
  }

#ifdef IM
  if (!locked && event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
      (keycode == 133 || keycode == 37 || keycode == 64 || keycode == 50 ||
       keycode == 134 || keycode == 105 || keycode == 108 || keycode == 62) &&
      selmon->sel) {
    dwl_input_method_relay_set_focus(input_relay, client_surface(selmon->sel));
  }
#endif

  /* On _press_ if there is no active screen locker,
   * attempt to process a compositor keybinding. */
  if (!locked && event->state == WL_KEYBOARD_KEY_STATE_PRESSED)
    for (i = 0; i < nsyms; i++)
      handled = keybinding(mods, syms[i]) || handled;

  if (handled && kb->wlr_keyboard->repeat_info.delay > 0) {
    kb->mods = mods;
    kb->keysyms = syms;
    kb->nsyms = nsyms;
    wl_event_source_timer_update(kb->key_repeat_source,
                                 kb->wlr_keyboard->repeat_info.delay);
  } else {
    kb->nsyms = 0;
    wl_event_source_timer_update(kb->key_repeat_source, 0);
  }

  if (handled)
    return;

	/* don't pass when popup is focused
	 * this is better than having popups (like fuzzel or wmenu) closing while typing in a passed keybind */
	pass = (xdg_surface && xdg_surface->role != WLR_XDG_SURFACE_ROLE_POPUP) || !last_surface
#ifdef XWAYLAND
		|| xsurface
#endif
		;
	/* passed keys don't get repeated */
	if (pass && syms)
    hit_global = keypressglobal(last_surface, kb->wlr_keyboard, event, mods, syms[0]);

  if(hit_global) {
    return;
  }

#ifdef IM
  /* if there is a keyboard grab, we send the key there */
  struct wlr_input_method_keyboard_grab_v2 *kb_grab = keyboard_get_im_grab(kb);
  if (kb_grab) {
    wlr_input_method_keyboard_grab_v2_set_keyboard(kb_grab, kb->wlr_keyboard);
    wlr_input_method_keyboard_grab_v2_send_key(kb_grab, event->time_msec,
                                               event->keycode, event->state);
    wlr_log(WLR_DEBUG, "keypress send to IM:%u mods %u state %u",
            event->keycode, mods, event->state);
    return;
  }
#endif

  /* Pass unhandled keycodes along to the client. */
  wlr_seat_set_keyboard(seat, kb->wlr_keyboard);
  wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
                               event->state);
}

void // 17
keypressmod(struct wl_listener *listener, void *data) {
  /* This event is raised when a modifier key, such as shift or alt, is
   * pressed. We simply communicate this to the client. */
  Keyboard *kb = wl_container_of(listener, kb, modifiers);
#ifdef IM
  struct wlr_input_method_keyboard_grab_v2 *kb_grab = keyboard_get_im_grab(kb);
  if (kb_grab) {
    wlr_input_method_keyboard_grab_v2_send_modifiers(
        kb_grab, &kb->wlr_keyboard->modifiers);
    wlr_log(WLR_DEBUG, "keypressmod send to IM");
    return;
  }
#endif
  /*
   * A seat can only have one keyboard, but this is a limitation of the
   * Wayland protocol - not wlroots. We assign all connected keyboards to the
   * same seat. You can swap out the underlying wlr_keyboard like this and
   * wlr_seat handles this transparently.
   */
  wlr_seat_set_keyboard(seat, kb->wlr_keyboard);
  /* Send modifiers to the client. */
  wlr_seat_keyboard_notify_modifiers(seat, &kb->wlr_keyboard->modifiers);
}

static bool scene_node_snapshot(struct wlr_scene_node *node, int lx, int ly,
                                struct wlr_scene_tree *snapshot_tree) {
  if (!node->enabled && node->type != WLR_SCENE_NODE_TREE) {
    return true;
  }

  lx += node->x;
  ly += node->y;

  struct wlr_scene_node *snapshot_node = NULL;
  switch (node->type) {
  case WLR_SCENE_NODE_TREE:;
    struct wlr_scene_tree *scene_tree = wlr_scene_tree_from_node(node);
    struct wlr_scene_node *child;
    wl_list_for_each(child, &scene_tree->children, link) {
      scene_node_snapshot(child, lx, ly, snapshot_tree);
    }
    break;
  case WLR_SCENE_NODE_RECT:;

    struct wlr_scene_rect *scene_rect = wlr_scene_rect_from_node(node);

    struct wlr_scene_rect *snapshot_rect =
        wlr_scene_rect_create(snapshot_tree, scene_rect->width,
                              scene_rect->height, scene_rect->color);
    snapshot_rect->node.data = scene_rect->node.data;
    if (snapshot_rect == NULL) {
      return false;
    }
    snapshot_node = &snapshot_rect->node;
    break;
  case WLR_SCENE_NODE_BUFFER:;

    struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);

    struct wlr_scene_buffer *snapshot_buffer =
        wlr_scene_buffer_create(snapshot_tree, NULL);
    if (snapshot_buffer == NULL) {
      return false;
    }
    snapshot_node = &snapshot_buffer->node;
    snapshot_buffer->node.data = scene_buffer->node.data;

    wlr_scene_buffer_set_dest_size(snapshot_buffer, scene_buffer->dst_width,
                                   scene_buffer->dst_height);
    wlr_scene_buffer_set_opaque_region(snapshot_buffer,
                                       &scene_buffer->opaque_region);
    wlr_scene_buffer_set_source_box(snapshot_buffer, &scene_buffer->src_box);
    wlr_scene_buffer_set_transform(snapshot_buffer, scene_buffer->transform);
    wlr_scene_buffer_set_filter_mode(snapshot_buffer,
                                     scene_buffer->filter_mode);

    // Effects
    wlr_scene_buffer_set_opacity(snapshot_buffer, scene_buffer->opacity);

    wlr_scene_buffer_set_opacity(snapshot_buffer, scene_buffer->opacity);

    snapshot_buffer->node.data = scene_buffer->node.data;

    struct wlr_scene_surface *scene_surface =
        wlr_scene_surface_try_from_buffer(scene_buffer);
    if (scene_surface != NULL && scene_surface->surface->buffer != NULL) {
      wlr_scene_buffer_set_buffer(snapshot_buffer,
                                  &scene_surface->surface->buffer->base);
    } else {
      wlr_scene_buffer_set_buffer(snapshot_buffer, scene_buffer->buffer);
    }
    break;
  }

  if (snapshot_node != NULL) {
    wlr_scene_node_set_position(snapshot_node, lx, ly);
  }

  return true;
}

struct wlr_scene_tree *wlr_scene_tree_snapshot(struct wlr_scene_node *node,
                                               struct wlr_scene_tree *parent) {
  struct wlr_scene_tree *snapshot = wlr_scene_tree_create(parent);
  if (snapshot == NULL) {
    return NULL;
  }

  // Disable and enable the snapshot tree like so to atomically update
  // the scene-graph. This will prevent over-damaging or other weirdness.
  wlr_scene_node_set_enabled(&snapshot->node, false);

  if (!scene_node_snapshot(node, 0, 0, snapshot)) {
    wlr_scene_node_destroy(&snapshot->node);
    return NULL;
  }

  wlr_scene_node_set_enabled(&snapshot->node, true);

  return snapshot;
}

void pending_kill_client(Client *c) {
  // c->iskilling = 1; //不可以提前标记已经杀掉，因为有些客户端可能拒绝
  client_send_close(c);
}

void killclient(const Arg *arg) {
  Client *c;
  c = selmon->sel;
  if (c) {
    pending_kill_client(c);
  }
}

void // 0.5
locksession(struct wl_listener *listener, void *data) {
  struct wlr_session_lock_v1 *session_lock = data;
  SessionLock *lock;
  wlr_scene_node_set_enabled(&locked_bg->node, 1);
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

void // 0.5
maplayersurfacenotify(struct wl_listener *listener, void *data) {
  motionnotify(0, NULL, 0, 0, 0, 0);
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
  // c->timer_tick = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
  // timer_tick_action, c); wl_event_source_timer_update(c->timer_tick, 0);

  /* Handle unmanaged clients first so we can return prior create borders */
  if (client_is_unmanaged(c)) {
    /* Unmanaged clients always are floating */
    wlr_scene_node_reparent(&c->scene->node, layers[LyrFloat]);
    wlr_scene_node_set_position(&c->scene->node, c->geom.x + borderpx,
                                c->geom.y + borderpx);
    if (client_wants_focus(c)) {
      focusclient(c, 1);
      exclusive_focus = c;
    }
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
  c->geom.width += 2 * c->bw;
  c->geom.height += 2 * c->bw;
  c->ismaxmizescreen = 0;
  c->isfullscreen = 0;
  c->need_float_size_reduce = 0;
  c->iskilling = 0;
  c->scroller_proportion = scroller_default_proportion;
  c->need_scale_first_frame = true;
  // nop
  if (new_is_master &&
      strcmp(selmon->pertag->ltidxs[selmon->pertag->curtag]->name,
             "scroller") != 0)
    // tile at the top
    wl_list_insert(&clients, &c->link); // 新窗口是master,头部入栈
  else if (strcmp(selmon->pertag->ltidxs[selmon->pertag->curtag]->name,
                  "scroller") == 0 &&
           selmon->sel && ISTILED(selmon->sel)) {
    selmon->sel->link.next->prev = &c->link;
    c->link.prev = &selmon->sel->link;
    c->link.next = selmon->sel->link.next;
    selmon->sel->link.next = &c->link;
  } else
    wl_list_insert(clients.prev, &c->link); // 尾部入栈
  wl_list_insert(&fstack, &c->flink);

  /* Set initial monitor, tags, floating status, and focus:
   * we always consider floating, clients that have parent and thus
   * we set the same tags and monitor than its parent, if not
   * try to apply rules for them */
  if ((p = client_get_parent(c))) {
    c->isfloating = 1;
    setmon(c, p->mon, p->tags,true);
  } else {
    applyrules(c);
  }

  if(!c->foreign_toplevel && c->mon)
    add_foreign_toplevel(c);

  if (selmon->sel && selmon->sel->foreign_toplevel)
    wlr_foreign_toplevel_handle_v1_set_activated(selmon->sel->foreign_toplevel,
                                                 false);
  if (c->foreign_toplevel)
    wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);

  // make sure the window animation is open animation
  c->is_open_animation = true;
  resize(c,c->geom,0);

  printstatus();
}

void // 0.5
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

  if (!c || c->iskilling)
    return;

  if (c->ismaxmizescreen || c->isfullscreen)
    setmaxmizescreen(c, 0);
  else
    setmaxmizescreen(c, 1);
}

void set_minized(Client *c) {
  if (c->isglobal) {
    c->isglobal = 0;
    selmon->sel->tags = selmon->tagset[selmon->seltags];
  }
  c->is_scratchpad_show = 0;
  c->oldtags = c->mon->sel->tags;
  c->tags = 0;
  c->isminied = 1;
  c->is_in_scratchpad = 1;
  c->is_scratchpad_show = 0;
  focusclient(focustop(selmon), 1);
  arrange(c->mon, false);
  wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, false);
  wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel, true);
  wl_list_remove(&c->link);               // 从原来位置移除
  wl_list_insert(clients.prev, &c->link); // 插入尾部
}

void // 0.5
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

  if (!c || c->iskilling)
    return;

  set_minized(c);
}

void // 17
monocle(Monitor *m, unsigned int gappo, unsigned int gappi) {
  Client *c;
  int n = 0;

  wl_list_for_each(c, &clients, link) {
    if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen ||
        c->ismaxmizescreen || c->iskilling || c->animation.tagouting)
      continue;
    resize(c, m->w, 0);
    n++;
  }
  if ((c = focustop(m)))
    wlr_scene_node_raise_to_top(&c->scene->node);
}

void // 0.5
motionabsolute(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits an _absolute_
   * motion event, from 0..1 on each axis. This happens, for example, when
   * wlroots is running under a Wayland window rather than KMS+DRM, and you
   * move the mouse over the window. You could enter the window from any edge,
   * so we have to warp the mouse there. There is also some hardware which
   * emits these events. */
  struct wlr_pointer_motion_absolute_event *event = data;
  double lx, ly, dx, dy;

  if (!event->time_msec) /* this is 0 with virtual pointers */
    wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x, event->y);

  wlr_cursor_absolute_to_layout_coords(cursor, &event->pointer->base, event->x,
                                       event->y, &lx, &ly);
  dx = lx - cursor->x;
  dy = ly - cursor->y;
  motionnotify(event->time_msec, &event->pointer->base, dx, dy, dx, dy);
}

void // fix for 0.5
motionnotify(uint32_t time, struct wlr_input_device *device, double dx,
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
      toplevel_from_wlr_surface(seat->pointer_state.focused_surface, &w, &l) >=
          0) {
    c = w;
    surface = seat->pointer_state.focused_surface;
    sx = cursor->x - (l ? l->geom.x : w->geom.x);
    sy = cursor->y - (l ? l->geom.y : w->geom.y);
  }

  /* time is 0 in internal calls meant to restore pointer focus. */
  if (time) {
    wlr_relative_pointer_manager_v1_send_relative_motion(
        relative_pointer_mgr, seat, (uint64_t)time * 1000, dx, dy, dx_unaccel,
        dy_unaccel);

    wl_list_for_each(constraint, &pointer_constraints->constraints, link)
        cursorconstrain(constraint);

    if (active_constraint && cursor_mode != CurResize &&
        cursor_mode != CurMove) {
      toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
      if (c &&
          active_constraint->surface == seat->pointer_state.focused_surface) {
        sx = cursor->x - c->geom.x - c->bw;
        sy = cursor->y - c->geom.y - c->bw;
        if (wlr_region_confine(&active_constraint->region, sx, sy, sx + dx,
                               sy + dy, &sx_confined, &sy_confined)) {
          dx = sx_confined - sx;
          dy = sy_confined - sy;
        }

        if (active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
          return;
      }
    }

    wlr_cursor_move(cursor, device, dx, dy);
    wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

    /* Update selmon (even while dragging a window) */
    if (sloppyfocus)
      selmon = xytomon(cursor->x, cursor->y);
  }

  /* Update drag icon's position */
  wlr_scene_node_set_position(&drag_icon->node, cursor->x, cursor->y);

  /* If we are currently grabbing the mouse, handle and return */
  if (cursor_mode == CurMove) {
    /* Move the grabbed client to the new position. */
    grabc->oldgeom = (struct wlr_box){.x = cursor->x - grabcx,
                                      .y = cursor->y - grabcy,
                                      .width = grabc->geom.width,
                                      .height = grabc->geom.height};
    resize(grabc, grabc->oldgeom, 1);
    return;
  } else if (cursor_mode == CurResize) {
    grabc->oldgeom = (struct wlr_box){.x = grabc->geom.x,
                                      .y = grabc->geom.y,
                                      .width = cursor->x - grabc->geom.x,
                                      .height = cursor->y - grabc->geom.y};
    resize(grabc, grabc->oldgeom, 1);
    return;
  }

  /* If there's no client surface under the cursor, set the cursor image to a
   * default. This is what makes the cursor image appear when you move it
   * off of a client or over its border. */
  if (!surface && !seat->drag)
    wlr_cursor_set_xcursor(cursor, cursor_mgr, "left_ptr");


  if(c && c->mon && !c->animation.running && (!(c->geom.x + c->geom.width > c->mon->m.x + c->mon->m.width || c->geom.x < c->mon->m.x) || !ISTILED(c))) {
    scroller_focus_lock = 0;
  } 

  should_lock = false;
  if(!scroller_focus_lock || !(c && c->mon && (c->geom.x + c->geom.width > c->mon->m.x + c->mon->m.width || c->geom.x < c->mon->m.x)) ) {
    if(c && c->mon && strcmp(c->mon->pertag->ltidxs[selmon->pertag->curtag]->name,
      "scroller") == 0  && (c->geom.x + c->geom.width > c->mon->m.x + c->mon->m.width || c->geom.x < c->mon->m.x)) {
        should_lock = true;
    } 
    pointerfocus(c, surface, sx, sy, time);

    if(should_lock && c && c->mon && ISTILED(c) && c == c->mon->sel) {
      scroller_focus_lock = 1;
    }
  }
}

void // fix for 0.5 光标相对位置移动事件处理
motionrelative(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits a _relative_
   * pointer motion event (i.e. a delta) */
  struct wlr_pointer_motion_event *event = data;
  /* The cursor doesn't move unless we tell it to. The cursor automatically
   * handles constraining the motion to the output layout, as well as any
   * special configuration applied for the specific input device which
   * generated the event. You can pass NULL for the device if you want to move
   * the cursor around without any input. */

  // //处理一些事件,比如窗口聚焦,图层聚焦通知到客户端
  // motionnotify(event->time_msec);
  // //扩展事件通知,没有这个鼠标移动的时候滑轮将无法使用
  // wlr_relative_pointer_manager_v1_send_relative_motion(
  // 	pointer_manager,
  // 	seat, (uint64_t)(event->time_msec) * 1000,
  // 	event->delta_x, event->delta_y,
  // 	event->unaccel_dx, event->unaccel_dy);
  // //通知光标设备移动
  // wlr_cursor_move(cursor, &event->pointer->base, event->delta_x,
  // event->delta_y);
  motionnotify(event->time_msec, &event->pointer->base, event->delta_x,
               event->delta_y, event->unaccel_dx, event->unaccel_dy);
  // 鼠标左下热区判断是否触发
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
    setfloating(grabc, 1);
  }

  switch (cursor_mode = arg->ui) {
  case CurMove:
    grabcx = cursor->x - grabc->geom.x;
    grabcy = cursor->y - grabc->geom.y;
    wlr_cursor_set_xcursor(cursor, cursor_mgr, "fleur");
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

void // 0.5
outputmgrapply(struct wl_listener *listener, void *data) {
  struct wlr_output_configuration_v1 *config = data;
  outputmgrapplyortest(config, 0);
}

void // 0.5
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
      wlr_output_state_set_custom_mode(&state,
                                       config_head->state.custom_mode.width,
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
      wlr_output_layout_add(output_layout, wlr_output, config_head->state.x,
                            config_head->state.y);

    wlr_output_state_finish(&state);
  }

  if (ok)
    wlr_output_configuration_v1_send_succeeded(config);
  else
    wlr_output_configuration_v1_send_failed(config);
  wlr_output_configuration_v1_destroy(config);

  /* TODO: use a wrapper function? */
  updatemons(NULL, NULL);
}

void
powermgrsetmode(struct wl_listener *listener, void *data)
{
	struct wlr_output_power_v1_set_mode_event *event = data;
	struct wlr_output_state state = {0};

	if (!event->output->data)
		return;

	wlr_output_state_set_enabled(&state, event->mode);
	wlr_output_commit_state(event->output, &state);

	((Monitor *)(event->output->data))->asleep = !event->mode;
}

void // 0.5
outputmgrtest(struct wl_listener *listener, void *data) {
  struct wlr_output_configuration_v1 *config = data;
  outputmgrapplyortest(config, 1);
}

void // 17
pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
             uint32_t time) {
  struct timespec now;
  int internal_call = !time;

  if (surface != seat->pointer_state.focused_surface && sloppyfocus && time &&
      c && !client_is_unmanaged(c))
    focusclient(c, 0);

  /* If surface is NULL, try use the focused client surface to set pointer foucs
   */
  if (time == 0 && !surface && selmon && selmon->sel) {
    surface = client_surface(selmon->sel);
  }

  /* If surface is still NULL, clear pointer focus */
  if (!surface) {
    wlr_seat_pointer_notify_clear_focus(seat);
    return;
  }

  if (internal_call) {
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

void // 0.5
quit(const Arg *arg) {
  wl_display_terminate(dpy);
  free_config();
}

void quitsignal(int signo) { quit(NULL); }

void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int sx, int sy,
                                void *data) {
  wlr_scene_buffer_set_opacity(buffer, *(double *)data);
}


void scene_buffer_apply_size(struct wlr_scene_buffer *buffer, int sx, int sy,
                             void *data) {
  animationScale *scale_data = (animationScale *)data;
  
  if(scale_data->height_scale <= 0 || scale_data->width_scale <= 0) {
    return;
  }

  if(scale_data->height <= 0 || scale_data->width <= 0) { 
    return;
  }

  struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(buffer);

  if(scene_surface == NULL) return;

  struct wlr_surface *surface = scene_surface->surface;

  uint32_t surface_width = surface->current.width;
  uint32_t surface_height = surface->current.height;

  surface_width *= scale_data->width_scale;
  surface_height *= scale_data->height_scale;

  if (wlr_subsurface_try_from_wlr_surface(surface) != NULL && 
      surface_width <= scale_data->m->m.width && 
      surface_height <= scale_data->m->m.height &&
      surface_height > 0 && surface_width > 0) {
    wlr_scene_buffer_set_dest_size(buffer, surface_width, surface_height);
  } else if(scale_data->width >0 && scale_data->height > 0) {
    wlr_scene_buffer_set_dest_size(buffer, scale_data->width,
                                   scale_data->height);
  }
}

void snap_scene_buffer_apply_size(struct wlr_scene_buffer *buffer, int sx,
                                  int sy, void *data) {
  animationScale *scale_data = (animationScale *)data;
  wlr_scene_buffer_set_dest_size(buffer, scale_data->width, scale_data->height);
}

void buffer_set_size(Client *c, animationScale data) {

  if (c->animation.current.width <= c->geom.width &&
      c->animation.current.height <= c->geom.height && !c->need_scale_first_frame) {
    return;
  }

  c->need_scale_first_frame = false;

  if (c->iskilling || c->animation.tagouting ||
      c->animation.tagouted || c->animation.tagining) {
    return;
  }

  if (c == grabc)
    return;

  wlr_scene_node_for_each_buffer(&c->scene_surface->node,
                                 scene_buffer_apply_size, &data);
}

void client_set_opacity(Client *c, double opacity) {
  wlr_scene_node_for_each_buffer(&c->scene_surface->node,
                                 scene_buffer_apply_opacity, &opacity);
}

void client_handle_opacity(Client *c) {
  if (!c || !c->mon || !client_surface(c)->mapped)
    return;

  double opacity = c->isfullscreen || c->ismaxmizescreen ? 1.0
                   : c == selmon->sel                    ? 0.8
                                                         : 0.5;

  wlr_scene_node_for_each_buffer(&c->scene_surface->node,
                                 scene_buffer_apply_opacity, &opacity);
}

void rendermon(struct wl_listener *listener, void *data) {
  Monitor *m = wl_container_of(listener, m, frame);
  Client *c, *tmp;
  struct wlr_output_state pending = {0};

  struct timespec now;
  bool need_more_frames = false;

  // Draw frames for all clients
  wl_list_for_each(c, &clients, link) {
    need_more_frames = client_draw_frame(c) || need_more_frames;
  }

  wl_list_for_each_safe(c, tmp, &fadeout_clients, fadeout_link) {
    need_more_frames = client_draw_fadeout_frame(c) || need_more_frames;
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

void // 0.5
requestdecorationmode(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, set_decoration_mode);
  wlr_xdg_toplevel_decoration_v1_set_mode(
      c->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void // 17
requeststartdrag(struct wl_listener *listener, void *data) {
  struct wlr_seat_request_start_drag_event *event = data;

  if (wlr_seat_validate_pointer_grab_serial(seat, event->origin, event->serial))
    wlr_seat_start_pointer_drag(seat, event->drag, event->serial);
  else
    wlr_data_source_destroy(event->drag->source);
}

void setborder_color(Client *c) {
  unsigned int i;
  if (!c || !c->mon)
    return;
  if (c->isurgent) {
    for (i = 0; i < 4; i++)
      wlr_scene_rect_set_color(c->border[i], urgentcolor);
    return;
  }
  if (c->is_in_scratchpad && c == selmon->sel) {
    for (i = 0; i < 4; i++)
      wlr_scene_rect_set_color(c->border[i], scratchpadcolor);
  } else if (c->isglobal && c == selmon->sel) {
    for (i = 0; i < 4; i++)
      wlr_scene_rect_set_color(c->border[i], globalcolor);
  } else if (c->ismaxmizescreen && c == selmon->sel) {
    for (i = 0; i < 4; i++)
      wlr_scene_rect_set_color(c->border[i], maxmizescreencolor);
  } else if (c == selmon->sel) {
    for (i = 0; i < 4; i++)
      wlr_scene_rect_set_color(c->border[i], focuscolor);
  } else {
    for (i = 0; i < 4; i++)
      wlr_scene_rect_set_color(c->border[i], bordercolor);
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

int is_special_animaiton_rule(Client *c) {
  int visible_client_number = 0;
  Client *count_c;
  wl_list_for_each(count_c, &clients, link) {
    if (count_c && VISIBLEON(count_c, selmon) && !count_c->isminied &&
        !count_c->iskilling && !count_c->isfloating) {
      visible_client_number++;
    }
  }

  if (strcmp(selmon->pertag->ltidxs[selmon->pertag->curtag]->name,
             "scroller") == 0 &&
      !c->isfloating) {
    return DOWN;
  } else if (visible_client_number < 2 && !c->isfloating) {
    return DOWN;
  } else if (visible_client_number == 2 && !c->isfloating && !new_is_master) {
    return RIGHT;
  } else if (!c->isfloating && new_is_master) {
    return LEFT;
  } else {
    return UNDIR;
  }
}

void set_open_animaiton(Client *c, struct wlr_box geo) {
  int slide_direction;
  int horizontal, horizontal_value;
  int vertical, vertical_value;
  int special_direction;
  int center_x, center_y;
  if ((!c->animation_type_open && strcmp(animation_type_open, "zoom") == 0) ||
      (c->animation_type_open && strcmp(c->animation_type_open, "zoom") == 0)) {
    c->animainit_geom.width = c->geom.width * zoom_initial_ratio;
    c->animainit_geom.height = c->geom.height * zoom_initial_ratio;
    c->animainit_geom.x = c->geom.x + (c->geom.width - c->animainit_geom.width) / 2;
    c->animainit_geom.y = c->geom.y + (c->geom.height - c->animainit_geom.height) / 2;
    return;
  } else {
    special_direction = is_special_animaiton_rule(c);
    center_x = c->geom.x + c->geom.width / 2;
    center_y = c->geom.y + c->geom.height / 2;
    if (special_direction == UNDIR) {
      horizontal =
          c->mon->w.x + c->mon->w.width - center_x < center_x - c->mon->w.x
              ? RIGHT
              : LEFT;
      horizontal_value = horizontal == LEFT
                             ? center_x - c->mon->w.x
                             : c->mon->w.x + c->mon->w.width - center_x;
      vertical =
          c->mon->w.y + c->mon->w.height - center_y < center_y - c->mon->w.y
              ? DOWN
              : UP;
      vertical_value = vertical == UP
                           ? center_y - c->mon->w.y
                           : c->mon->w.y + c->mon->w.height - center_y;
      slide_direction =
          horizontal_value < vertical_value ? horizontal : vertical;
    } else {
      slide_direction = special_direction;
    }
    c->animainit_geom.width = c->geom.width;
    c->animainit_geom.height = c->geom.height;
    switch (slide_direction) {
    case UP:
      c->animainit_geom.x = c->geom.x;
      c->animainit_geom.y = c->mon->m.y - c->geom.height;
      break;
    case DOWN:
      c->animainit_geom.x = c->geom.x;
      c->animainit_geom.y =
          c->geom.y + c->mon->m.height - (c->geom.y - c->mon->m.y);
      break;
    case LEFT:
      c->animainit_geom.x = c->mon->m.x - c->geom.width;
      c->animainit_geom.y = c->geom.y;
      break;
    case RIGHT:
      c->animainit_geom.x =
          c->geom.x + c->mon->m.width - (c->geom.x - c->mon->m.x);
      c->animainit_geom.y = c->geom.y;
      break;
    default:
      c->animainit_geom.x = c->geom.x;
      c->animainit_geom.y = 0 - c->geom.height;
    }
  }
}

void resize(Client *c, struct wlr_box geo, int interact) {

  // 动画设置的起始函数，这里用来计算一些动画的起始值
  // 动画起始位置大小是由于c->animainit_geom确定的

  if (!c || !c->mon || !client_surface(c)->mapped)
    return;

  struct wlr_box *bbox;
  struct wlr_box clip;

  if (!c->mon)
    return;

  // wl_event_source_timer_update(c->timer_tick, 10);
  c->need_output_flush = true;

  // oldgeom = c->geom;
  bbox = interact ? &sgeom : &c->mon->w;

  if (strcmp(c->mon->pertag->ltidxs[c->mon->pertag->curtag]->name,
             "scroller") == 0) {
    c->geom = geo;
  } else { // 这里会限制不允许窗口划出屏幕
    client_set_bounds(
        c, geo.width,
        geo.height); // 去掉这个推荐的窗口大小,因为有时推荐的窗口特别大导致平铺异常
    c->geom = geo;
    applybounds(
        c, bbox); // 去掉这个推荐的窗口大小,因为有时推荐的窗口特别大导致平铺异常
  }

  if(c->geom.width <0 || c->geom.height <0) {
    return;
  }

  if (!c->is_open_animation) {
    c->animation.begin_fade_in = false;
    client_set_opacity(c, 1);
  }

  if(c->animation.action == OPEN && wlr_box_equal(&c->geom, &c->current)) {
    c->animation.action = c->animation.action;
  } else if (c->animation.tagouting) {
    c->animation.duration = animation_duration_tag;
    c->animation.action = TAG;
  } else if (c->animation.tagining) {
    c->animation.duration = animation_duration_tag;
    c->animation.action = TAG;
  } else if (c->is_open_animation) {
    c->animation.duration = animation_duration_open;
    c->animation.action = OPEN;
  } else {
    c->animation.duration = animation_duration_move;
    c->animation.action = MOVE;
  }

  // 动画起始位置大小设置
  if (c->animation.tagouting) {
    c->animainit_geom = c->animation.current;
  } else if (c->animation.tagining) {
    c->animainit_geom.height = c->animation.current.height;
    c->animainit_geom.width = c->animation.current.width;
    c->animainit_geom.y = c->animation.current.y;
  } else if (c->is_open_animation) {
    set_open_animaiton(c, c->geom);
  } else {
    c->animainit_geom = c->animation.current;
  }

  if (c->isnoborder || c->iskilling) {
    c->bw = 0;
  }

  // c->geom 是真实的窗口大小和位置，跟过度的动画无关，用于计算布局
  c->configure_serial =
      client_set_size(c, c->geom.width - 2 * c->bw, c->geom.height - 2 * c->bw);

  if (c == grabc) {
    c->animation.running = false;
    c->need_output_flush = false;
    c->animainit_geom = c->current = c->pending = c->animation.current =
        c->geom;
    wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
    apply_border(c, c->geom, 0, 0);
    client_get_clip(c, &clip);
    wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip);
    return;
  }

  // 如果不是工作区切换时划出去的窗口，就让动画的结束位置，就是上面的真实位置和大小
  // c->pending 决定动画的终点，一般在其他调用resize的函数的附近设置了
  if (!c->animation.tagouting && !c->iskilling) {
    c->pending = c->geom;
  }

  if (c->swallowedby && c->animation.action == OPEN) {
    c->animainit_geom = c->swallowedby->animation.current;
  } 

  if (c->swallowing) {
    c->animainit_geom = c->geom;
  }

  if (c->animation_type_open && strcmp(c->animation_type_open,"none") == 0 && c->animation.action == OPEN) {
    c->animainit_geom = c->geom;
  }

  // 开始应用动画设置
  client_set_pending_state(c);

  setborder_color(c);
}

void // 17
run(char *startup_cmd) {
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
  autostartexec();
  if (startup_cmd) {
    int piperw[2];
    if (pipe(piperw) < 0)
      die("startup: pipe:");
    if ((child_pid = fork()) < 0)
      die("startup: fork:");
    if (child_pid == 0) {
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
   * event, which will result in the client requesting set the cursor surface */
  if (cursor_mode != CurNormal && cursor_mode != CurPressed)
    return;
  /* This can be sent by any client, so we check to make sure this one is
   * actually has pointer focus first. If so, we can tell the cursor to
   * use the provided surface as the cursor image. It will set the
   * hardware cursor on the output that it's currently on and continue to
   * do so as the cursor moves between outputs. */
  if (event->seat_client == seat->pointer_state.focused_client) {
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

  if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
    return;

  wlr_scene_node_reparent(&c->scene->node,
                          layers[c->isfloating ? LyrFloat : LyrTile]);

  target_box = c->geom;

  if (floating == 1 && c != grabc) {
    if (c->need_float_size_reduce && !c->swallowing && !c->is_open_animation) {
      target_box.height = target_box.height * 0.8;
      target_box.width = target_box.width * 0.8;
    }
    // 重新计算居中的坐标
    target_box = setclient_coordinate_center(target_box);
    backup_box = c->geom;
    hit = applyrulesgeom(c);
    target_box = hit == 1 ? c->geom : target_box;
    c->geom = backup_box;
    if (c->oldgeom.width > 0 && c->oldgeom.height > 0) {
      resize(c, c->oldgeom, 0);
    } else {
      resize(c, target_box, 0);
    }
    c->need_float_size_reduce = 0;
  } else if(c->isfloating && c == grabc) {
    c->need_float_size_reduce = 0;
  } else {
    c->need_float_size_reduce = 1;
    c->is_scratchpad_show = 0;
    c->is_in_scratchpad = 0;
    // 让当前tag中的全屏窗口退出全屏参与平铺
    wl_list_for_each(fc, &clients,
                     link) if (fc && fc != c && c->tags & fc->tags && ISFULLSCREEN(fc)) {
      clear_fullscreen_flag(fc);
    }
  }

  arrange(c->mon, false);
  printstatus();
}

void setmaxmizescreen(Client *c, int maxmizescreen) {
  struct wlr_box maxmizescreen_box;
  if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
    return;

  c->ismaxmizescreen = maxmizescreen;

  // c->bw = fullscreen ? 0 : borderpx;
  // client_set_fullscreen(c, maxmizescreen);
  wlr_scene_node_reparent(&c->scene->node, layers[maxmizescreen   ? LyrTile
                                                  : c->isfloating ? LyrFloat
                                                                  : LyrTile]);
  if (maxmizescreen) {
    if (c->isfloating)
      c->oldgeom = c->geom;
    if (selmon->isoverview) {
      Arg arg = {0};
      toggleoverview(&arg);
    }

    c->prev = c->geom;
    maxmizescreen_box.x = c->mon->w.x + gappov;
    maxmizescreen_box.y = c->mon->w.y + gappoh;
    maxmizescreen_box.width = c->mon->w.width - 2 * gappov;
    maxmizescreen_box.height = c->mon->w.height - 2 * gappov;
    wlr_scene_node_raise_to_top(&c->scene->node); // 将视图提升到顶层
    resize(c, maxmizescreen_box, 0);
    c->ismaxmizescreen = 1;
    // c->isfloating = 0;
  } else {
    /* restore previous size instead of arrange for floating windows since
     * client positions are set by the user and cannot be recalculated */
    // resize(c, c->prev, 0);
    c->bw = borderpx;
    c->ismaxmizescreen = 0;
    c->isfullscreen = 0;
    client_set_fullscreen(c, false);
    if (c->isfloating)
      setfloating(c, 1);
    arrange(c->mon, false);
  }
}

void setfullscreen(Client *c, int fullscreen) // 用自定义全屏代理自带全屏
{
  c->isfullscreen = fullscreen;

  if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
    return;

  client_set_fullscreen(c, fullscreen);
  wlr_scene_node_reparent(&c->scene->node, layers[fullscreen      ? LyrTile
                                                  : c->isfloating ? LyrFloat
                                                                  : LyrTile]);

  if (fullscreen) {
    if (c->isfloating)
      c->oldgeom = c->geom;
    if (selmon->isoverview) {
      Arg arg = {0};
      toggleoverview(&arg);
    }

    c->bw = 0;
    wlr_scene_node_raise_to_top(&c->scene->node); // 将视图提升到顶层
    resize(c, c->mon->m, 0);
    c->isfullscreen = 1;
    // c->isfloating = 0;
  } else {
    /* restore previous size instead of arrange for floating windows since
     * client positions are set by the user and cannot be recalculated */
    // resize(c, c->prev, 0);
    c->bw = borderpx;
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

void switch_layout(const Arg *arg) {

  int jk, ji;
  char *target_layout_name = NULL;
  size_t len;

  if (config.circle_layout_count != 0) {
    for (jk = 0; jk < config.circle_layout_count; jk++) {

      len = MAX(strlen(config.circle_layout[jk]),
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

void
setmon(Client *c, Monitor *m, uint32_t newtags,bool focus)
{
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;
	c->mon = m;
	c->prev = c->geom;

	/* Scene graph sends surface leave/enter events on move and resize */
	if (oldmon)
		arrange(oldmon,false);
	if (m) {
		/* Make sure window actually overlaps with the monitor */
		resize(c, c->geom, 0);
		c->tags = newtags ? newtags : m->tagset[m->seltags]; /* assign tags of target monitor */
		setfullscreen(c, c->isfullscreen); /* This will call arrange(c->mon) */
		setfloating(c, c->isfloating);
	}
  if(focus)
	  focusclient(focustop(selmon), 1);
}

void // 17
setpsel(struct wl_listener *listener, void *data) {
  /* This event is raised by the seat when a client wants to set the selection,
   * usually when the user copies something. wlroots allows compositors to
   * ignore such requests if they so choose, but in dwl we always honor
   */
  struct wlr_seat_request_set_primary_selection_event *event = data;
  wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

void // 17
setsel(struct wl_listener *listener, void *data) {
  /* This event is raised by the seat when a client wants to set the selection,
   * usually when the user copies something. wlroots allows compositors to
   * ignore such requests if they so choose, but in dwl we always honor
   */
  struct wlr_seat_request_set_selection_event *event = data;
  wlr_seat_set_selection(seat, event->source, event->serial);
}

// 获取tags中最坐标的tag的tagmask
unsigned int get_tags_first_tag(unsigned int source_tags) {
  unsigned int i, target, tag;
  tag = 0;
  for (i = 0; !(tag & 1); i++) {
    tag = source_tags >> i;
  }
  target = 1 << (i - 1);
  return target;
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

void handle_foreign_activate_request(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, foreign_activate_request);
  unsigned int target;

  if (c && c->swallowing)
    return;

  if (c && !c->isminied && c == selmon->sel) {
    set_minized(c);
    return;
  }

  if (c->isminied) {
    c->is_in_scratchpad = 0;
    c->is_scratchpad_show = 0;
    setborder_color(c);
    show_hide_client(c);
    return;
  }

  target = get_tags_first_tag(c->tags);
  view(&(Arg){.ui = target}, true);
  focusclient(c, 1);
  wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);
}

void handle_foreign_fullscreen_request(struct wl_listener *listener,
                                       void *data) {
  return;
}

void handle_foreign_close_request(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, foreign_close_request);

  if (c && c->swallowing)
    return;

  if (c) {
    pending_kill_client(c);
  }
}

void handle_foreign_destroy(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, foreign_destroy);
  if (c) {
    wl_list_remove(&c->foreign_activate_request.link);
    wl_list_remove(&c->foreign_fullscreen_request.link);
    wl_list_remove(&c->foreign_close_request.link);
    wl_list_remove(&c->foreign_destroy.link);
  }
}

void signalhandler(int signalnumber) {
  void *array[64];
  size_t size;
  char **strings;
  size_t i;
  char filename[1024];

  // 获取 MAOMAOCONFIG 环境变量
  const char *maomaoconfig = getenv("MAOMAOCONFIG");

  // 如果 MAOMAOCONFIG 环境变量不存在或为空，则使用 HOME 环境变量
  if (!maomaoconfig || maomaoconfig[0] == '\0') {
    // 获取当前用户家目录
    const char *homedir = getenv("HOME");
    if (!homedir) {
      // 如果获取失败，则无法继续
      return;
    }
    // 构建日志文件路径
    snprintf(filename, sizeof(filename), "%s/.config/maomao/crash.log",
             homedir);
  } else {
    // 使用 MAOMAOCONFIG 环境变量作为配置文件夹路径
    snprintf(filename, sizeof(filename), "%s/crash.log", maomaoconfig);
  }

  // 打开日志文件
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    // 如果无法打开日志文件，则不处理
    return;
  }

  // 获取堆栈跟踪
  size = backtrace(array, 64);
  strings = backtrace_symbols(array, size);

  // 写入错误信息和堆栈跟踪到文件
  fprintf(fp, "Received signal %d:\n", signalnumber);
  for (i = 0; i < size; ++i) {
    fprintf(fp, "%zu %s\n", i, strings[i]);
  }

  // 关闭文件
  fclose(fp);

  // 释放分配的内存
  free(strings);

  // 不调用 exit 以允许生成核心转储文件
}

// int timer_tick_action(void *data) {
//   Client *c = (Client *)data;

//   if (c->animation.running) {
//     wlr_output_schedule_frame(c->mon->wlr_output);
//     wl_event_source_timer_update(c->timer_tick, 10);
//   } else {
//     wl_event_source_timer_update(c->timer_tick, 0);
//   }

//   return 0;
// }

void setup(void) {

  signal(SIGSEGV, signalhandler);

  parse_config();

  init_baked_points();

  int i, sig[] = {SIGCHLD, SIGINT, SIGTERM, SIGPIPE};
  struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handlesig};
  sigemptyset(&sa.sa_mask);

  for (i = 0; i < LENGTH(sig); i++)
    sigaction(sig[i], &sa, NULL);

  wlr_log_init(log_level, NULL);

  /* The Wayland display is managed by libwayland. It handles accepting
   * clients from the Unix socket, manging Wayland globals, and so on. */
  dpy = wl_display_create();
  pointer_manager = wlr_relative_pointer_manager_v1_create(dpy);
  /* The backend is a wlroots feature which abstracts the underlying input and
   * output hardware. The autocreate option will choose the most suitable
   * backend based on the current environment, such as opening an X11 window
   * if an X11 server is running. The NULL argument here optionally allows you
   * to pass in a custom renderer if wlr_renderer doesn't meet your needs. The
   * backend uses the renderer, for example, to fall back to software cursors
   * if the backend does not support hardware cursors (some older GPUs
   * don't). */
  if (!(backend = wlr_backend_autocreate(dpy, &session)))
    die("couldn't create backend");

  /* Initialize the scene graph used to lay out windows */
  scene = wlr_scene_create();
  for (i = 0; i < NUM_LAYERS; i++)
    layers[i] = wlr_scene_tree_create(&scene->tree);
  drag_icon = wlr_scene_tree_create(&scene->tree);
  wlr_scene_node_place_below(&drag_icon->node, &layers[LyrBlock]->node);

  /* Create a renderer with the default implementation */
  if (!(drw = wlr_renderer_autocreate(backend)))
    die("couldn't create renderer");

  /* Create shm, drm and linux_dmabuf interfaces by ourselves.
   * The simplest way is call:
   *      wlr_renderer_init_wl_display(drw);
   * but we need to create manually the linux_dmabuf interface to integrate it
   * with wlr_scene. */
  wlr_renderer_init_wl_shm(drw, dpy);

  if (wlr_renderer_get_dmabuf_texture_formats(drw)) {
    wlr_drm_create(dpy, drw);
    wlr_scene_set_linux_dmabuf_v1(
        scene, wlr_linux_dmabuf_v1_create_with_renderer(dpy, 4, drw));
  }

  /* Create a default allocator */
  if (!(alloc = wlr_allocator_autocreate(backend, drw)))
    die("couldn't create allocator");

  /* This creates some hands-off wlroots interfaces. The compositor is
   * necessary for clients to allocate surfaces and the data device manager
   * handles the clipboard. Each of these wlroots interfaces has room for you
   * to dig your fingers in and play with their behavior if you want. Note that
   * the clients cannot set the selection directly without compositor approval,
   * see the setsel() function. */
  compositor = wlr_compositor_create(dpy, 6, drw);
  wlr_export_dmabuf_manager_v1_create(dpy);
  wlr_screencopy_manager_v1_create(dpy);
  wlr_data_control_manager_v1_create(dpy);
  wlr_data_device_manager_create(dpy);
  wlr_primary_selection_v1_device_manager_create(dpy);
  wlr_viewporter_create(dpy);
  wlr_single_pixel_buffer_manager_v1_create(dpy);
  wlr_fractional_scale_manager_v1_create(dpy, 1);
  wlr_subcompositor_create(dpy);

  /* Initializes the interface used to implement urgency hints */
  activation = wlr_xdg_activation_v1_create(dpy);
  LISTEN_STATIC(&activation->events.request_activate, urgent);

  gamma_control_mgr = wlr_gamma_control_manager_v1_create(dpy);
  LISTEN_STATIC(&gamma_control_mgr->events.set_gamma, setgamma);

	power_mgr = wlr_output_power_manager_v1_create(dpy);
	LISTEN_STATIC(&power_mgr->events.set_mode, powermgrsetmode);

  /* Creates an output layout, which a wlroots utility for working with an
   * arrangement of screens in a physical layout. */
  output_layout = wlr_output_layout_create();
  LISTEN_STATIC(&output_layout->events.change, updatemons);
  wlr_xdg_output_manager_v1_create(dpy, output_layout);

  /* Configure a listener to be notified when new outputs are available on the
   * backend. */
  wl_list_init(&mons);
  LISTEN_STATIC(&backend->events.new_output, createmon);

  /* Set up our client lists and the xdg-shell. The xdg-shell is a
   * Wayland protocol which is used for application windows. For more
   * detail on shells, refer to the article:
   *
   * https://drewdevault.com/2018/07/29/Wayland-shells.html
   */
  wl_list_init(&clients);
  wl_list_init(&fstack);
  wl_list_init(&fadeout_clients);

  idle_notifier = wlr_idle_notifier_v1_create(dpy);

  idle_inhibit_mgr = wlr_idle_inhibit_v1_create(dpy);
  LISTEN_STATIC(&idle_inhibit_mgr->events.new_inhibitor, createidleinhibitor);

  layer_shell = wlr_layer_shell_v1_create(dpy, 3);
  LISTEN_STATIC(&layer_shell->events.new_surface, createlayersurface);

  xdg_shell = wlr_xdg_shell_create(dpy, 6);
  LISTEN_STATIC(&xdg_shell->events.new_surface, createnotify);

  session_lock_mgr = wlr_session_lock_manager_v1_create(dpy);
  wl_signal_add(&session_lock_mgr->events.new_lock, &lock_listener);
  LISTEN_STATIC(&session_lock_mgr->events.destroy, destroysessionmgr);
  locked_bg = wlr_scene_rect_create(layers[LyrBlock], sgeom.width, sgeom.height,
                                    (float[4]){0.1, 0.1, 0.1, 1.0});
  wlr_scene_node_set_enabled(&locked_bg->node, 0);

  /* Use decoration protocols to negotiate server-side decorations */
  wlr_server_decoration_manager_set_default_mode(
      wlr_server_decoration_manager_create(dpy),
      WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
  xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(dpy);
  LISTEN_STATIC(&xdg_decoration_mgr->events.new_toplevel_decoration,
                createdecoration);
  pointer_constraints = wlr_pointer_constraints_v1_create(dpy);
  LISTEN_STATIC(&pointer_constraints->events.new_constraint,
                createpointerconstraint);

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
  cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
  setenv("XCURSOR_SIZE", "24", 1);

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
  LISTEN_STATIC(&cursor->events.motion, motionrelative);
  LISTEN_STATIC(&cursor->events.motion_absolute, motionabsolute);
  LISTEN_STATIC(&cursor->events.button, buttonpress);
  LISTEN_STATIC(&cursor->events.axis, axisnotify);
  LISTEN_STATIC(&cursor->events.frame, cursorframe);

  // 这两句代码会造成obs窗口里的鼠标光标消失,不知道注释有什么影响
  cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(dpy, 1);
  LISTEN_STATIC(&cursor_shape_mgr->events.request_set_shape, setcursorshape);

  /*
   * Configures a seat, which is a single "seat" at which a user sits and
   * operates the computer. This conceptually includes up to one keyboard,
   * pointer, touch, and drawing tablet device. We also rig up a listener to
   * let us know when new input devices are available on the backend.
   */
  wl_list_init(&keyboards);
  LISTEN_STATIC(&backend->events.new_input, inputdevice);
  virtual_keyboard_mgr = wlr_virtual_keyboard_manager_v1_create(dpy);
  LISTEN_STATIC(&virtual_keyboard_mgr->events.new_virtual_keyboard,
                virtualkeyboard);
  virtual_pointer_mgr = wlr_virtual_pointer_manager_v1_create(dpy);
  LISTEN_STATIC(&virtual_pointer_mgr->events.new_virtual_pointer,
                virtualpointer);

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
  LISTEN_STATIC(&seat->events.request_set_cursor, setcursor);
  LISTEN_STATIC(&seat->events.request_set_selection, setsel);
  LISTEN_STATIC(&seat->events.request_set_primary_selection, setpsel);
  LISTEN_STATIC(&seat->events.request_start_drag, requeststartdrag);
  LISTEN_STATIC(&seat->events.start_drag, startdrag);

  output_mgr = wlr_output_manager_v1_create(dpy);
  LISTEN_STATIC(&output_mgr->events.apply, outputmgrapply);
  LISTEN_STATIC(&output_mgr->events.test, outputmgrtest);

  wlr_scene_set_presentation(scene, wlr_presentation_create(dpy, backend));
#ifdef IM
  /* create text_input-, and input_method-protocol relevant globals */
  input_method_manager = wlr_input_method_manager_v2_create(dpy);
  text_input_manager = wlr_text_input_manager_v3_create(dpy);

  input_relay = calloc(1, sizeof(*input_relay));
  dwl_input_method_relay_init(input_relay);
#endif
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
  xwayland = wlr_xwayland_create(dpy, compositor, 1);
  if (xwayland) {
    LISTEN_STATIC(&xwayland->events.ready, xwaylandready);
    LISTEN_STATIC(&xwayland->events.new_surface, createnotifyx11);

    setenv("DISPLAY", xwayland->display_name, 1);
  } else {
    fprintf(stderr,
            "failed to setup XWayland X server, continuing without it\n");
  }
#endif
}

void sigchld(int unused) {
  siginfo_t in;
  /* We should be able to remove this function in favor of a simple
   *     struct sigaction sa = {.sa_handler = SIG_IGN};
   *     sigaction(SIGCHLD, &sa, NULL);
   * but the Xwayland implementation in wlroots currently prevents us from
   * setting our own disposition for SIGCHLD.
   */
  /* WNOWAIT leaves the child in a waitable state, in case this is the
   * XWayland process
   */
  while (!waitid(P_ALL, 0, &in, WEXITED | WNOHANG | WNOWAIT) && in.si_pid
#ifdef XWAYLAND
         && (!xwayland || in.si_pid != xwayland->server->pid)
#endif
  ) {
    pid_t *p, *lim;
    waitpid(in.si_pid, NULL, 0);
    if (in.si_pid == child_pid)
      child_pid = -1;
    if (!(p = autostart_pids))
      continue;
    lim = &p[autostart_len];

    for (; p < lim; p++) {
      if (*p == in.si_pid) {
        *p = -1;
        break;
      }
    }
  }
}

void spawn(const Arg *arg) {
  if (fork() == 0) {
    dup2(STDERR_FILENO, STDOUT_FILENO);
    setsid();

    // 将 arg->v 拆分为字符串数组
    char *argv[64]; // 假设最多有 64 个参数
    int argc = 0;
    char *token = strtok((char *)arg->v, " ");
    while (token != NULL && argc < 63) {
      // 扩展 ~ 为家目录路径
      wordexp_t p;
      if (wordexp(token, &p, 0) == 0) {
        argv[argc++] = p.we_wordv[0];
      } else {
        argv[argc++] = token; // 如果扩展失败，使用原始 token
      }
      token = strtok(NULL, " ");
    }
    argv[argc] = NULL; // execvp 需要以 NULL 结尾的数组

    // 执行命令
    execvp(argv[0], argv);
    die("dwl: execvp %s failed:", argv[0]);
  }
}

void spawn_on_empty(const Arg *arg) {
  bool is_empty = true;
  Client *c;
  
  wl_list_for_each(c, &clients, link) {
    if (arg->ui & c->tags) {
      is_empty = false;
      break;
    }
  }
  if(!is_empty) {
    view(arg,true);
    return;
  } else {
    view(arg,true);
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

void tagmon(const Arg *arg) {
  Client *c = focustop(selmon);
  Monitor *m;
  if (c) {
    if(c == selmon->sel) {
      selmon->sel = NULL;
    }
    m = dirtomon(arg->i);
    setmon(c, m, 0,true);
    reset_foreign_tolevel(c);
    // 重新计算居中的坐标
    if (c->isfloating) {
      c->geom.width = (int)(c->geom.width * c->mon->w.width / selmon->w.width);
      c->geom.height =
        (int)(c->geom.height * c->mon->w.height / selmon->w.height);
      selmon = c->mon;
      c->geom = setclient_coordinate_center(c->geom);
      focusclient(c, 1);
      resize(c, c->geom, 1);
    } else {
      selmon = c->mon;
      focusclient(c, 1);
      arrange(selmon, false);
    }
    warp_cursor_to_selmon(c->mon);
  }
}


void overview(Monitor *m, unsigned int gappo, unsigned int gappi) {
  grid(m, overviewgappo, overviewgappi);
}

void fibonacci(Monitor *mon, int s) {
  unsigned int i = 0, n = 0, nx, ny, nw, nh;
  Client *c;

  wl_list_for_each(c, &clients, link) if (VISIBLEON(c, mon) && !c->isfloating &&
                                          !c->iskilling && !c->isfullscreen &&
                                          !c->ismaxmizescreen &&
                                          !c->animation.tagouting) n++;
  if (n == 0)
    return;

  nx = mon->w.x + gappoh;
  ny = mon->w.y + gappov;
  nw = mon->w.width - gappoh;
  nh = mon->w.height - gappov;

  wl_list_for_each(c, &clients, link) if (VISIBLEON(c, mon) && !c->isfloating &&
                                          !c->iskilling && !c->isfullscreen &&
                                          !c->ismaxmizescreen &&
                                          !c->animation.tagouting) {
    if ((i % 2 && nh / 2 > 2 * c->bw) || (!(i % 2) && nw / 2 > 2 * c->bw)) {
      if (i < n - 1) {
        if (i % 2) {
          if (i == 1)
            nh = nh * c->mon->pertag->smfacts[selmon->pertag->curtag];
          else
            nh /= 2;
        } else
          nw /= 2;
        if ((i % 4) == 2 && !s)
          nx += nw;
        else if ((i % 4) == 3 && !s)
          ny += nh;
      }

      if ((i % 4) == 0) {
        if (s)
          ny += nh;
        else
          ny -= nh;
      } else if ((i % 4) == 1)
        nx += nw;
      else if ((i % 4) == 2)
        ny += nh;
      else if ((i % 4) == 3) {
        if (s)
          nx += nw;
        else
          nx -= nw;
      }

      if (i == 0) {
        if (n != 1)
          nw = (mon->w.width - gappoh) *
               mon->pertag->mfacts[mon->pertag->curtag];
        ny = mon->w.y + gappov;
      } else if (i == 1) {
        nw = mon->w.width - gappoh - nw;
      } else if (i == 2)
        nh = mon->w.height - gappov - nh;
      i++;
    }

    resize(c,
           (struct wlr_box){
               .x = nx, .y = ny, .width = nw - gappih, .height = nh - gappiv},
           0);
  }
}

void dwindle(Monitor *mon, unsigned int gappo, unsigned int gappi) {
  fibonacci(mon, 1);
}

void spiral(Monitor *mon, unsigned int gappo, unsigned int gappi) {
  fibonacci(mon, 0);
}

// 网格布局窗口大小和位置计算
void grid(Monitor *m, unsigned int gappo, unsigned int gappi) {
  unsigned int i, n;
  unsigned int cx, cy, cw, ch;
  unsigned int dx;
  unsigned int cols, rows, overcols;
  Client *c;
  n = 0;

  // 第一次遍历，计算 n 的值
  wl_list_for_each(c, &clients, link) {
    if (VISIBLEON(c, c->mon) && !c->iskilling && !c->animation.tagouting &&
        c->mon == selmon) {
      n++;
    }
  }

  if (n == 0) {
    return; // 没有需要处理的客户端，直接返回
  }

  if (n == 1) {
    wl_list_for_each(c, &clients, link) {
      if (VISIBLEON(c, c->mon) && !c->iskilling && !c->animation.tagouting &&
          c->mon == selmon) {
        cw = (m->w.width - 2 * gappo) * 0.7;
        ch = (m->w.height - 2 * gappo) * 0.8;
        c->geom.x = m->w.x + (m->w.width - cw) / 2;
        c->geom.y = m->w.y + (m->w.height - ch) / 2;
        c->geom.width = cw - 2 * c->bw;
        c->geom.height = ch - 2 * c->bw;
        resize(c, c->geom, 0);
        return;
      }
    }
  }

  if (n == 2) {
    cw = (m->w.width - 2 * gappo - gappi) / 2;
    ch = (m->w.height - 2 * gappo) * 0.65;
    i = 0;
    wl_list_for_each(c, &clients, link) {
      if (VISIBLEON(c, c->mon) && !c->iskilling && !c->animation.tagouting &&
          c->mon == selmon) {
        if (i == 0) {
          c->geom.x = m->w.x + gappo;
          c->geom.y = m->w.y + (m->w.height - ch) / 2 + gappo;
          c->geom.width = cw - 2 * c->bw;
          c->geom.height = ch - 2 * c->bw;
          resize(c, c->geom, 0);
        } else if (i == 1) {
          c->geom.x = m->w.x + cw + gappo + gappi;
          c->geom.y = m->w.y + (m->w.height - ch) / 2 + gappo;
          c->geom.width = cw - 2 * c->bw;
          c->geom.height = ch - 2 * c->bw;
          resize(c, c->geom, 0);
        }
        i++;
      }
    }
    return;
  }

  // 计算列数和行数
  for (cols = 0; cols <= n / 2; cols++) {
    if (cols * cols >= n) {
      break;
    }
  }
  rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;

  // 计算每个客户端的高度和宽度
  ch = (m->w.height - 2 * gappo - (rows - 1) * gappi) / rows;
  cw = (m->w.width - 2 * gappo - (cols - 1) * gappi) / cols;

  // 处理多余的列
  overcols = n % cols;
  if (overcols) {
    dx = (m->w.width - overcols * cw - (overcols - 1) * gappi) / 2 - gappo;
  }

  // 调整每个客户端的位置和大小
  i = 0;
  wl_list_for_each(c, &clients, link) {
    if (VISIBLEON(c, c->mon) && !c->iskilling && !c->animation.tagouting &&
        c->mon == selmon) {
      cx = m->w.x + (i % cols) * (cw + gappi);
      cy = m->w.y + (i / cols) * (ch + gappi);
      if (overcols && i >= n - overcols) {
        cx += dx;
      }
      c->geom.x = cx + gappo;
      c->geom.y = cy + gappo;
      c->geom.width = cw - 2 * c->bw;
      c->geom.height = ch - 2 * c->bw;
      resize(c, c->geom, 0);
      i++;
    }
  }
}

// 滚动布局
void scroller(Monitor *m, unsigned int gappo, unsigned int gappi) {
  unsigned int i, n;

  Client *c, *root_client = NULL;
  Client **tempClients = NULL; // 初始化为 NULL
  n = 0;
  struct wlr_box target_geom;
  int focus_client_index = 0;
  bool need_scroller = false;

  unsigned int max_client_width = m->w.width - 2 * scroller_structs - gappih;

  // 第一次遍历，计算 n 的值
  wl_list_for_each(c, &clients, link) {
    if (VISIBLEON(c, c->mon) && !c->isfloating && !c->isfullscreen &&
        !c->ismaxmizescreen && !c->iskilling && !c->animation.tagouting &&
        c->mon == m) {
      n++;
    }
  }

  if (n == 0) {
    return; // 没有需要处理的客户端，直接返回
  }

  // 动态分配内存
  tempClients = malloc(n * sizeof(Client *));
  if (!tempClients) {
    // 处理内存分配失败的情况
    return;
  }

  // 第二次遍历，填充 tempClients
  n = 0;
  wl_list_for_each(c, &clients, link) {
    if (VISIBLEON(c, c->mon) && !c->isfloating && !c->isfullscreen &&
        !c->ismaxmizescreen && !c->iskilling && !c->animation.tagouting &&
        c->mon == m) {
      tempClients[n] = c;
      n++;
    }
  }

  if (n == 1) {
    c = tempClients[0];
    target_geom.height = m->w.height - 2 * gappov;
    target_geom.width = max_client_width * c->scroller_proportion;
    target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
    target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
    resize(c, target_geom, 0);
    free(tempClients); // 释放内存
    return;
  }

  if (m->sel && !m->sel->isfloating && !m->sel->ismaxmizescreen &&
      !m->sel->isfullscreen) {
    root_client = m->sel;
  } else if (m->prevsel && !m->prevsel->isfloating &&
             !m->prevsel->ismaxmizescreen && !m->prevsel->isfullscreen) {
    root_client = m->prevsel;
  } else {
    wl_list_for_each(c, &clients, link) {
      if (c->iskilling)
        continue;
      if (c->tags == m->tagset[m->seltags] && !c->isfloating) {
        root_client = c;
      }
    }
  }

  for (i = 0; i < n; i++) {
    c = tempClients[i];
    if (root_client == c) {
      if (c->geom.x >= m->w.x + scroller_structs &&
          c->geom.x + c->geom.width <= m->w.x + m->w.width - scroller_structs) {
        need_scroller = false;
      } else {
        need_scroller = true;
      }
      focus_client_index = i;
      break;
    }
  }

  target_geom.height = m->w.height - 2 * gappov;
  target_geom.width = max_client_width * c->scroller_proportion;
  target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;

  if (need_scroller) {
    if (scroller_focus_center || m->prevsel == NULL ||
        ((m->prevsel->scroller_proportion * max_client_width) +
                (root_client->scroller_proportion * max_client_width) >
            m->w.width - 2 * scroller_structs - gappih && scroller_prefer_center)) {
      target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
    } else {
      target_geom.x =
          root_client->geom.x > m->w.x + (m->w.width) / 2
              ? m->w.x + (m->w.width -
                          root_client->scroller_proportion * max_client_width -
                          scroller_structs)
              : m->w.x + scroller_structs;
    }
    resize(tempClients[focus_client_index], target_geom, 0);
  } else {
    target_geom.x = c->geom.x;
    resize(tempClients[focus_client_index], target_geom, 0);
  }

  for (i = 1; i <= focus_client_index; i++) {
    c = tempClients[focus_client_index - i];
    target_geom.width = max_client_width * c->scroller_proportion;
    target_geom.x = tempClients[focus_client_index - i + 1]->geom.x - gappih -
                    target_geom.width;
    resize(c, target_geom, 0);
  }

  for (i = 1; i < n - focus_client_index; i++) {
    c = tempClients[focus_client_index + i];
    target_geom.width = max_client_width * c->scroller_proportion;
    target_geom.x = tempClients[focus_client_index + i - 1]->geom.x + gappih +
                    tempClients[focus_client_index + i - 1]->geom.width;
    resize(c, target_geom, 0);
  }

  free(tempClients); // 最后释放内存
}

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
    // if (c->bw == 0) { // 真全屏窗口清除x11全屏属性
    //   XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
    //                   PropModeReplace, (unsigned char *)0, 0);
    // }
    c->isfullscreen = 0; // 清除窗口全屏标志
    c->ismaxmizescreen = 0;
  }
  c->bw = borderpx; // 恢复非全屏的border
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
      client_set_fullscreen(c, false);
    }
  } else {
    if (c->is_restoring_from_ov) {
      c->is_restoring_from_ov = false;
      resize(c, c->overview_backup_geom, 0);
    }
  }

  if (c->bw == 0 && !c->isnoborder &&
      !c->isfullscreen) { // 如果是在ov模式中创建的窗口,没有bw记录
    c->bw = borderpx;
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
          target_proportion = config.scroller_proportion_preset[i + 1];
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

void increase_proportion(const Arg *arg) {
  if (selmon->sel) {
    unsigned int max_client_width =
        selmon->w.width - 2 * scroller_structs - gappih;
    selmon->sel->scroller_proportion = MIN(MAX(arg->f + selmon->sel->scroller_proportion,0.1),1.0);
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
                     link) if (c && c->mon == selmon && !c->isminied) {
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
      if (c)
        overview_backup(c);
    }
  } else {
    wl_list_for_each(c, &clients, link) {
      if (c && !c->iskilling && client_surface(c)->mapped)
        overview_restore(c, &(Arg){.ui = target});
    }
  }

  view(&(Arg){.ui = target}, false);

  if (ov_tab_mode && selmon->isoverview && selmon->sel) {
    focusstack(&(Arg){.i = 1});
  }
}

void tile(Monitor *m, unsigned int gappo, unsigned int uappi) {
  unsigned int i, n = 0, h, r, oe = enablegaps, ie = enablegaps, mw, my, ty;
  Client *c;

  wl_list_for_each(c, &clients,
                   link) if (VISIBLEON(c, m) && !c->animation.tagouting &&
                             !c->iskilling && !c->isfloating &&
                             !c->isfullscreen && !c->ismaxmizescreen) n++;
  if (n == 0)
    return;

  if (smartgaps == n) {
    oe = 0; // outer gaps disabled
  }

  if (n > selmon->pertag->nmasters[selmon->pertag->curtag])
    mw = selmon->pertag->nmasters[selmon->pertag->curtag]
             ? (m->w.width + m->gappiv * ie) *
                   selmon->pertag->mfacts[selmon->pertag->curtag]
             : 0;
  else
    mw = m->w.width - 2 * m->gappov * oe + m->gappiv * ie;
  i = 0;
  my = ty = m->gappoh * oe;
  wl_list_for_each(c, &clients, link) {
    if (!VISIBLEON(c, m) || c->iskilling || c->animation.tagouting ||
        c->isfloating || c->isfullscreen || c->ismaxmizescreen)
      continue;
    if (i < selmon->pertag->nmasters[selmon->pertag->curtag]) {
      r = MIN(n, selmon->pertag->nmasters[selmon->pertag->curtag]) - i;
      h = (m->w.height - my - m->gappoh * oe - m->gappih * ie * (r - 1)) / r;
      resize(c,
             (struct wlr_box){.x = m->w.x + m->gappov * oe,
                              .y = m->w.y + my,
                              .width = mw - m->gappiv * ie,
                              .height = h},
             0);
      my += c->geom.height + m->gappih * ie;
    } else {
      r = n - i;
      h = (m->w.height - ty - m->gappoh * oe - m->gappih * ie * (r - 1)) / r;
      resize(c,
             (struct wlr_box){.x = m->w.x + mw + m->gappov * oe,
                              .y = m->w.y + ty,
                              .width = m->w.width - mw - 2 * m->gappov * oe,
                              .height = h},
             0);
      ty += c->geom.height + m->gappih * ie;
    }
    i++;
  }
}

void togglefloating(const Arg *arg) {
  Client *sel = focustop(selmon);

  if (!sel)
    return;

  if (sel->isfullscreen || sel->ismaxmizescreen) {
    sel->isfullscreen = 0; // 清除窗口全屏标志
    sel->ismaxmizescreen = 0;
    sel->bw = borderpx; // 恢复非全屏的border
  }
  /* return if fullscreen */
  setfloating(sel, !sel->isfloating);
  setborder_color(sel);
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
    arrange(selmon, true);
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

void // 0.5
unlocksession(struct wl_listener *listener, void *data) {
  SessionLock *lock = wl_container_of(listener, lock, unlock);
  destroylock(lock, 1);
}

void // 0.5
unmaplayersurfacenotify(struct wl_listener *listener, void *data) {
  LayerSurface *l = wl_container_of(listener, l, unmap);

  l->mapped = 0;
  wlr_scene_node_set_enabled(&l->scene->node, 0);
  if (l == exclusive_focus)
    exclusive_focus = NULL;
  if (l->layer_surface->output && (l->mon = l->layer_surface->output->data))
    arrangelayers(l->mon);
  if (l->layer_surface->surface == seat->keyboard_state.focused_surface)
    focusclient(focustop(selmon), 1);
  motionnotify(0, NULL, 0, 0, 0, 0);
}

void init_fadeout_client(Client *c) {

  if (!c->mon || client_is_unmanaged(c))
    return;

  if (!c->scene) {
    return;
  }

  if(c->animation_type_close && strcmp(c->animation_type_close, "none") == 0) {
    return;
  }

  Client *fadeout_cient = ecalloc(1, sizeof(*fadeout_cient));

  wlr_scene_node_set_enabled(&c->scene->node, true);
  client_set_border_color(c, bordercolor);
  fadeout_cient->scene =
      wlr_scene_tree_snapshot(&c->scene->node, layers[LyrFadeOut]);
  wlr_scene_node_set_enabled(&c->scene->node, false);

  if (!fadeout_cient->scene) {
    free(fadeout_cient);
    return;
  }

  fadeout_cient->animation.duration = animation_duration_close;
  fadeout_cient->geom = fadeout_cient->current = fadeout_cient->animainit_geom =
      fadeout_cient->animation.initial = c->animation.current;
  fadeout_cient->mon = c->mon;
  fadeout_cient->animation_type_close = c->animation_type_close;
  fadeout_cient->animation.action = CLOSE;
  fadeout_cient->bw = c->bw;

  // 这里snap节点的坐标设置是使用的相对坐标，所以不能加上原来坐标
  // 这跟普通node有区别

  fadeout_cient->animation.initial.x = 0;
  fadeout_cient->animation.initial.y = 0;
  if ((c->animation_type_close && strcmp(c->animation_type_close, "slide") == 0) ||
      (!c->animation_type_close && strcmp(animation_type_close, "slide") == 0)) {
    fadeout_cient->current.y =
        c->geom.y + c->geom.height / 2 > c->mon->m.y + c->mon->m.height / 2
            ? c->mon->m.height -
                  (c->animation.current.y - c->mon->m.y) // down out
            : c->mon->m.y - c->geom.height;              // up out
    fadeout_cient->current.x = 0;                        // x无偏差，垂直划出
  } else {
    fadeout_cient->current.y =
        (fadeout_cient->geom.height - fadeout_cient->geom.height * zoom_initial_ratio) / 2;
    fadeout_cient->current.x =
        (fadeout_cient->geom.width - fadeout_cient->geom.width * zoom_initial_ratio) / 2;
    fadeout_cient->current.width = fadeout_cient->geom.width * zoom_initial_ratio;
    fadeout_cient->current.height = fadeout_cient->geom.height * zoom_initial_ratio;
  }

  fadeout_cient->animation.passed_frames = 0;
  fadeout_cient->animation.total_frames =
      fadeout_cient->animation.duration / output_frame_duration_ms(c);
  fadeout_cient->is_fadeout_client = true;
  wlr_scene_node_set_enabled(&fadeout_cient->scene->node, true);
  wl_list_insert(&fadeout_clients, &fadeout_cient->fadeout_link);
}

void unmapnotify(struct wl_listener *listener, void *data) {
  /* Called when the surface is unmapped, and should no longer be shown. */
  Client *c = wl_container_of(listener, c, unmap);
  c->iskilling = 1;
  if (c->is_fadeout_client)
    return;

  if (animations)
    init_fadeout_client(c);

  if (c->swallowedby) {
    c->swallowedby->mon = c->mon;
  	swallow(c->swallowedby, c);
  }

  if (c == grabc) {
    cursor_mode = CurNormal;
    grabc = NULL;
  }

  if (c == selmon->prevsel)
    selmon->prevsel = NULL;

  if (c == selmon->sel) {
    selmon->sel = NULL;
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
    if (c == exclusive_focus)
      exclusive_focus = NULL;
    if (client_surface(c) == seat->keyboard_state.focused_surface)
      focusclient(focustop(selmon), 1);
  } else {
    if (!c->swallowing)
    	wl_list_remove(&c->link);
    setmon(c, NULL, 0,true);
    if (!c->swallowing)
    	wl_list_remove(&c->flink);
  }

  if (c->foreign_toplevel) {
    wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
    c->foreign_toplevel = NULL;
  }

  if (c->swallowedby) {
  	c->swallowedby->prev = c->geom;
  	setfullscreen(c->swallowedby, c->isfullscreen);
    setmaxmizescreen(c->swallowedby, c->ismaxmizescreen);
  	c->swallowedby->swallowing = NULL;
  	c->swallowedby = NULL;
  }

  if (c->swallowing) {
  	c->swallowing->swallowedby = NULL;
  	c->swallowing = NULL;
  }

  // wl_event_source_remove(c->timer_tick);
  wlr_scene_node_destroy(&c->scene->node);
  printstatus();
  motionnotify(0, NULL, 0, 0, 0, 0);
}

void // 0.5
updatemons(struct wl_listener *listener, void *data) {
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

  /* Make sure the clients are hidden when dwl is locked */
  wlr_scene_node_set_position(&locked_bg->node, sgeom.x, sgeom.y);
  wlr_scene_rect_set_size(locked_bg, sgeom.width, sgeom.height);

  wl_list_for_each(m, &mons, link) {
    if (!m->wlr_output->enabled)
      continue;
    config_head =
        wlr_output_configuration_head_v1_create(config, m->wlr_output);

    /* Get the effective monitor geometry to use for surfaces */
    wlr_output_layout_get_box(output_layout, m->wlr_output, &m->m);
    m->w = m->m;
    wlr_scene_output_set_position(m->scene_output, m->m.x, m->m.y);

    // wlr_scene_node_set_position(&m->fullscreen_bg->node, m->m.x, m->m.y);
    // wlr_scene_rect_set_size(m->fullscreen_bg, m->m.width, m->m.height);

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
    if (!selmon) {
      selmon = m;
    }
  }

  if (selmon && selmon->wlr_output->enabled) {
    wl_list_for_each(c, &clients, link) {
      if (!c->mon && client_surface(c)->mapped) {
        setmon(c, selmon, c->tags,true);
        reset_foreign_tolevel(c);
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
  size_t i, tmptag;

  if (!m || (arg->ui != ~0 && m->isoverview)) {
    return;
  }
  if ((m->tagset[m->seltags] & arg->ui & TAGMASK) != 0) {
    want_animation = false;
  }

  m->seltags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK) {
    m->tagset[m->seltags] = arg->ui & TAGMASK;
    m->pertag->prevtag = m->pertag->curtag;

    if (arg->ui == ~0)
    m->pertag->curtag = 0;
    else {
      for (i = 0; !(arg->ui & 1 << i); i++)
        ;
        m->pertag->curtag = i + 1;
    }
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
  size_t tmptag;
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
  size_t tmptag;
  Client *c;
  unsigned int found = 0;
  unsigned int n = 1;
  unsigned int target = selmon->tagset[selmon->seltags];

  if (selmon->isoverview || selmon->pertag->curtag == 0) {
    return;
  }

  for (target <<= 1; target & TAGMASK; target <<= 1, n++) {
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
  if (target) {
    selmon->tagset[selmon->seltags] = target;
    selmon->pertag->prevtag = selmon->pertag->curtag;
    selmon->pertag->curtag = selmon->pertag->curtag + n;
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
  size_t tmptag;
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
  size_t tmptag;
  Client *c;
  unsigned int found = 0;
  unsigned int n = 1;
  unsigned int target = selmon->tagset[selmon->seltags];

  if (selmon->isoverview || selmon->pertag->curtag == 0) {
    return;
  }

  for (target >>= 1; target > 0; target >>= 1, n++) {
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
  if (target) {
    selmon->tagset[selmon->seltags] = target;
    selmon->pertag->prevtag = selmon->pertag->curtag;
    selmon->pertag->curtag = selmon->pertag->curtag - n;
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
  struct wlr_virtual_keyboard_v1 *keyboard = data;
  createkeyboard(&keyboard->keyboard);
}

void warp_cursor(const Client *c) {
  if (cursor->x < c->geom.x || cursor->x > c->geom.x + c->geom.width ||
      cursor->y < c->geom.y || cursor->y > c->geom.y + c->geom.height) {
    wlr_cursor_warp_closest(cursor, NULL, c->geom.x + c->geom.width / 2.0,
                            c->geom.y + c->geom.height / 2.0);
    motionnotify(0, NULL, 0, 0, 0, 0);
  }
}

void warp_cursor_to_selmon(const Monitor *m) {

  wlr_cursor_warp_closest(cursor, NULL, m->w.x + m->w.width / 2.0,
                          m->w.y + m->w.height / 2.0);
}

void virtualpointer(struct wl_listener *listener, void *data) {
  struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
  struct wlr_input_device *device = &event->new_pointer->pointer.base;

  wlr_cursor_attach_input_device(cursor, device);
  if (event->suggested_output)
    wlr_cursor_map_input_to_output(cursor, device, event->suggested_output);
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
#ifdef IM
    if (layer == LyrIMPopup)
      continue;
#endif
    if (layer == LyrFadeOut)
      continue;

    if (!(node = wlr_scene_node_at(&layers[layer]->node, x, y, nx, ny)))
      continue;

    if (node->type == WLR_SCENE_NODE_BUFFER)
      surface =
          wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node))
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

void movewin(const Arg *arg) {
  Client *c, *tc;
  int nx, ny;
  int buttom, top, left, right, tar;
  c = selmon->sel;
  if (!c || c->isfullscreen)
    return;
  if (!c->isfloating)
    togglefloating(NULL);
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
    ny = MAX(ny, c->mon->w.y + gappov);
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
    ny = MIN(ny, c->mon->w.y + c->mon->w.height - gappov - c->geom.height);
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
    nx = MAX(nx, c->mon->w.x + gappoh);
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
    nx = MIN(nx, c->mon->w.x + c->mon->w.width - gappoh - c->geom.width);
    break;
  }

  resize(
      c,
      (struct wlr_box){
          .x = nx, .y = ny, .width = c->geom.width, .height = c->geom.height},
      1);
}

void resizewin(const Arg *arg) {
  Client *c, *tc;
  int nw, nh;
  int buttom, top, left, right, tar;
  c = selmon->sel;
  if (!c || c->isfullscreen)
    return;
  if (!c->isfloating)
    togglefloating(NULL);
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

  resize(c,
         (struct wlr_box){
             .x = c->geom.x, .y = c->geom.y, .width = nw, .height = nh},
         1);
}

#ifdef XWAYLAND
void activatex11(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, activate);

  if (!c || c->iskilling)
    return;

  /* Only "managed" windows can be activated */
  if (!client_is_unmanaged(c))
    wlr_xwayland_surface_activate(c->surface.xwayland, 1);

  if (!c || !c->foreign_toplevel)
    return;

  if (focus_on_activate && c != selmon->sel) {
    view(&(Arg){.ui = c->tags}, true);
    focusclient(c, 1);
  } else if (c != focustop(selmon)) {
    if (client_surface(c)->mapped)
      client_set_border_color(
          c,
          urgentcolor); // 在使用窗口剪切补丁后,这里启动gdm-settings的字体更改那里点击就会崩溃,增加过滤条件为是toplevel窗口后似乎已经解决
    c->isurgent = 1;
    printstatus();
  }
}

void // 0.7
configurex11(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, configure);

  if (!c || c->iskilling)
    return;

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
      !c->mon->pertag->ltidxs[c->mon->pertag->curtag]->arrange)
    resize(c,
           (struct wlr_box){.x = event->x - c->bw,
                            .y = event->y - c->bw,
                            .width = event->width + c->bw * 2,
                            .height = event->height + c->bw * 2},
           0);
  else
    arrange(c->mon, false);
}

/*创建窗口监测函数*/
void createnotifyx11(struct wl_listener *listener, void *data) {
  struct wlr_xwayland_surface *xsurface = data;
  Client *c;

  /* Allocate a Client for this surface */
  c = xsurface->data = ecalloc(1, sizeof(*c));
  c->surface.xwayland = xsurface;
  c->type = X11;
  c->bw = borderpx;

  /* Listen to the various events it can emit */
  LISTEN(&xsurface->events.associate, &c->associate, associatex11);
  LISTEN(&xsurface->events.dissociate, &c->dissociate, dissociatex11);
  LISTEN(&xsurface->events.request_activate, &c->activate, activatex11);
  LISTEN(&xsurface->events.request_configure, &c->configure, configurex11);
  LISTEN(&xsurface->events.set_hints, &c->set_hints, sethints);
  LISTEN(&xsurface->events.set_title, &c->set_title, updatetitle);
  LISTEN(&xsurface->events.destroy, &c->destroy, destroynotify);
  LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen,
         fullscreennotify);
  LISTEN(&xsurface->events.request_maximize, &c->maximize, maximizenotify);
  LISTEN(&xsurface->events.request_minimize, &c->minimize, minimizenotify);
}

Atom getatom(xcb_connection_t *xc, const char *name) {
  Atom atom = 0;
  xcb_intern_atom_reply_t *reply;
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xc, 0, strlen(name), name);
  if ((reply = xcb_intern_atom_reply(xc, cookie, NULL)))
    atom = reply->atom;
  free(reply);

  return atom;
}

void sethints(struct wl_listener *listener, void *data) {
  Client *c = wl_container_of(listener, c, set_hints);
  if (c != focustop(selmon)) {
    c->isurgent = xcb_icccm_wm_hints_get_urgency(c->surface.xwayland->hints);
    printstatus();
  }
}

void xwaylandready(struct wl_listener *listener, void *data) {
  struct wlr_xcursor *xcursor;
  xcb_connection_t *xc = xcb_connect(xwayland->display_name, NULL);
  int err = xcb_connection_has_error(xc);
  if (err) {
    fprintf(stderr,
            "xcb_connect to X server failed with code %d\n. Continuing with "
            "degraded functionality.\n",
            err);
    return;
  }

  /* Collect atoms we are interested in.  If getatom returns 0, we will
   * not detect that window type. */
  netatom[NetWMWindowTypeDialog] = getatom(xc, "_NET_WM_WINDOW_TYPE_DIALOG");
  netatom[NetWMWindowTypeSplash] = getatom(xc, "_NET_WM_WINDOW_TYPE_SPLASH");
  netatom[NetWMWindowTypeToolbar] = getatom(xc, "_NET_WM_WINDOW_TYPE_TOOLBAR");
  netatom[NetWMWindowTypeUtility] = getatom(xc, "_NET_WM_WINDOW_TYPE_UTILITY");

  /* assign the one and only seat */
  wlr_xwayland_set_seat(xwayland, seat);

  /* Set the default XWayland cursor to match the rest of dwl. */
  if ((xcursor = wlr_xcursor_manager_get_xcursor(cursor_mgr, "left_ptr", 1)))
    wlr_xwayland_set_cursor(
        xwayland, xcursor->images[0]->buffer, xcursor->images[0]->width * 4,
        xcursor->images[0]->width, xcursor->images[0]->height,
        xcursor->images[0]->hotspot_x, xcursor->images[0]->hotspot_y);

  xcb_disconnect(xc);
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

  /* Wayland requires XDG_RUNTIME_DIR for creating its communications socket */
  if (!getenv("XDG_RUNTIME_DIR"))
    die("XDG_RUNTIME_DIR must be set");
  setup();
  run(startup_cmd);
  cleanup();
  return EXIT_SUCCESS;

usage:
  die("Usage: %s [-v] [-d] [-s startup command]", argv[0]);
}
