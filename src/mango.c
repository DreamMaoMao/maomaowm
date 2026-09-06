/*
 * See LICENSE file for copyright and license details.
 */
#include "mango/mango.h"
#include "wlr/util/box.h"
/* Mango includes */
#include "mango/animation/common.h"
#include <fcntl.h>
#include <getopt.h>
#include <libinput.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
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
#include <wordexp.h>
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <X11/Xlib.h>
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>
#endif
#include "mango/common/log.h"
#include "mango/common/util.h"
#include "mango/draw/text-node.h"

/* function declarations */
void applybounds(
	Client *c,
	struct wlr_box *bbox);	// 设置边界规则,能让一些窗口拥有比较适合的大小
void applyrules(Client *c); // 窗口规则应用,应用config.h中定义的窗口规则
void apply_window_snap(Client *c);
void arrange(Monitor *m, bool want_animation,
			 bool from_view); // 布局函数,让窗口俺平铺规则移动和重置大小
void arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area,
				  int32_t exclusive);
void arrangelayers(Monitor *m);
void handle_print_status(struct wl_listener *listener, void *data);
void axisnotify(struct wl_listener *listener,
				void *data); // 滚轮事件处理
void buttonpress(struct wl_listener *listener,
				 void *data); // 鼠标按键事件处理
bool handle_buttonpress(struct wlr_pointer_button_event *event);
int32_t ongesture(struct wlr_pointer_swipe_end_event *event);
void swipe_begin(struct wl_listener *listener, void *data);
void swipe_update(struct wl_listener *listener, void *data);
void swipe_end(struct wl_listener *listener, void *data);
void pinch_begin(struct wl_listener *listener, void *data);
void pinch_update(struct wl_listener *listener, void *data);
void pinch_end(struct wl_listener *listener, void *data);
void hold_begin(struct wl_listener *listener, void *data);
void hold_end(struct wl_listener *listener, void *data);
void checkidleinhibitor(struct wlr_surface *exclude);
void cleanupmon(struct wl_listener *listener, void *data); // 退出清理
void closemon(Monitor *m);
void toggle_hotarea(int32_t x_root, int32_t y_root); // 触发热区
void maplayersurfacenotify(struct wl_listener *listener, void *data);
void commitlayersurfacenotify(struct wl_listener *listener, void *data);
void commitnotify(struct wl_listener *listener, void *data);
void createdecoration(struct wl_listener *listener, void *data);
void createidleinhibitor(struct wl_listener *listener, void *data);
void createkeyboard(struct wlr_keyboard *keyboard);
void requestmonstate(struct wl_listener *listener, void *data);
void createlayersurface(struct wl_listener *listener, void *data);
void createlocksurface(struct wl_listener *listener, void *data);
void createmon(struct wl_listener *listener, void *data);
void createpointer(struct wlr_pointer *pointer);
void configure_pointer(struct wlr_input_device *wlr_device,
					   struct libinput_device *device);
void destroyinputdevice(struct wl_listener *listener, void *data);
void createswitch(struct wlr_switch *switch_device);
void switch_toggle(struct wl_listener *listener, void *data);
void createpointerconstraint(struct wl_listener *listener, void *data);
void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
void commitpopup(struct wl_listener *listener, void *data);
void cursorwarptohint(void);
void destroydecoration(struct wl_listener *listener, void *data);
void destroydragicon(struct wl_listener *listener, void *data);
void destroyidleinhibitor(struct wl_listener *listener, void *data);
void destroylayernodenotify(struct wl_listener *listener, void *data);
void destroylock(SessionLock *lock, int32_t unlocked);
void destroylocksurface(struct wl_listener *listener, void *data);
void destroynotify(struct wl_listener *listener, void *data);
void destroypointerconstraint(struct wl_listener *listener, void *data);
void destroysessionlock(struct wl_listener *listener, void *data);
void destroykeyboardgroup(struct wl_listener *listener, void *data);
Monitor *dirtomon(enum wlr_direction dir);
void setcursorshape(struct wl_listener *listener, void *data);

void focuslayer(LayerSurface *l);
void focusclient(Client *c, int32_t lift);

void setborder_color(Client *c);
Client *focustop(Monitor *m);
void fullscreennotify(struct wl_listener *listener, void *data);
void gpureset(struct wl_listener *listener, void *data);

int32_t keyrepeat(void *data);

void inputdevice(struct wl_listener *listener, void *data);
int32_t keybinding(uint32_t state, bool locked, uint32_t mods, xkb_keysym_t sym,
				   uint32_t keycode);
void keypress(struct wl_listener *listener, void *data);
void keypressmod(struct wl_listener *listener, void *data);
bool keypressglobal(struct wlr_surface *last_surface,
					struct wlr_keyboard *keyboard,
					struct wlr_keyboard_key_event *event, uint32_t mods,
					xkb_keysym_t keysym, uint32_t keycode);
