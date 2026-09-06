#include "mango/manage/monitor.h"
#include "mango/manage/misc.h"
#include "mango/manage/layer.h"
#include "mango/manage/client.h"
#include "mango/animation/client.h"
#include "mango/layout/layout.h"
#include "mango/layout/arrange.h"
#include "mango/layout/dwindle.h"
#include "mango/layout/scroll.h"
#include "mango/common/util.h"
#include "mango/common/log.h"
#include "mango/common/globals.h"
#include "mango/ext-protocol/text-input.h"
#include "mango/ext-protocol/tearing.h"
#include "mango/ext-protocol/ext-workspace.h"
#include "mango/ext-protocol/hdr.h"
#include "mango/ext-protocol/foreign-toplevel.h"
#include "mango/ext-protocol/xdg-output.h"
#include "mango/ipc/ipc.h"
#include "mango/animation/common.h"
#include "mango/dispatch/bind.h"
#include "mango/animation/layer.h"
#include "mango/mango.h"

bool is_special_active(const Monitor *m) {
	return m && !m->isoverview && (m->tagset[m->seltags] & TAG0_MASK);
}

uint32_t get_mon_curtag(const Monitor *m) {
	if (!m || !m->pertag)
		return 0;
	// special workspace uses slot 0; all-tags view (curtag==0) uses its own
	// slot
	if (is_special_active(m))
		return 0;
	return m->pertag->curtag ? m->pertag->curtag : PERTAG_ALL_TAGS_IDX;
}

// true if m still has a live (not minimized/destroyed) special window
bool special_has_clients(const Monitor *m) {
	Client *c;
	if (!m)
		return false;
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m && !c->iskilling && !c->isminimized &&
			(c->tags & TAG0_MASK)) {
			return true;
		}
	}
	return false;
}

uint32_t get_monitor_active_tagset(const Monitor *m) {
	if (!m)
		return 0;
	if (is_special_active(m) && m->pertag) {
		uint32_t prev_set = m->tagset[m->seltags ^ 1] & TAGMASK;
		return prev_set ? prev_set
						: (m->pertag->prevtag ? (1U << (m->pertag->prevtag - 1))
											  : 1);
	}
	return m->tagset[m->seltags];
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

bool is_scroller_layout(Monitor *m) {
	if (m->pertag->ltidxs[get_mon_curtag(m)]->id == SCROLLER)
		return true;

	if (m->pertag->ltidxs[get_mon_curtag(m)]->id == VERTICAL_SCROLLER)
		return true;

	return false;
}

bool is_monocle_layout(Monitor *m) {
	if (m->pertag->ltidxs[get_mon_curtag(m)]->id == MONOCLE)
		return true;

	return false;
}

bool is_centertile_layout(Monitor *m) {
	if (m->pertag->ltidxs[get_mon_curtag(m)]->id == CENTER_TILE)
		return true;

	return false;
}

// sync the special overlay dim layer to the monitor and view state
void special_update_dim(Monitor *m) {
	if (!m || !m->special_dim_rect)
		return;
	wlr_scene_rect_set_size(m->special_dim_rect, m->m.width, m->m.height);
	wlr_scene_node_set_position(&m->special_dim_rect->node, m->m.x, m->m.y);
	if (is_special_active(m)) {
		wlr_scene_rect_set_color(
			m->special_dim_rect,
			(float[4]){0.0f, 0.0f, 0.0f, config.special_dim});
		wlr_scene_node_set_enabled(&m->special_dim_rect->node, true);
	} else {
		wlr_scene_node_set_enabled(&m->special_dim_rect->node, false);
	}
}

uint32_t get_tag_status(uint32_t tag, Monitor *m) {
	Client *c = NULL;
	uint32_t status = 0;
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m && !c->is_logic_hide &&
			c->tags & 1 << (tag - 1) & TAGMASK) {
			if (c->isurgent) {
				status = 2;
				break;
			}
			status = 1;
		}
	}
	return status;
}

uint32_t get_tags_first_tag_num(uint32_t source_tags) {
	uint32_t i, tag;
	tag = 0;

	if (!source_tags) {
		return 0;
	}

	if (source_tags & TAG0_MASK) {
		return 0;
	}

	for (i = 0; !(tag & 1) && source_tags != 0 && i < (uint32_t)config.tag_num;
		 i++) {
		tag = source_tags >> i;
	}

	if (i == 1) {
		return 1;
	} else if (i >= (uint32_t)config.tag_num) {
		return config.tag_num;
	} else {
		return i;
	}
}

