#ifndef MANGO_TEXTURE_H
#define MANGO_TEXTURE_H

#include <stdbool.h>
#include <stdint.h>

#define MANGO_COLOR_COMPONENTS 4
#define MANGO_TEXTURE_SLOTS 3

struct Client;
struct wlr_buffer;

typedef struct {
	float color[MANGO_COLOR_COMPONENTS];
	float degree;
} GradientStop;

typedef struct {
	GradientStop *stops;
	int stopcount;
} GradientBorder;

typedef enum {
	TEXTURE_LINEAR_GRADIENT,
	TEXTURE_RADIAL_GRADIENT,
	TEXTURE_CONIC_GRADIENT,
	TEXTURE_TILED_IMAGE,
	TEXTURE_FIT_IMAGE,
	TEXTURE_FIT_OPACITY,
	TEXTURE_TILE_OPACITY,
	TEXTURE_SEGMENT_IMAGE,
	TEXTURE_SOLID,
	TEXTURE_COLOR_NOCLIP,
	TEXTURE_STATIC_IMAGE,
	TEXTURE_COLOR_SEGMENT,
	TEXTURE_STORE_IMAGE,
	TEXTURE_STORE_IMAGE_SCALED,
	TEXTURE_SEGMENT_TILE_TOP,
	TEXTURE_SEGMENT_TILE_BOTTOM,
	TEXTURE_SEGMENT_TILE_LEFT,
	TEXTURE_SEGMENT_TILE_RIGHT,
	TEXTURE_SEGMENT_TILE_TL,
	TEXTURE_SEGMENT_TILE_TR,
	TEXTURE_SEGMENT_TILE_BL,
	TEXTURE_SEGMENT_TILE_BR,
	TEXTURE_STYLE_COUNT,
} TextureStyle;

// never initialize positionally or memcmp/memcpy this whole struct
// this will cause breakage if another field is ever added to this struct.
typedef struct {
	TextureStyle style;
	GradientBorder gradient;
	char *string;
} BorderTextureKey;

struct TextureOps {
	bool (*key_empty)(const BorderTextureKey *key);
	bool (*key_equal)(const BorderTextureKey *a, const BorderTextureKey *b);
	bool (*key_copy)(const BorderTextureKey *source,
					 BorderTextureKey *destination);
	void (*key_destroy)(BorderTextureKey *key);
	bool bypass_cache;
	bool skip_make_ring;
	struct wlr_buffer *(*render)(const BorderTextureKey *key,
								 struct Client *target);
};
void texture_style_register(TextureStyle style, struct TextureOps ops);
struct wlr_buffer *texture_cache_get(const BorderTextureKey *key,
									 struct Client *target, bool *from_cache);
void init_texture_system(void);
bool texture_style_bypasses_cache(TextureStyle style);
bool texture_style_skip_make_ring(TextureStyle style);

bool texture_key_empty(const BorderTextureKey *key);
bool texture_key_equal(const BorderTextureKey *a, const BorderTextureKey *b);
bool texture_key_copy(const BorderTextureKey *source,
					  BorderTextureKey *destination);
void texture_key_destroy(BorderTextureKey *key);
bool texture_parse_value(const char *value, BorderTextureKey *out);
struct wlr_buffer *texture_make_ring(struct wlr_buffer *canvas, int width,
									 int height, int border, double corners,
									 bool stamp_inside);
void texture_collect_garbage(bool clean_image_store);
void texture_cache_teardown(void);

struct wlr_buffer *texture_composite_slots(const BorderTextureKey slots[MANGO_TEXTURE_SLOTS], struct Client *target, int width, int height);

#endif