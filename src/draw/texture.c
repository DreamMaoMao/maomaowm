#include "mango/draw/texture.h"
#include "mango/common/server.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include "mango/draw/renderers.h"

#include <cairo.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_buffer.h>

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884
#endif

struct TextureCacheEntry {
	BorderTextureKey key;
	struct wlr_buffer *canvas;
	uint64_t generation;
	uint32_t hash;
};

typedef struct {
	const char *name;
	TextureStyle style;
} TextureStyleName;

// define texture style names
static const TextureStyleName texture_style_names[] = {
	{"linear_gradient", TEXTURE_LINEAR_GRADIENT},
	{"radial_gradient", TEXTURE_RADIAL_GRADIENT},
	{"tiled_image", TEXTURE_TILED_IMAGE},
	{"fit_image", TEXTURE_FIT_IMAGE},
	{"fit_overlay", TEXTURE_FIT_OPACITY},
	{"tile_overlay", TEXTURE_TILE_OPACITY},
	{"segment_image", TEXTURE_SEGMENT_IMAGE},
	{"conic_gradient", TEXTURE_CONIC_GRADIENT},
	{"solid_color", TEXTURE_SOLID},
	{"solid_color_noclip", TEXTURE_COLOR_NOCLIP},
	{"static_image", TEXTURE_STATIC_IMAGE},
	{"segment_color", TEXTURE_COLOR_SEGMENT},
};

static struct TextureCacheEntry *texture_cache = NULL;
static size_t texture_cache_count = 0;
static size_t texture_cache_cap = 0;
static uint64_t texture_generation = 0;

// buffer handlers
static void texture_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct texture_image_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	cairo_surface_destroy(buf->surface);
	free(buf);
}
static bool texture_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
												 uint32_t flags, void **data,
												 uint32_t *format,
												 size_t *stride) {
	(void)flags;
	struct texture_image_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	*data = cairo_image_surface_get_data(buf->surface);
	*format = DRM_FORMAT_ARGB8888;
	*stride = cairo_image_surface_get_stride(buf->surface);
	return true;
}
static void texture_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {}
const struct wlr_buffer_impl texture_buffer_impl = {
	.destroy = texture_buffer_destroy,
	.begin_data_ptr_access = texture_buffer_begin_data_ptr_access,
	.end_data_ptr_access = texture_buffer_end_data_ptr_access,
};

struct wlr_buffer *texture_create_buffer(int width, int height) {
	if (width <= 0 || height <= 0)
		return NULL;

	struct texture_image_buffer *buf = calloc(1, sizeof(*buf));
	if (buf == NULL)
		return NULL;

	buf->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&buf->base, &texture_buffer_impl, width, height);

	return &buf->base;
}

// canvas max size helper
void max_needed_canvas_size(int *width, int *height) {
	*width = 0;
	*height = 0;

	Monitor *monitor;
	wl_list_for_each(monitor, &server.monitors, link) {
		if (monitor->m.width > *width)
			*width = monitor->m.width;
		if (monitor->m.height > *height)
			*height = monitor->m.height;
	}
}

// rgba helper
bool texture_rgba_from_hex(const char *hex, float rgba[4]) {
	if (hex == NULL || *hex == '\0')
		return false;
	char *end = NULL;
	long long value = strtoll(hex, &end, 16);
	if (end == hex || *end != '\0')
		return false;
	rgba[0] = ((value >> 24) & 0xFF) / 255.0f;
	rgba[1] = ((value >> 16) & 0xFF) / 255.0f;
	rgba[2] = ((value >> 8) & 0xFF) / 255.0f;
	rgba[3] = (value & 0xFF) / 255.0f;
	return true;
}
// parser
bool texture_parse_two_colors(const char *input, float colors[2][4],
									 float *param) {
	if (input == NULL)
		return false;
	char *copy = strdup(input);
	if (copy == NULL)
		return false;
	char *save = NULL;
	char *color1 = strtok_r(copy, "|", &save);
	char *color2 = strtok_r(NULL, "|", &save);
	char *param_str = strtok_r(NULL, "|", &save);
	bool parsed = color1 && color2 && param_str &&
				  texture_rgba_from_hex(color1, colors[0]) &&
				  texture_rgba_from_hex(color2, colors[1]);
	if (parsed)
		*param = strtof(param_str, NULL);
	free(copy);
	return parsed;
}

