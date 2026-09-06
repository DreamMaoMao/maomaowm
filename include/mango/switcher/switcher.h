#ifndef __SWITCHER_SWITCHER_H__
#define __SWITCHER_SWITCHER_H__ 1

#include "mango/mango.h"

#define SW_PAD 6
#define SW_GAP 14
#define SW_MARGIN 28
#define SW_PANEL_FRAC 0.8f
#define SW_TILE_FRAC 0.25f
#define SW_ASPECT_MIN (9.0f / 16.0f)
#define SW_ASPECT_MAX (12.0f / 5.0f)

extern const float switcher_panel_color[4];

struct switcher_tile {
	Client *c;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *frame;
	struct wlr_scene_tree *content;
	struct wl_list surfaces; /* switcher_surface.link */
	int cw;
	int row;
};

struct switcher_surface {
	struct switcher_tile *tile;
	struct wlr_surface *surface;
	struct wlr_scene_buffer *buffer;
	int sx, sy;
	bool is_root;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener output_sample;
	struct wl_listener frame_done;
	struct wl_list link;
};

struct switcher_state {
	Monitor *mon;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *bg;
	struct switcher_tile **tiles;
	int count;
	int index;
	int tile_h;
	int scope;
};
extern struct switcher_state
	sw; // TODO refractor-header: move this global variable

/* Function Definitions */
bool switcher_is_active(void);
bool switcher_candidate(Client *c);
void switcher_content_size(Client *c, float *w, float *h);
// map the surface tree into the fixed tile box, re-run after each commit
// because the content size or clip may change
void switcher_tile_layout(struct switcher_tile *tile);
void switcher_surface_finish(struct switcher_surface *entry);
void switcher_surface_update_buffer(struct switcher_surface *entry);
void switcher_surface_commit(struct wl_listener *listener, void *data);
void switcher_surface_destroy(struct wl_listener *listener, void *data);
void switcher_surface_output_sample(struct wl_listener *listener, void *data);
// hidden clients are paced by the preview buffer
void switcher_surface_frame_done(struct wl_listener *listener, void *data);
void switcher_tile_add_surface(struct wlr_surface *surface, int sx, int sy,
							   void *data);
void switcher_tile_create(struct switcher_tile *tile, Client *c);
void switcher_layout(void);
void switcher_apply_highlight(void);
void switcher_close(void);
void switcher_remove_client(Client *c);
void switcher_commit_client(Client *tc);
void switcher_commit(void);
Client *switcher_client_at(double lx, double ly);
void switcher_open(int scope);
void switcher_cycle(int dir);
void switcher(const Arg *arg);

#endif
