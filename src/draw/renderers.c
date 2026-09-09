#include "mango/draw/renderers.h"
#include "mango/draw/texture.h"
#include "mango/common/server.h"      // for `config` global (conic, solid, segments)
#include "mango/manage/client.h"      // for `Client` struct
#include "mango/manage/monitor.h"     // if needed by max_needed_canvas_size usage

#include <cairo.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wlr/interfaces/wlr_buffer.h>

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884
#endif

// old gradient
struct wlr_buffer *texture_render_gradient(const BorderTextureKey *key,
												  Client *target) {
	(void)target;

	if (key == NULL || key->gradient.stopcount <= 0)
		return NULL;
	int width, height;
	max_needed_canvas_size(&width, &height);
	if (width <= 0 || height <= 0)
		return NULL;

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL)
		return NULL;

	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	cairo_t *create = cairo_create(buf->surface);

	double center_x = width / 2.0;
	double center_y = height / 2.0;
	double radius = (width > height ? width : height) / 2.0;

	cairo_pattern_t *pattern = cairo_pattern_create_radial(
		center_x, center_y, 0.0, center_x, center_y, radius);

	for (int index = 0; index < key->gradient.stopcount; index++) {
		double degree = key->gradient.stops[index].degree;
		double fraction = (degree / 360.0) + 0.25;
		if (fraction >= 1.0)
			fraction -= 1.0;
		cairo_pattern_add_color_stop_rgba(pattern, fraction,
										  key->gradient.stops[index].color[0],
										  key->gradient.stops[index].color[1],
										  key->gradient.stops[index].color[2],
										  key->gradient.stops[index].color[3]);
	}
	cairo_rectangle(create, 0, 0, width, height);
	cairo_set_source(create, pattern);
	cairo_fill(create);

	cairo_pattern_destroy(pattern);
	cairo_destroy(create);

	return &buf->base;
}

// linear gradient
struct wlr_buffer *texture_render_linear(const BorderTextureKey *key,
												Client *target) {
	if (key == NULL || target == NULL)
		return NULL;
	float colors[2][4];
	float degree;
	if (!texture_parse_two_colors(key->string, colors, &degree))
		return NULL;

	int32_t width = target->animation.current.width;
	int32_t height = target->animation.current.height;
	if (width <= 0 || height <= 0)
		return NULL;

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL)
		return NULL;
	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	double radians = degree * M_PI / 180.0;
	double length = hypot((double)width, (double)height) / 2.0;
	double cx = width / 2.0;
	double cy = height / 2.0;
	double dx = sin(radians) * length;
	double dy = cos(radians) * length;

	cairo_t *render = cairo_create(buf->surface);
	cairo_pattern_t *pattern =
		cairo_pattern_create_linear(cx - dx, cy - dy, cx + dx, cy + dy);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, colors[0][0], colors[0][1],
									  colors[0][2], colors[0][3]);
	cairo_pattern_add_color_stop_rgba(pattern, 1.0, colors[1][0], colors[1][1],
									  colors[1][2], colors[1][3]);
	cairo_rectangle(render, 0, 0, width, height);
	cairo_set_source(render, pattern);
	cairo_fill(render);
	cairo_pattern_destroy(pattern);
	cairo_destroy(render);

	return &buf->base;
}

// radial gradient renderer
struct wlr_buffer *texture_render_radial(const BorderTextureKey *key,
												Client *target) {
	if (key == NULL || target == NULL)
		return NULL;

	float colors[2][4];
	float scale;
	if (!texture_parse_two_colors(key->string, colors, &scale))
		return NULL;

	int32_t width = target->animation.current.width;
	int32_t height = target->animation.current.height;
	if (width <= 0 || height <= 0)
		return NULL;

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL)
		return NULL;

	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	double cx = width / 2.0;
	double cy = height / 2.0;
	double radius = (width > height ? width : height) * scale;

	cairo_t *render = cairo_create(buf->surface);
	cairo_pattern_t *pattern =
		cairo_pattern_create_radial(cx, cy, 0.0, cx, cy, radius);

	cairo_pattern_add_color_stop_rgba(pattern, 0.0, colors[0][0], colors[0][1],
									  colors[0][2], colors[0][3]);

	cairo_pattern_add_color_stop_rgba(pattern, 1.0, colors[1][0], colors[1][1],
									  colors[1][2], colors[1][3]);

	cairo_rectangle(render, 0, 0, width, height);
	cairo_set_source(render, pattern);
	cairo_fill(render);

	cairo_pattern_destroy(pattern);
	cairo_destroy(render);

	return &buf->base;
}