// cairo/general border draw stuff
void cairo_rounded_rect(cairo_t *render, double x, double y, double w,
							   double h, double r) {
	if (r <= 0.0) {
		cairo_rectangle(render, x, y, w, h);
		return;
	}
	cairo_new_sub_path(render);
	cairo_arc(render, x + w - r, y + r, r, -M_PI / 2.0, 0.0);
	cairo_arc(render, x + w - r, y + h - r, r, 0.0, M_PI / 2.0);
	cairo_arc(render, x + r, y + h - r, r, M_PI / 2.0, M_PI);
	cairo_arc(render, x + r, y + r, r, M_PI, M_PI * 3.0 / 2.0);
	cairo_close_path(render);
}

struct wlr_buffer *texture_make_ring(struct wlr_buffer *canvas, int width,
									 int height, int border, double corners,
									 bool stamp_inside) {
	if (canvas == NULL || width <= 0 || height <= 0 || border < 0)
		return NULL;

	struct texture_image_buffer *source = wl_container_of(canvas, source, base);

	struct texture_image_buffer *ring = calloc(1, sizeof(*ring));
	if (ring == NULL)
		return NULL;

	ring->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	wlr_buffer_init(&ring->base, &texture_buffer_impl, width, height);

	int canvas_width = cairo_image_surface_get_width(source->surface);
	int canvas_height = cairo_image_surface_get_height(source->surface);
	double horizontal_sample_offset = ((canvas_width - width) / 2.0) * -1.0;
	double vertical_sample_offset = ((canvas_height - height) / 2.0) * -1.0;

	cairo_t *render = cairo_create(ring->surface);

	cairo_set_source_surface(render, source->surface, horizontal_sample_offset,
							 vertical_sample_offset);
	cairo_paint(render);

	if (stamp_inside) {
		cairo_set_operator(render, CAIRO_OPERATOR_CLEAR);
		cairo_rounded_rect(render, border, border, width - 2 * border,
						   height - 2 * border, corners);
		cairo_fill(render);
	}
	cairo_destroy(render);

	return &ring->base;
}

struct wlr_buffer *texture_composite_slots(const BorderTextureKey *slots, struct Client *target, int width, int height) {
	if (target == NULL || width <= 0 || height <= 0)
		return NULL;
	struct wlr_buffer *output = texture_create_buffer(width, height);
	if (output == NULL)
		return NULL;
	struct texture_image_buffer *output_buf = wl_container_of(output, output_buf, base);

	double corners = target->has_border_radius_override ? target->border_radius_override : config.border_radius;

	cairo_t *render = cairo_create(output_buf->surface);

	for (int slot = MANGO_TEXTURE_SLOTS -1; slot >=0; slot--) { // loop bottom > middle > top
		if (texture_key_empty(&slots[slot]))
			continue;

		bool from_cache;
		struct wlr_buffer *canvas = texture_cache_get(&slots[slot], target, &from_cache);
		if (canvas == NULL)
			continue;

		struct wlr_buffer *layer = canvas;
		bool owned_layer = false;
		if (!texture_style_skip_make_ring(slots[slot].style)) {
			layer = texture_make_ring(canvas, width, height, (int32_t)target->bw, corners, true);
			if (layer != NULL)
				owned_layer = true;
		}
		if (layer != NULL) {
			struct texture_image_buffer *layer_buf = wl_container_of(layer, layer_buf, base);
			cairo_set_source_surface(render, layer_buf->surface, 0, 0);
			cairo_paint(render);
		}
		if (owned_layer)
			wlr_buffer_drop(layer);
		if (texture_style_bypasses_cache(slots[slot].style))
			wlr_buffer_drop(canvas);
		else
			wlr_buffer_unlock(canvas);
			
	}
	cairo_destroy(render);

	return output;
}

// gradient-specific functions (key empty check, key equal check, key copy, key
// destroy) to implement another renderer, these must be provided to the texture
// renderer using TextureOps

bool gradient_key_empty(const BorderTextureKey *key) {
	return key->gradient.stopcount <= 0;
}

