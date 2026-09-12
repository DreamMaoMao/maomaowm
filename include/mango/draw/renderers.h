#ifndef MANGO_TEXTURE_INTERNAL_H
#define MANGO_TEXTURE_INTERNAL_H

#include "mango/common/types.h"
#include "mango/draw/texture.h"

#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wlr/types/wlr_buffer.h>

struct texture_image_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

extern const struct wlr_buffer_impl texture_buffer_impl;

struct wlr_buffer *texture_create_buffer(int width, int height);

struct wlr_buffer *texture_render_gradient(const BorderTextureKey *key,
					   Client *target);
struct wlr_buffer *texture_render_linear(const BorderTextureKey *key,
					 Client *target);
struct wlr_buffer *texture_render_radial(const BorderTextureKey *key,
					 Client *target);
struct wlr_buffer *texture_render_tile(const BorderTextureKey *key,
				       Client *target);
struct wlr_buffer *texture_render_fit(const BorderTextureKey *key,
				      Client *target);
struct wlr_buffer *texture_render_fit_opacity(const BorderTextureKey *key,
					      Client *target);
struct wlr_buffer *texture_render_tile_opacity(const BorderTextureKey *key,
					      Client *target);
struct wlr_buffer *texture_render_segment(const BorderTextureKey *key,
					  Client *target);
struct wlr_buffer *texture_render_conic(const BorderTextureKey *key,
					Client *target);
struct wlr_buffer *texture_render_solid(const BorderTextureKey *key,
					Client *target);
struct wlr_buffer *texture_render_color_noclip(const BorderTextureKey *key,
					       Client *target);
struct wlr_buffer *texture_render_image_static(const BorderTextureKey *key,
					       Client *target);
struct wlr_buffer *texture_render_color_segments(const BorderTextureKey *key,
						 Client *target);
struct wlr_buffer *texture_render_store_image(const BorderTextureKey *key,
					      Client *target);
struct wlr_buffer *texture_render_store_image_scaled(const BorderTextureKey *key,
					             Client *target);

struct wlr_buffer *texture_render_segment_top(const BorderTextureKey *key,
                                              Client *target);
struct wlr_buffer *texture_render_segment_bottom(const BorderTextureKey *key,
                                                 Client *target);
struct wlr_buffer *texture_render_segment_left(const BorderTextureKey *key,
                                               Client *target);
struct wlr_buffer *texture_render_segment_right(const BorderTextureKey *key,
                                                Client *target);
struct wlr_buffer *texture_render_segment_tl(const BorderTextureKey *key,
                                             Client *target);
struct wlr_buffer *texture_render_segment_tr(const BorderTextureKey *key,
                                             Client *target);
struct wlr_buffer *texture_render_segment_bl(const BorderTextureKey *key,
                                             Client *target);
struct wlr_buffer *texture_render_segment_br(const BorderTextureKey *key,
                                             Client *target);

void cairo_rounded_rect(cairo_t *render, double x, double y, double w,
                        double h, double r);
bool texture_rgba_from_hex(const char *hex, float rgba[4]);
bool texture_parse_two_colors(const char *input, float colors[2][4],
                              float *param);
void max_needed_canvas_size(int *width, int *height);

bool gradient_key_empty(const BorderTextureKey *key);
bool gradient_key_equal(const BorderTextureKey *a, const BorderTextureKey *b);
bool gradient_key_copy(const BorderTextureKey *source,
                       BorderTextureKey *destination);
void gradient_key_destroy(BorderTextureKey *key);
bool string_key_empty(const BorderTextureKey *key);
bool string_key_equal(const BorderTextureKey *a, const BorderTextureKey *b);
bool string_key_copy(const BorderTextureKey *source,
                     BorderTextureKey *destination);
void string_key_destroy(BorderTextureKey *key);

#endif