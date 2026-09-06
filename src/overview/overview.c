#include "mango/overview/overview.h"
#include "mango/layout/layout.h"
#include "mango/common/globals.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include "mango/animation/client.h"
#include "mango/common/util.h"

// 目标窗口有其他窗口和它同个tag就返回0
uint32_t want_restore_fullscreen(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {
		if (c && c != target_client && c->tags == target_client->tags &&
			c == selmon->sel && c->mon &&
			c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id !=
				SCROLLER &&
			c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id !=
				VERTICAL_SCROLLER) {
			return 0;
		}
	}

	return 1;
}

// surface 提交后重算布局（scene_surface 提交时会重置
// dest/source，需要重新套用）
void overview_card_surface_commit(struct wl_listener *listener, void *data) {
	struct ov_card_surface *entry = wl_container_of(listener, entry, commit);
	if (entry->c && entry->c->ov_card_tree)
		overview_layout_card(entry->c);
}

// surface 销毁时移除并释放节点
void overview_card_surface_destroy(struct wl_listener *listener, void *data) {
	struct ov_card_surface *entry = wl_container_of(listener, entry, destroy);
	wl_list_remove(&entry->link);
	wl_list_remove(&entry->commit.link);
	wl_list_remove(&entry->destroy.link);
	free(entry);
}

// 为每个 surface（含 subsurface）建一个卡片 scene_surface 节点
void overview_card_surface_add(struct wlr_surface *surface, int sx, int sy,
							   void *data) {
	Client *c = data;
	if (!c->ov_card_tree)
		return;

	struct ov_card_surface *entry = ecalloc(1, sizeof(*entry));
	entry->c = c;
	entry->surface = surface;
	entry->sx = sx;
	entry->sy = sy;
	entry->is_root = (surface == client_surface(c));

	entry->scene_surface = wlr_scene_surface_create(c->ov_card_tree, surface);
	if (!entry->scene_surface) {
		free(entry);
		return;
	}
	entry->buffer = entry->scene_surface->buffer;
	wlr_scene_buffer_set_filter_mode(entry->buffer, WLR_SCALE_FILTER_BILINEAR);
	entry->buffer->node.data = c; /* 命中测试 */

	entry->commit.notify = overview_card_surface_commit;
	wl_signal_add(&surface->events.commit, &entry->commit);
	entry->destroy.notify = overview_card_surface_destroy;
	wl_signal_add(&surface->events.destroy, &entry->destroy);

	wl_list_insert(&c->ov_card_surfaces, &entry->link);
}
// 按当前几何更新卡片位置与缩放；内容起点用 client_get_clip 的 geometry 偏移
void overview_layout_card(Client *c) {
	if (!c->ov_card_tree)
		return;

	struct wlr_box geo = c->animation.current;
	if (geo.width <= 0 || geo.height <= 0)
		client_get_geometry(c, &geo);
	int32_t w = geo.width - 2 * (int32_t)c->bw;
	int32_t h = geo.height - 2 * (int32_t)c->bw;
	if (w <= 0 || h <= 0)
		return;

	wlr_scene_node_set_position(&c->ov_card_tree->node, c->bw, c->bw);

	// 内容起点（geometry 偏移）与卡片内容尺寸
	struct wlr_box clip;
	client_get_clip(c, &clip);

	float content_w, content_h;
#ifdef XWAYLAND
	struct wlr_surface *s = client_surface(c);
	if (client_is_x11(c)) {
		content_w = s->current.width;
		content_h = s->current.height;
	} else
#endif
	{
		content_w = c->surface.xdg->geometry.width;
		content_h = c->surface.xdg->geometry.height;
	}
	if (content_w <= 0 || content_h <= 0)
		return;

	float scale_x = (float)w / content_w;
	float scale_y = (float)h / content_h;

	struct ov_card_surface *entry;
	wl_list_for_each(entry, &c->ov_card_surfaces, link) {
		struct wlr_surface *es = entry->surface;
		/* current.width/height 是逻辑坐标，buffer_width/height 是像素坐标 */
		float lw = es->current.width;
		float lh = es->current.height;

		if (entry->is_root) {
			/* 根 surface 按内容区域裁剪填满卡片；source_box 为 buffer
			 * 像素，用比例换算 */
			float ratio_x =
				es->current.width > 0
					? (float)es->current.buffer_width / es->current.width
					: 1.0f;
			float ratio_y =
				es->current.height > 0
					? (float)es->current.buffer_height / es->current.height
					: 1.0f;
			wlr_scene_node_set_position(&entry->buffer->node, 0, 0);
			wlr_scene_buffer_set_dest_size(entry->buffer, w, h);
			struct wlr_fbox src = {
				.x = clip.x * ratio_x,
				.y = clip.y * ratio_y,
				.width = content_w * ratio_x,
				.height = content_h * ratio_y,
			};
			wlr_scene_buffer_set_source_box(entry->buffer, &src);
		} else {
			/* subsurface 相对内容起点摆放并缩放 */
			int px = (int)((entry->sx - clip.x) * scale_x);
			int py = (int)((entry->sy - clip.y) * scale_y);
			wlr_scene_node_set_position(&entry->buffer->node, px, py);
			wlr_scene_buffer_set_dest_size(entry->buffer, (int)(lw * scale_x),
										   (int)(lh * scale_y));
		}
	}
}

