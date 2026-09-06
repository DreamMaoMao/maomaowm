#ifndef __MANAGE_MONITOR_H__
#define __MANAGE_MONITOR_H__ 1

#include "mango/mango.h"
#include "mango/config/parse_config.h"

bool is_special_active(const Monitor *m);
uint32_t get_mon_curtag(const Monitor *m);
bool special_has_clients(const Monitor *m);
uint32_t get_monitor_active_tagset(const Monitor *m);
Monitor *monitor_from_direction(enum wlr_direction dir);
bool is_scroller_layout(Monitor *m);
bool is_monocle_layout(Monitor *m);
bool is_centertile_layout(Monitor *m);
void special_update_dim(Monitor *m);
uint32_t get_tag_status(uint32_t tag, Monitor *m);
uint32_t get_tags_first_tag_num(uint32_t source_tags);
uint32_t get_tags_first_tag(uint32_t source_tags);
Monitor *monitor_at_point(double x, double y);
Monitor *get_monitor_nearest_to(int32_t lx, int32_t ly);
bool match_monitor_spec(char *spec, Monitor *m);
bool mango_output_commit(Monitor *m);
void enable_adaptive_sync(Monitor *m, struct wlr_output_state *state);
void disable_adaptive_sync(Monitor *m, struct wlr_output_state *state);
bool monitor_matches_rule(Monitor *m, const ConfigMonitorRule *rule);
struct wlr_color_transform *monitor_load_icc_transform(const char *path);
void monitor_set_icc(Monitor *m, const char *path);
void handle_new_output(struct wl_listener *listener, void *data);
void handle_output_destroy(struct wl_listener *listener, void *data);
void monitor_close(Monitor *m);
void handle_output_request_state(struct wl_listener *listener, void *data);
void create_output(struct wlr_backend *b, void *data);
void handle_output_layout_change(struct wl_listener *listener, void *data);
void handle_output_manager_apply(struct wl_listener *listener, void *data);
void output_manager_apply_or_test(struct wlr_output_configuration_v1 *config,
								  int32_t test);
void handle_output_manager_test(struct wl_listener *listener, void *data);
void handle_output_power_manager_set_mode(struct wl_listener *listener,
										  void *data);
void monitor_stop_skip_frame_timer(Monitor *m);
int monitor_skip_frame_timeout_callback(void *data);
void monitor_check_skip_frame_timeout(Monitor *m);
void handle_output_frame(struct wl_listener *listener, void *data);
void check_vrr_enable(Client *c);
void handle_renderer_lost(struct wl_listener *listener, void *data);
void setgaps(int32_t oh, int32_t ov, int32_t ih, int32_t iv);
bool mango_scene_output_commit(struct wlr_scene_output *scene_output,
							   struct wlr_output_state *state);
struct wlr_output_mode *get_nearest_output_mode(struct wlr_output *output,
												int32_t width, int32_t height,
												float refresh);
bool apply_rule_to_state(Monitor *m, const ConfigMonitorRule *rule,
						 struct wlr_output_state *state);

#endif