// 获取tags中最前面的tag的tagmask
uint32_t get_tags_first_tag(uint32_t source_tags) {
	uint32_t i, tag;
	tag = 0;

	if (!source_tags) {
		return is_special_active(selmon)
				   ? TAG0_MASK
				   : (selmon ? (1 << (selmon->pertag->curtag
										  ? selmon->pertag->curtag - 1
										  : 0))
							 : 1);
	}

	if (source_tags & TAG0_MASK) {
		return TAG0_MASK;
	}

	for (i = 0; !(tag & 1) && source_tags != 0 && i < (uint32_t)config.tag_num;
		 i++) {
		tag = source_tags >> i;
	}

	if (i == 1) {
		return 1;
	} else if (i >= (uint32_t)config.tag_num) {
		return 1 << (config.tag_num - 1);
	} else {
		return 1 << (i - 1);
	}
}

Monitor *xytomon(double x, double y) {
	struct wlr_output *o = wlr_output_layout_output_at(output_layout, x, y);
	return o ? o->data : NULL;
}

Monitor *get_monitor_nearest_to(int32_t lx, int32_t ly) {
	double closest_x, closest_y;
	wlr_output_layout_closest_point(output_layout, NULL, lx, ly, &closest_x,
									&closest_y);

	return output_from_wlr_output(
		wlr_output_layout_output_at(output_layout, closest_x, closest_y));
}

bool match_monitor_spec(char *spec, Monitor *m) {
	if (!spec || !m)
		return false;

	// if the spec does not contain a colon, treat it as a match on the monitor
	// name
	if (strchr(spec, ':') == NULL) {
		return regex_match(spec, m->wlr_output->name);
	}

	char *spec_copy = strdup(spec);
	if (!spec_copy)
		return false;

	char *name_rule = NULL;
	char *make_rule = NULL;
	char *model_rule = NULL;
	char *serial_rule = NULL;

	char *token = strtok(spec_copy, "&&");
	while (token) {
		char *colon = strchr(token, ':');
		if (colon) {
			*colon = '\0';
			char *key = token;
			char *value = colon + 1;

			if (strcmp(key, "name") == 0)
				name_rule = strdup(value);
			else if (strcmp(key, "make") == 0)
				make_rule = strdup(value);
			else if (strcmp(key, "model") == 0)
				model_rule = strdup(value);
			else if (strcmp(key, "serial") == 0)
				serial_rule = strdup(value);
		}
		token = strtok(NULL, "&&");
	}

	bool match = true;

	if (name_rule) {
		if (!regex_match(name_rule, m->wlr_output->name))
			match = false;
	}
	if (make_rule) {
		if (!m->wlr_output->make || strcmp(make_rule, m->wlr_output->make) != 0)
			match = false;
	}
	if (model_rule) {
		if (!m->wlr_output->model ||
			strcmp(model_rule, m->wlr_output->model) != 0)
			match = false;
	}
	if (serial_rule) {
		if (!m->wlr_output->serial ||
			strcmp(serial_rule, m->wlr_output->serial) != 0)
			match = false;
	}

	free(spec_copy);
	free(name_rule);
	free(make_rule);
	free(model_rule);
	free(serial_rule);

	return match;
}

bool mango_scene_output_commit(struct wlr_scene_output *scene_output,
							   struct wlr_output_state *state) {
	struct wlr_output *wlr_output = scene_output->output;
	Monitor *m = wlr_output->data;
	bool committed = false;

	bool frame_allow_tearing = check_tearing_frame_allow(m);

	/* mode/scale/transform 等输出状态变化时必须提交，即使场景无帧
	 * （needs_frame 只反映场景 damage），否则 wl_output 事件不会发送 */
	bool state_changed =
		state->committed &
		(WLR_OUTPUT_STATE_MODE | WLR_OUTPUT_STATE_SCALE |
		 WLR_OUTPUT_STATE_TRANSFORM | WLR_OUTPUT_STATE_ENABLED |
		 WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED);
	if (!state_changed && !wlr_scene_output_needs_frame(scene_output))
		return true;

	// build the state, attaching the scene's Buffer to it
	bool has_img_desc =
		(state->committed & WLR_OUTPUT_STATE_IMAGE_DESCRIPTION) ||
		scene_output->output->image_description != NULL;
	struct wlr_scene_output_state_options opts = {0};
	if (m->icc_transform && !has_img_desc)
		opts.color_transform = m->icc_transform;
	if (!wlr_scene_output_build_state(scene_output, state, &opts))
		return false;

	if (frame_allow_tearing) {
		state->tearing_page_flip = true;
	} else {
		state->tearing_page_flip = false;
	}

	// test whether tearing is supported
	if (state->tearing_page_flip == true) {
		if (!wlr_output_test_state(wlr_output, state)) {
			// if DRM rejects (e.g. the current output/driver doesn't support
			// tearing), fall back to disabling tearing
			state->tearing_page_flip = false;
		}
	}

	// commit state
	committed = wlr_output_commit_state(wlr_output, state);
	if (!committed && state->tearing_page_flip) {
		// retry once
		state->tearing_page_flip = false;
		committed = wlr_output_commit_state(wlr_output, state);
	}

	if (committed) {
		if (state == &m->pending) {
			wlr_output_state_finish(&m->pending);
			wlr_output_state_init(&m->pending);
		}
	} else {
		mango_error(true, WLR_INFO, "Failed to commit output %s",
					m->wlr_output->name);
		return false;
	}

	return committed;
}

