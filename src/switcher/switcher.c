#include "mango/switcher/switcher.h"
#include "mango/manage/client.h"
#include "mango/common/server.h"
#include "mango/manage/monitor.h"
#include "mango/common/util.h"

static struct switcher_state switcher_state;

// map the surface tree into the fixed tile box, re-run after each commit
// because the content size or clip may change
void switcher_tile_layout(struct switcher_tile *tile) {
	struct wlr_box clip;
	float src_w, src_h;
	client_get_clip(tile->c, &clip);
	switcher_content_size(tile->c, &src_w, &src_h);
	if (src_w <= 0 || src_h <= 0)
		return;

	float scale_x = (float)tile->cw / src_w;
	float scale_y = (float)switcher_state.tile_h / src_h;
	struct switcher_surface *entry;
	wl_list_for_each(entry, &tile->surfaces, link) {
		struct wlr_surface *s = entry->surface;
		if (entry->is_root) {
			float ratio_x =
				s->current.width > 0
					? (float)s->current.buffer_width / s->current.width
					: 1.0f;
			float ratio_y =
				s->current.height > 0
					? (float)s->current.buffer_height / s->current.height
					: 1.0f;
			struct wlr_fbox src = {
				.x = clip.x * ratio_x,
				.y = clip.y * ratio_y,
				.width = src_w * ratio_x,
				.height = src_h * ratio_y,
			};
			wlr_scene_buffer_set_source_box(entry->buffer, &src);
			wlr_scene_node_set_position(&entry->buffer->node, 0, 0);
			wlr_scene_buffer_set_dest_size(entry->buffer, tile->cw,
										   switcher_state.tile_h);
		} else {
			wlr_scene_node_set_position(&entry->buffer->node,
										(int)((entry->sx - clip.x) * scale_x),
										(int)((entry->sy - clip.y) * scale_y));
			wlr_scene_buffer_set_dest_size(entry->buffer,
										   (int)(s->current.width * scale_x),
										   (int)(s->current.height * scale_y));
		}
	}
}

void switcher_surface_finish(struct switcher_surface *entry) {
	wl_list_remove(&entry->commit.link);
	wl_list_remove(&entry->destroy.link);
	wl_list_remove(&entry->output_sample.link);
	wl_list_remove(&entry->frame_done.link);
	wl_list_remove(&entry->link);
	free(entry);
}

void switcher_surface_update_buffer(struct switcher_surface *entry) {
	struct wlr_surface *surface = entry->surface;
	struct wlr_scene_buffer *buffer = entry->buffer;

	struct wlr_fbox source;
	wlr_surface_get_buffer_source_box(surface, &source);
	wlr_scene_buffer_set_source_box(buffer, &source);
	wlr_scene_buffer_set_transform(buffer, surface->current.transform);

	const struct wlr_alpha_modifier_surface_v1_state *alpha =
		wlr_alpha_modifier_v1_get_surface_state(surface);
	wlr_scene_buffer_set_opacity(buffer,
								 alpha ? (float)alpha->multiplier : 1.0f);

	enum wlr_color_transfer_function transfer_function =
		WLR_COLOR_TRANSFER_FUNCTION_GAMMA22;
	enum wlr_color_named_primaries primaries = WLR_COLOR_NAMED_PRIMARIES_SRGB;
	const struct wlr_image_description_v1_data *image =
		wlr_surface_get_image_description_v1_data(surface);
	if (image) {
		transfer_function =
			wlr_color_manager_v1_transfer_function_to_wlr(image->tf_named);
		primaries =
			wlr_color_manager_v1_primaries_to_wlr(image->primaries_named);
	}
	wlr_scene_buffer_set_transfer_function(buffer, transfer_function);
	wlr_scene_buffer_set_primaries(buffer, primaries);

	enum wlr_color_encoding encoding = WLR_COLOR_ENCODING_NONE;
	enum wlr_color_range range = WLR_COLOR_RANGE_NONE;
	const struct wlr_color_representation_v1_surface_state *representation =
		wlr_color_representation_v1_get_surface_state(surface);
	if (representation) {
		if (representation->coefficients)
			encoding = wlr_color_representation_v1_color_encoding_to_wlr(
				representation->coefficients);
		if (representation->range)
			range = wlr_color_representation_v1_color_range_to_wlr(
				representation->range);
	}
	wlr_scene_buffer_set_color_encoding(buffer, encoding);
	wlr_scene_buffer_set_color_range(buffer, range);

	if (!surface->buffer || surface->current.width <= 0 ||
		surface->current.height <= 0) {
		wlr_scene_buffer_set_buffer(buffer, NULL);
		return;
	}

	struct wlr_scene_buffer_set_buffer_options options = {
		.damage = &surface->buffer_damage,
	};
	struct wlr_linux_drm_syncobj_surface_v1_state *syncobj =
		wlr_linux_drm_syncobj_v1_get_surface_state(surface);
	if (syncobj) {
		options.wait_timeline = syncobj->acquire_timeline;
		options.wait_point = syncobj->acquire_point;
	}
	wlr_scene_buffer_set_buffer_with_options(buffer, &surface->buffer->base,
											 &options);
}