void locksession(struct wl_listener *listener, void *data);
void mapnotify(struct wl_listener *listener, void *data);
void maximizenotify(struct wl_listener *listener, void *data);
void minimizenotify(struct wl_listener *listener, void *data);
void motionabsolute(struct wl_listener *listener, void *data);
void motionnotify(uint32_t time, struct wlr_input_device *device, double sx,
				  double sy, double sx_unaccel, double sy_unaccel);
void motionrelative(struct wl_listener *listener, void *data);

void reset_foreign_tolevel(Client *c, Monitor *oldmon, Monitor *newmon);
void add_foreign_topleve(Client *c);
void exchange_two_client(Client *c1, Client *c2);
void outputmgrapply(struct wl_listener *listener, void *data);
void outputmgrapplyortest(struct wlr_output_configuration_v1 *config,
						  int32_t test);
void outputmgrtest(struct wl_listener *listener, void *data);
void pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
				  uint32_t time);
void printstatus(enum ipc_watch_type type);
void powermgrsetmode(struct wl_listener *listener, void *data);
void rendermon(struct wl_listener *listener, void *data);
void requestdecorationmode(struct wl_listener *listener, void *data);
void requestdrmlease(struct wl_listener *listener, void *data);
void requeststartdrag(struct wl_listener *listener, void *data);
void setcursor(struct wl_listener *listener, void *data);
void setfloating(Client *c, int32_t floating);
void setfakefullscreen(Client *c, int32_t fakefullscreen);
void setfullscreen(Client *c, int32_t fullscreen, bool rearrange);
void setmaximizescreen(Client *c, int32_t maximizescreen, bool rearrange);
void reset_maximizescreen_size(Client *c);
void setgaps(int32_t oh, int32_t ov, int32_t ih, int32_t iv);

void setmon(Client *c, Monitor *m, uint32_t newtags, bool focus);
void setpsel(struct wl_listener *listener, void *data);
void setsel(struct wl_listener *listener, void *data);
void startdrag(struct wl_listener *listener, void *data);

void unlocksession(struct wl_listener *listener, void *data);
void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
void unmapnotify(struct wl_listener *listener, void *data);
void updatemons(struct wl_listener *listener, void *data);
void updatetitle(struct wl_listener *listener, void *data);
void urgent(struct wl_listener *listener, void *data);
void view(const Arg *arg, bool want_animation);

void handle_keyboard_shortcuts_inhibit_new_inhibitor(
	struct wl_listener *listener, void *data);
void virtualkeyboard(struct wl_listener *listener, void *data);
void virtualpointer(struct wl_listener *listener, void *data);
void warp_cursor(const Client *c);
Monitor *xytomon(double x, double y);
Monitor *get_monitor_nearest_to(int32_t x, int32_t y);
void handle_iamge_copy_capture_new_session(struct wl_listener *listener,
										   void *data);
void xytonode(double x, double y, struct wlr_surface **psurface, Client **pc,
			  LayerSurface **pl, MangoGroupBar **tb, double *nx, double *ny);
void clear_fullscreen_flag(Client *c);
pid_t getparentprocess(pid_t p);
int32_t isdescprocess(pid_t p, pid_t c);
Client *termforwin(Client *w);
void client_replace(Client *c, Client *w, bool is_group_change_member,
					bool is_swallow);

void warp_cursor_to_selmon(Monitor *m);
uint32_t want_restore_fullscreen(Client *target_client);
void overview_restore(Client *c, const Arg *arg);
void overview_backup(Client *c);
void set_minimized(Client *c);

void show_scratchpad(Client *c);
void show_hide_client(Client *c);
void tag_client(const Arg *arg, Client *target_client);

struct wlr_box setclient_coordinate_center(Client *c, Monitor *m,
										   struct wlr_box geom, int32_t offsetx,
										   int32_t offsety);
uint32_t get_tags_first_tag(uint32_t tags);

struct wlr_output_mode *get_nearest_output_mode(struct wlr_output *output,
												int32_t width, int32_t height,
												float refresh);

void client_set_opacity(Client *c, double opacity);
void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int32_t sx,
								int32_t sy, void *data);

Client *direction_select(const Arg *arg);
void view_in_mon(const Arg *arg, bool want_animation, Monitor *m,
				 bool changefocus);
void client_send_frame_done(Client *c, const struct timespec *now);
void overview_layout_card(Client *c);
void overview_destroy_card(Client *c);
Client *center_tiled_select(Monitor *m);
void handlecursoractivity(void);
int32_t hidecursor(void *data);
bool check_hit_no_border(Client *c);
void reset_keyboard_layout(void);
void client_update_oldmonname_record(Client *c, Monitor *m);
void pending_kill_client(Client *c);
uint32_t get_tags_first_tag_num(uint32_t source_tags);
void set_layer_open_animaiton(LayerSurface *l, struct wlr_box geo);