bool mango_output_commit(Monitor *m) {
	bool committed = wlr_output_commit_state(m->wlr_output, &m->pending);
	if (committed) {
		wlr_output_state_finish(&m->pending);
		wlr_output_state_init(&m->pending);
	} else {
		mango_error(true, WLR_ERROR, "Failed to commit frame");
	}
	return committed;
}

struct wlr_output_mode *get_nearest_output_mode(struct wlr_output *output,
												int32_t width, int32_t height,
												float refresh) {
	struct wlr_output_mode *mode, *nearest_mode = NULL;
	float min_diff = 99999.0f;

	wl_list_for_each(mode, &output->modes, link) {
		if (mode->width == width && mode->height == height) {
			float mode_refresh = mode->refresh / 1000.0f;
			float diff = fabsf(mode_refresh - refresh);

			if (diff < min_diff) {
				min_diff = diff;
				nearest_mode = mode;
			}
		}
	}

	return nearest_mode;
}

void enable_adaptive_sync(Monitor *m, struct wlr_output_state *state) {
	wlr_output_state_set_adaptive_sync_enabled(state, true);
	if (!wlr_output_test_state(m->wlr_output, state)) {
		wlr_output_state_set_adaptive_sync_enabled(state, false);
		mango_error(true, WLR_DEBUG,
					"failed to enable adaptive sync for output %s",
					m->wlr_output->name);
	} else {
		m->is_vrr_enabling = true;
		mango_error(true, WLR_INFO, "adaptive sync enabled for output %s",
					m->wlr_output->name);
	}
}

void disable_adaptive_sync(Monitor *m, struct wlr_output_state *state) {
	wlr_output_state_set_adaptive_sync_enabled(state, false);
	m->is_vrr_enabling = false;
}

bool monitor_matches_rule(Monitor *m, const ConfigMonitorRule *rule) {
	if (rule->name != NULL && !regex_match(rule->name, m->wlr_output->name))
		return false;
	if (rule->make != NULL && (m->wlr_output->make == NULL ||
							   strcmp(rule->make, m->wlr_output->make) != 0))
		return false;
	if (rule->model != NULL && (m->wlr_output->model == NULL ||
								strcmp(rule->model, m->wlr_output->model) != 0))
		return false;
	if (rule->serial != NULL &&
		(m->wlr_output->serial == NULL ||
		 strcmp(rule->serial, m->wlr_output->serial) != 0))
		return false;
	return true;
}

struct wlr_color_transform *monitor_load_icc_transform(const char *path) {
	int fd = open(path, O_RDONLY | O_NOCTTY | O_CLOEXEC);
	if (fd == -1) {
		mango_error(true, WLR_ERROR, "Failed to open ICC profile %s", path);
		return NULL;
	}

	struct stat info;
	if (fstat(fd, &info) == -1 || !S_ISREG(info.st_mode) || info.st_size <= 0) {
		close(fd);
		mango_error(true, WLR_ERROR, "Invalid ICC profile file %s", path);
		return NULL;
	}

	size_t size = (size_t)info.st_size;
	void *data = malloc(size);
	if (!data) {
		close(fd);
		return NULL;
	}

	size_t nread = 0;
	while (nread < size) {
		ssize_t r = read(fd, (char *)data + nread, size - nread);
		if ((r == -1 && errno != EINTR) || r == 0) {
			free(data);
			close(fd);
			mango_error(true, WLR_ERROR, "Failed to read ICC profile %s", path);
			return NULL;
		}
		if (r > 0)
			nread += (size_t)r;
	}
	close(fd);

	struct wlr_color_transform *tr =
		wlr_color_transform_init_linear_to_icc(data, size);
	free(data);
	if (!tr)
		mango_error(true, WLR_ERROR, "Failed to parse ICC profile %s", path);
	return tr;
}

/* 加载/更新输出的 ICC 变换 */
void monitor_set_icc(Monitor *m, const char *path) {
	if (!path || !path[0]) {
		wlr_color_transform_unref(m->icc_transform);
		m->icc_transform = NULL;
		m->icc_path[0] = '\0';
		return;
	}
	if (m->icc_path[0] && strcmp(m->icc_path, path) == 0)
		return;

	wlr_color_transform_unref(m->icc_transform);
	m->icc_transform = NULL;
	m->icc_path[0] = '\0';

	struct wlr_color_transform *tr = monitor_load_icc_transform(path);
	if (!tr)
		return;
	m->icc_transform = tr;
	snprintf(m->icc_path, sizeof(m->icc_path), "%s", path);
}