// 销毁卡片树并释放全部 surface 节点
void overview_destroy_card(Client *c) {
	if (!c->ov_card_tree)
		return;

	struct ov_card_surface *entry, *tmp;
	wl_list_for_each_safe(entry, tmp, &c->ov_card_surfaces, link) {
		wl_list_remove(&entry->commit.link);
		wl_list_remove(&entry->destroy.link);
		wl_list_remove(&entry->link);
		free(entry);
	}

	wlr_scene_node_destroy(&c->ov_card_tree->node);
	c->ov_card_tree = NULL;
}

// 进入 overview：保存并禁用真实 scene_surface 树，建独立卡片树显示内容
void overview_backup_surface(Client *c) {
	if (c->ov_card_tree)
		return;
	/* 被吞噬/隐藏的窗口相当于完全消失，不给它建卡片、也不重置其隐藏状态 */
	if (c->is_logic_hide)
		return;
	if (!client_surface(c) || !client_surface(c)->mapped)
		return;

	// 禁用真实 surface 树
	c->overview_scene_surface = c->scene_surface;
	wlr_scene_node_set_enabled(&c->scene_surface->node, false);

	// overview 里所有 tag 的窗口都要显示卡片，不能被子树隐藏逻辑禁用
	c->is_logic_hide = false;
	c->is_clip_to_hide = false;
	wlr_scene_node_set_enabled(&c->scene->node, true);

	c->ov_card_tree = wlr_scene_tree_create(c->scene);
	if (!c->ov_card_tree)
		return;
	c->ov_card_tree->node.data = c; // 命中测试

	// 遍历 surface 树为每个 surface 建卡片节点
	wlr_surface_for_each_surface(client_surface(c), overview_card_surface_add,
								 c);

	// 卡片树建在 scene 顶层，把已启用的 jump label 提到卡片之上
	if (c->jump_label_node && c->jump_label_node->scene_buffer->node.enabled)
		wlr_scene_node_raise_to_top(&c->jump_label_node->scene_buffer->node);

	overview_layout_card(c);

	// 喂一帧启动渲染循环（之后由 scene_surface 的 frame-done 驱动）
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	client_send_frame_done(c, &now);
}
// 普通视图切换到overview时保存窗口的旧状态
void overview_backup(Client *c) {
	c->overview_isfloatingbak = c->isfloating;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->overview_ismaximizescreenbak = c->ismaximizescreen;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->animation.tagining = false;
	c->animation.tagouted = false;
	c->animation.tagouting = false;
	c->overview_backup_geom = c->geom;
	c->overview_backup_bw = c->bw;
	if (c->isfloating) {
		c->isfloating = 0;
	}

	overview_backup_surface(c);

	if (c->isfullscreen || c->ismaximizescreen) {
		client_pending_fullscreen_state(c, 0); // 清除窗口全屏标志
		client_pending_maximized_state(c, 0);
	}
	c->bw = c->isnoborder ? 0 : config.borderpx;

	client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
							WLR_EDGE_RIGHT);
}
// overview切回到普通视图还原窗口的状态
void overview_restore(Client *c, const Arg *arg) {
	/* 被吞噬/隐藏的窗口保持原状（不还原、不显示） */
	if (c->is_logic_hide)
		return;

	c->isfloating = c->overview_isfloatingbak;
	c->isfullscreen = c->overview_isfullscreenbak;
	c->ismaximizescreen = c->overview_ismaximizescreenbak;
	c->overview_isfloatingbak = 0;
	c->overview_isfullscreenbak = 0;
	c->overview_ismaximizescreenbak = 0;
	c->geom = c->overview_backup_geom;
	c->bw = c->overview_backup_bw;
	c->animation.tagining = false;
	c->is_restoring_from_ov = (arg->ui & c->tags & TAGMASK) == 0 ? true : false;

	// 销毁卡片树，恢复真实 scene_surface 树
	overview_destroy_card(c);
	if (c->overview_scene_surface) {
		c->scene_surface = c->overview_scene_surface;
		c->overview_scene_surface = NULL;
		wlr_scene_node_set_enabled(&c->scene_surface->node, true);
	}

	if (c->isfloating) {
		// XRaiseWindow(dpy, c->win); // 提升悬浮窗口到顶层
		resize(c, c->overview_backup_geom, 0);
	} else if (c->isfullscreen || c->ismaximizescreen) {
		if (want_restore_fullscreen(c) && c->ismaximizescreen) {
			setmaximizescreen(c, 1, false);
		} else if (want_restore_fullscreen(c) && c->isfullscreen) {
			setfullscreen(c, 1, false);
		} else {
			client_pending_fullscreen_state(c, 0);
			client_pending_maximized_state(c, 0);
			setfullscreen(c, false, false);
		}
	} else {
		if (c->is_restoring_from_ov) {
			c->is_restoring_from_ov = false;
			resize(c, c->overview_backup_geom, 0);
		}
	}

	if (c->bw == 0 &&
		!c->isfullscreen) { // 如果是在ov模式中创建的窗口,没有bw记录
		c->bw = c->isnoborder ? 0 : config.borderpx;
	}

	if (c->isfloating && !c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_NONE);
	}
}