void apply_opacity_to_rect_nodes(Client *c, struct wlr_scene_node *node,
								 double animation_passed);
double all_output_frame_duration_ms();
bool is_scroller_layout(Monitor *m);
bool is_monocle_layout(Monitor *m);
bool is_centertile_layout(Monitor *m);
void create_output(struct wlr_backend *backend, void *data);
void get_layout_abbr(char *abbr, const char *full_name);
void apply_named_scratchpad(Client *target_client);
Client *get_client_by_id_or_title(const char *arg_id, const char *arg_title);
bool switch_scratchpad_client_state(Client *c);
bool check_trackpad_disabled(struct wlr_pointer *pointer);
uint32_t get_tag_status(uint32_t tag, Monitor *m);
void view_insert_shift_tags(Monitor *m, uint32_t target);
void enable_adaptive_sync(Monitor *m, struct wlr_output_state *state);
Client *get_next_stack_client(Client *c, bool reverse);
void set_float_malposition(Client *tc);
void set_size_per(Monitor *m, Client *c);
void resize_tile_client(Client *grabc, bool isdrag, int32_t offsetx,
						int32_t offsety, uint32_t time);
void refresh_monitors_workspaces_status(Monitor *m);
void init_client_properties(Client *c);
float *get_border_color(Client *c);
void clear_fullscreen_and_maximized_state(Monitor *m);
void request_fresh_all_monitors(void);
Client *find_client_by_direction(Client *tc, const Arg *arg, bool findfloating);
void exit_scroller_stack(Client *c);
Client *scroll_get_stack_head_client(Client *c);
bool client_only_in_one_tag(Client *c);
Client *get_focused_stack_client(Client *sc, Client *custom_focus_client);
bool client_is_in_same_stack(Client *sc, Client *tc, Client *fc);
void monitor_stop_skip_frame_timer(Monitor *m);
int monitor_skip_frame_timeout_callback(void *data);
Monitor *get_monitor_nearest_to(int32_t lx, int32_t ly);
bool match_monitor_spec(char *spec, Monitor *m);
void last_cursor_surface_destroy(struct wl_listener *listener, void *data);
int32_t keep_idle_inhibit(void *data);
void check_keep_idle_inhibit(Client *c);
void pre_calculate_before_arrange(Monitor *m, bool want_animation,
								  bool from_view, bool only_calculate);
void client_pending_fullscreen_state(Client *c, int32_t isfullscreen);
void client_pending_maximized_state(Client *c, int32_t ismaximized);
void client_pending_minimized_state(Client *c, int32_t isminimized);
void scroller_insert_stack(Client *c, Client *target_client,
						   bool insert_before);
void dwindle_move_client(DwindleNode **root, Client *c, Client *target,
						 float ratio, int32_t dir);
void dwindle_resize_client_step(Monitor *m, Client *c, int32_t dx, int32_t dy);
void dwindle_resize_client(Monitor *m, Client *c);

struct TagScrollerState *ensure_scroller_state(Monitor *m, uint32_t tag);
struct ScrollerStackNode *find_scroller_node(struct TagScrollerState *st,
											 Client *c);
void sync_scroller_state_to_clients(Monitor *m, uint32_t tag);
void scroller_node_remove(struct TagScrollerState *st,
						  struct ScrollerStackNode *target);
struct ScrollerStackNode *scroller_node_create(struct TagScrollerState *st,
											   Client *c);
void update_scroller_state(Monitor *m);
Client *scroll_get_stack_tail_client(Client *c);
DwindleNode *dwindle_find_leaf(DwindleNode *node, Client *c);
void overview_backup_surface(Client *c);

void create_jump_hints(Monitor *m);
void finish_jump_mode(Monitor *m);
void begin_jump_mode(Monitor *m);

void client_reparent_group(Client *c);
void client_change_mon(Client *c, Monitor *m);
void check_vrr_enable(Client *c);
void output_enable_hdr(Monitor *m, struct wlr_output_state *os, bool enabled,
					   bool silent);
bool mango_scene_output_commit(struct wlr_scene_output *scene_output,
							   struct wlr_output_state *state);
bool mango_output_commit(Monitor *m);
void client_set_group_config(Client *c);

int32_t client_is_x11(Client *c);
struct wlr_surface *client_surface(Client *c);
int32_t toplevel_from_wlr_surface(struct wlr_surface *s, Client **pc,
								  LayerSurface **pl);