/* 将规则中的显示参数应用到 wlr_output_state 中，返回是否设置了自定义模式 */
bool apply_rule_to_state(Monitor *m, const ConfigMonitorRule *rule,
						 struct wlr_output_state *state) {
	bool mode_set = false;
	m->vrr_global_enable = rule->vrr >= 0 ? rule->vrr : 0;
	m->hdr_enable = rule->hdr >= 0 ? rule->hdr : 0;
	m->prefer_disable = rule->disable >= 0 ? rule->disable : 0;
	m->hdr_min_lum = rule->hdr_min_lum;
	m->hdr_max_lum = rule->hdr_max_lum;
	m->hdr_max_avg_lum = rule->hdr_max_avg_lum;
	m->hdr_force = rule->hdr_force >= 0 ? rule->hdr_force : 0;
	monitor_set_icc(m, rule->icc);

	if (m->hdr_enable && m->icc_transform)
		mango_error(true, WLR_ERROR,
					"ICC profile ignored on output %s: HDR is enabled",
					m->wlr_output->name);

	if (rule->width > 0 && rule->height > 0 && rule->refresh > 0) {
		struct wlr_output_mode *internal_mode = get_nearest_output_mode(
			m->wlr_output, rule->width, rule->height, rule->refresh);
		if (internal_mode) {
			wlr_output_state_set_mode(state, internal_mode);
			mode_set = true;
		} else if (rule->custom || wlr_output_is_headless(m->wlr_output)) {
			wlr_output_state_set_custom_mode(
				state, rule->width, rule->height,
				(int32_t)roundf(rule->refresh * 1000));
			mode_set = true;
		}
	}
	if (m->vrr_global_enable) {
		enable_adaptive_sync(m, state);
	} else {
		disable_adaptive_sync(m, state);
	}
	wlr_output_state_set_scale(state, rule->scale);
	wlr_output_state_set_transform(state, rule->rr);
	return mode_set;
}

