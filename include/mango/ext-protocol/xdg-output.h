#ifndef __EXT_PROTOCOL_XDG_OUTPUT_H__
#define __EXT_PROTOCOL_XDG_OUTPUT_H__ 1

/* 自定义 xdg-output：xwayland_ignore_scale 时给 XWayland 发物理坐标/尺寸 */
#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>

#include "xdg-output-unstable-v1-protocol.h"

#define MANGO_XDG_OUTPUT_MANAGER_VERSION 3
#define MANGO_XDG_OUTPUT_DONE_DEPRECATED_SINCE_VERSION 3
#define MANGO_XDG_OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3

/* 一个客户端绑定到的 zxdg_output_v1 资源 */
struct MangoXDGOutputResource {
	struct wl_resource *resource;
	struct wl_list link; /* xdg_output->resources */
};

/* 一个 wlr_output 对应的 xdg-output 状态 */
struct MangoXDGOutput {
	struct wl_list link; /* xdg_outputs */
	struct wlr_output *wlr_output;
	struct wl_list resources; /* MangoXDGOutputResource.link */
	struct {
		struct wl_listener description;
	};
	/* 最近一次发送给普通客户端（非 XWayland）的逻辑值，用于
	 * 判断是否需要重发，避免 reload_config 等无实际变化时打扰
	 * 按 xdg-output 调整 DPI/窗口大小的客户端 */
	int32_t last_lx, last_ly, last_lw, last_lh;
	bool sent;
	/* 最近一次发送给 XWayland 的物理值，用于判断 XWayland 视角
	 * 是否需要收 wl_output.done（例如切换 xwayland_ignore_scale
	 * 而布局未变时，逻辑值不变但 XWayland 收到的值变了） */
	int32_t last_px, last_py, last_pw, last_ph;
	bool xwl_sent;
};

extern struct wl_global *xdg_output_global;
extern struct wl_list xdg_outputs;
extern const struct zxdg_output_v1_interface xdg_output_impl;
extern const struct zxdg_output_manager_v1_interface xdg_output_manager_impl;

/* Declarations */
/* XWayland 的 X server 也是一个 wayland 客户端。每次发送时动态判断，
 * 避免在资源创建时缓存导致 xwayland 重启/初始化时序问题 */
bool xdg_output_resource_is_xwayland(struct wl_resource *resource);
/* 从当前输出状态计算逻辑值与物理(含旋转)值 */
void xdg_output_get_values(struct MangoXDGOutput *output, int32_t *lx,
						   int32_t *ly, int32_t *lw, int32_t *lh, int32_t *px,
						   int32_t *py, int32_t *pw, int32_t *ph);
void xdg_output_send_details(struct MangoXDGOutput *output,
							 struct wl_resource *resource);
/* 普通客户端视角的逻辑值是否与上次发送时不同 */
bool xdg_output_logical_changed(struct MangoXDGOutput *output);
/* XWayland 视角的物理值是否与上次发送时不同。
 * 独立比较物理值基线并就地更新（X server 通常不绑定 zxdg_output_v1，
 * 不能依赖 xwl_sent 判断），仅在物理值真正变化时才补发 done。 */
bool xdg_output_xwayland_changed(struct MangoXDGOutput *output);
/* 更新该输出的 xdg-output 详情,XWayland 资源无条件重发（mango 的
 * XWayland 坐标模型依赖每次布局/配置变化后都收到 position/size，即使
 * 数值未变）；普通客户端仅在逻辑值变化时重发，与 wlroots 标准实现一致。 */
void xdg_output_update(struct MangoXDGOutput *output);
void xdg_output_resource_handle_destroy(struct wl_resource *resource);
void xdg_output_handle_destroy(struct wl_client *client,
							   struct wl_resource *resource);
void xdg_output_manager_handle_destroy(struct wl_client *client,
									   struct wl_resource *resource);
void xdg_output_handle_description(struct wl_listener *listener, void *data);
struct MangoXDGOutput *xdg_output_find(struct wlr_output *wlr_output);
struct MangoXDGOutput *xdg_output_create(struct wlr_output *wlr_output);
void xdg_output_destroy(struct MangoXDGOutput *output);
void xdg_output_manager_handle_get_xdg_output(
	struct wl_client *client, struct wl_resource *manager_resource, uint32_t id,
	struct wl_resource *output_resource);
void xdg_output_manager_bind(struct wl_client *client, void *data,
							 uint32_t version, uint32_t id);
/* 更新所有输出的 xdg-output 详情，并在值真正变化时调度 wl_output.done。
 * done 是客户端应用 wl_output/xdg-output 变更的事务边界：wlroots 只在
 * mode/scale/geometry 变化时自行调度，纯布局移动时需要这里补上。
 * 注意 done 会广播给该输出上的所有 wl_output 客户端，因此只在值变化时才发；
 * 若只有 XWayland 视角的值变了（逻辑布局没变），则只给 XWayland 的 wl_output
 * 资源补 done， 避免无谓打扰普通客户端。 */
void xdg_output_update_all(void);
/* 输出被移除时，让对应的 xdg-output 资源变为惰性，而不是销毁它。
 * 该函数在 cleanupmon()（wlr_output destroy 监听器）中调用，保证幂等。 */
void xdg_output_cleanup_output(struct wlr_output *wlr_output);
void xdg_output_init(void);

#endif