void client_activate_surface(struct wlr_surface *s, int32_t activated);
const char *client_get_appid(Client *c);
int32_t client_get_pid(Client *c);
void client_get_clip(Client *c, struct wlr_box *clip);
void client_get_geometry(Client *c, struct wlr_box *geom);
Client *client_get_parent(Client *c);
int32_t client_has_children(Client *c);
const char *client_get_title(Client *c);
int32_t client_is_float_type(Client *c);
int32_t client_is_rendered_on_mon(Client *c, Monitor *m);
int32_t client_is_unmanaged(Client *c);
void client_notify_enter(struct wlr_surface *s, struct wlr_keyboard *kb);
void client_send_close(Client *c);
void client_set_border_color(Client *c, const float color[4]);
void client_set_fullscreen(Client *c, int32_t fullscreen);
void client_set_scale(struct wlr_surface *s, float scale);
void client_update_xwayland_clip(Client *c, struct wlr_box *clip);
void client_update_xwayland_dest_size(Client *c);
uint32_t client_set_size(Client *c, uint32_t width, uint32_t height);
void client_set_minimized(Client *c, bool minimized);
void client_set_maximized(Client *c, bool maximized);
void client_set_tiled(Client *c, uint32_t edges);
int32_t client_should_ignore_focus(Client *c);
int32_t client_is_x11_popup(Client *c);
int32_t client_should_global(Client *c);
int32_t client_should_overtop(Client *c);
int32_t client_wants_focus(Client *c);
int32_t client_wants_fullscreen(Client *c);
bool client_request_minimize(Client *c, void *data);
bool client_request_maximize(Client *c, void *data);
void client_set_size_bound(Client *c);
void client_swap_layout_properties(Client *c1, Client *c2);
void client_swap_monitors_and_tags(Client *c1, Client *c2);
void finish_exchange_arrange_and_focus(Client *c1, Client *c2, Monitor *m1,
									   Monitor *m2);
void client_tile_resize(Client *c, struct wlr_box geo, int32_t interact);
uint32_t generate_client_id(void);
void client_active(Client *c);
void client_pending_force_kill(Client *c);
void client_add_jump_label_node(Client *c);
void client_add_group_bar(Client *c);
void client_focus_group_member(Client *c);
void client_check_tab_node_visible(Client *c);
void client_raise_group(Client *c);
void client_handle_decorate_click(MangoGroupBar *gb);
void client_set_group_mon(Client *c, Monitor *m);
void client_group_detach(Client *c);
void client_group_replace(Client *old, Client *new);
void mango_surface_frame_done(struct wlr_surface *surface, int sx, int sy,
							  void *data);
bool client_force_render(Client *c);
int32_t is_single_bit_set(uint32_t x);
bool layer_ignores_focus(LayerSurface *l);

/* 重排后补充的前置声明 */
void iter_xdg_scene_buffers(struct wlr_scene_buffer *buffer, int32_t sx,
							int32_t sy, void *user_data);
void unminimize(Client *c);
Client *termforwin(Client *w);
Client *get_client_by_id_or_title(const char *arg_id, const char *arg_title);
Client *center_tiled_select(Monitor *m);
Client *find_client_by_direction(Client *tc, const Arg *arg, bool findfloating);
Client *direction_select(const Arg *arg);
Client *focustop(Monitor *m);
Client *get_next_stack_client(Client *c, bool reverse);
float *get_border_color(Client *c);
Client *get_focused_stack_client(Client *sc, Client *custom_focus_client);
Client *xytoclient(double x, double y);
Monitor *dirtomon(enum wlr_direction dir);
Monitor *xytomon(double x, double y);
Monitor *get_monitor_nearest_to(int32_t lx, int32_t ly);

#include "mango/dispatch/bind.h"
#include "mango/common/globals.h"
#include "mango/common/log.h"
#include "mango/animation/common.h"
#include "mango/input/touch.h"
#include "mango/input/tablet.h"
#include "mango/ipc/ipc.h"
#include "mango/input/keyboard.h"
#include "mango/input/pointer.h"
#include "mango/manage/monitor.h"
#include "mango/manage/misc.h"
#include "mango/ext-protocol/ext-workspace.h"
#include "mango/ext-protocol/foreign-toplevel.h"
#include "mango/ext-protocol/hdr.h"
#include "mango/ext-protocol/tearing.h"
#include "mango/ext-protocol/text-input.h"
#include "mango/ext-protocol/xdg-activation.h"
#include "mango/ext-protocol/xdg-output.h"
#include "mango/common/util.h"

void handlesig(int32_t signo) {
	if (signo == SIGCHLD)
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
	else if (signo == SIGINT || signo == SIGTERM)
		quit(NULL);
}

static void restore_child_signals(void) {
	sigset_t set;
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, NULL);

	struct sigaction sa_dfl = {.sa_flags = 0, .sa_handler = SIG_DFL};
	sigaction(SIGCHLD, &sa_dfl, NULL);
	sigaction(SIGPIPE, &sa_dfl, NULL);
}