void createmon(struct wl_listener *listener, void *data) {
	struct wlr_output *wlr_output = data;
	const ConfigMonitorRule *r;
	uint32_t i;
	int32_t ji;
	Monitor *m = NULL;
	bool custom_monitor_mode = false;

	if (!wlr_output_init_render(wlr_output, alloc, drw))
		return;

	if (wlr_output->non_desktop) {
		if (drm_lease_manager) {
			wlr_drm_lease_v1_manager_offer_output(drm_lease_manager,
												  wlr_output);
		}
		return;
	}

	struct wl_event_loop *loop = wl_display_get_event_loop(dpy);
	m = wlr_output->data = ecalloc(1, sizeof(*m));

	m->iscleanuping = false;
	m->skip_frame_timeout =
		wl_event_loop_add_timer(loop, monitor_skip_frame_timeout_callback, m);
	m->skiping_frame = false;
	m->resizing_count_pending = 0;
	m->resizing_count_current = 0;
	m->carousel_anim_dir = 0;
	m->vrr_global_enable = false;
	m->is_vrr_enabling = false;
	m->hdr_enable = false;
	m->prefer_disable = false;
	m->is_hdr_enabling = false;
	m->hdr_min_lum = 0.0f;
	m->hdr_max_lum = 0.0f;
	m->hdr_max_avg_lum = 0.0f;
	m->hdr_force = false;

	m->wlr_output = wlr_output;
	m->wlr_output->data = m;

	for (i = 0; i < LENGTH(m->layers); i++)
		wl_list_init(&m->layers[i]);

	m->gappih = config.gappih;
	m->gappiv = config.gappiv;
	m->gappoh = config.gappoh;
	m->gappov = config.gappov;
	m->special_gappih = config.special_gappih;
	m->special_gappiv = config.special_gappiv;
	m->special_gappoh = config.special_gappoh;
	m->special_gappov = config.special_gappov;
	m->isoverview = 0;
	m->sel = NULL;
	m->is_in_hotarea = 0;
	m->ov_normal_mode = 0;
	m->ov_tab_layout = 0;
	m->m.x = INT32_MAX;
	m->m.y = INT32_MAX;

	// 临时 pending 状态，用于匹配规则时暂存设置
	struct wlr_output_state pending;
	wlr_output_state_init(&pending);
	float scale = 1;
	enum wl_output_transform rr = WL_OUTPUT_TRANSFORM_NORMAL;
	wlr_output_state_set_scale(&pending, scale);
	wlr_output_state_set_transform(&pending, rr);

	for (ji = 0; ji < config.monitor_rules_count; ji++) {
		r = &config.monitor_rules[ji];

		if (monitor_matches_rule(m, r)) {
			m->m.x = r->x == INT32_MAX ? INT32_MAX : r->x;
			m->m.y = r->y == INT32_MAX ? INT32_MAX : r->y;

			if (apply_rule_to_state(m, r, &pending)) {
				custom_monitor_mode = true;
			}
			break; // 只应用第一个匹配规则
		}
	}

	if (!custom_monitor_mode) {
		struct wlr_output_mode *preferred_mode =
			wlr_output_preferred_mode(wlr_output);
		if (preferred_mode) {
			wlr_output_state_set_mode(&pending, preferred_mode);
		} else {
			struct wlr_output_state custom_test_mode;
			wlr_output_state_init(&custom_test_mode);
			wlr_output_state_set_custom_mode(&custom_test_mode, 1920, 1080,
											 60000);
			if (wlr_output_test_state(wlr_output, &custom_test_mode)) {
				wlr_output_state_set_custom_mode(&pending, 1920, 1080, 60000);
			}
			wlr_output_state_finish(&custom_test_mode);
		}
	}

	// ===================================================
	// 构建最终的输出状态，包含 HDR，并通过 scene 提交
	// ===================================================
	struct wlr_output_state state;
	wlr_output_state_init(&state);

	// 启用/禁用
	if (m->prefer_disable) {
		wlr_output_state_set_enabled(&state, false);
	} else {
		wlr_output_state_set_enabled(&state, true);
	}

	// 模式设置
	if (pending.committed & WLR_OUTPUT_STATE_MODE) {
		if (pending.mode_type == WLR_OUTPUT_STATE_MODE_FIXED) {
			wlr_output_state_set_mode(&state, pending.mode);
		} else if (pending.mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM) {
			wlr_output_state_set_custom_mode(&state, pending.custom_mode.width,
											 pending.custom_mode.height,
											 pending.custom_mode.refresh);
		}
	} else {
		// 兜底,使用首选模式
		struct wlr_output_mode *pref = wlr_output_preferred_mode(wlr_output);
		if (pref)
			wlr_output_state_set_mode(&state, pref);
	}

	// 缩放、变换
	if (pending.committed & WLR_OUTPUT_STATE_SCALE)
		wlr_output_state_set_scale(&state, pending.scale);
	if (pending.committed & WLR_OUTPUT_STATE_TRANSFORM)
		wlr_output_state_set_transform(&state, pending.transform);

	// 自适应同步 (VRR)
	if (pending.committed & WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED)
		wlr_output_state_set_adaptive_sync_enabled(
			&state, pending.adaptive_sync_enabled);

	// HDR 设置
	if (m->hdr_enable) {
		output_state_setup_hdr(m, false, &state);
	} else {
		output_enable_hdr(m, &state, false, false);
	}

	// 创建 scene_output（如果尚未创建）
	m->scene_output = wlr_scene_output_create(scene, wlr_output);

	// 通过 scene 构建最终提交状态（初始化 swapchain）
	bool has_img_desc =
		(state.committed & WLR_OUTPUT_STATE_IMAGE_DESCRIPTION) ||
		wlr_output->image_description != NULL;
	struct wlr_scene_output_state_options opts = {
		.swapchain = NULL, // 让 scene 自动创建
		.color_transform = NULL,
	};
	if (m->icc_transform && !has_img_desc)
		opts.color_transform = m->icc_transform;

	wlr_scene_output_build_state(m->scene_output, &state, &opts);

	wlr_output_commit_state(wlr_output, &state);

	wlr_output_state_finish(&state);
	wlr_output_state_finish(&pending);

	// 加入布局
	struct wlr_output_layout_output *layout_output;
	if (m->m.x == INT32_MAX || m->m.y == INT32_MAX)
		layout_output = wlr_output_layout_add_auto(output_layout, wlr_output);
	else
		layout_output =
			wlr_output_layout_add(output_layout, wlr_output, m->m.x, m->m.y);

	wlr_scene_output_layout_add_output(scene_layout, layout_output,
									   m->scene_output);

	// 获取有效分辨率
	wlr_output_effective_resolution(wlr_output, &m->m.width, &m->m.height);

	// 加入全局 monitor 链表
	wl_list_insert(&mons, &m->link);

	// 初始化 Pertag 等
	m->pertag = calloc(1, sizeof(Pertag));
	for (int i = 0; i < PERTAG_SLOTS; i++)
		m->pertag->scroller_state[i] = NULL;

	if (chvt_backup_tag &&
		regex_match(chvt_backup_selmon, m->wlr_output->name)) {
		m->tagset[0] = m->tagset[1] = (1 << (chvt_backup_tag - 1)) & TAGMASK;
		m->pertag->curtag = m->pertag->prevtag = chvt_backup_tag;
		chvt_backup_tag = 0;
		memset(chvt_backup_selmon, 0, sizeof(chvt_backup_selmon));
	} else {
		m->tagset[0] = m->tagset[1] = 1;
		m->pertag->curtag = m->pertag->prevtag = 1;
	}

	for (i = 0; i <= config.tag_num; i++) {
		m->pertag->nmasters[i] = config.default_nmaster;
		m->pertag->mfacts[i] = config.default_mfact;
		m->pertag->ltidxs[i] = &layouts[0];
	}

	// 应用 tag rule
	parse_tagrule(m);

	m->special_dim_rect =
		wlr_scene_rect_create(layers[LyrSpecialDim], m->m.width, m->m.height,
							  (float[4]){0.0f, 0.0f, 0.0f, config.special_dim});
	special_update_dim(m);

	// ext workspace group
	m->ext_group = wlr_ext_workspace_group_handle_v1_create(
		ext_manager, EXT_WORKSPACE_ENABLE_CAPS);
	wlr_ext_workspace_group_handle_v1_output_enter(m->ext_group, m->wlr_output);

	for (i = 1; i <= config.tag_num; i++) {
		add_workspace_by_tag(i, m);
	}

	updatemons(NULL, NULL);

	// 设置监听器
	LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
	LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
	LISTEN(&wlr_output->events.request_state, &m->request_state,
		   requestmonstate);

	printstatus(IPC_WATCH_ARRANGGE);
}