// store_image renderer: decodes the PNG once and hands it to the texture cache,
// where it is keyed by {TEXTURE_STORE_IMAGE, path} and shared by all image
// renderers. The style is never exposed to the user.
struct wlr_buffer *texture_render_store_image(const BorderTextureKey *key,
											  Client *target) {
	(void)target;
	if (key == NULL || key->string == NULL)
		return NULL;

	cairo_surface_t *image = cairo_image_surface_create_from_png(key->string);
	if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(image);
		return NULL;
	}

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL) {
		cairo_surface_destroy(image);
		return NULL;
	}

	int width = cairo_image_surface_get_width(image);
	int height = cairo_image_surface_get_height(image);
	buf->surface = image;
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	return &buf->base;
}

// fetch the decoded image for this key's path from the store_image cache entry.
// Returns the cairo surface and a locked buffer that the caller must release
// with wlr_buffer_unlock() once done painting.
static cairo_surface_t *texture_acquire_store_image(const BorderTextureKey *key,
													struct wlr_buffer **ref) {
	if (key == NULL || key->string == NULL)
		return NULL;

	BorderTextureKey store_key = {
		.style = TEXTURE_STORE_IMAGE,
		.string = key->string,
	};
	bool from_cache;
	*ref = texture_cache_get(&store_key, NULL, &from_cache);
	if (*ref == NULL)
		return NULL;

	struct texture_image_buffer *img = wl_container_of(*ref, img, base);
	return img->surface;
}

// tile renderer
struct wlr_buffer *texture_render_tile(const BorderTextureKey *key,
											  Client *target) {
	(void)target;
	if (key == NULL || key->string == NULL)
		return NULL;

	struct wlr_buffer *ref;
	cairo_surface_t *image = texture_acquire_store_image(key, &ref);
	if (image == NULL)
		return NULL;

	int width, height;
	max_needed_canvas_size(&width, &height);
	if (width <= 0 || height <= 0) {
		wlr_buffer_unlock(ref);
		return NULL;
	}

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL) {
		wlr_buffer_unlock(ref);
		return NULL;
	}

	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	cairo_t *render = cairo_create(buf->surface);
	cairo_pattern_t *pattern = cairo_pattern_create_for_surface(image);

	cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
	cairo_rectangle(render, 0, 0, width, height);
	cairo_set_source(render, pattern);

	cairo_fill(render);

	cairo_pattern_destroy(pattern);
	cairo_destroy(render);
	wlr_buffer_unlock(ref);

	return &buf->base;
}

// fit renderer
struct wlr_buffer *texture_render_fit(const BorderTextureKey *key,
											 Client *target) {
	if (key == NULL || key->string == NULL || target == NULL)
		return NULL;

	struct wlr_buffer *ref;
	cairo_surface_t *image = texture_acquire_store_image(key, &ref);
	if (image == NULL)
		return NULL;

	int32_t width = target->animation.current.width;
	int32_t height = target->animation.current.height;
	if (width <= 0 || height <= 0) {
		wlr_buffer_unlock(ref);
		return NULL;
	}

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL) {
		wlr_buffer_unlock(ref);
		return NULL;
	}

	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);
	int image_width = cairo_image_surface_get_width(image) + 10;
	int image_height = cairo_image_surface_get_height(image) + 10;

	cairo_t *render = cairo_create(buf->surface);

	cairo_scale(render, (double)width / image_width,
				(double)height / image_height);
	cairo_set_source_surface(render, image, 0, 0);
	cairo_paint(render);

	cairo_destroy(render);
	wlr_buffer_unlock(ref);

	return &buf->base;
}