void cleanuplisteners(void) {
	wl_list_remove(&ext_manager_commit_listener.link); // 0.7
	wl_list_remove(&print_status_listener.link);
	wl_list_remove(&cursor_axis.link);
	wl_list_remove(&cursor_button.link);
	wl_list_remove(&cursor_frame.link);
	wl_list_remove(&cursor_motion.link);
	wl_list_remove(&cursor_motion_absolute.link);
	wl_list_remove(&cursor_touch_down.link);
	wl_list_remove(&cursor_touch_up.link);
	wl_list_remove(&cursor_touch_cancel.link);
	wl_list_remove(&cursor_touch_motion.link);
	wl_list_remove(&cursor_touch_frame.link);
	wl_list_remove(&tablet_tool_proximity.link);
	wl_list_remove(&tablet_tool_axis.link);
	wl_list_remove(&tablet_tool_button.link);
	wl_list_remove(&tablet_tool_tip.link);
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
	wl_list_remove(&request_cursor.link);
	wl_list_remove(&request_set_psel.link);
	wl_list_remove(&request_set_sel.link);
	wl_list_remove(&request_set_cursor_shape.link);
	wl_list_remove(&request_start_drag.link);
	wl_list_remove(&start_drag.link);
	wl_list_remove(&new_session_lock.link);
	wl_list_remove(&new_foreign_toplevel_capture_request.link);
	wl_list_remove(&tearing_new_object.link);
	wl_list_remove(&keyboard_shortcuts_inhibit_new_inhibitor.link);
	wl_list_remove(&ext_image_copy_capture_mgr_new_session.link);
	if (drm_lease_manager) {
		wl_list_remove(&drm_lease_request.link);
	}
#ifdef XWAYLAND
	wl_list_remove(&new_xwayland_surface.link);
	wl_list_remove(&xwayland_ready.link);
#endif
}

void cleanup(void) {
	allow_frame_scheduling = false;

	ipc_cleanup();
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

	mango_im_relay_finish(mango_input_method_relay);

	/* If it's not destroyed manually it will cause a use-after-free of
	 * wlr_seat. Destroy it until it's fixed in the wlroots side */
	wlr_backend_destroy(backend);

	wl_display_destroy(dpy);
	/* Destroy after the wayland display (when the monitors are already
	   destroyed) to avoid destroying them with an invalid scene output. */
	wlr_scene_node_destroy(&scene->tree.node);

	mango_text_global_finish();
}

// 修改printstatus函数，接受掩码参数

void quitsignal(int32_t signo) { quit(NULL); }

void set_activation_env() {
	if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
		mango_error(true, WLR_INFO,
					"Not updating dbus execution environment: "
					"DBUS_SESSION_BUS_ADDRESS not set");
		return;
	}

	mango_error(true, WLR_INFO, "Updating dbus execution environment");

	char *env_keys = join_strings(env_vars, " ");

	// first command: dbus-update-activation-environment
	const char *arg1 = env_keys;
	char *cmd1 = string_printf("dbus-update-activation-environment %s", arg1);
	if (!cmd1) {
		mango_error(true, WLR_ERROR, "Failed to allocate command string");
		goto cleanup;
	}
	spawn(&(Arg){.v = cmd1});
	free(cmd1);

	// second command: systemctl --user
	const char *action = "import-environment";
	char *cmd2 = string_printf("systemctl --user %s %s", action, env_keys);
	if (!cmd2) {
		mango_error(true, WLR_ERROR, "Failed to allocate command string");
		goto cleanup;
	}
	spawn(&(Arg){.v = cmd2});
	free(cmd2);

cleanup:
	free(env_keys);
}

void // 17
run(char *startup_cmd, int readiness_fd) {
	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(dpy);
	if (!socket)
		die("startup: display_add_socket_auto");
	setenv("WAYLAND_DISPLAY", socket, 1);

	set_env_display();

	/* Start the backend. This will enumerate outputs and inputs, become the
	 * DRM master, etc */
	if (!wlr_backend_start(backend))
		die("startup: backend_start");

	/* Now that the socket exists and the backend is started, run the
	 * startup command */

	if (startup_cmd) {
		int32_t piperw[2];
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

	printstatus(IPC_WATCH_ARRANGGE);

	/* At this point the outputs are initialized, choose initial selmon
	 * based on cursor position, and set default cursor image */
	selmon = xytomon(cursor->x, cursor->y);

	/* TODO hack to get cursor to display in its initial location (100, 100)
	 * instead of (0, 0) and then jumping. still may not be fully
	 * initialized, as the image/coordinates are not transformed for the
	 * monitor when displayed here */
	wlr_cursor_warp_closest(cursor, NULL, cursor->x, cursor->y);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "left_ptr");
	handlecursoractivity();

	set_activation_env();

	run_exec();
	run_exec_once();

	/*
	 * If running inside supervision suite like s6, notify about successfull
	 * startup by writing \n to the provided file descriptor and closing it
	 */
	if (readiness_fd > 2) {
		write(readiness_fd, "\n", 1);
		close(readiness_fd);
	}

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */

	wl_display_run(dpy);
}

// 修改信号处理函数，接收掩码参数