bool gradient_key_equal(const BorderTextureKey *a,
							   const BorderTextureKey *b) {
	if (a->gradient.stopcount != b->gradient.stopcount)
		return false;

	for (int index = 0; index < a->gradient.stopcount; index++) {
		if (memcmp(a->gradient.stops[index].color,
				   b->gradient.stops[index].color, sizeof(float) * 4) != 0)
			return false;
		if (a->gradient.stops[index].degree != b->gradient.stops[index].degree)
			return false;
	}
	return true;
}

bool gradient_key_copy(const BorderTextureKey *source,
							  BorderTextureKey *destination) {
	destination->style = source->style;
	destination->string = NULL;

	if (source->gradient.stopcount <= 0) {
		destination->gradient.stops = NULL;
		destination->gradient.stopcount = 0;
		return true;
	}

	destination->gradient.stops =
		malloc((size_t)source->gradient.stopcount * sizeof(GradientStop));
	if (destination->gradient.stops == NULL)
		return false;
	memcpy(destination->gradient.stops, source->gradient.stops,
		   (size_t)source->gradient.stopcount * sizeof(GradientStop));
	destination->gradient.stopcount = source->gradient.stopcount;
	return true;
}

void gradient_key_destroy(BorderTextureKey *key) {
	free(key->gradient.stops);
	key->gradient.stops = NULL;
	key->gradient.stopcount = 0;
}

static bool texture_parse_gradient(const char *input, GradientBorder *output) {
	free(output->stops);
	output->stops = NULL;
	output->stopcount = 0;

	if (input == NULL || *input == '\0')
		return true;

	char *copy = strdup(input);
	if (copy == NULL)
		return false;

	char *save = NULL;
	int stopcapacity = 0;

	for (char *token = strtok_r(copy, "|", &save); token != NULL;
		 token = strtok_r(NULL, "|", &save)) {
		char *colon = strchr(token, ':');
		if (colon == NULL) {
			free(copy);
			free(output->stops);
			output->stops = NULL;
			output->stopcount = 0;
			return false;
		}

		*colon = '\0';

		if (output->stopcount == stopcapacity) {
			int grown_cap = stopcapacity == 0 ? 4 : stopcapacity * 2;
			GradientStop *grown = realloc(
				output->stops, (size_t)grown_cap * sizeof(GradientStop));

			if (grown == NULL) {
				free(copy);
				free(output->stops);
				output->stops = NULL;
				output->stopcount = 0;
				return false;
			}

			output->stops = grown;
			stopcapacity = grown_cap;
		}
		GradientStop *stop = &output->stops[output->stopcount++];
		if (*(colon + 1) == '\0' ||
			!texture_rgba_from_hex(token, stop->color)) {
			free(copy);
			free(output->stops);
			output->stops = NULL;
			output->stopcount = 0;
			return false;
		}
		stop->degree = fmodf(strtof(colon + 1, NULL), 360.0f);
		if (stop->degree < 0.0f)
			stop->degree += 360.0f;
	}
	free(copy);
	return true;
}



// general texture engine
static struct TextureOps texture_styles[TEXTURE_STYLE_COUNT];

static struct TextureOps *texture_style_ops(TextureStyle style) {
	if (style >= TEXTURE_STYLE_COUNT)
		return NULL;
	return &texture_styles[style];
}

void texture_style_register(TextureStyle style, struct TextureOps ops) {
	struct TextureOps *slot = texture_style_ops(style);
	if (slot == NULL)
		return;
	*slot = ops;
}

bool texture_key_empty(const BorderTextureKey *key) {
	if (key == NULL)
		return true;
	struct TextureOps *ops = texture_style_ops(key->style);
	if (ops == NULL)
		return true;
	return ops->key_empty(key);
}

bool texture_key_equal(const BorderTextureKey *a, const BorderTextureKey *b) {
	if (a == b)
		return true;
	if (a == NULL || b == NULL)
		return false;
	if (a->style != b->style)
		return false;
	struct TextureOps *ops = texture_style_ops(a->style);
	if (ops == NULL)
		return false;
	return ops->key_equal(a, b);
}

bool texture_key_copy(const BorderTextureKey *source,
					  BorderTextureKey *destination) {
	if (source == NULL || destination == NULL)
		return false;
	struct TextureOps *ops = texture_style_ops(source->style);
	if (ops == NULL)
		return false;
	return ops->key_copy(source, destination);
}

void texture_key_destroy(BorderTextureKey *key) {
	if (key == NULL)
		return;
	struct TextureOps *ops = texture_style_ops(key->style);
	if (ops == NULL)
		return;
	ops->key_destroy(key);
}

