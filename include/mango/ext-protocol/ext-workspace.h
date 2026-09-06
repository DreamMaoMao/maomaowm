#ifndef __EXT_PROTOCOL_WORKSPACE_H__
#define __EXT_PROTOCOL_WORKSPACE_H__ 1

#include "mango/common/types.h"
#include <wlr/types/wlr_ext_workspace_v1.h>

#define EXT_WORKSPACE_ENABLE_CAPS                                              \
	EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE |                  \
		EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_DEACTIVATE

struct workspace {
	struct wl_list link;
	uint32_t tag;
	Monitor *m;
	struct wlr_ext_workspace_handle_v1 *ext_workspace;
	struct wl_listener commit;
};

void handle_ext_commit(struct wl_listener *listener, void *data);

void goto_workspace(struct workspace *target);
void toggle_workspace(struct workspace *target);
const char *get_name_from_tag(uint32_t tag);
void destroy_workspace(struct workspace *workspace);
void cleanup_workspaces_by_monitor(Monitor *m);
void remove_workspace_by_tag(uint32_t tag, Monitor *m);
void add_workspace_by_tag(int32_t tag, Monitor *m);
void mango_ext_workspace_printstatus(Monitor *m);
void refresh_monitors_workspaces_status(Monitor *m);
void sync_workspaces_to_tag_num(Monitor *m);
void workspaces_init();

#endif
