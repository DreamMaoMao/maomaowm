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

/*
 * Clips the XWayland root surface via source_box.
 *
 * The X11 buffer is physical (apps render 1:1) while clip is mango logical
 * visibility. wlr_scene_subsurface_tree_set_clip would treat clip as surface
 * logical coordinates (XWayland state width/height is actually physical) and
 * scale up the content. Instead clip xwl_root_buffer directly with source_box +
 * dest_size: source_box uses physical coordinates (logical clip *
 * xwayland_scale) to sample 1:1; dest_size uses logical coordinates so the
 * display stays logically scaled; the buffer node is moved to (clip.x, clip.y)
 * so the visible content shifts right when the window overflows to the left
 * instead of overflowing off-screen.
 */
void client_update_xwayland_clip(Client *c, struct wlr_box *clip);
/* Syncs the dest_size (logical size) of the XWayland root surface. */
void client_update_xwayland_dest_size(Client *c);
uint32_t client_set_size(Client *c, uint32_t width, uint32_t height);
void client_set_minimized(Client *c, bool minimize_window);
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
Client *client_find_terminal(Client *w);
Client *get_client_by_id_or_title(const char *arg_id, const char *arg_title);

struct wlr_box // Computes the centered coordinates of a client.
client_center_geometry(Client *c, Monitor *tm, struct wlr_box geom,
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
Client *client_focus_top(Monitor *m);
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
void client_apply_rules(Client *c);
void apply_window_snap(Client *c);
/*
 * Client management: window lifecycle, rules, focus, tiled/floating/fullscreen
 * state switching, and XWayland client handling.
 */
void client_update_geometry(Client *c);
void client_init_xwayland(Client *c);
bool client_init_unmanaged(Client *c);
void client_apply_xwayland(Client *c);
bool xwayland_scene_buffer_point_accepts_input(struct wlr_scene_buffer *buffer,
											   double *sx, double *sy);
void handle_new_xdg_toplevel(struct wl_listener *listener, void *data);
void init_client_properties(Client *c);
void handle_client_map(struct wl_listener *listener, void *data);
void handle_client_commit(struct wl_listener *listener, void *data);
void handle_client_unmap(struct wl_listener *listener, void *data);
void handle_client_destroy(struct wl_listener *listener, void *data);
void handle_client_request_fullscreen(struct wl_listener *listener, void *data);
void handle_client_request_maximize(struct wl_listener *listener, void *data);
void handle_client_request_minimize(struct wl_listener *listener, void *data);
void handle_client_set_title(struct wl_listener *listener, void *data);
void handle_client_activation_request(struct wl_listener *listener, void *data);
void pending_kill_client(Client *c);
void iter_xdg_scene_buffers(struct wlr_scene_buffer *buffer, int32_t sx,
							int32_t sy, void *user_data);
void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int32_t sx,
								int32_t sy, void *data);
void client_set_opacity(Client *c, double opacity);
void client_focus(Client *c, int32_t lift);
void client_active(Client *c);
void client_view_on_monitor(const Arg *arg, bool want_animation, Monitor *m,
							bool changefocus);
void client_switch_view(const Arg *arg, bool want_animation);
void tag_client(const Arg *arg, Client *target_client);
void show_hide_client(Client *c);
void client_set_monitor(Client *c, Monitor *m, uint32_t newtags, bool focus);
void client_change_mon(Client *c, Monitor *m);
void view_insert_shift_tags(Monitor *m, uint32_t target);
void client_set_floating(Client *c, int32_t floating);
void client_apply_fullscreen(Client *c, int32_t fullscreen, bool rearrange);
void client_set_fake_fullscreen(Client *c, int32_t fakefullscreen);
void client_set_maximize_screen(Client *c, int32_t maximizescreen,
								bool rearrange);
void reset_maximizescreen_size(Client *c);
void set_minimized(Client *c);
void unminimize(Client *c);
void exit_scroller_stack(Client *c);
void clear_fullscreen_and_maximized_state(Monitor *m);

/* Clears the fullscreen flag and restores the border zeroed at fullscreen. */
void clear_fullscreen_flag(Client *c);
void client_pending_fullscreen_state(Client *c, int32_t isfullscreen);
void client_pending_maximized_state(Client *c, int32_t ismaximized);
void client_pending_minimized_state(Client *c, int32_t isminimized);
void show_scratchpad(Client *c);
bool switch_scratchpad_client_state(Client *c);
void apply_named_scratchpad(Client *target_client);
void client_update_border_color(Client *c);
void client_exchange(Client *c1, Client *c2);
void client_replace(Client *c, Client *w, bool is_group_change_member,
					bool is_swallow);
void client_update_oldmonname_record(Client *c, Monitor *m);
void client_apply_bounds(Client *c, struct wlr_box *bbox);
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
// Feeds frame callbacks to all surfaces (including subsurfaces) of hidden
// windows so clients keep rendering in overview previews (stops frame callback
// throttling). wlr_scene_node_for_each_buffer cannot walk the original
// scene_surface tree: after snapshotting it is disabled, and scenefx skips
// disabled nodes (wlr_scene.c scene_node_for_each_scene_buffer), so no surface
// gets fed and ordinary windows would stall without frame callbacks.
void client_send_frame_done(Client *c, const struct timespec *now);
bool client_force_render(Client *c);

/* Returns the monitor of the current XWayland client (falls back to the
 * selected monitor when not bound yet). */
#ifdef XWAYLAND

/* X11 coordinate scale relative to logical coordinates: monitor scale with fzs,
 * otherwise 1. */
Monitor *xwayland_monitor(Client *c);

/* Tells X11 clients at which resolution to render. */
float xwayland_client_scale(Client *c);

/* Tells X11 clients at which resolution to render. */
float xwayland_preferred_scale(Client *c);

/* Updates the XWayland scale and notifies the client. */
void xwayland_apply_scale(Client *c);

/* Wayland logical coordinates -> X11 physical size (X11 = logical * scale). */
void xwayland_logical_to_x11(struct wlr_box *box, float scale);

/* X11 physical size -> Wayland logical coordinates (logical = X11 / scale). */
void xwayland_x11_to_logical(struct wlr_box *box, float scale);
void fix_xwayland_coordinate(struct wlr_box *geom);
void handle_xwayland_surface_request_activate(struct wl_listener *listener,
											  void *data);
void handle_xwayland_surface_request_configure(struct wl_listener *listener,
											   void *data);
void handle_new_xwayland_surface(struct wl_listener *listener, void *data);
void handle_xwayland_surface_commit(struct wl_listener *listener, void *data);
void handle_xwayland_surface_associate(struct wl_listener *listener,
									   void *data);
void handle_xwayland_surface_dissociate(struct wl_listener *listener,
										void *data);
void handle_xwayland_surface_set_hints(struct wl_listener *listener,
									   void *data);
void handle_xwayland_ready(struct wl_listener *listener, void *data);
void handle_xwayland_surface_set_geometry(struct wl_listener *listener,
										  void *data);

#endif

#endif