void cleanupmon(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, destroy);
	LayerSurface *l = NULL, *tmp = NULL;
	uint32_t i;

	m->iscleanuping = true;

	/* m->layers[i] are intentionally not unlinked */
	for (i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmp, &m->layers[i], link)
			wlr_layer_surface_v1_destroy(l->layer_surface);
	}

	// clean ext-workspaces grouplab
	wlr_ext_workspace_group_handle_v1_output_leave(m->ext_group, m->wlr_output);
	wlr_ext_workspace_group_handle_v1_destroy(m->ext_group);
	cleanup_workspaces_by_monitor(m);

	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->frame.link);
	wl_list_remove(&m->link);
	wl_list_remove(&m->request_state.link);
	if (m->lock_surface)
		destroylocksurface(&m->destroy_lock_surface, NULL);
	m->wlr_output->data = NULL;

	wlr_scene_output_destroy(m->scene_output);
	wlr_output_layout_remove(output_layout, m->wlr_output);

	closemon(m);

	if (m->special_dim_rect) {
		wlr_scene_node_destroy(&m->special_dim_rect->node);
		m->special_dim_rect = NULL;
	}

	if (m->skip_frame_timeout) {
		monitor_stop_skip_frame_timer(m);
		wl_event_source_remove(m->skip_frame_timeout);
		m->skip_frame_timeout = NULL;
	}

	m->wlr_output->data = NULL;
	xdg_output_cleanup_output(m->wlr_output);

	cleanup_monitor_dwindle(m);
	cleanup_monitor_scroller(m);

	wlr_color_transform_unref(m->icc_transform);
	m->icc_transform = NULL;
	free(m->pertag);
	free(m);
}

void closemon(Monitor *m) {
	/* update selmon if needed and
	 * move closed monitor's clients to the focused one */
	Client *c = NULL;
	int32_t i = 0, nmons = wl_list_length(&mons);

	if (m->isoverview) {
		toggleoverview(&(Arg){0});
	}

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
				if (c->foreign_toplevel) {
					wlr_foreign_toplevel_handle_v1_output_leave(
						c->foreign_toplevel, c->mon->wlr_output);
					wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
					c->foreign_toplevel = NULL;
				}

				client_set_group_mon(c, NULL);
			} else {
				client_set_group_mon(c, selmon);
			}
			// record the oldmonname which is used to restore
			if (c->oldmonname[0] == '\0') {
				client_update_oldmonname_record(c, m);
			}
		}
	}
	if (selmon) {
		focusclient(focustop(selmon), 1);
		printstatus(IPC_WATCH_ARRANGGE);
	}
}