void setup(void) {
	setenv("XDG_CURRENT_DESKTOP", "mango", 1);
	setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 1);

	parse_config();
	if (cli_debug_log) {
		config.log_level = WLR_DEBUG;
	}
	init_baked_points();

	set_env_without_display();

	int32_t drm_fd, i;
	int32_t sig[] = {SIGCHLD, SIGINT,
					 SIGTERM}; // 不设置SIGPIPE,因为ipc发送失败不应该影响主程序
	struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handlesig};
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < LENGTH(sig); i++)
		sigaction(sig[i], &sa, NULL);

	// 单独为 SIGPIPE 设置忽略
	struct sigaction sa_pipe = {.sa_flags = 0, .sa_handler = SIG_IGN};
	sigemptyset(&sa_pipe.sa_mask);
	sigaction(SIGPIPE, &sa_pipe, NULL);

	pthread_atfork(NULL, NULL, restore_child_signals);

	wlr_log_init(config.log_level, NULL);

	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	dpy = wl_display_create();

	wl_display_set_default_max_buffer_size(dpy, 1024 * 1024);

	event_loop = wl_display_get_event_loop(dpy);

	ipc_init(event_loop);

	tablet_mgr = wlr_tablet_v2_create(dpy);
	/* The backend is a wlroots feature which abstracts the underlying input
	 * and output hardware. The autocreate option will choose the most
	 * suitable backend based on the current environment, such as opening an
	 * X11 window if an X11 server is running. The NULL argument here
	 * optionally allows you to pass in a custom renderer if wlr_renderer
	 * doesn't meet your needs. The backend uses the renderer, for example,
	 * to fall back to software cursors if the backend does not support
	 * hardware cursors (some older GPUs don't). */
	if (!(backend = wlr_backend_autocreate(event_loop, &session)))
		die("couldn't create backend");

	headless_backend = wlr_headless_backend_create(event_loop);
	if (!headless_backend) {
		mango_error(true, WLR_ERROR,
					"Failed to create secondary headless backend");
	} else {
		wlr_multi_backend_add(backend, headless_backend);
	}

	/* Initialize the scene graph used to lay out windows */
	scene = wlr_scene_create();
	root_bg = wlr_scene_rect_create(&scene->tree, 0, 0, config.rootcolor);
	for (i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);
	drag_icon = wlr_scene_tree_create(&scene->tree);
	wlr_scene_node_place_below(&drag_icon->node, &layers[LyrBlock]->node);

	/* Create a renderer with the default implementation */
	if (!(drw = wlr_renderer_autocreate(backend)))
		die("couldn't recreate renderer");

	if (drw->features.input_color_transform) {
		const enum wp_color_manager_v1_render_intent render_intents[] = {
			WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL,
		};
		size_t transfer_functions_len = 0;
		enum wp_color_manager_v1_transfer_function *transfer_functions =
			wlr_color_manager_v1_transfer_function_list_from_renderer(
				drw, &transfer_functions_len);

		size_t primaries_len = 0;
		enum wp_color_manager_v1_primaries *primaries =
			wlr_color_manager_v1_primaries_list_from_renderer(drw,
															  &primaries_len);

		struct wlr_color_manager_v1 *cm = wlr_color_manager_v1_create(
			dpy, 2,
			&(struct wlr_color_manager_v1_options){
				.features =
					{
						.parametric = true,
						.set_mastering_display_primaries = true,
					},
				.render_intents = render_intents,
				.render_intents_len = ARRAY_SIZE(render_intents),
				.transfer_functions = transfer_functions,
				.transfer_functions_len = transfer_functions_len,
				.primaries = primaries,
				.primaries_len = primaries_len,
			});

		free(transfer_functions);
		free(primaries);

		if (cm) {
			wlr_scene_set_color_manager_v1(scene, cm);
		} else {
			mango_error(true, WLR_ERROR, "unable to create color manager");
		}
	}

	wlr_color_representation_manager_v1_create_with_renderer(dpy, 1, drw);

	wl_signal_add(&drw->events.lost, &gpu_reset);

	/* Create shm, drm and linux_dmabuf interfaces by ourselves.
	 * The simplest way is call:
	 *      wlr_renderer_init_wl_display(drw);
	 * but we need to create manually the linux_dmabuf interface to
	 * integrate it with wlr_scene. */
	wlr_renderer_init_wl_shm(drw, dpy);

	if (wlr_renderer_get_texture_formats(drw, WLR_BUFFER_CAP_DMABUF)) {
		wlr_drm_create(dpy, drw);
		wlr_scene_set_linux_dmabuf_v1(
			scene, wlr_linux_dmabuf_v1_create_with_renderer(dpy, 4, drw));
	}

	if (config.syncobj_enable && (drm_fd = wlr_renderer_get_drm_fd(drw)) >= 0 &&
		drw->features.timeline && backend->features.timeline)
		wlr_linux_drm_syncobj_manager_v1_create(dpy, 1, drm_fd);

	/* Create a default allocator */
	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't create allocator");

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device
	 * manager handles the clipboard. Each of these wlroots interfaces has
	 * room for you to dig your fingers in and play with their behavior if
	 * you want. Note that the clients cannot set the selection directly
	 * without compositor approval, see the setsel() function. */
	compositor = wlr_compositor_create(dpy, 6, drw);
	wlr_export_dmabuf_manager_v1_create(dpy);
	wlr_screencopy_manager_v1_create(dpy);
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
	wlr_fixes_create(dpy, 1);

	// 在 setup 函数中
	wl_signal_init(&mango_print_status);
	wl_signal_add(&mango_print_status, &print_status_listener);

	ext_image_copy_capture_mgr =
		wlr_ext_image_copy_capture_manager_v1_create(dpy, 1);
	wl_signal_add(&ext_image_copy_capture_mgr->events.new_session,
				  &ext_image_copy_capture_mgr_new_session);

	xdg_activation_init();

	wlr_scene_set_gamma_control_manager_v1(
		scene, wlr_gamma_control_manager_v1_create(dpy));

	power_mgr = wlr_output_power_manager_v1_create(dpy);
	wl_signal_add(&power_mgr->events.set_mode, &output_power_mgr_set_mode);

	foreign_toplevel_list = wlr_ext_foreign_toplevel_list_v1_create(dpy, 1);
	ext_foreign_toplevel_image_capture_source_manager_v1 =
		wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(dpy, 1);
	new_foreign_toplevel_capture_request.notify =
		handle_new_foreign_toplevel_capture_request;
	wl_signal_add(&ext_foreign_toplevel_image_capture_source_manager_v1->events
					   .new_request,
				  &new_foreign_toplevel_capture_request);

	tearing_control = wlr_tearing_control_manager_v1_create(dpy, 1);
	tearing_new_object.notify = handle_tearing_new_object;
	wl_signal_add(&tearing_control->events.new_object, &tearing_new_object);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	output_layout = wlr_output_layout_create(dpy);
	wl_signal_add(&output_layout->events.change, &layout_change);
	/* 自定义 xdg-output：对 XWayland 单独处理 */
	xdg_output_init();

	/* Configure a listener to be notified when new outputs are available on
	 * the backend. */
	wl_list_init(&mons);
	wl_signal_add(&backend->events.new_output, &new_output);
	scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

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

	keep_idle_inhibit_source = wl_event_loop_add_timer(
		wl_display_get_event_loop(dpy), keep_idle_inhibit, NULL);

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
	 * Xcursor themes to source cursor images from and makes sure that
	 * cursor images are available at all scale factors on the screen
	 * (necessary for HiDPI support). Scaled cursors will be loaded with
	 * each output. */

	set_xcursor_env();

	cursor_mgr =
		wlr_xcursor_manager_create(config.cursor_theme, config.cursor_size);
	/*
	 * wlr_cursor *only* displays an image on screen. It does not move
	 * around when the pointer moves. However, we can attach input devices
	 * to it, and it will generate aggregate events for all of them. In
	 * these events, we can choose how we want to process them, forwarding
	 * them to clients and moving the cursor around. More detail on this
	 * process is described in my input handling blog post:
	 *
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	 *
	 * And more comments are sprinkled throughout the notify functions
	 * above.
	 */
	wl_signal_add(&cursor->events.motion, &cursor_motion);
	wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);
	wl_signal_add(&cursor->events.button, &cursor_button);
	wl_signal_add(&cursor->events.axis, &cursor_axis);
	wl_signal_add(&cursor->events.frame, &cursor_frame);
	wl_signal_add(&cursor->events.tablet_tool_proximity,
				  &tablet_tool_proximity);
	wl_signal_add(&cursor->events.tablet_tool_axis, &tablet_tool_axis);
	wl_signal_add(&cursor->events.tablet_tool_button, &tablet_tool_button);
	wl_signal_add(&cursor->events.tablet_tool_tip, &tablet_tool_tip);

	// 这两句代码会造成obs窗口里的鼠标光标消失,不知道注释有什么影响
	cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(dpy, 1);
	wl_signal_add(&cursor_shape_mgr->events.request_set_shape,
				  &request_set_cursor_shape);
	hide_cursor_source = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
												 hidecursor, cursor);
	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener
	 * to let us know when new input devices are available on the backend.
	 */
	wl_list_init(&inputdevices);
	wl_list_init(&standalone_keyboards);
	wl_list_init(&virtual_keyboards);
	wl_list_init(&tablets);
	wl_list_init(&tablet_pads);
	wl_list_init(&keyboard_shortcut_inhibitors);
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

	/* 触摸支持：初始化触点链表并连接 cursor 触摸事件 */
	wl_list_init(&touch_points);
	wl_signal_add(&cursor->events.touch_down, &cursor_touch_down);
	wl_signal_add(&cursor->events.touch_up, &cursor_touch_up);
	wl_signal_add(&cursor->events.touch_cancel, &cursor_touch_cancel);
	wl_signal_add(&cursor->events.touch_motion, &cursor_touch_motion);
	wl_signal_add(&cursor->events.touch_frame, &cursor_touch_frame);

	seat = wlr_seat_create(dpy, "seat0");

	wl_list_init(&last_cursor_surface_destroy_listener.link);
	wl_signal_add(&seat->events.request_set_cursor, &request_cursor);
	wl_signal_add(&seat->events.request_set_selection, &request_set_sel);
	wl_signal_add(&seat->events.request_set_primary_selection,
				  &request_set_psel);
	wl_signal_add(&seat->events.request_start_drag, &request_start_drag);
	wl_signal_add(&seat->events.start_drag, &start_drag);

	kb_group = createkeyboardgroup();
	last_active_keyboard = kb_group->keyboard;
	wl_list_init(&kb_group->destroy.link);

	keyboard_shortcuts_inhibit = wlr_keyboard_shortcuts_inhibit_v1_create(dpy);
	wl_signal_add(&keyboard_shortcuts_inhibit->events.new_inhibitor,
				  &keyboard_shortcuts_inhibit_new_inhibitor);

	output_mgr = wlr_output_manager_v1_create(dpy);
	wl_signal_add(&output_mgr->events.apply, &output_mgr_apply);
	wl_signal_add(&output_mgr->events.test, &output_mgr_test);

	/* create text_input-, and input_method-protocol relevant globals */
	input_method_manager = wlr_input_method_manager_v2_create(dpy);
	text_input_manager = wlr_text_input_manager_v3_create(dpy);

	mango_input_method_relay = mango_im_relay_create();

	drm_lease_manager = wlr_drm_lease_v1_manager_create(dpy, backend);
	if (drm_lease_manager) {
		wl_signal_add(&drm_lease_manager->events.request, &drm_lease_request);
	} else {
		mango_error(true, WLR_DEBUG,
					"Failed to create wlr_drm_lease_device_v1.");
		mango_error(true, WLR_INFO, "VR will not be available.");
	}

	// 创建顶层管理句柄
	foreign_toplevel_manager = wlr_foreign_toplevel_manager_v1_create(dpy);
	struct wlr_xdg_foreign_registry *foreign_registry =
		wlr_xdg_foreign_registry_create(dpy);
	wlr_xdg_foreign_v1_create(dpy, foreign_registry);
	wlr_xdg_foreign_v2_create(dpy, foreign_registry);

	// ext-workspace协议
	workspaces_init();
