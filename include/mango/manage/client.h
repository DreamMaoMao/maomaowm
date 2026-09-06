#ifndef __MANAGE_CLIENT_H__
#define __MANAGE_CLIENT_H__ 1

#include "mango/config/parse_config.h"

void client_update_geometry(Client *c);
void client_init_xwayland(Client *c);
bool client_init_unmanaged(Client *c);
void client_apply_xwayland(Client *c);
void apply_rule_properties(Client *c, const ConfigWinRule *r);
bool is_window_rule_matches(const ConfigWinRule *r, const char *appid,
							const char *title);
void client_swap_layout_properties(Client *c1, Client *c2);
void client_swap_monitors_and_tags(Client *c1, Client *c2);
void finish_exchange_arrange_and_focus(Client *c1, Client *c2, Monitor *m1,
									   Monitor *m2);
#ifdef XWAYLAND
bool xwayland_scene_buffer_point_accepts_input(struct wlr_scene_buffer *buffer,
											   double *sx, double *sy);
void xwayland_apply_scale(Client *c);
void xwayland_logical_to_x11(struct wlr_box *box, float scale);
void xwayland_x11_to_logical(struct wlr_box *box, float scale);
void fix_xwayland_coordinate(struct wlr_box *geom);
Monitor *xwayland_monitor(Client *c);
#endif

int32_t client_is_x11(Client *c);
struct wlr_surface *client_surface(Client *c);
int32_t toplevel_from_wlr_surface(struct wlr_surface *s, Client **pc,
								  LayerSurface **pl);

/* The others */
void client_activate_surface(struct wlr_surface *s, int32_t activated);
const char *client_get_appid(Client *c);
uint32_t get_client_tag_idx(const Client *c);
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

/* XWayland 根 surface 的 source_box 裁剪。
 *
 * X11 buffer 是物理尺寸（应用按 1:1 渲染），而 clip 是 mango 的逻辑可见
 * 区域。wlr_scene_subsurface_tree_set_clip 会把 clip 当作 surface 逻辑坐标
 * （XWayland 的 state->width/height 实为物理）去裁剪并重设 dest_size，导致
 * 内容被放大。这里改用 source_box + dest_size 直接裁剪 xwl_root_buffer：
 *   - source_box 用物理坐标（clip 逻辑 × xwayland_scale），保持 1:1 采样；
 *   - dest_size 用逻辑坐标（clip 逻辑尺寸），保持逻辑缩放显示；
 *   - buffer 节点平移到 (clip.x, clip.y)，让可见内容保持在原屏幕位置
 *     （否则窗口向左溢出时，裁剩的内容不会右移，仍会溢出到屏幕外）。 */
void client_update_xwayland_clip(Client *c, struct wlr_box *clip);
/* 同步 XWayland 根 surface 的 dest_size（逻辑尺寸） */
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
bool check_hit_no_border(Client *c);
Client *termforwin(Client *w);
Client *get_client_by_id_or_title(const char *arg_id, const char *arg_title);

struct wlr_box // 计算客户端居中坐标
setclient_coordinate_center(Client *c, Monitor *tm, struct wlr_box geom,
							int32_t offsetx, int32_t offsety);
/* Helper: Check if rule matches client */
bool is_window_rule_matches(const ConfigWinRule *r, const char *appid,
							const char *title);
Client *center_tiled_select(Monitor *m);
Client *find_client_by_direction(Client *tc, const Arg *arg, bool findfloating);
Client *direction_select(const Arg *arg);
/* We probably should change the name of this, it sounds like
 * will focus the topmost client of this mon, when actually will
 * only return that client */
Client *focustop(Monitor *m);
Client *get_next_stack_client(Client *c, bool reverse);
float *get_border_color(Client *c);

int32_t is_single_bit_set(uint32_t x);
bool client_only_in_one_tag(Client *c);
bool client_is_in_same_stack(Client *sc, Client *tc, Client *fc);
Client *get_focused_stack_client(Client *sc, Client *custom_focus_client);
void apply_rule_properties(Client *c, const ConfigWinRule *r);
void set_float_malposition(Client *tc);
void client_reset_mon_tags(Client *c, Monitor *mon, uint32_t newtags);
void check_match_tag_floating_rule(Client *c, Monitor *mon);
void applyrules(Client *c);
void apply_window_snap(Client *c);
/*
 * Client 管理：窗口生命周期、规则、聚焦、平铺/浮动/全屏状态切换，
 * 以及 XWayland 客户端相关处理。
 */
void client_update_geometry(Client *c);
void client_init_xwayland(Client *c);
bool client_init_unmanaged(Client *c);
void client_apply_xwayland(Client *c);
bool xwayland_scene_buffer_point_accepts_input(struct wlr_scene_buffer *buffer,
											   double *sx, double *sy);
void createnotify(struct wl_listener *listener, void *data);
void init_client_properties(Client *c);
void mapnotify(struct wl_listener *listener, void *data);
void commitnotify(struct wl_listener *listener, void *data);
void unmapnotify(struct wl_listener *listener, void *data);
void destroynotify(struct wl_listener *listener, void *data);
void fullscreennotify(struct wl_listener *listener, void *data);
void maximizenotify(struct wl_listener *listener, void *data);
void minimizenotify(struct wl_listener *listener, void *data);
void updatetitle(struct wl_listener *listener, void *data);
void urgent(struct wl_listener *listener, void *data);
void pending_kill_client(Client *c);
void iter_xdg_scene_buffers(struct wlr_scene_buffer *buffer, int32_t sx,
							int32_t sy, void *user_data);
