#ifndef __OVERVIEW_OVERVIEW_H__
#define __OVERVIEW_OVERVIEW_H__ 1

#include "mango/mango.h"
#include <stdint.h>

// overview 预览：每个客户端建一个独立卡片树，遍历其 surface 树（含
// subsurface）为每个 surface 建 scene_surface 节点直接绑定纹理，尺寸由
// GPU 采样缩放，坐标用 client_get_clip 的 geometry 偏移。提交后自动刷新。

// 目标窗口有其他窗口和它同个tag就返回0
uint32_t want_restore_fullscreen(Client *target_client);
// surface 提交后重算布局（scene_surface 提交时会重置
// dest/source，需要重新套用）
void overview_card_surface_commit(struct wl_listener *listener, void *data);
// surface 销毁时移除并释放节点
void overview_card_surface_destroy(struct wl_listener *listener, void *data);
// 为每个 surface（含 subsurface）建一个卡片 scene_surface 节点
void overview_card_surface_add(struct wlr_surface *surface, int sx, int sy,
							   void *data);
// 按当前几何更新卡片位置与缩放；内容起点用 client_get_clip 的 geometry 偏移
void overview_layout_card(Client *c);

// 销毁卡片树并释放全部 surface 节点
void overview_destroy_card(Client *c);

// 给卡片所有 buffer 节点统一应用圆角
// 进入 overview：保存并禁用真实 scene_surface 树，建独立卡片树显示内容
void overview_backup_surface(Client *c);

// 普通视图切换到overview时保存窗口的旧状态
void overview_backup(Client *c);

// overview切回到普通视图还原窗口的状态
void overview_restore(Client *c, const Arg *arg);

#endif
