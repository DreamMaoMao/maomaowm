#include "mango/dispatch/bind.h"
#include "mango/manage/client.h"
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/ext-protocol/ext-workspace.h"
#include "mango/common/util.h"
#include "mango/manage/monitor.h"

static struct wl_list workspaces;

void goto_workspace(struct workspace *target) {
	uint32_t tag;
	tag = 1 << (target->tag - 1);
	if (target->tag == 0) {
		toggle_overview(&(Arg){0});
		return;
	} else {
		client_switch_view(&(Arg){.ui = tag}, true);
	}
}

void toggle_workspace(struct workspace *target) {
	uint32_t tag;
	tag = 1 << (target->tag - 1);
	if (target->tag == 0) {
		toggle_view(&(Arg){.i = -1});
		return;
	} else {
		toggle_view(&(Arg){.ui = tag});
	}
}

void remove_workspace_by_tag(uint32_t tag, Monitor *m) {
	struct workspace *workspace, *tmp;
	wl_list_for_each_safe(workspace, tmp, &workspaces, link) {
		if (workspace->tag == tag && workspace->m == m) {
			destroy_workspace(workspace);
			return;
		}
	}
}
const char *get_name_from_tag(uint32_t tag) {
	if (tag == 0)
		return "overview";
	if (tag <= (uint32_t)config.tag_num)
		return tags[tag - 1];
	return NULL;
}

void destroy_workspace(struct workspace *workspace) {
	wlr_ext_workspace_handle_v1_destroy(workspace->ext_workspace);
	wl_list_remove(&workspace->link);
	free(workspace);
}

void cleanup_workspaces_by_monitor(Monitor *m) {
	struct workspace *workspace, *tmp;
	wl_list_for_each_safe(workspace, tmp, &workspaces, link) {
		if (workspace->m == m) {
			destroy_workspace(workspace);
		}
	}
}

void add_workspace_by_tag(int32_t tag, Monitor *m) {
	const char *name = get_name_from_tag(tag);

	struct workspace *workspace = ecalloc(1, sizeof(*workspace));
	wl_list_append(&workspaces, &workspace->link);

	workspace->tag = tag;
	workspace->m = m;
	workspace->ext_workspace = wlr_ext_workspace_handle_v1_create(
		server.ext_workspace_manager, name, EXT_WORKSPACE_ENABLE_CAPS);

	workspace->ext_workspace->data = workspace;

	wlr_ext_workspace_handle_v1_set_group(workspace->ext_workspace,
										  m->ext_group);
	wlr_ext_workspace_handle_v1_set_name(workspace->ext_workspace, name);
}
void refresh_monitors_workspaces_status(Monitor *m) {
	int32_t i;

	if (!m || !m->wlr_output->enabled || m->iscleanuping) {
		return;
	}

	if (m->isoverview) {
		add_workspace_by_tag(0, m);
		for (i = 1; i <= config.tag_num; i++) {
			remove_workspace_by_tag(i, m);
		}
	} else {
		remove_workspace_by_tag(0, m);
		for (i = 1; i <= config.tag_num; i++) {
			add_workspace_by_tag(i, m);
		}
	}

	mango_ext_workspace_printstatus(m);
}
void workspaces_init() {
	server.ext_workspace_manager =
		wlr_ext_workspace_manager_v1_create(server.display, 1);

	wl_list_init(&workspaces);

	wl_signal_add(&server.ext_workspace_manager->events.commit,
				  &server.ext_workspace_commit_listener);
}
// New from here
void handle_ext_commit(struct wl_listener *listener, void *data) {
	struct wlr_ext_workspace_v1_commit_event *event = data;
	struct wlr_ext_workspace_v1_request *request;

	wl_list_for_each(request, event->requests, link) {
		switch (request->type) {
		case WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE: {
			if (!request->activate.workspace) {
				break;
			}

			struct workspace *workspace = NULL;
			struct workspace *w;
			wl_list_for_each(w, &workspaces, link) {
				if (w->ext_workspace == request->activate.workspace) {
					workspace = w;
					break;
				}
			}

			if (!workspace || workspace->m->isoverview) {
				break;
			}

			goto_workspace(workspace);
			mango_error(true, WLR_INFO, "ext activating workspace %d",
						workspace->tag);
			break;
		}
		case WLR_EXT_WORKSPACE_V1_REQUEST_DEACTIVATE: {
			if (!request->deactivate.workspace) {
				break;
			}

			struct workspace *workspace = NULL;
			struct workspace *w;
			wl_list_for_each(w, &workspaces, link) {
				if (w->ext_workspace == request->deactivate.workspace) {
					workspace = w;
					break;
				}
			}

			if (!workspace || workspace->m->isoverview) {
				break;
			}

			toggle_workspace(workspace);
			mango_error(true, WLR_INFO, "ext deactivating workspace %d",
						workspace->tag);
			break;
		}
		default:
			break;
		}
	}
}

void mango_ext_workspace_printstatus(Monitor *m) {
	struct workspace *w;
	uint32_t tag_status = 0;

	wl_list_for_each(w, &workspaces, link) {
		if (w && w->m == m) {

			tag_status = get_tag_status(w->tag, m);
			if (tag_status == 2) {
				wlr_ext_workspace_handle_v1_set_hidden(w->ext_workspace, false);
				wlr_ext_workspace_handle_v1_set_urgent(w->ext_workspace, true);
			} else if (tag_status == 1) {
				wlr_ext_workspace_handle_v1_set_urgent(w->ext_workspace, false);
				wlr_ext_workspace_handle_v1_set_hidden(w->ext_workspace, false);
			} else {
				wlr_ext_workspace_handle_v1_set_urgent(w->ext_workspace, false);
				if (!w->m->pertag->no_hide[w->tag])
					wlr_ext_workspace_handle_v1_set_hidden(w->ext_workspace,
														   true);
				else {
					wlr_ext_workspace_handle_v1_set_hidden(w->ext_workspace,
														   false);
				}
			}

			uint32_t active_tagset = get_monitor_active_tagset(m);
			if ((active_tagset & (1 << (w->tag - 1)) & TAGMASK) ||
				m->isoverview) {
				wlr_ext_workspace_handle_v1_set_hidden(w->ext_workspace, false);
				wlr_ext_workspace_handle_v1_set_active(w->ext_workspace, true);
			} else {
				wlr_ext_workspace_handle_v1_set_active(w->ext_workspace, false);
			}
		}
	}
}

void sync_workspaces_to_tag_num(Monitor *m) {
	if (!m || !m->wlr_output->enabled || m->iscleanuping)
		return;

	struct workspace *w, *tmp;

	if (m->isoverview) {
		bool has_overview = false;
		wl_list_for_each_safe(w, tmp, &workspaces, link) {
			if (w->m == m) {
				if (w->tag == 0) {
					has_overview = true;
				} else {
					destroy_workspace(w);
				}
			}
		}
		if (!has_overview)
			add_workspace_by_tag(0, m);
	} else {
		// Normal state: destroy old tag workspaces beyond tag_num.
		wl_list_for_each_safe(w, tmp, &workspaces, link) {
			if (w->m == m && w->tag > config.tag_num)
				destroy_workspace(w);
		}
		// Adds missing tags.
		for (int32_t i = 1; i <= config.tag_num; i++) {
			bool exists = false;
			wl_list_for_each(w, &workspaces, link) {
				if (w->m == m && w->tag == i) {
					exists = true;
					break;
				}
			}
			if (!exists)
				add_workspace_by_tag(i, m);
		}
	}

	mango_ext_workspace_printstatus(m);
}