void handle_switcher_surface_commit(struct wl_listener *listener, void *data) {
	struct switcher_surface *entry = wl_container_of(listener, entry, commit);
	switcher_surface_update_buffer(entry);
	switcher_tile_layout(entry->tile);
}

void handle_switcher_surface_destroy(struct wl_listener *listener, void *data) {
	struct switcher_surface *entry = wl_container_of(listener, entry, destroy);
	struct wlr_scene_buffer *buffer = entry->buffer;
	switcher_surface_finish(entry);
	wlr_scene_node_destroy(&buffer->node);
}

void handle_switcher_surface_output_sample(struct wl_listener *listener,
										   void *data) {
	struct switcher_surface *entry =
		wl_container_of(listener, entry, output_sample);
	const struct wlr_scene_output_sample_event *event = data;
	if (entry->buffer->primary_output != event->output)
		return;

	struct wlr_output *output = event->output->output;
	if (event->direct_scanout)
		wlr_presentation_surface_scanned_out_on_output(entry->surface, output);
	else
		wlr_presentation_surface_textured_on_output(entry->surface, output);

	struct wlr_linux_drm_syncobj_surface_v1_state *syncobj =
		wlr_linux_drm_syncobj_v1_get_surface_state(entry->surface);
	if (syncobj && event->release_timeline)
		wlr_linux_drm_syncobj_v1_state_add_release_point(
			syncobj, event->release_timeline, event->release_point,
			output->event_loop);
}

// hidden clients are paced by the preview buffer
void handle_switcher_surface_frame_done(struct wl_listener *listener,
										void *data) {
	struct switcher_surface *entry =
		wl_container_of(listener, entry, frame_done);
	struct wlr_scene_frame_done_event *event = data;
	if (entry->buffer->primary_output == event->output)
		wlr_surface_send_frame_done(entry->surface, &event->when);
}

void switcher_tile_add_surface(struct wlr_surface *surface, int sx, int sy,
							   void *data) {
	struct switcher_tile *tile = data;

	// only the real scene surface may update wl_surface output membership
	struct wlr_scene_buffer *buffer =
		wlr_scene_buffer_create(tile->content, NULL);
	if (!buffer)
		return;

	struct switcher_surface *entry = ecalloc(1, sizeof(*entry));
	entry->tile = tile;
	entry->surface = surface;
	entry->buffer = buffer;
	entry->sx = sx;
	entry->sy = sy;
	entry->is_root = surface == client_surface(tile->c);
	wlr_scene_buffer_set_filter_mode(entry->buffer, WLR_SCALE_FILTER_BILINEAR);
	switcher_surface_update_buffer(entry);

	LISTEN(&surface->events.commit, &entry->commit,
		   handle_switcher_surface_commit);
	LISTEN(&surface->events.destroy, &entry->destroy,
		   handle_switcher_surface_destroy);
	LISTEN(&buffer->events.output_sample, &entry->output_sample,
		   handle_switcher_surface_output_sample);
	LISTEN(&buffer->events.frame_done, &entry->frame_done,
		   handle_switcher_surface_frame_done);
	wl_list_insert(&tile->surfaces, &entry->link);
}

