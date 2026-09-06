#ifndef ___IPC_H__
#define ___IPC_H__

#include <cjson/cJSON.h>
#include <stdint.h>
#include <wayland-util.h>
#include "mango/common/types.h"

enum ipc_watch_type {
	IPC_WATCH_NONE = 0,
	IPC_WATCH_MONITOR = 1 << 0,
	IPC_WATCH_CLIENT = 1 << 1,
	IPC_WATCH_TAGS = 1 << 2,
	IPC_WATCH_ALL_MONITORS = 1 << 3,
	IPC_WATCH_ALL_TAGS = 1 << 4,
	IPC_WATCH_ALL_CLIENTS = 1 << 5,
	IPC_WATCH_KEYMODE = 1 << 6,
	IPC_WATCH_KB_LAYOUT = 1 << 7,
	IPC_WATCH_LAST_OPEN_SURFACE = 1 << 8,
	IPC_WATCH_FOCUSING_CLIENT = 1 << 9,
	IPC_WATCH_DEVICE = 1 << 10,
};

#define IPC_WATCH_ARRANGGE                                                     \
	IPC_WATCH_MONITOR | IPC_WATCH_CLIENT | IPC_WATCH_TAGS |                    \
		IPC_WATCH_ALL_MONITORS | IPC_WATCH_ALL_TAGS | IPC_WATCH_ALL_CLIENTS |  \
		IPC_WATCH_LAST_OPEN_SURFACE | IPC_WATCH_FOCUSING_CLIENT

struct ipc_client_state {
	int fd;
	struct wl_event_source *source;
	struct wl_event_loop *loop;
	char *buf;
	size_t buf_len;
	size_t buf_cap;
};
struct ipc_watch_client {
	struct wl_list link;
	int fd;
	struct wl_event_source *source;
	enum ipc_watch_type type;
	union {
		struct {
			char name[64];
		} monitor;
		struct {
			uint32_t id;
		} client;
		struct {
			char mon_name[64];
		} tags;
	} target;
};

void ipc_init(struct wl_event_loop *event_loop);
void ipc_remove_watch_client(struct ipc_watch_client *wc);
void ipc_cleanup(void);

void ipc_notify_json_to_fd(int fd, cJSON *json);
void ipc_notify_monitor(Monitor *m);
void ipc_notify_last_surface_ws_name(Monitor *m);
void ipc_notify_focusing_client(void);
void ipc_notify_device_event(struct wlr_input_device *dev);
void ipc_notify_client(Client *c);
void ipc_notify_tags(Monitor *m);
void ipc_notify_all_monitors(void);
void ipc_notify_all_clients(void);
void ipc_notify_all_tags(void);
void ipc_notify_keymode(void);
void ipc_notify_kb_layout(void);
/* ---------- Watch mode support ---------- */
void ipc_notify_json_to_fd(int fd, cJSON *json);

cJSON *tags_mask_to_array(uint32_t tag_mask);
cJSON *build_tags_json(Monitor *m);
cJSON *monitor_active_tags(Monitor *m);
cJSON *build_client_json(Client *c);
cJSON *build_monitor_json(Monitor *m);
cJSON *build_all_tags_entry(Monitor *m);
cJSON *build_all_tags_response(void);
cJSON *build_monitor_tags_response(Monitor *m);
cJSON *build_layouts_response(void);

void printstatus(enum ipc_watch_type type);
void handle_print_status(struct wl_listener *listener, void *data);

void send_static_json(int fd, const char *json_str);

/* ---------- One-shot command handling ---------- */
void handle_command(int client_fd, const char *cmd_raw);

/* Pushes the device that triggered the last event to watch all-devices clients.
 */

int ipc_watch_data_handler(int fd, uint32_t mask, void *data);
bool handle_watch_command(int fd, const char *cmd,
						  struct ipc_client_state *client);

/* ---------- Socket event handling ---------- */
int ipc_handle_client_data(int fd, uint32_t mask, void *data);
#endif