bool texture_style_bypasses_cache(TextureStyle style) {
	struct TextureOps *ops = texture_style_ops(style);
	if (ops == NULL)
		return false;
	return ops->bypass_cache;
}

bool texture_style_skip_make_ring(TextureStyle style) {
	struct TextureOps *ops = texture_style_ops(style);
	if (ops == NULL)
		return false;
	return ops->skip_make_ring;
}

static uint32_t texture_hash_bytes(uint32_t hash, const void *bytes,
								   size_t length) {
	const uint8_t *current = bytes;
	for (size_t index = 0; index < length; index++)
		hash = (hash ^ current[index]) * 16777619u;
	return hash;
}

static uint32_t texture_key_hash(const BorderTextureKey *key) {
	uint32_t hash = texture_hash_bytes(2166136261u, &key->style,
									   sizeof(key->style));
	if (key->gradient.stopcount > 0) {
		hash = texture_hash_bytes(hash, &key->gradient.stopcount,
								  sizeof(key->gradient.stopcount));
		for (int index = 0; index < key->gradient.stopcount; index++) {
			GradientStop *stop = &key->gradient.stops[index];
			hash = texture_hash_bytes(hash, stop->color, sizeof(stop->color));
			hash = texture_hash_bytes(hash, &stop->degree,
									  sizeof(stop->degree));
		}
		return hash;
	}
	if (key->string != NULL) {
		hash = texture_hash_bytes(hash, key->string, strlen(key->string));
		return hash;
	}
	return hash;
}

static size_t texture_cache_lower_bound(uint32_t hash) {
	size_t low = 0;
	size_t high = texture_cache_count;
	while (low < high) {
		size_t middle = low + (high - low) / 2;
		if (texture_cache[middle].hash < hash)
			low = middle + 1;
		else
			high = middle;
	}
	return low;
}


static bool texture_cache_store(const BorderTextureKey *key,
								struct wlr_buffer *canvas) {
	if (texture_cache_count == texture_cache_cap) {
		size_t new_cap = texture_cache_cap ? texture_cache_cap * 2 : 8;
		struct TextureCacheEntry *grown =
			realloc(texture_cache, new_cap * sizeof(*grown));
		if (grown == NULL)
			return false;
		texture_cache = grown;
		texture_cache_cap = new_cap;
	}
	
	uint32_t hash = texture_key_hash(key);
	size_t position = texture_cache_lower_bound(hash);

	memmove(&texture_cache[position + 1], &texture_cache[position], (texture_cache_count -  position) * sizeof(struct TextureCacheEntry));
	texture_cache_count++;

	struct TextureCacheEntry *entry = &texture_cache[position];
	memset(entry, 0, sizeof(*entry));
	if (!texture_key_copy(key, &entry->key)) {
		memmove(&texture_cache[position], &texture_cache[position + 1], (texture_cache_count - 1 - position) * sizeof(struct TextureCacheEntry));
		texture_cache_count--;
		return false;
	}
	entry->hash = hash;
	entry->canvas = canvas;
	entry->generation = 0;
	return true;
}

struct wlr_buffer *texture_cache_get(const BorderTextureKey *key,
									 Client *target, bool *from_cache) {
	if (key == NULL || from_cache == NULL)
		return NULL;
	*from_cache = false;

	struct TextureOps *ops = texture_style_ops(key->style);
	if (ops == NULL)
		return NULL;

	if (ops->key_empty(key))
		return NULL;

	if (ops->bypass_cache)
		return ops->render(key, target);

	uint32_t hash = texture_key_hash(key);
	size_t index = texture_cache_lower_bound(hash);
	for (; index < texture_cache_count && texture_cache[index].hash == hash; index++) {
		if (texture_key_equal(&texture_cache[index].key, key)) {
			*from_cache = true;
			wlr_buffer_lock(texture_cache[index].canvas);
			return texture_cache[index].canvas;
		}
	}


	struct wlr_buffer *canvas = ops->render(key, target);
	if (canvas == NULL)
		return NULL;
	if (!texture_cache_store(key, canvas)) {
		wlr_buffer_drop(canvas);
		return NULL;
	}
	wlr_buffer_lock(canvas);
	return canvas;
}


