#ifndef __CONFIG_PARSE_CONFIG_H__
#define __CONFIG_PARSE_CONFIG_H__ 1

#include "mango/common/types.h"
#include "mango/dispatch/bind.h"
#include <scenefx/types/fx/blur_data.h>
#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

#include "mango/draw/text-node.h"

/* Macros */
// Integer version: truncates the fractional part.
#define CLAMP_INT(x, min, max)                                                 \
	((int32_t)(x) < (int32_t)(min)                                             \
		 ? (int32_t)(min)                                                      \
		 : ((int32_t)(x) > (int32_t)(max) ? (int32_t)(max) : (int32_t)(x)))

// Float version: keeps the fractional part.
#define CLAMP_FLOAT(x, min, max)                                               \
	((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

/* Rule application helpers (window / layer rules). */
#define APPLY_INT_PROP(obj, rule, prop)                                        \
	if (rule->prop >= 0)                                                       \
	obj->prop = rule->prop

#define APPLY_FLOAT_PROP(obj, rule, prop)                                      \
	if (rule->prop > 0.0f)                                                     \
	obj->prop = rule->prop

#define APPLY_STRING_PROP(obj, rule, prop)                                     \
	if (rule->prop != NULL)                                                    \
	obj->prop = rule->prop

/* Tag animation / folding / global switch state. */
enum { VERTICAL, HORIZONTAL };
enum { UNFOLD, FOLD, INVALIDFOLD };
enum { STATE_UNSPECIFIED = 0, STATE_ENABLED, STATE_DISABLED };

enum tearing_mode {
	TEARING_DISABLED = 0,
	TEARING_ENABLED,
	TEARING_FULLSCREEN_ONLY,
};

enum seat_config_shortcuts_inhibit {
	SHORTCUTS_INHIBIT_DISABLE,
	SHORTCUTS_INHIBIT_ENABLE,
};

/* Variables */
// Default jump-label character sequence (static array, used when jump_labels is
// not configured).
extern const char default_jump_labels[];

/* Enums */
enum { NUM_TYPE_MINUS, NUM_TYPE_PLUS, NUM_TYPE_DEFAULT };

enum { KEY_TYPE_CODE, KEY_TYPE_SYM };

enum render_bit_depth {
	MANGO_RENDER_BIT_DEPTH_DEFAULT = 0,
	MANGO_RENDER_BIT_DEPTH_8,
	MANGO_RENDER_BIT_DEPTH_10,
};

/* Functions */
typedef void (*FuncType)(const Arg *);

typedef struct {
	uint32_t keycode1;
	uint32_t keycode2;
	uint32_t keycode3;
} MultiKeycode;

typedef struct {
	xkb_keysym_t keysym;
	MultiKeycode keycode;
	int32_t type;
} KeySymCode;

typedef struct {
	uint32_t mod;
	KeySymCode keysymcode;
	void (*func)(const Arg *);
	Arg arg;
	char mode[28];
	bool iscommonmode;
	bool isdefaultmode;
	bool islockapply;
	bool isreleaseapply;
	bool ispassapply;
	bool isallowconflict;
	int line_number;
	int file_index;
} KeyBinding;

typedef struct {
	const char *id;
	const char *title;
	int32_t is_once;
	int32_t is_once_applied;
	uint32_t tags;
	int32_t isfloating;
	int32_t isfullscreen;
	int32_t isfakefullscreen;
	float scroller_proportion;
	const char *animation_type_open;
	const char *animation_type_close;
	const char *layer_animation_type_open;
	const char *layer_animation_type_close;
	int32_t isnoborder;
	int32_t isnoshadow;
	int32_t isnoradius;
	int32_t isnoanimation;
	int32_t isopensilent;
	int32_t istagsilent;
	int32_t isnamedscratchpad;
	int32_t isunglobal;
	int32_t isglobal;
	int32_t isoverlay;
	int32_t shield_when_capture;
	int32_t allow_shortcuts_inhibit;
	int32_t ignore_maximize;
	int32_t ignore_minimize;
	int32_t isnosizehint;
	int32_t idleinhibit_when_focus;
	int32_t vrr_only_fullscreen;
	int32_t confine_pointer;
	int32_t force_render;
	int32_t activation_bypass;
	char *monitor;
	int32_t offsetx;
	int32_t offsety;
	float width;
	float height;
	int32_t nofocus;
	int32_t nofadein;
	int32_t nofadeout;
	int32_t no_force_center;
	int32_t isterm;
	int32_t allow_csd;
	int32_t force_fakemaximize;
	int32_t force_tiled_state;
	int32_t force_tearing;
	int32_t noswallow;
	int32_t noblur;
	float focused_opacity;
	float unfocused_opacity;
	float scroller_proportion_single;
	uint32_t passmod;
	xkb_keysym_t keysym;
	KeyBinding globalkeybinding;
} ConfigWinRule;

typedef struct {
	char *type;
	char *value;
} ConfigEnv;

typedef struct {
	const char *name;			 // Monitor name
	char *make, *model, *serial; // may be NULL
	int32_t rr;					 // Rotate and flip (assume integer)
	float scale;				 // Monitor scale factor
	int32_t x, y;				 // Monitor position
	int32_t width, height;		 // Monitor resolution
	float refresh;				 // Refresh rate
	int32_t vrr;				 // variable refresh rate
	int32_t custom;				 // enable custom mode
	int32_t hdr;				 // enable hdr mode
	float hdr_min_lum;			 // mastering min luminance, cd/m² (0 = unset)
	float hdr_max_lum;			 // mastering max luminance / max_cll, cd/m²
	float hdr_max_avg_lum;		 // max frame-average light level, cd/m²
	int32_t hdr_force;			 // ignore EDID-derived HDR capability checks
	char *icc;					 // ICC profile path
	int32_t disable;			 // prefer disable
} ConfigMonitorRule;

typedef struct {
	int32_t id;
	bool id_wildcard;
	char *layout_name;
	char *monitor_name;
	char *monitor_make;
	char *monitor_model;
	char *monitor_serial;
	float mfact;
	int32_t nmaster;
	float scroller_default_proportion;
	float scroller_default_proportion_single;
	int32_t scroller_ignore_proportion_single;
	int32_t no_render_border;
	int32_t open_as_floating;
	int32_t no_hide;
} ConfigTagRule;

typedef struct {
	char *layer_name; // Layout name
	char *animation_type_open;
	char *animation_type_close;
	int32_t shield_when_capture;
	int32_t animation_direction;
	int32_t noblur;
	int32_t noanim;
	int32_t noshadow;
} ConfigLayerRule;

typedef struct {
	/*
	 * Match condition: name matches the device name or the vendor:product:name
	 * identifier; type matches
	 * keyboard/pointer/trackpad/touch/switch/tablet/pad.
	 */
	char *name;
	char type[32];

	/* Keyboard parameters (-1 / empty string means unset and falls back to the
	 * global default). */
	int32_t repeat_rate;
	int32_t repeat_delay;
	char kb_rules[128];
	char kb_model[128];
	char kb_layout[128];
	char kb_variant[128];
	char kb_options[128];

	/* Mouse / trackpad libinput parameters. */
	int32_t natural_scrolling;
	int32_t accel_profile;
	double accel_speed;
	int32_t left_handed;
	int32_t middle_button_emulation;
	uint32_t scroll_method;
	uint32_t scroll_button;
	uint32_t click_method;
	uint32_t send_events_mode;
	int32_t tap_to_click;
	int32_t tap_and_drag;
	int32_t drag_lock;
	uint32_t button_map;
	int32_t disable_while_typing;
	char monitor[128];
} ConfigDeviceRule;

typedef struct {
	uint32_t mod;
	uint32_t dir;
	void (*func)(const Arg *);
	Arg arg;
	char mode[28];
	bool iscommonmode;
	bool isdefaultmode;
	int line_number;
	int file_index;
} AxisBinding;

typedef struct {
	uint32_t mod;
	uint32_t motion;
	uint32_t fingers_count;
	void (*func)(const Arg *);
	Arg arg;
	char mode[28];
	bool iscommonmode;
	bool isdefaultmode;
	int line_number;
	int file_index;
} GestureBinding;

typedef struct {
	uint32_t fold;
	void (*func)(const Arg *);
	Arg arg;
	char mode[28];
	bool iscommonmode;
	bool isdefaultmode;
	int line_number;
	int file_index;
} SwitchBinding;

typedef struct {
	uint32_t mod;
	uint32_t button;
	void (*func)(const Arg *);
	Arg arg;
	char mode[28];
	bool iscommonmode;
	bool isdefaultmode;
	int line_number;
	int file_index;
} MouseBinding;

typedef struct {
	int32_t animations;
	int32_t layer_animations;
	char animation_type_open[10];
	char animation_type_close[10];
	char layer_animation_type_open[10];
	char layer_animation_type_close[10];
	int32_t animation_fade_in;
	int32_t animation_fade_out;
	int32_t tag_animation_direction;
	float zoom_initial_ratio;
	float zoom_end_ratio;
	float fadein_begin_opacity;
	float fadeout_begin_opacity;
	uint32_t animation_duration_move;
	uint32_t animation_duration_open;
	uint32_t animation_duration_tag;
	uint32_t animation_duration_close;
	uint32_t animation_duration_focus;
	double animation_curve_move[4];
	double animation_curve_open[4];
	double animation_curve_tag[4];
	double animation_curve_close[4];
	double animation_curve_focus[4];
	double animation_curve_opafadein[4];
	double animation_curve_opafadeout[4];

	int32_t scroller_structs;
	float scroller_default_proportion;
	float scroller_default_proportion_single;
	int32_t scroller_ignore_proportion_single;
	int32_t scroller_focus_center;
	int32_t scroller_prefer_center;
	int32_t scroller_prefer_overspread;
	int32_t edge_scroller_pointer_focus;
	double edge_scroller_focus_allow_speed;
	int32_t focus_cross_monitor;
	int32_t focusdir_only_zone_overlap;
	int32_t exchange_cross_monitor;
	int32_t scratchpad_cross_monitor;
	int32_t focus_cross_tag;
	int32_t view_current_to_back;
	int32_t no_border_when_single;
	int32_t no_radius_when_single;
	int32_t snap_distance;
	int32_t enable_floating_snap;
	int32_t drag_tile_to_tile;
	int32_t drag_tile_small;
	uint32_t swipe_min_threshold;
	int32_t gesture_live;
	uint32_t gesture_swipe_distance;
	double gesture_swipe_cancel_ratio;
	double gesture_swipe_min_speed_to_force;
	float focused_opacity;
	float unfocused_opacity;
	float *scroller_proportion_preset;
	int32_t scroller_proportion_preset_count;

	char **circle_layout;
	int32_t circle_layout_count;

	uint32_t new_is_master;
	float default_mfact;
	uint32_t default_nmaster;
	int32_t tag_num;	// Configurable tag count, range 1..tag_num_MAX.
	int32_t tag_gather; // Compact tags to remove gaps
	int32_t center_master_overspread;
	int32_t center_when_single_stack;

	/* dwindle layout */
	int32_t dwindle_vsplit;
	int32_t dwindle_hsplit;
	int32_t dwindle_preserve_split;
	int32_t dwindle_smart_split;
	int32_t dwindle_smart_resize;
	int32_t dwindle_drop_simple_split;
	int32_t dwindle_manual_split;
	float dwindle_split_ratio;

	int32_t hotarea_size;
	int32_t hotarea_corner;
	int32_t enable_hotarea;

	int32_t overviewgappi;
	int32_t overviewgappo;
	float overcircle_center_ratio;
	char *jump_labels;
	uint32_t cursor_hide_timeout;
	uint32_t cursor_hide_on_keypress;

	uint32_t axis_bind_apply_timeout;
	uint32_t focus_on_activate;
	int32_t idleinhibit_ignore_visible;
	int32_t sloppyfocus;
	int32_t warpcursor;
	int32_t drag_corner;
	int32_t drag_warp_cursor;

	/* keyboard */
	int32_t repeat_rate;
	int32_t repeat_delay;
	uint32_t numlockon;

	/* common pointer */
	uint32_t send_events_mode;

	/* mouse */
	int32_t mouse_natural_scrolling;
	uint32_t mouse_accel_profile;
	double mouse_accel_speed;
	double axis_scroll_factor;
	/* Mouse-specific parameters. */
	int32_t mouse_left_handed;
	int32_t mouse_middle_button_emulation;
	uint32_t mouse_scroll_method;
	uint32_t mouse_scroll_button;
	uint32_t mouse_click_method;
	uint32_t mouse_send_events_mode;

	/* Trackpad */
	int32_t trackpad_natural_scrolling;
	uint32_t trackpad_accel_profile;
	double trackpad_accel_speed;
	double trackpad_scroll_factor;
	int32_t disable_trackpad;
	int32_t tap_to_click;
	int32_t tap_and_drag;
	int32_t drag_lock;
	uint32_t button_map;
	/* Trackpad-specific parameters. */
	int32_t trackpad_left_handed;
	int32_t trackpad_middle_button_emulation;
	int32_t trackpad_disable_while_typing;
	uint32_t trackpad_scroll_method;
	uint32_t trackpad_scroll_button;
	uint32_t trackpad_click_method;
	uint32_t trackpad_send_events_mode;

	/* touch */
	int32_t touch_enable;
	int32_t touch_enable_mouse_emulation;

	/* window effects */
	int32_t blur;
	int32_t blur_layer;
	int32_t blur_optimized;
	int32_t border_radius;
	int32_t border_radius_location_default;
	struct blur_data blur_params;
	int32_t shadows;
	int32_t shadow_only_floating;
	int32_t layer_shadows;
	uint32_t shadows_size;
	float shadows_blur;
	int32_t shadows_position_x;
	int32_t shadows_position_y;
	float shadowscolor[4];

	/* appearance */
	int32_t smartgaps;
	uint32_t gappih;
	uint32_t gappiv;
	uint32_t gappoh;
	uint32_t gappov;
	uint32_t special_gappih;
	uint32_t special_gappiv;
	uint32_t special_gappoh;
	uint32_t special_gappov;
	uint32_t borderpx;
	uint32_t group_bar_height;
	float scratchpad_width_ratio;
	float scratchpad_height_ratio;
	float special_dim;
	float rootcolor[4];
	float bordercolor[4];
	float dropcolor[4];
	float splitcolor[4];
	float focuscolor[4];
	float maximizescreencolor[4];
	float urgentcolor[4];
	float scratchpadcolor[4];
	float globalcolor[4];
	float overlaycolor[4];

	int32_t log_level;
	uint32_t capslock;

	ConfigTagRule *tag_rules; // dynamic array
	int32_t tag_rules_count;  // count

	ConfigLayerRule *layer_rules; // dynamic array
	int32_t layer_rules_count;	  // count

	ConfigWinRule *window_rules;
	int32_t window_rules_count;

	ConfigMonitorRule *monitor_rules; // dynamic array
	int32_t monitor_rules_count;	  // count

	ConfigDeviceRule *device_rules; // dynamic array
	int32_t device_rules_count;		// count

	KeyBinding *key_bindings;
	int32_t key_bindings_count;

	MouseBinding *mouse_bindings;
	int32_t mouse_bindings_count;

	AxisBinding *axis_bindings;
	int32_t axis_bindings_count;

	SwitchBinding *switch_bindings;
	int32_t switch_bindings_count;

	GestureBinding *gesture_bindings;
	int32_t gesture_bindings_count;

	ConfigEnv **env;
	int32_t env_count;

	char **exec;
	int32_t exec_count;

	char **exec_once;
	int32_t exec_once_count;

	char *cursor_theme;
	uint32_t cursor_size;

	int32_t single_scratchpad;
	int32_t xwayland_persistence;
	int32_t xwayland_ignore_scale;
	int32_t syncobj_enable;
	int32_t tag_carousel;
	float drag_tile_refresh_interval;
	float drag_floating_refresh_interval;
	int32_t allow_tearing;
	int32_t allow_shortcuts_inhibit;
	int32_t allow_lock_transparent;

	struct xkb_rule_names xkb_rules;
	char xkb_rules_rules[128];
	char xkb_rules_model[128];
	char xkb_rules_layout[128];
	char xkb_rules_variant[128];
	char xkb_rules_options[128];

	char keymode[28];

	struct xkb_context *ctx;
	struct xkb_keymap *keymap;
	DecorateDrawData jumplabeldata;
	DecorateDrawData groupbardata;

	int32_t hdr_depth;
} Config;

/* Global config instance. */
extern Config config;

typedef struct {
	const char *mode;
	bool iscommonmode;
	int file_index;
	int line_number;
} BindingConflictMeta;
typedef void (*BindingMetaFunc)(const void *elem, BindingConflictMeta *meta);

ConfigDeviceRule *find_device_rule(struct wlr_input_device *device);
bool device_rule_has_keyboard_settings(ConfigDeviceRule *rule);
void standalone_keyboard_apply_config(KeyboardGroup *group,
									  ConfigDeviceRule *rule);
void create_standalone_keyboard(InputDevice *input_dev,
								struct wlr_keyboard *keyboard,
								ConfigDeviceRule *rule);
void handle_standalone_keyboard_destroy(struct wl_listener *listener,
										void *data);

bool apply_rule_to_state(Monitor *m, const ConfigMonitorRule *rule,
						 struct wlr_output_state *state);
bool monitor_matches_rule(Monitor *m, const ConfigMonitorRule *rule);
void sync_workspaces_to_tag_num(Monitor *m);

void trim_whitespace(char *str);

void remove_comment(char *str);

int32_t parse_double_array(const char *input, double *output,
						   int32_t max_count);

char *sanitize_string(char *str);

void parse_bind_flags(const char *str, KeyBinding *kb);

int32_t parse_circle_direction(const char *str);

int32_t parse_direction(const char *str);

int32_t parse_force(const char *str);

int32_t parse_fold_state(const char *str);

int64_t parse_color(const char *hex_str);

uint32_t parse_mod(const char *mod_str);

void cleanup_config_keymap(void);

KeySymCode parse_key(const char *key_str, bool isbindsym);

uint32_t parse_button(const char *str);

int32_t parse_mouse_action(const char *str);

void convert_hex_to_rgba(float *color, uint32_t hex);

uint32_t parse_num_type(char *str);

uint32_t parse_tag_mask(char *str);
bool check_key_binding_conflicts(Config *config);

bool check_mouse_binding_conflicts(Config *config);

bool check_axis_binding_conflicts(Config *config);

bool check_switch_binding_conflicts(Config *config);

bool check_gesture_binding_conflicts(Config *config);

void free_circle_layout(Config *config);

void free_baked_points(void);
void free_config(void);

void update_global_var(void);

void override_config(void);

void set_value_default();

void set_default_key_bindings(Config *config);

bool parse_config(void);

void reset_blur_params(void);

void reapply_monitor_rules(void);

void set_xcursor_env();

void reapply_cursor_style(void);

void reapply_rootbg(void);
void reapply_property(void);

void reapply_keyboard(void);

void reapply_pointer(void);

void reapply_master(void);

void parse_tagrule(Monitor *m);

void run_exec();

void run_exec_once();

bool parse_option(Config *config, char *key, char *value, int line_number);

bool parse_config_line(Config *config, const char *line, int line_number);

bool parse_config_file(Config *config, const char *file_path, bool must_exist);
void reapply_tagrule(void);

void reset_option(void);

void reset_tag(int old_tag_num);

void reload_config(const Arg *arg);

void tag_slot_set_defaults(Monitor *m, uint32_t tag);

bool tag_rule_matches_monitor(const ConfigTagRule *tr, Monitor *m);

void tag_rule_apply_to_slot(Monitor *m, const ConfigTagRule *tr, uint32_t tag);
void set_env_without_display();

void set_env_display();

FuncType parse_func_name(char *func_name, Arg *arg, char *arg_value,
						 char *arg_value2, char *arg_value3, char *arg_value4,
						 char *arg_value5);
bool check_simple_binding_conflicts(void *arr, size_t count, size_t elem_size,
									bool (*same_key)(const void *,
													 const void *),
									BindingMetaFunc get_meta, const char *kind);
#endif