// fit renderer with opacity adjustment.
struct wlr_buffer *texture_render_fit_opacity(const BorderTextureKey *key,
											  Client *target) {
	if (key == NULL || key->string == NULL || target == NULL)
		return NULL;

	char *copy = strdup(key->string);
	if (copy == NULL)
		return NULL;

	char *colon = strchr(copy, ':');
	if (colon == NULL || *(colon + 1) == '\0') {
		free(copy);
		return NULL;
	}
	*colon = '\0';

	float opacity = strtof(colon + 1, NULL);
	if (opacity < 0.0f)
		opacity = 0.0f;
	if (opacity > 1.0f)
		opacity = 1.0f;

	BorderTextureKey fit_key = {
		.style = TEXTURE_FIT_IMAGE,
		.string = copy,
	};
	struct wlr_buffer *fit = texture_render_fit(&fit_key, target);
	free(copy);
	if (fit == NULL)
		return NULL;

	struct texture_image_buffer *fit_buf =
		wl_container_of(fit, fit_buf, base);
	int width = cairo_image_surface_get_width(fit_buf->surface);
	int height = cairo_image_surface_get_height(fit_buf->surface);

	struct wlr_buffer *result = texture_create_buffer(width, height);
	if (result == NULL) {
		wlr_buffer_drop(fit);
		return NULL;
	}

	struct texture_image_buffer *result_buf =
		wl_container_of(result, result_buf, base);
	cairo_t *render = cairo_create(result_buf->surface);
	cairo_set_source_surface(render, fit_buf->surface, 0, 0);
	cairo_paint_with_alpha(render, opacity);
	cairo_destroy(render);

	wlr_buffer_drop(fit);
	return result;
}

struct wlr_buffer *texture_render_tile_opacity(const BorderTextureKey *key,
											  Client *target) {
	if (key == NULL || key->string == NULL || target == NULL)
		return NULL;

	char *copy = strdup(key->string);
	if (copy == NULL)
		return NULL;

	char *colon = strchr(copy, ':');
	if (colon == NULL || *(colon + 1) == '\0') {
		free(copy);
		return NULL;
	}
	*colon = '\0';

	float opacity = strtof(colon + 1, NULL);
	if (opacity < 0.0f)
		opacity = 0.0f;
	if (opacity > 1.0f)
		opacity = 1.0f;

	BorderTextureKey fit_key = {
		.style = TEXTURE_FIT_IMAGE,
		.string = copy,
	};
	struct wlr_buffer *fit = texture_render_tile(&fit_key, target);
	free(copy);
	if (fit == NULL)
		return NULL;

	struct texture_image_buffer *fit_buf =
		wl_container_of(fit, fit_buf, base);
	int width = cairo_image_surface_get_width(fit_buf->surface);
	int height = cairo_image_surface_get_height(fit_buf->surface);

	struct wlr_buffer *result = texture_create_buffer(width, height);
	if (result == NULL) {
		wlr_buffer_drop(fit);
		return NULL;
	}

	struct texture_image_buffer *result_buf =
		wl_container_of(result, result_buf, base);
	cairo_t *render = cairo_create(result_buf->surface);
	cairo_set_source_surface(render, fit_buf->surface, 0, 0);
	cairo_paint_with_alpha(render, opacity);
	cairo_destroy(render);

	wlr_buffer_drop(fit);
	return result;
}


// segment renderer

// segment cut-out helper
static cairo_surface_t *texture_slice(cairo_surface_t *source, int x, int y,
									  int width, int height) {
	cairo_surface_t *slice =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *copy = cairo_create(slice);
	cairo_set_source_surface(copy, source, -x, -y);
	cairo_paint(copy);
	cairo_destroy(copy);
	return slice;
}
// segment tile helper
static void texture_tile(cairo_t *render, cairo_surface_t *slice, int x, int y,
						 int width, int height, int ox, int oy) {
	cairo_set_source_surface(render, slice, ox, oy);
	cairo_pattern_set_extend(cairo_get_source(render), CAIRO_EXTEND_REPEAT);
	cairo_rectangle(render, x, y, width, height);
	cairo_fill(render);
}