static void texture_cache_count_use(const BorderTextureKey *key) {
	if (texture_key_empty(key))
		return;
	uint32_t hash = texture_key_hash(key);
	size_t index = texture_cache_lower_bound(hash);
	for (; index < texture_cache_count &&
		   texture_cache[index].hash == hash; index++)
		if (texture_key_equal(&texture_cache[index].key, key))
			texture_cache[index].generation = texture_generation;
}


static void texture_use_count(void) {
	texture_generation++;

	Client *client;
	wl_list_for_each(client, &server.clients, link) {
		for (int slot = 0; slot < MANGO_TEXTURE_SLOTS; slot++) {
			texture_cache_count_use(&client->active_textures[slot]);
			texture_cache_count_use(&client->inactive_textures[slot]);
		}
	}
}
static bool texture_style_persists(TextureStyle style) {
    switch (style) {
    case TEXTURE_STORE_IMAGE:
    case TEXTURE_SEGMENT_TILE_TOP:
    case TEXTURE_SEGMENT_TILE_BOTTOM:
    case TEXTURE_SEGMENT_TILE_LEFT:
    case TEXTURE_SEGMENT_TILE_RIGHT:
    case TEXTURE_SEGMENT_TILE_TL:
    case TEXTURE_SEGMENT_TILE_TR:
    case TEXTURE_SEGMENT_TILE_BL:
    case TEXTURE_SEGMENT_TILE_BR:
        return true;
    default:
        return false;
    }
}


void texture_collect_garbage(bool clean_image_store) {
	texture_use_count();
	size_t write = 0;

	for (size_t read = 0; read < texture_cache_count; read++) {
		struct TextureCacheEntry *entry = &texture_cache[read];

		bool persists = texture_style_persists(entry->key.style);
		if (entry->generation != texture_generation &&
			(!persists || clean_image_store)) {
			texture_key_destroy(&entry->key);
			wlr_buffer_drop(entry->canvas);
			continue;
		}
		if (write != read)
			texture_cache[write] = texture_cache[read];
		write++;
	}
	texture_cache_count = write;
}

void texture_cache_teardown(void) {
	for (size_t index = 0; index < texture_cache_count; index++) {
		texture_key_destroy(&texture_cache[index].key);
		wlr_buffer_drop(texture_cache[index].canvas);
	}
	free(texture_cache);
	texture_cache = NULL;
	texture_cache_count = 0;
	texture_cache_cap = 0;
}

static TextureStyle texture_style_from_name(const char *name) {
	for (size_t index = 0;
		 index < sizeof(texture_style_names) / sizeof(texture_style_names[0]);
		 ++index) {
		if (strcmp(name, texture_style_names[index].name) == 0)
			return texture_style_names[index].style;
	}
	return TEXTURE_STYLE_COUNT;
}

bool texture_parse_value(const char *value, BorderTextureKey *out) {
	const char *comma = strchr(value, ',');
	if (comma == NULL)
		return false;
	char *type = strndup(value, comma - value);
	if (type == NULL)
		return false;
	TextureStyle style = texture_style_from_name(type);
	free(type);
	if (style == TEXTURE_STYLE_COUNT)
		return false;
	free(out->string);
	out->style = style;
	out->string = strdup(comma + 1);
	if (out->string == NULL)
		return false;
	if (style == TEXTURE_CONIC_GRADIENT) {
		bool parsed = texture_parse_gradient(out->string, &out->gradient);
		if (!parsed) {
			free(out->string);
			out->string = NULL;
		}
		return parsed;
	}
	return true;
}

//  string texture key helpers
bool string_key_empty(const BorderTextureKey *key) {
	return key->string == NULL || key->string[0] == '\0';
}

bool string_key_equal(const BorderTextureKey *a,
							 const BorderTextureKey *b) {
	if (a->style != b->style)
		return false;
	if (a->string == NULL || b->string == NULL)
		return a->string == b->string;
	return strcmp(a->string, b->string) == 0;
}

bool string_key_copy(const BorderTextureKey *source,
							BorderTextureKey *destination) {
	destination->style = source->style;
	destination->string = NULL;
	if (source->string == NULL)
		return true;

	destination->string = strdup(source->string);
	return destination->string != NULL;
}

void string_key_destroy(BorderTextureKey *key) {
	free(key->string);
	key->string = NULL;
}

// register texture styles here