void switcher_tile_create(struct switcher_tile *tile, Client *c) {
	tile->c = c;
	wl_list_init(&tile->surfaces);
	float src_w, src_h;
	switcher_content_size(c, &src_w, &src_h);
	float aspect = src_w > 0 && src_h > 0 ? src_w / src_h : 1.0f;
	aspect = MANGO_MAX(SW_ASPECT_MIN, MANGO_MIN(aspect, SW_ASPECT_MAX));
	tile->cw = MANGO_MAX(1, (int)(switcher_state.tile_h * aspect));

	tile->tree = wlr_scene_tree_create(switcher_state.tree);
	tile->frame = wlr_scene_rect_create(tile->tree, 1, 1, config.bordercolor);
	tile->content = wlr_scene_tree_create(tile->tree);
	wlr_scene_node_set_position(&tile->content->node, SW_PAD, SW_PAD);
	wlr_surface_for_each_surface(client_surface(c), switcher_tile_add_surface,
								 tile);
	switcher_tile_layout(tile);
}

void switcher_layout(void) {
	int frame_h = switcher_state.tile_h + 2 * SW_PAD;
	int maxrow_w = MANGO_MAX(
		1, (int)(switcher_state.mon->m.width * SW_PANEL_FRAC) - 2 * SW_MARGIN);

	int nrows = 1;
	int row_w = 0;
	int content_w = 0;
	for (int i = 0; i < switcher_state.count; i++) {
		struct switcher_tile *tile = switcher_state.tiles[i];
		int fw = tile->cw + 2 * SW_PAD;
		if (row_w > 0 && row_w + SW_GAP + fw > maxrow_w) {
			nrows++;
			row_w = fw;
		} else {
			row_w = row_w == 0 ? fw : row_w + SW_GAP + fw;
		}
		tile->row = nrows - 1;
		content_w = MANGO_MAX(content_w, row_w);
	}

	int *rowsw = ecalloc(nrows, sizeof(*rowsw));
	for (int i = 0; i < switcher_state.count; i++) {
		struct switcher_tile *tile = switcher_state.tiles[i];
		int fw = tile->cw + 2 * SW_PAD;
		rowsw[tile->row] += rowsw[tile->row] == 0 ? fw : SW_GAP + fw;
	}

	int panel_w = content_w + 2 * SW_MARGIN;
	int panel_h = 2 * SW_MARGIN + nrows * frame_h + (nrows - 1) * SW_GAP;
	wlr_scene_node_set_position(
		&switcher_state.tree->node,
		switcher_state.mon->m.x + (switcher_state.mon->m.width - panel_w) / 2,
		switcher_state.mon->m.y + (switcher_state.mon->m.height - panel_h) / 2);
	wlr_scene_rect_set_size(switcher_state.bg, panel_w, panel_h);

	int row = -1;
	int x = 0;
	for (int i = 0; i < switcher_state.count; i++) {
		struct switcher_tile *tile = switcher_state.tiles[i];
		if (tile->row != row) {
			row = tile->row;
			x = SW_MARGIN + (content_w - rowsw[row]) / 2;
		}
		wlr_scene_node_set_position(&tile->tree->node, x,
									SW_MARGIN + row * (frame_h + SW_GAP));
		wlr_scene_rect_set_size(tile->frame, tile->cw + 2 * SW_PAD, frame_h);
		x += tile->cw + 2 * SW_PAD + SW_GAP;
	}
	free(rowsw);
}