struct wlr_buffer *texture_render_segment(const BorderTextureKey *key,
												 Client *target) {
	if (key == NULL || key->string == NULL || target == NULL)
		return NULL;

	struct wlr_buffer *ref;
	cairo_surface_t *image = texture_acquire_store_image(key, &ref);
	if (image == NULL)
		return NULL;

	int32_t width = target->animation.current.width;
	int32_t height = target->animation.current.height;
	if (width <= 0 || height <= 0) {
		wlr_buffer_unlock(ref);
		return NULL;
	}

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL) {
		wlr_buffer_unlock(ref);
		return NULL;
	}
	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	int tile_width = cairo_image_surface_get_width(image) / 3;
	int tile_height = cairo_image_surface_get_height(image) / 3;

	// slice map:
	// 0 - top edge
	// 1 - bottom edge
	// 2 - left edge
	// 3 - right edge
	// 4 - topleft corner
	// 5 - topright corner
	// 6 - bottomleft corner
	// 7 - bottomright corner
	cairo_surface_t *slices[8];
	slices[0] = texture_slice(image, tile_width, 0, tile_width, tile_height);
	slices[1] = texture_slice(image, tile_width, tile_height * 2, tile_width,
							  tile_height);
	slices[2] = texture_slice(image, 0, tile_height, tile_width, tile_height);
	slices[3] = texture_slice(image, tile_width * 2, tile_height, tile_width,
							  tile_height);
	slices[4] = texture_slice(image, 0, 0, tile_width, tile_height);
	slices[5] =
		texture_slice(image, tile_width * 2, 0, tile_width, tile_height);
	slices[6] =
		texture_slice(image, 0, tile_height * 2, tile_width, tile_height);
	slices[7] = texture_slice(image, tile_width * 2, tile_height * 2,
							  tile_width, tile_height);

	cairo_t *render = cairo_create(buf->surface);

	// tile edge pieces along the edges
	texture_tile(render, slices[0], 0, 0, width, tile_height, 0, 0);
	texture_tile(render, slices[1], 0, height - tile_height, width, tile_height,
				 0, height - tile_height);
	texture_tile(render, slices[2], 0, 0, tile_width, height, 0, 0);
	texture_tile(render, slices[3], width - tile_width, 0, tile_width, height,
				 width - tile_width, 0);

	// clear the corner sections
	cairo_set_operator(render, CAIRO_OPERATOR_CLEAR);
	cairo_rectangle(render, 0, 0, tile_width, tile_height);
	cairo_fill(render);
	cairo_rectangle(render, width - tile_width, 0, tile_width, tile_height);
	cairo_fill(render);
	cairo_rectangle(render, 0, height - tile_height, tile_width, tile_height);
	cairo_fill(render);
	cairo_rectangle(render, width - tile_width, height - tile_height,
					tile_width, tile_height);
	cairo_fill(render);

	// render the corner sections
	cairo_set_operator(render, CAIRO_OPERATOR_OVER);
	cairo_set_source_surface(render, slices[4], 0, 0);
	cairo_paint(render);
	cairo_set_source_surface(render, slices[5], width - tile_width, 0);
	cairo_paint(render);
	cairo_set_source_surface(render, slices[6], 0, height - tile_height);
	cairo_paint(render);
	cairo_set_source_surface(render, slices[7], width - tile_width,
							 height - tile_height);
	cairo_paint(render);

	// render is done, destroy cairo renderers
	cairo_destroy(render);

	for (size_t index = 0; index < sizeof(slices) / sizeof(slices[0]); index++)
		cairo_surface_destroy(slices[index]);
	wlr_buffer_unlock(ref);

	return &buf->base;
}

// conic renderer

// conic gradient helpers