void init_texture_system(void) {
	struct TextureOps linear_gradient_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = true,
		.render = texture_render_linear,
	};
	struct TextureOps radial_gradient_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = true,
		.render = texture_render_radial,
	};
	struct TextureOps tiled_image_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = false,
		.render = texture_render_tile,
	};
	struct TextureOps fit_image_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = true,
		.render = texture_render_fit,
	};
	struct TextureOps fit_image_opacity_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = true,
		.skip_make_ring = true,
		.render = texture_render_fit_opacity,
	};
	struct TextureOps tile_image_opacity_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = false,
		.skip_make_ring = true,
		.render = texture_render_tile_opacity,
	};
	struct TextureOps segment_image_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = true,
		.skip_make_ring = true,
		.render = texture_render_segment,
	};
	struct TextureOps conic_gradient_ops = {
		.key_empty = gradient_key_empty,
		.key_equal = gradient_key_equal,
		.key_copy = gradient_key_copy,
		.key_destroy = gradient_key_destroy,
		.bypass_cache = true,
		.skip_make_ring = true,
		.render = texture_render_conic,
	};
	struct TextureOps static_image_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = false,
		.render = texture_render_image_static,
	};
	struct TextureOps solid_color_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = true,
		.render = texture_render_solid,
	};
	struct TextureOps solid_noclip_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = true,
		.skip_make_ring = true,
		.render = texture_render_color_noclip,
	};
	struct TextureOps segment_color_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = true,
		.render = texture_render_color_segments,
	};
	struct TextureOps store_image_ops = {
		.key_empty = string_key_empty,
		.key_equal = string_key_equal,
		.key_copy = string_key_copy,
		.key_destroy = string_key_destroy,
		.bypass_cache = false,
		.render = texture_render_store_image,
	};

	texture_style_register(TEXTURE_LINEAR_GRADIENT, linear_gradient_ops);
	texture_style_register(TEXTURE_RADIAL_GRADIENT, radial_gradient_ops);
	texture_style_register(TEXTURE_TILED_IMAGE, tiled_image_ops);
	texture_style_register(TEXTURE_FIT_IMAGE, fit_image_ops);
	texture_style_register(TEXTURE_FIT_OPACITY, fit_image_opacity_ops);
	texture_style_register(TEXTURE_TILE_OPACITY, tile_image_opacity_ops);
	texture_style_register(TEXTURE_SEGMENT_IMAGE, segment_image_ops);
	texture_style_register(TEXTURE_CONIC_GRADIENT, conic_gradient_ops);
	texture_style_register(TEXTURE_STATIC_IMAGE, static_image_ops);
	texture_style_register(TEXTURE_SOLID, solid_color_ops);
	texture_style_register(TEXTURE_COLOR_NOCLIP, solid_noclip_ops);
	texture_style_register(TEXTURE_COLOR_SEGMENT, segment_color_ops);
	texture_style_register(TEXTURE_STORE_IMAGE, store_image_ops);

	 const struct {
        TextureStyle style;
        struct wlr_buffer *(*render)(const BorderTextureKey *, Client *);
    } segment_tile_styles[] = {
        { TEXTURE_SEGMENT_TILE_TOP, texture_render_segment_top },
        { TEXTURE_SEGMENT_TILE_BOTTOM, texture_render_segment_bottom },
        { TEXTURE_SEGMENT_TILE_LEFT, texture_render_segment_left },
        { TEXTURE_SEGMENT_TILE_RIGHT, texture_render_segment_right },
        { TEXTURE_SEGMENT_TILE_TL, texture_render_segment_tl },
        { TEXTURE_SEGMENT_TILE_TR, texture_render_segment_tr },
        { TEXTURE_SEGMENT_TILE_BL, texture_render_segment_bl },
        { TEXTURE_SEGMENT_TILE_BR, texture_render_segment_br },
    };

    for (size_t index = 0;
         index < sizeof(segment_tile_styles) / sizeof(segment_tile_styles[0]);
         index++) {
        struct TextureOps tile_ops = {
            .key_empty = string_key_empty,
            .key_equal = string_key_equal,
            .key_copy = string_key_copy,
            .key_destroy = string_key_destroy,
            .skip_make_ring = true,
            .render = segment_tile_styles[index].render,
        };
        texture_style_register(segment_tile_styles[index].style, tile_ops);
    }
}