void switcher_apply_highlight(void) {
	for (int i = 0; i < switcher_state.count; i++)
		wlr_scene_rect_set_color(
			switcher_state.tiles[i]->frame,
			i == switcher_state.index ? config.focuscolor : config.bordercolor);
}

void switcher_close(void) {
	if (!switcher_is_active())
		return;
	for (int i = 0; i < switcher_state.count; i++) {
		struct switcher_surface *entry, *tmp;
		wl_list_for_each_safe(entry, tmp, &switcher_state.tiles[i]->surfaces,
							  link) {
			switcher_surface_finish(entry);
		}
	}
	wlr_scene_node_destroy(&switcher_state.tree->node);
	for (int i = 0; i < switcher_state.count; i++)
		free(switcher_state.tiles[i]);
	free(switcher_state.tiles);
	memset(&switcher_state, 0, sizeof(switcher_state));
}

// remove a single tile while the switcher stays open, then relayout
void switcher_remove_client(Client *c) {
	if (!switcher_is_active())
		return;
	int i;
	for (i = 0; i < switcher_state.count; i++) {
		if (switcher_state.tiles[i]->c == c)
			break;
	}
	if (i == switcher_state.count)
		return;

	struct switcher_surface *entry, *tmp;
	wl_list_for_each_safe(entry, tmp, &switcher_state.tiles[i]->surfaces,
						  link) {
		switcher_surface_finish(entry);
	}
	wlr_scene_node_destroy(&switcher_state.tiles[i]->tree->node);
	free(switcher_state.tiles[i]);

	if (switcher_state.index > i)
		switcher_state.index--;
	memmove(&switcher_state.tiles[i], &switcher_state.tiles[i + 1],
			(switcher_state.count - i - 1) * sizeof(*switcher_state.tiles));
	switcher_state.count--;
	if (switcher_state.count == 0) {
		switcher_close();
		return;
	}
	if (switcher_state.index >= switcher_state.count)
		switcher_state.index = switcher_state.count - 1;
	switcher_layout();
	switcher_apply_highlight();
}

void switcher_commit_client(Client *tc) {
	switcher_close();
	if (!tc || !tc->mon || tc->iskilling || tc->isminimized || tc->isunglobal ||
		tc->is_logic_hide || !client_surface(tc) ||
		!client_surface(tc)->mapped || client_is_unmanaged(tc) ||
		client_is_x11_popup(tc))
		return;
	if (!VISIBLEON(tc, tc->mon))
		client_view_on_monitor(&(Arg){.ui = get_tags_first_tag(tc->tags)}, true,
							   tc->mon, true);
	client_focus(tc, 1);
}

void switcher_commit(void) {
	if (!switcher_is_active())
		return;
	switcher_commit_client(switcher_state.tiles[switcher_state.index]->c);
}

Client *switcher_client_at(double lx, double ly) {
	if (!switcher_is_active())
		return NULL;
	for (int i = 0; i < switcher_state.count; i++) {
		struct switcher_tile *tile = switcher_state.tiles[i];
		struct wlr_box box = {
			.width = tile->cw + 2 * SW_PAD,
			.height = switcher_state.tile_h + 2 * SW_PAD,
		};
		if (!wlr_scene_node_coords(&tile->tree->node, &box.x, &box.y) ||
			!wlr_box_contains_point(&box, lx, ly))
			continue;
		return tile->c;
	}
	return NULL;
}