struct conic_stop {
	double angle;
	const float *color;
};

static double conic_wrap_angle(double degree) {
	double angle = fmod(degree, 360.0);
	if (angle < 0.0)
		angle += 360.0;
	return angle;
}

static void conic_build_stops(const BorderTextureKey *key,
							  struct conic_stop *stops) {
	
	for (int i = 0; i < key->gradient.stopcount; i++) {
		stops[i].angle = conic_wrap_angle(key->gradient.stops[i].degree);
		stops[i].color = key->gradient.stops[i].color;
	}
	for (int i = 1; i < key->gradient.stopcount; i++) {
		struct conic_stop stop = stops[i];
		int j = i - 1;
		while (j >= 0 && stops[j].angle > stop.angle) {
			stops[j + 1] = stops[j];
			j--;
		}
		stops[j + 1] = stop;
	}
}

static void conic_color_at(const struct conic_stop *stops, int stopcount,
						   double degree, double color[4]) {
	double point = stops[0].angle + conic_wrap_angle(degree - stops[0].angle);
	for (int i = 0; i < stopcount; i++) {
		double next =
			(i + 1 < stopcount) ? stops[i + 1].angle : stops[0].angle + 360.0;
		if (point < next) {
			double span = next - stops[i].angle;
			double mix = (span > 0.0) ? (point - stops[i].angle) / span : 0.0;
			const float *from = stops[i].color;
			const float *to = stops[(i + 1) % stopcount].color;
			for (int channel = 0; channel < 4; channel++)
				color[channel] =
					(double)from[channel] +
					mix * ((double)to[channel] - (double)from[channel]);
			return;
		}
	}
}

static double conic_square_degree(double u, double v) {
	double m = fmax(fabs(u), fabs(v));
	if (m <= 0.0)
		return 0.0;
	if (fabs(u) > fabs(v))
		return u > 0.0 ? 90.0 + 45.0 * (v / m) : 270.0 - 45.0 * (v / m);
	if (v < 0.0)
		return conic_wrap_angle(45.0 * (u / m));
	return 180.0 - 45.0 * (u / m);
}

static uint32_t conic_premultiplied_argb(const double color[4]) {
	uint8_t alpha = (uint8_t)lround(color[3] * 255.0);
	return ((uint32_t)alpha << 24) |
		   ((uint32_t)lround(color[0] * color[3] * 255.0) << 16) |
		   ((uint32_t)lround(color[1] * color[3] * 255.0) << 8) |
		   ((uint32_t)lround(color[2] * color[3] * 255.0));
}

static void conic_fill_pixel(uint32_t *data, int stride, int x, int y,
							 int width, int height, double inv_hw,
							 double inv_hh, const struct conic_stop *stops,
							 int stopcount) {
	double u = ((double)x - width / 2.0) * inv_hw;
	double v = ((double)y - height / 2.0) * inv_hh;
	double color[4];
	conic_color_at(stops, stopcount, conic_square_degree(u, v), color);
	data[(size_t)y * stride + x] = conic_premultiplied_argb(color);
}

