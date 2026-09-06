#include "mango/ext-protocol/xdg-output.h"
#include "mango/common/globals.h"

/* XWayland 的 X server 也是一个 wayland 客户端。每次发送时动态判断，
 * 避免在资源创建时缓存导致 xwayland 重启/初始化时序问题 */
bool xdg_output_resource_is_xwayland(struct wl_resource *resource) {
#ifdef XWAYLAND
	if (xwayland && xwayland->server &&
		xwayland->server->client == wl_resource_get_client(resource))
		return true;
#endif
	return false;
}
/* 从当前输出状态计算逻辑值与物理(含旋转)值 */
void xdg_output_get_values(struct MangoXDGOutput *output, int32_t *lx,
						   int32_t *ly, int32_t *lw, int32_t *lh, int32_t *px,
						   int32_t *py, int32_t *pw, int32_t *ph) {
	int32_t x = 0, y = 0;
	Monitor *m = output->wlr_output->data;
	if (m) {
		x = m->m.x;
		y = m->m.y;
	}

	float scale =
		output->wlr_output->scale > 0.f ? output->wlr_output->scale : 1.f;

	/* 逻辑尺寸 = 变换后（含旋转）的有效分辨率 ÷ scale，与
	 * wl_output.mode/transform/scale 严格一致，避免依赖 m->m（布局缓存）。
	 * 注意不能直接用 wlr_output->width/height：它们是未旋转的模式尺寸，
	 * 旋转 90/270 时会导致逻辑宽高不交换，grim/slurp 等按 xdg-output
	 * 逻辑尺寸计算截图/选区就会得到未旋转的缓冲区。 */
	int32_t tw, th;

	// 获取未经过缩放，但是经过旋转后的物理分辨率
	wlr_output_transformed_resolution(output->wlr_output, &tw, &th);

	// 对于wayland应用，应该使用逻辑分辨率，也就是缩放后的宽高
	int32_t w = (int32_t)roundf(tw / scale);
	int32_t h = (int32_t)roundf(th / scale);

	// wayland应用缩放后使用的逻辑分辨率和坐标，和wlroots原先实现一致
	*lx = x;
	*ly = y;
	*lw = w;
	*lh = h;

	// xwayland缩放后的坐标需要映射回实际物理显示器的坐标，用于欺骗xwayland应用它们还处于
	// 原先的物理分辨率中
	*px = (int32_t)roundf(x * scale);
	*py = (int32_t)roundf(y * scale);
	*pw = tw; // 直接使用物理分辨率的宽高
	*ph = th;
}
void xdg_output_send_details(struct MangoXDGOutput *output,
							 struct wl_resource *resource) {
	int32_t lx, ly, lw, lh, px, py, pw, ph;
	xdg_output_get_values(output, &lx, &ly, &lw, &lh, &px, &py, &pw, &ph);

	int32_t x, y, w, h;
	if (xdg_output_resource_is_xwayland(resource) &&
		config.xwayland_ignore_scale) {
		/* 欺骗 XWayland：mango 把 X11 窗口放在 逻辑×scale 的物理坐标空间，
		 * 因此告诉 XWayland 屏幕原点在 逻辑×scale、矩形为物理(含旋转)尺寸 */
		x = px;
		y = py;
		w = pw;
		h = ph;
	} else {
		/* xwayland_ignore_scale=0：与普通客户端一样用逻辑坐标 */
		x = lx;
		y = ly;
		w = lw;
		h = lh;
	}

	zxdg_output_v1_send_logical_position(resource, x, y);
	zxdg_output_v1_send_logical_size(resource, w, h);

	if (wl_resource_get_version(resource) <
		MANGO_XDG_OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
		zxdg_output_v1_send_done(resource);

	/* 同步已发送的逻辑值，供 xdg_output_update() 做变化判断 */
	output->last_lx = lx;
	output->last_ly = ly;
	output->last_lw = lw;
	output->last_lh = lh;
	output->sent = true;
	if (xdg_output_resource_is_xwayland(resource)) {
		output->last_px = px;
		output->last_py = py;
		output->last_pw = pw;
		output->last_ph = ph;
		output->xwl_sent = true;
	}
}
/* 普通客户端视角的逻辑值是否与上次发送时不同 */
bool xdg_output_logical_changed(struct MangoXDGOutput *output) {
	int32_t lx, ly, lw, lh, px, py, pw, ph;
	xdg_output_get_values(output, &lx, &ly, &lw, &lh, &px, &py, &pw, &ph);

	/* 从未向任何客户端发送过（无 zxdg_output_v1 资源）时无基线，
	 * 不视为变化，避免无谓调度 wl_output.done */
	if (!output->sent)
		return false;

	return output->last_lx != lx || output->last_ly != ly ||
		   output->last_lw != lw || output->last_lh != lh;
}

/* XWayland 视角的物理值是否与上次发送时不同。
 * 独立比较物理值基线并就地更新（X server 通常不绑定 zxdg_output_v1，
 * 不能依赖 xwl_sent 判断），仅在物理值真正变化时才补发 done。 */
bool xdg_output_xwayland_changed(struct MangoXDGOutput *output) {
	int32_t lx, ly, lw, lh, px, py, pw, ph;
	xdg_output_get_values(output, &lx, &ly, &lw, &lh, &px, &py, &pw, &ph);

	bool changed = output->last_px != px || output->last_py != py ||
				   output->last_pw != pw || output->last_ph != ph;
	output->last_px = px;
	output->last_py = py;
	output->last_pw = pw;
	output->last_ph = ph;
	return changed;
}

/* 更新该输出的 xdg-output 详情,XWayland 资源无条件重发（mango 的
 * XWayland 坐标模型依赖每次布局/配置变化后都收到 position/size，即使
 * 数值未变）；普通客户端仅在逻辑值变化时重发，与 wlroots 标准实现一致。 */
void xdg_output_update(struct MangoXDGOutput *output) {
	bool logical_changed = xdg_output_logical_changed(output);
	struct MangoXDGOutputResource *res;
	wl_list_for_each(res, &output->resources, link) {
		if (!xdg_output_resource_is_xwayland(res->resource) && !logical_changed)
			continue;
		xdg_output_send_details(output, res->resource);
	}
}

void xdg_output_resource_handle_destroy(struct wl_resource *resource) {
	struct MangoXDGOutputResource *res = wl_resource_get_user_data(resource);
	if (!res)
		return;
	wl_list_remove(&res->link);
	free(res);
}
void xdg_output_handle_destroy(struct wl_client *client,
							   struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

void xdg_output_manager_handle_destroy(struct wl_client *client,
									   struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

/* 输出描述变化（version>=3 的客户端需要重发） */
void xdg_output_handle_description(struct wl_listener *listener, void *data) {
	struct MangoXDGOutput *output =
		wl_container_of(listener, output, description);
	struct wlr_output *wlr_output = output->wlr_output;
	if (!wlr_output->description)
		return;

	struct MangoXDGOutputResource *res;
	wl_list_for_each(res, &output->resources, link) {
		if (wl_resource_get_version(res->resource) >=
			MANGO_XDG_OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION)
			zxdg_output_v1_send_description(res->resource,
											wlr_output->description);
	}
}
struct MangoXDGOutput *xdg_output_find(struct wlr_output *wlr_output) {
	struct MangoXDGOutput *output;
	wl_list_for_each(output, &xdg_outputs, link) {
		if (output->wlr_output == wlr_output)
			return output;
	}
	return NULL;
}

struct MangoXDGOutput *xdg_output_create(struct wlr_output *wlr_output) {
	struct MangoXDGOutput *output = calloc(1, sizeof(*output));
	if (!output)
		return NULL;
	output->wlr_output = wlr_output;
	wl_list_init(&output->resources);
	output->description.notify = xdg_output_handle_description;
	wl_signal_add(&wlr_output->events.description, &output->description);
	wl_list_insert(&xdg_outputs, &output->link);
	return output;
}

void xdg_output_destroy(struct MangoXDGOutput *output) {
	/* 惰性化所有资源：zxdg_output_v1 由客户端创建，只有客户端能销毁。
	 * 直接销毁会让 libwayland 为该 id 发 delete_id，客户端在自身 destroy
	 * 请求仍在途时复用该 id，下一个请求会命中未知对象。 */
	struct MangoXDGOutputResource *res, *tmp;
	wl_list_for_each_safe(res, tmp, &output->resources, link) {
		wl_resource_set_user_data(res->resource, NULL);
		wl_list_remove(&res->link);
		free(res);
	}
	wl_list_remove(&output->description.link);
	wl_list_remove(&output->link);
	free(output);
}
void xdg_output_manager_handle_get_xdg_output(
	struct wl_client *client, struct wl_resource *manager_resource, uint32_t id,
	struct wl_resource *output_resource) {
	struct wlr_output *wlr_output = wlr_output_from_resource(output_resource);

	struct wl_resource *resource =
		wl_resource_create(client, &zxdg_output_v1_interface,
						   wl_resource_get_version(manager_resource), id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	/* 无效输出：保持惰性，不发送任何事件 */
	if (!wlr_output) {
		wl_resource_set_implementation(resource, &xdg_output_impl, NULL,
									   xdg_output_resource_handle_destroy);
		return;
	}

	struct MangoXDGOutput *output = xdg_output_find(wlr_output);
	if (!output)
		output = xdg_output_create(wlr_output);
	if (!output) {
		wl_client_post_no_memory(client);
		wl_resource_destroy(resource);
		return;
	}

	struct MangoXDGOutputResource *res = calloc(1, sizeof(*res));
	if (!res) {
		wl_client_post_no_memory(client);
		wl_resource_destroy(resource);
		return;
	}
	res->resource = resource;
	wl_list_insert(&output->resources, &res->link);
	wl_resource_set_implementation(resource, &xdg_output_impl, res,
								   xdg_output_resource_handle_destroy);

	uint32_t version = wl_resource_get_version(resource);
	if (version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION && wlr_output->name)
		zxdg_output_v1_send_name(resource, wlr_output->name);
	if (version >= ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION &&
		wlr_output->description)
		zxdg_output_v1_send_description(resource, wlr_output->description);

	xdg_output_send_details(output, resource);

	if (wl_resource_get_version(output_resource) >=
			WL_OUTPUT_DONE_SINCE_VERSION &&
		version >= MANGO_XDG_OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
		wl_output_send_done(output_resource);
}
void xdg_output_manager_bind(struct wl_client *client, void *data,
							 uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(
		client, &zxdg_output_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &xdg_output_manager_impl, NULL,
								   NULL);
}
/* 更新所有输出的 xdg-output 详情，并在值真正变化时调度 wl_output.done。
 * done 是客户端应用 wl_output/xdg-output 变更的事务边界：wlroots 只在
 * mode/scale/geometry 变化时自行调度，纯布局移动时需要这里补上。
 * 注意 done 会广播给该输出上的所有 wl_output 客户端，因此只在值变化时才发；
 * 若只有 XWayland 视角的值变了（逻辑布局没变），则只给 XWayland 的 wl_output
 * 资源补 done， 避免无谓打扰普通客户端。 */
void xdg_output_update_all(void) {
	struct MangoXDGOutput *output, *tmp;
	wl_list_for_each_safe(output, tmp, &xdg_outputs, link) {
		bool logical_changed = xdg_output_logical_changed(output);
		bool xwayland_changed = xdg_output_xwayland_changed(output);

		xdg_output_update(output);

		if (logical_changed) {
			wlr_output_schedule_done(output->wlr_output);
		} else if (xwayland_changed) {
			struct wl_resource *wl_res;
			wl_resource_for_each(wl_res, &output->wlr_output->resources) {
				if (wl_resource_get_version(wl_res) >=
						WL_OUTPUT_DONE_SINCE_VERSION &&
					xdg_output_resource_is_xwayland(wl_res))
					wl_output_send_done(wl_res);
			}
		}
	}
}
/* 输出被移除时，让对应的 xdg-output 资源变为惰性，而不是销毁它。
 * 该函数在 cleanupmon()（wlr_output destroy 监听器）中调用，保证幂等。 */
void xdg_output_cleanup_output(struct wlr_output *wlr_output) {
	struct MangoXDGOutput *output = xdg_output_find(wlr_output);
	if (output)
		xdg_output_destroy(output);
}

void xdg_output_init(void) {
	wl_list_init(&xdg_outputs);
	xdg_output_global = wl_global_create(dpy, &zxdg_output_manager_v1_interface,
										 MANGO_XDG_OUTPUT_MANAGER_VERSION, NULL,
										 xdg_output_manager_bind);
	if (!xdg_output_global)
		wlr_log(WLR_ERROR, "failed to create zxdg_output_manager_v1 global");
}

struct wl_global *xdg_output_global;
struct wl_list xdg_outputs;

const struct zxdg_output_v1_interface xdg_output_impl = {
	.destroy = xdg_output_handle_destroy,
};

const struct zxdg_output_manager_v1_interface xdg_output_manager_impl = {
	.destroy = xdg_output_manager_handle_destroy,
	.get_xdg_output = xdg_output_manager_handle_get_xdg_output,
};