void switcher_open(int scope) {
	Client *c;
	Monitor *m;
	int n = 0;

	wl_list_for_each(m, &server.monitors, link) {
		if (m->isoverview)
			return;
	}

	switcher_state.mon = server.selected_monitor;
	switcher_state.scope = scope;

	wl_list_for_each(c, &server.focus_stack, flink) {
		if (switcher_candidate(c))
			n++;
	}
	if (n == 0)
		return;

	int max_row_w =
		(int)(switcher_state.mon->m.width * SW_PANEL_FRAC) - 2 * SW_MARGIN;
	int max_panel_h = (int)(switcher_state.mon->m.height * SW_PANEL_FRAC);
	// size against the widest allowed tile so the panel always fits
	switcher_state.tile_h = 1;
	for (int cols = 1; cols <= n; cols++) {
		int rows = (n + cols - 1) / cols;
		int h_by_w =
			(int)((max_row_w - (cols - 1) * SW_GAP - 2 * cols * SW_PAD) /
				  (cols * SW_ASPECT_MAX));
		int h_by_h =
			(max_panel_h - 2 * SW_MARGIN - (rows - 1) * SW_GAP) / rows -
			2 * SW_PAD;
		int h = MANGO_MIN((int)(switcher_state.mon->m.height * SW_TILE_FRAC),
						  MANGO_MIN(h_by_w, h_by_h));
		switcher_state.tile_h = MANGO_MAX(switcher_state.tile_h, h);
	}

	switcher_state.tree = wlr_scene_tree_create(server.layers[LyrOverlay]);
	switcher_state.bg =
		wlr_scene_rect_create(switcher_state.tree, 1, 1, switcher_panel_color);
	switcher_state.tiles = ecalloc(n, sizeof(*switcher_state.tiles));
	wl_list_for_each(c, &server.focus_stack, flink) {
		if (!switcher_candidate(c))
			continue;
		struct switcher_tile *tile = ecalloc(1, sizeof(*tile));
		switcher_tile_create(tile, c);
		switcher_state.tiles[switcher_state.count++] = tile;
	}
	// The current window is at the focus_stack head (tiles[0]); selecting the
	// second one and committing inserts it at the front, so the next open puts
	// the original first as second, toggling between the two most recent
	// windows.
	switcher_state.index = switcher_state.count > 1 ? 1 : 0;
	switcher_layout();
	switcher_apply_highlight();

	// restart render loops of clients idling without frame callbacks so
	// tiles of hidden windows stay live, like overview cards
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	for (int i = 0; i < switcher_state.count; i++)
		client_send_frame_done(switcher_state.tiles[i]->c, &now);
}

void switcher_cycle(int dir) {
	switcher_state.index = (switcher_state.index + dir + switcher_state.count) %
						   switcher_state.count;
	switcher_apply_highlight();
}

void switcher(const Arg *arg) {
	int dir = arg && arg->i == PREV ? -1 : 1;
	int scope = arg ? arg->i2 : SW_CURRENT_TAG;
	if (server.session_locked || !server.selected_monitor ||
		server.selected_monitor->is_jump_mode)
		return;
	if (switcher_is_active()) {
		if (scope != switcher_state.scope) {
			switcher_close();
			switcher_open(scope);
		} else {
			switcher_cycle(dir);
		}
	} else {
		switcher_open(scope);
	}
}
bool switcher_is_active(void) { return switcher_state.tree != NULL; }

bool switcher_candidate(Client *c) {
	if (!c->mon || c->iskilling || c->isminimized || c->isunglobal ||
		c->is_logic_hide || !client_surface(c) || !client_surface(c)->mapped ||
		client_is_unmanaged(c) || client_is_x11_popup(c))
		return false;

	if (switcher_state.scope == SW_CURRENT_TAG)
		return c->mon == switcher_state.mon && VISIBLEON(c, switcher_state.mon);
	if (switcher_state.scope == SW_ALL_TAG)
		return c->mon == switcher_state.mon;
	return (int32_t)c->tags > 0;
}

void switcher_content_size(Client *c, float *w, float *h) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_surface *s = client_surface(c);
		*w = s->current.width;
		*h = s->current.height;
		return;
	}
#endif
	*w = c->surface.xdg->geometry.width;
	*h = c->surface.xdg->geometry.height;
}

const float switcher_panel_color[4] = {0.09f, 0.09f, 0.11f, 0.92f};