#ifdef XWAYLAND
	/*
	 * Initialise the XWayland X server.
	 * It will be started when the first X client is started.
	 */
	xwayland =
		wlr_xwayland_create(dpy, compositor, !config.xwayland_persistence);
	if (xwayland) {
		wl_signal_add(&xwayland->events.ready, &xwayland_ready);
		wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);

		setenv("DISPLAY", xwayland->display_name, 1);
	} else {
		mango_error(
			true, WLR_ERROR,
			"failed to setup XWayland X server, continuing without it\n");
	}
	sync_keymap = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
										  synckeymap, NULL);
#endif
}

int32_t main(int32_t argc, char *argv[]) {
	char *startup_cmd = NULL;
	int32_t c;
	int readiness_fd = 0;

	while ((c = getopt(argc, argv, "s:c:r:hdvp")) != -1) {
		if (c == 's') {
			startup_cmd = optarg;
		} else if (c == 'd') {
			cli_debug_log = true;
		} else if (c == 'v') {
			printf("mango " VERSION "\n");
			return EXIT_SUCCESS;
		} else if (c == 'c') {
			snprintf(cli_config_path, sizeof(cli_config_path), "%s", optarg);
		} else if (c == 'p') {
			return parse_config() ? EXIT_SUCCESS : EXIT_FAILURE;
		} else if (c == 'r') {
			readiness_fd = atoi(optarg);
			if (readiness_fd < 3) {
				goto usage;
			}
		} else {
			goto usage;
		}
	}
	if (optind < argc)
		goto usage;

	/* Wayland requires XDG_RUNTIME_DIR for creating its communications
	 * socket
	 */
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");
	setup();
	run(startup_cmd, readiness_fd);
	cleanup();
	return EXIT_SUCCESS;
usage:
	printf("Usage: mango [OPTIONS]\n"
		   "\n"
		   "Options:\n"
		   "  -v             Show mango version\n"
		   "  -d             Enable debug log\n"
		   "  -c <file>      Use custom configuration file\n"
		   "  -s <command>   Execute startup command\n"
		   "  -r <fdnum>     When WM is ready, write '\\n' to the given file "
		   "descriptor and close it. fdnum >= 3\n"
		   "  -p             Check configuration file error\n");
	return EXIT_SUCCESS;
}