void requestmonstate(struct wl_listener *listener, void *data) {
	/* This ensures nested backends can be resized */
	Monitor *m = wl_container_of(listener, m, request_state);
	const struct wlr_output_event_request_state *event = data;

	if (event->state->committed == WLR_OUTPUT_STATE_MODE) {

		switch (event->state->mode_type) {
		case WLR_OUTPUT_STATE_MODE_FIXED:
			wlr_output_state_set_mode(&m->pending, event->state->mode);
			break;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			wlr_output_state_set_custom_mode(&m->pending,
											 event->state->custom_mode.width,
											 event->state->custom_mode.height,
											 event->state->custom_mode.refresh);
			break;
		}
		updatemons(NULL, NULL);
		wlr_output_schedule_frame(m->wlr_output);
		return;
	}

	if (!wlr_output_commit_state(m->wlr_output, event->state)) {
		mango_error(false, WLR_ERROR,
					"Backend requested a new state that could not be applied");
	}
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

void updatemons(struct wl_listener *listener, void *data) {
	/*
	 * Called whenever the output layout changes: adding or removing a
	 * monitor, changing an output's mode or position, etc. This is where
	 * the change officially happens and we update geometry, window
	 * positions, focus, and the stored configuration in wlroots'
	 * output-manager implementation.
	 */
	struct wlr_output_configuration_v1 *output_config =
		wlr_output_configuration_v1_create();
	Client *c = NULL;
	struct wlr_output_configuration_head_v1 *config_head;
	Monitor *m = NULL;
	int32_t mon_pos_offsetx, mon_pos_offsety, oldx, oldy;

	/* First remove from the layout the disabled monitors */
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled)
			continue;
		config_head = wlr_output_configuration_head_v1_create(output_config,
															  m->wlr_output);
		config_head->state.enabled = 0;

		if (m->only_sleep) {
			continue;
		}
		/* Remove this output from the layout to avoid cursor enter inside
		 * it */
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
		config_head = wlr_output_configuration_head_v1_create(output_config,
															  m->wlr_output);

		oldx = m->m.x;
		oldy = m->m.y;
		/* Get the effective monitor geometry to use for surfaces */
		wlr_output_layout_get_box(output_layout, m->wlr_output, &m->m);
		m->w = m->m;
		mon_pos_offsetx = m->m.x - oldx;
		mon_pos_offsety = m->m.y - oldy;

		wl_list_for_each(c, &clients, link) {
#ifdef XWAYLAND
			// 显示器 scale 变化时，重新应用 XWayland 缩放并重配窗口
			if (client_is_x11(c) && c->mon == m) {
				xwayland_apply_scale(c);
				if (client_surface(c)->mapped)
					resize(c, c->geom, 0);
			}
#endif
			// floating window position auto adjust the change of monitor
			// position
			if (c->isfloating && c->mon == m) {
				c->geom.x += mon_pos_offsetx;
				c->geom.y += mon_pos_offsety;
				c->float_geom = c->geom;
				if (VISIBLEON(c, m))
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

		special_update_dim(m);

		if (m->lock_surface) {
			struct wlr_scene_tree *scene_tree = m->lock_surface->surface->data;
			wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
			wlr_session_lock_surface_v1_configure(m->lock_surface, m->m.width,
												  m->m.height);
		}

		/* Calculate the effective monitor geometry to use for clients */
		arrangelayers(m);
		/* Don't move clients to the left output when plugging monitors */
		arrange(m, false, false);
		/* make sure fullscreen clients have the right size */
		if ((c = focustop(m)) && c->isfullscreen)
			resize(c, m->m, 0);

		config_head->state.x = m->m.x;
		config_head->state.y = m->m.y;

		if (!selmon)
			selmon = m;
	}

	if (selmon && selmon->wlr_output->enabled) {
		wl_list_for_each(c, &clients, link) {
			if (!c->mon && client_surface(c)->mapped) {
				c->mon = selmon;
				reset_foreign_tolevel(c, NULL, c->mon);
			}
			if (c->tags == 0 && !c->is_in_scratchpad) {
				c->tags = selmon->tagset[selmon->seltags];
				set_size_per(selmon, c);
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
	 * wl_pointer.motion event for the clients, it's only the image what
	 * it's at the wrong position after all. */
	wlr_cursor_move(cursor, NULL, 0, 0);

	wlr_output_manager_v1_set_configuration(output_mgr, output_config);

	/* 布局变化后更新 xdg-output 详情 */
	xdg_output_update_all();
}

void outputmgrapply(struct wl_listener *listener, void *data) {
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 0);
}

void // 0.7 custom
outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int32_t test) {
	/*
	 * Called when a client such as wlr-randr requests a change in output
	 * configuration. This is only one way that the layout can be changed,
	 * so any Monitor information should be updated by updatemons() after an
	 * output_layout.change event, not here.
	 */
	struct wlr_output_configuration_head_v1 *config_head;
	int32_t ok = 1;

	wl_list_for_each(config_head, &config->heads, link) {
		struct wlr_output *wlr_output = config_head->state.output;
		Monitor *m = wlr_output->data;
		struct wlr_output_state state;

		/* Ensure displays previously disabled by
		 * wlr-output-power-management-v1 are properly handled*/
		m->only_sleep = 0;

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

		if (config_head->state.adaptive_sync_enabled) {
			enable_adaptive_sync(m, &state);
		} else {
			disable_adaptive_sync(m, &state);
		}

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

void powermgrsetmode(struct wl_listener *listener, void *data) {
	struct wlr_output_power_v1_set_mode_event *event = data;
	Monitor *m = event->output->data;

	if (!m)
		return;

	wlr_output_state_set_enabled(&m->pending, event->mode);
	mango_output_commit(m);
	m->only_sleep = !event->mode;
	updatemons(NULL, NULL);
}

void monitor_stop_skip_frame_timer(Monitor *m) {
	if (m->skip_frame_timeout)
		wl_event_source_timer_update(m->skip_frame_timeout, 0);
	m->skiping_frame = false;
	m->resizing_count_pending = 0;
	m->resizing_count_current = 0;
}

int monitor_skip_frame_timeout_callback(void *data) {
	Monitor *m = data;
	Client *c, *tmp;

	wl_list_for_each_safe(c, tmp, &clients, link) { c->configure_serial = 0; }

	monitor_stop_skip_frame_timer(m);
	wlr_output_schedule_frame(m->wlr_output);

	return 1;
}

void monitor_check_skip_frame_timeout(Monitor *m) {
	if (m->skiping_frame &&
		m->resizing_count_pending == m->resizing_count_current) {
		return;
	}

	if (m->skip_frame_timeout) {
		m->resizing_count_current = m->resizing_count_pending;
		m->skiping_frame = true;
		wl_event_source_timer_update(m->skip_frame_timeout, 100); // 100ms
	}
}

void rendermon(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, frame);
	Client *c = NULL, *tmp = NULL;
	LayerSurface *l = NULL, *tmpl = NULL;
	int32_t i;
	struct wl_list *layer_list;
	struct timespec now;
	bool need_more_frames = false;
	bool resizing_on_this_mon = false;

	if (session && !session->active) {
		return;
	}

	if (!m->wlr_output->enabled || !allow_frame_scheduling)
		return;

	// 绘制层和淡出效果
	for (i = 0; i < LENGTH(m->layers); i++) {
		layer_list = &m->layers[i];
		wl_list_for_each_safe(l, tmpl, layer_list, link) {
			need_more_frames = layer_draw_frame(l) || need_more_frames;
		}
	}

	wl_list_for_each_safe(c, tmp, &fadeout_clients, fadeout_link) {
		if (c->is_logic_hide)
			continue;

		need_more_frames = client_draw_fadeout_frame(c) || need_more_frames;
	}

	wl_list_for_each_safe(l, tmpl, &fadeout_layers, fadeout_link) {
		need_more_frames = layer_draw_fadeout_frame(l) || need_more_frames;
	}

	wl_list_for_each(c, &clients, link) {
		if (c->is_logic_hide)
			continue;

		need_more_frames = client_draw_frame(c) || need_more_frames;

		if (!c->force_render && c->configure_serial &&
			client_is_rendered_on_mon(c, m))
			resizing_on_this_mon = true;
	}

	if (!config.animations && !grabc && !need_more_frames &&
		resizing_on_this_mon) {
		monitor_check_skip_frame_timeout(m);
		goto skip;
	}

	if (m->skiping_frame) {
		monitor_stop_skip_frame_timer(m);
	}

	mango_scene_output_commit(m->scene_output, &m->pending);

skip:
	// 发送帧完成通知
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(m->scene_output, &now);

	// 如果需要更多帧，确保安排下一帧
	if (need_more_frames && allow_frame_scheduling) {
		request_fresh_all_monitors();
	}
}

void check_vrr_enable(Client *c) {
	Monitor *m = c && c->mon ? c->mon : selmon;

	if (!m)
		return;

	if (!c && m && !m->iscleanuping && m->is_vrr_enabling &&
		!m->vrr_global_enable) {
		disable_adaptive_sync(m, &m->pending);
		mango_output_commit(m);
		return;
	}

	if (!c)
		return;

	if (VISIBLEON(c, c->mon) && c->vrr_only_fullscreen && c->isfullscreen &&
		!c->mon->is_vrr_enabling) {
		enable_adaptive_sync(c->mon, &m->pending);
		mango_output_commit(m);
		return;
	}

	if (!c->mon->is_vrr_enabling && c->mon->vrr_global_enable) {
		enable_adaptive_sync(c->mon, &m->pending);
		mango_output_commit(m);
	} else if (c->mon->is_vrr_enabling && !c->mon->vrr_global_enable &&
			   (!c->vrr_only_fullscreen || !c->isfullscreen)) {
		disable_adaptive_sync(c->mon, &m->pending);
		mango_output_commit(m);
	}
}

/*
 * Output / Monitor：输出创建与销毁、模式/自适应刷新/ICC、
 * 输出管理器协议、渲染与跳帧控制。
 */
void gpureset(struct wl_listener *listener, void *data) {
	struct wlr_renderer *old_drw = drw;
	struct wlr_allocator *old_alloc = alloc;
	struct Monitor *m = NULL;

	mango_error(true, WLR_DEBUG, "gpu reset");

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

void setgaps(int32_t oh, int32_t ov, int32_t ih, int32_t iv) {
	if (!selmon)
		return;
	if (selmon->pertag && is_special_active(selmon)) {
		selmon->special_gappoh = MANGO_MAX(oh, 0);
		selmon->special_gappov = MANGO_MAX(ov, 0);
		selmon->special_gappih = MANGO_MAX(ih, 0);
		selmon->special_gappiv = MANGO_MAX(iv, 0);
	} else {
		selmon->gappoh = MANGO_MAX(oh, 0);
		selmon->gappov = MANGO_MAX(ov, 0);
		selmon->gappih = MANGO_MAX(ih, 0);
		selmon->gappiv = MANGO_MAX(iv, 0);
	}
	arrange(selmon, false, false);
}
