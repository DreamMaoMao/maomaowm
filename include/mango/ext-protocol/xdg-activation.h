#ifndef __EXT_PROTOCOL_XDG_ACTIVATION_H__
#define __EXT_PROTOCOL_XDG_ACTIVATION_H__ 1

/*
 * xdg-activation-v1
 */
#include <wlr/types/wlr_xdg_activation_v1.h>

/* Tracks a wlr token so we can check auth later. */
struct mango_xdg_activation_token {
	struct wl_listener destroy;
	struct wlr_xdg_activation_token_v1 *wlr_token;
	bool had_focused_surface; /* token came with a surface */
	bool internal;			  /* we created it (spawn), so it's trusted */
};

extern struct wlr_xdg_activation_v1 *activation;

extern struct wl_listener activation_request_activate_listener;
extern struct wl_listener activation_new_token_listener;
extern struct wl_listener activation_destroy_listener;

/* Declarations */
void handle_xdg_activation_token_destroy(struct wl_listener *listener,
										 void *data);
void handle_xdg_activation_new_token(struct wl_listener *listener, void *data);
/* Tokens from spawn are trusted; client tokens need a focused surface. */
bool xdg_activation_token_can_activate(
	struct wlr_xdg_activation_token_v1 *wlr_token);
void handle_xdg_activation_request_activate(struct wl_listener *listener,
											void *data);
void handle_xdg_activation_destroy(struct wl_listener *listener, void *data);
void xdg_activation_init();
/* Make a token for spawn to export as XDG_ACTIVATION_TOKEN. */
const char *xdg_activation_v1_export_token(void);

#endif