struct wlr_buffer *texture_render_conic(const BorderTextureKey *key,
											   Client *target) {
	if (key == NULL || key->gradient.stopcount <= 0 || target == NULL)
		return NULL;
	int32_t width = target->animation.current.width;
	int32_t height = target->animation.current.height;
	if (width <= 0 || height <= 0)
		return NULL;

	int stopcount = key->gradient.stopcount;
	struct conic_stop *stops = malloc((size_t)stopcount * sizeof(*stops));
	if (stops == NULL)
		return NULL;
	conic_build_stops(key, stops);

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL) {
		free(stops);
		return NULL;
	}
	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	uint32_t *data = (uint32_t *)cairo_image_surface_get_data(buf->surface);
	int stride = cairo_image_surface_get_stride(buf->surface) / 4;

	int border = (int32_t)target->bw;
	int radius = config.border_radius > 0.0 ? (int)config.border_radius : 0;
	int left_inner = border;
	int right_inner = width - border;
	int top_inner = border;
	int bottom_inner = height - border;

	if (border <= 0) {
		cairo_surface_mark_dirty(buf->surface);
		free(stops);
		return &buf->base;
	}

	double inv_hw = 2.0 / width;
	double inv_hh = 2.0 / height;

	for (int y = 0; y < top_inner && y < height; y++)
		for (int x = 0; x < width; x++)
			conic_fill_pixel(data, stride, x, y, width, height, inv_hw, inv_hh,
							 stops, stopcount);
	for (int y = bottom_inner > top_inner ? bottom_inner : 0; y < height; y++)
		for (int x = 0; x < width; x++)
			conic_fill_pixel(data, stride, x, y, width, height, inv_hw, inv_hh,
							 stops, stopcount);
	for (int y = top_inner; y < bottom_inner && y < height; y++) {
		for (int x = 0; x < left_inner && x < width; x++)
			conic_fill_pixel(data, stride, x, y, width, height, inv_hw, inv_hh,
							 stops, stopcount);
		for (int x = right_inner > 0 ? right_inner : 0; x < width; x++)
			conic_fill_pixel(data, stride, x, y, width, height, inv_hw, inv_hh,
							 stops, stopcount);
	}

	if (radius > 0) {
		for (int corner = 0; corner < 4; corner++) {
			int box_x = (corner == 0 || corner == 2) ? left_inner
													 : right_inner - radius;
			int box_y = (corner == 0 || corner == 1) ? top_inner
													 : bottom_inner - radius;
			double center_x = (corner == 0 || corner == 2)
								  ? left_inner + radius
								  : right_inner - radius;
			double center_y = (corner == 0 || corner == 1)
								  ? top_inner + radius
								  : bottom_inner - radius;
			for (int y = box_y; y < box_y + radius && y < height; y++)
				for (int x = box_x; x < box_x + radius && x < width; x++) {
					double dx = ((double)x + 0.5) - center_x;
					double dy = ((double)y + 0.5) - center_y;
					if (dx * dx + dy * dy > (double)radius * radius)
						conic_fill_pixel(data, stride, x, y, width, height,
										 inv_hw, inv_hh, stops, stopcount);
				}
		}
	}

	cairo_surface_mark_dirty(buf->surface);
	free(stops);

	return &buf->base;
}

// solid renderer
struct wlr_buffer *texture_render_solid(const BorderTextureKey *key,
											   Client *target) {
	if (key == NULL || key->string == NULL || target == NULL)
		return NULL;
	float color[4];
	if (!texture_rgba_from_hex(key->string, color))
		return NULL;
	int32_t width = target->animation.current.width;
	int32_t height = target->animation.current.height;
	if (width <= 0 || height <= 0)
		return NULL;

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL)
		return NULL;
	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	double margin = target->bw;
	cairo_t *render = cairo_create(buf->surface);
	cairo_set_source_rgba(render, color[0], color[1], color[2], color[3]);
	cairo_rectangle(render, 0, 0, width, height);
	cairo_rounded_rect(render, margin, margin, width - 2 * margin,
					   height - 2 * margin, config.border_radius);
	cairo_set_fill_rule(render, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_fill(render);
	cairo_destroy(render);

	return &buf->base;
}

// color renderer with no ring clip
struct wlr_buffer *
texture_render_color_noclip(const BorderTextureKey *key, Client *target) {
	if (key == NULL || key->string == NULL || target == NULL)
		return NULL;
	float color[4];
	if (!texture_rgba_from_hex(key->string, color))
		return NULL;
	int32_t width = target->animation.current.width;
	int32_t height = target->animation.current.height;
	if (width <= 0 || height <= 0)
		return NULL;

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL)
		return NULL;
	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	cairo_t *render = cairo_create(buf->surface);
	cairo_set_source_rgba(render, color[0], color[1], color[2], color[3]);
	cairo_paint(render);
	cairo_destroy(render);

	return &buf->base;
}