void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int32_t sx,
								int32_t sy, void *data);
void client_set_opacity(Client *c, double opacity);
void focusclient(Client *c, int32_t lift);
void client_active(Client *c);
void view_in_mon(const Arg *arg, bool want_animation, Monitor *m,
				 bool changefocus);
void view(const Arg *arg, bool want_animation);
void tag_client(const Arg *arg, Client *target_client);
void show_hide_client(Client *c);
void setmon(Client *c, Monitor *m, uint32_t newtags, bool focus);
void client_change_mon(Client *c, Monitor *m);
void view_insert_shift_tags(Monitor *m, uint32_t target);
void setfloating(Client *c, int32_t floating);
void setfullscreen(Client *c, int32_t fullscreen, bool rearrange);
void setfakefullscreen(Client *c, int32_t fakefullscreen);
void setmaximizescreen(Client *c, int32_t maximizescreen, bool rearrange);
void reset_maximizescreen_size(Client *c);
void set_minimized(Client *c);
void unminimize(Client *c);
void exit_scroller_stack(Client *c);
void clear_fullscreen_and_maximized_state(Monitor *m);

/*清除全屏标志,还原全屏时清0的border*/
void clear_fullscreen_flag(Client *c);
void client_pending_fullscreen_state(Client *c, int32_t isfullscreen);
void client_pending_maximized_state(Client *c, int32_t ismaximized);
void client_pending_minimized_state(Client *c, int32_t isminimized);
void show_scratchpad(Client *c);
bool switch_scratchpad_client_state(Client *c);
void apply_named_scratchpad(Client *target_client);
void setborder_color(Client *c);
void exchange_two_client(Client *c1, Client *c2);
void client_replace(Client *c, Client *w, bool is_group_change_member,
					bool is_swallow);
void client_update_oldmonname_record(Client *c, Monitor *m);
void applybounds(Client *c, struct wlr_box *bbox);
void client_swap_layout_properties(Client *c1, Client *c2);
void client_swap_monitors_and_tags(Client *c1, Client *c2);
void finish_exchange_arrange_and_focus(Client *c1, Client *c2, Monitor *m1,
									   Monitor *m2);
void client_tile_resize(Client *c, struct wlr_box geo, int32_t interact);
uint32_t generate_client_id(void);
void client_pending_force_kill(Client *c);
void client_add_jump_label_node(Client *c);
uint32_t client_target_layer(Client *c);
void client_sync_layer(Client *c);
void client_add_group_bar(Client *c);
void client_focus_group_member(Client *c);
void client_check_tab_node_visible(Client *c);
void client_raise_group(Client *c);
void client_reparent_group(Client *c);
void client_handle_decorate_click(MangoGroupBar *gb);
void client_set_group_mon(Client *c, Monitor *m);
void client_set_group_config(Client *c);
void client_group_detach(Client *c);
void client_group_replace(Client *old, Client *new);
void mango_surface_frame_done(struct wlr_surface *surface, int sx, int sy,
							  void *data);
// 给被隐藏窗口的所有 surface（含 subsurface）喂 frame callback，
// 让客户端在 overview 预览中继续渲染（解除帧回调节流导致的停画）。
// 不能用 wlr_scene_node_for_each_buffer 遍历原 scene_surface 树：
// 该树在拍完快照后被 disabled，scenefx 的 for_each_buffer 会直接跳过
// disabled 节点（wlr_scene.c scene_node_for_each_scene_buffer），导致
// 一个 surface 都喂不到——普通窗口就会因收不到 frame callback 而停画。
void client_send_frame_done(Client *c, const struct timespec *now);
bool client_force_render(Client *c);

extern uint32_t next_client_id;

/* 获取当前 XWayland 客户端的 monitor（尚未绑定 monitor 时回退到 selmon） */
#ifdef XWAYLAND

/* X11 坐标相对逻辑坐标的缩放：fzs 时为 monitor scale，否则为 1 */
Monitor *xwayland_monitor(Client *c);

/* 提示 X11 客户端按何分辨率渲染 */
float xwayland_client_scale(Client *c);

/* 提示 X11 客户端按何分辨率渲染 */
float xwayland_preferred_scale(Client *c);

/* 更新 XWayland 缩放并通知客户端 */
void xwayland_apply_scale(Client *c);

/* wayland 逻辑坐标 -> X11 物理尺寸（X11 = 逻辑 * scale） */
void xwayland_logical_to_x11(struct wlr_box *box, float scale);

/* X11 物理尺寸 -> wayland 逻辑坐标（逻辑 = X11 / scale） */
void xwayland_x11_to_logical(struct wlr_box *box, float scale);
void fix_xwayland_coordinate(struct wlr_box *geom);
void activatex11(struct wl_listener *listener, void *data);
void configurex11(struct wl_listener *listener, void *data);
void createnotifyx11(struct wl_listener *listener, void *data);
void commitx11(struct wl_listener *listener, void *data);
void associatex11(struct wl_listener *listener, void *data);
void dissociatex11(struct wl_listener *listener, void *data);
void sethints(struct wl_listener *listener, void *data);
void xwaylandready(struct wl_listener *listener, void *data);
void setgeometrynotify(struct wl_listener *listener, void *data);

#endif

#endif
