#ifndef __MANAGE_MONITOR_H__
#define __MANAGE_MONITOR_H__ 1

#include "mango/common/types.h"
#include "mango/config/parse_config.h"
#include "mango/config/preset.h"
#include <limits.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/box.h>

#define INSIDEMON(A)                                                           \
	(A->geom.x >= A->mon->m.x && A->geom.y >= A->mon->m.y &&                   \
	 A->geom.x + A->geom.width <= A->mon->m.x + A->mon->m.width &&             \
	 A->geom.y + A->geom.height <= A->mon->m.y + A->mon->m.height)
#define GEOMINSIDEMON(A, M)                                                    \
	(A->x >= M->m.x && A->y >= M->m.y &&                                       \
	 A->x + A->width <= M->m.x + M->m.width &&                                 \
	 A->y + A->height <= M->m.y + M->m.height)

#ifndef PERTAG_SLOTS
#define PERTAG_SLOTS (tag_num_MAX + 1)
#endif

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
	struct wlr_scene_optimized_blur *blur;
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