// static image renderer
struct wlr_buffer *
texture_render_image_static(const BorderTextureKey *key, Client *target) {
	if (key == NULL || key->string == NULL || target == NULL)
		return NULL;

	struct wlr_buffer *ref;
	cairo_surface_t *image = texture_acquire_store_image(key, &ref);
	if (image == NULL)
		return NULL;

	int width, height;
	max_needed_canvas_size(&width, &height);
	if (width <= 0 || height <= 0) {
		wlr_buffer_unlock(ref);
		return NULL;
	}

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL) {
		wlr_buffer_unlock(ref);
		return NULL;
	}
	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	double image_width = cairo_image_surface_get_width(image);
	double image_height = cairo_image_surface_get_height(image);
	cairo_t *render = cairo_create(buf->surface);
	cairo_set_source_surface(render, image, (width - image_width) / 2.0,
							 (height - image_height) / 2.0);
	cairo_paint(render);
	cairo_destroy(render);
	wlr_buffer_unlock(ref);

	return &buf->base;
}

// segment renderer
struct wlr_buffer *
texture_render_color_segments(const BorderTextureKey *key, Client *target) {
	if (key == NULL || key->string == NULL || target == NULL)
		return NULL;

	float colors[4][4];
	char *copy = strdup(key->string);
	if (copy == NULL)
		return NULL;
	char *save = NULL;
	int count = 0;
	for (char *token = strtok_r(copy, "|", &save); token != NULL && count < 4;
		 token = strtok_r(NULL, "|", &save)) {
		if (!texture_rgba_from_hex(token, colors[count]))
			break;
		count++;
	}
	free(copy);
	if (count != 4)
		return NULL;

	int32_t width = target->animation.current.width;
	int32_t height = target->animation.current.height;
	if (width <= 0 || height <= 0)
		return NULL;

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL)
		return NULL;
	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	double margin = target->bw;
	double t = margin + 2 * config.border_radius + 2;
	cairo_t *render = cairo_create(buf->surface);
	cairo_set_operator(render, CAIRO_OPERATOR_SOURCE);

	cairo_save(render);
	cairo_move_to(render, 0, 0);
	cairo_line_to(render, width, 0);
	cairo_line_to(render, width - t, t);
	cairo_line_to(render, t, t);
	cairo_close_path(render);
	cairo_clip(render);
	cairo_set_source_rgba(render, colors[0][0], colors[0][1], colors[0][2],
						  colors[0][3]);
	cairo_paint(render);
	cairo_restore(render);

	cairo_save(render);
	cairo_move_to(render, width, 0);
	cairo_line_to(render, width - t, t);
	cairo_line_to(render, width - t, height - t);
	cairo_line_to(render, width, height);
	cairo_close_path(render);
	cairo_clip(render);
	cairo_set_source_rgba(render, colors[1][0], colors[1][1], colors[1][2],
						  colors[1][3]);
	cairo_paint(render);
	cairo_restore(render);

	cairo_save(render);
	cairo_move_to(render, width, height);
	cairo_line_to(render, width - t, height - t);
	cairo_line_to(render, t, height - t);
	cairo_line_to(render, 0, height);
	cairo_close_path(render);
	cairo_clip(render);
	cairo_set_source_rgba(render, colors[2][0], colors[2][1], colors[2][2],
						  colors[2][3]);
	cairo_paint(render);
	cairo_restore(render);

	cairo_save(render);
	cairo_move_to(render, 0, height);
	cairo_line_to(render, t, height - t);
	cairo_line_to(render, t, t);
	cairo_line_to(render, 0, 0);
	cairo_close_path(render);
	cairo_clip(render);
	cairo_set_source_rgba(render, colors[3][0], colors[3][1], colors[3][2],
						  colors[3][3]);
	cairo_paint(render);
	cairo_restore(render);
	cairo_set_operator(render, CAIRO_OPERATOR_CLEAR);
	cairo_rounded_rect(render, margin, margin, width - 2 * margin,
					   height - 2 * margin, config.border_radius);
	cairo_fill(render);
	cairo_destroy(render);

	return &buf->base;
}