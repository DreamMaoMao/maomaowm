#ifndef __OVERVIEW_OVERVIEW_H__
#define __OVERVIEW_OVERVIEW_H__ 1

#include "mango/mango.h"
#include <stdint.h>

// Overview preview: each client gets its own card tree; walk its surface tree
// (including subsurfaces) and create a scene_surface node per surface bound
// directly to the texture. Sizes are scaled by GPU sampling and coordinates
// offset by client_get_clip geometry; auto-refreshes after commit.

// Returns 0 when the target window shares its tag with other windows.
uint32_t want_restore_fullscreen(Client *target_client);
// Recomputes layout after surface commit (scene_surface commit resets
// dest/source and needs to be reapplied).
void handle_overview_card_surface_commit(struct wl_listener *listener,
										 void *data);
// Removes and frees the node when the surface is destroyed.
void handle_overview_card_surface_destroy(struct wl_listener *listener,
										  void *data);
// Creates a card scene_surface node for every surface (including subsurfaces).
void overview_card_surface_add(struct wlr_surface *surface, int sx, int sy,
							   void *data);
// Updates card position and scale from the current geometry; content origin
// uses client_get_clip geometry offset.
void overview_layout_card(Client *c);

// Destroys the card tree and frees all surface nodes.
void overview_destroy_card(Client *c);

// Applies rounded corners to all buffer nodes of the card.
void overview_card_set_corner_radii(Client *c, struct fx_corner_radii corners);
// Entering overview: saves and disables the real scene_surface tree and builds
// an independent card tree to display content.
void overview_backup_surface(Client *c);

// Saves the window old state when switching from the normal view to overview.
void overview_backup(Client *c);

// Restores window state when switching back from overview to the normal view.
void overview_restore(Client *c, const Arg *arg);

#endif
