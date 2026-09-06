#include <ctype.h>
#include <libgen.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

// 整数版本 - 截断小数部分
#define CLAMP_INT(x, min, max)                                                 \
	((int32_t)(x) < (int32_t)(min)                                             \
		 ? (int32_t)(min)                                                      \
		 : ((int32_t)(x) > (int32_t)(max) ? (int32_t)(max) : (int32_t)(x)))

// 浮点数版本 - 保留小数部分
#define CLAMP_FLOAT(x, min, max)                                               \
	((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

enum { NUM_TYPE_MINUS, NUM_TYPE_PLUS, NUM_TYPE_DEFAULT };

enum { KEY_TYPE_CODE, KEY_TYPE_SYM };

enum render_bit_depth {
	MANGO_RENDER_BIT_DEPTH_DEFAULT = 0,
	MANGO_RENDER_BIT_DEPTH_8,
	MANGO_RENDER_BIT_DEPTH_10,
};
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
	char *type;
	char *value;
} ConfigEnv;

typedef struct {
	const char *id;
	const char *title;
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
	/* 匹配条件：name 匹配设备名或 vendor:product:name 标识符；
	 * type 匹配 keyboard/pointer/touchpad/touch/switch/tablet/pad */
	char *name;
	char type[32];

	/* 键盘参数（-1 / 空串 表示未设置，沿用全局默认） */
	int32_t repeat_rate;
	int32_t repeat_delay;
	char kb_rules[128];
	char kb_model[128];
	char kb_layout[128];
	char kb_variant[128];
	char kb_options[128];

	/* 鼠标/触摸板 libinput 参数 */
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
} ConfigDeviceRule;

static ConfigDeviceRule *find_device_rule(struct wlr_input_device *device);
static bool device_rule_has_keyboard_settings(ConfigDeviceRule *rule);
static void standalone_keyboard_apply_config(KeyboardGroup *group,
											 ConfigDeviceRule *rule);
static void create_standalone_keyboard(InputDevice *input_dev,
									   struct wlr_keyboard *keyboard,
									   ConfigDeviceRule *rule);
static void destroy_standalone_keyboard(struct wl_listener *listener,
										void *data);
static void create_standalone_keyboard(InputDevice *input_dev,
									   struct wlr_keyboard *keyboard,
									   ConfigDeviceRule *rule);
static void destroy_standalone_keyboard(struct wl_listener *listener,
										void *data);

// 修改后的宏定义
#define CHVT(n)                                                                \
	{                                                                          \
		WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT,                                  \
			{.keysym = XKB_KEY_XF86Switch_VT_##n, .type = KEY_TYPE_SYM}, chvt, \
		{                                                                      \
			.ui = (n)                                                          \
		}                                                                      \
	}

// 默认按键绑定数组
KeyBinding default_key_bindings[] = {CHVT(1), CHVT(2),	CHVT(3),  CHVT(4),
									 CHVT(5), CHVT(6),	CHVT(7),  CHVT(8),
									 CHVT(9), CHVT(10), CHVT(11), CHVT(12)};

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
	char *layer_name; // 布局名称
	char *animation_type_open;
	char *animation_type_close;
	int32_t shield_when_capture;
	int32_t noblur;
	int32_t noanim;
	int32_t noshadow;
} ConfigLayerRule;

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
	float focused_opacity;
	float unfocused_opacity;
	float *scroller_proportion_preset;
	int32_t scroller_proportion_preset_count;

	char **circle_layout;
	int32_t circle_layout_count;

	uint32_t new_is_master;
	float default_mfact;
	uint32_t default_nmaster;
	int32_t tag_num;	// 可配置的 tag 数量,范围 1..tag_num_MAX
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
	int32_t smartkill;
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
	/* 鼠标独立参数 */
	int32_t mouse_left_handed;
	int32_t mouse_middle_button_emulation;
	uint32_t mouse_scroll_method;
	uint32_t mouse_scroll_button;
	uint32_t mouse_click_method;
	uint32_t mouse_send_events_mode;

	/* tablet */
	char *tablet_map_to_mon;

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
	/* 触摸板独立参数 */
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
	char *touch_map_to_mon;

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

	ConfigTagRule *tag_rules; // 动态数组
	int32_t tag_rules_count;  // 数量

	ConfigLayerRule *layer_rules; // 动态数组
	int32_t layer_rules_count;	  // 数量

	ConfigWinRule *window_rules;
	int32_t window_rules_count;

	ConfigMonitorRule *monitor_rules; // 动态数组
	int32_t monitor_rules_count;	  // 条数

	ConfigDeviceRule *device_rules; // 动态数组
	int32_t device_rules_count;		// 条数

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

typedef struct {
	const char *mode;
	bool iscommonmode;
	int file_index;
	int line_number;
} BindingConflictMeta;

typedef void (*BindingMetaFunc)(const void *elem, BindingConflictMeta *meta);

typedef void (*FuncType)(const Arg *);
Config config;

// 默认跳转标签字符序列（静态数组，未配置 jump_labels 时使用）
static const char default_jump_labels[] = "HJKLASDFGQWERTYUIOPZXCVBNM";
static char **file_paths = NULL;
static int file_paths_count = 0;
static int current_file_index = -1;

bool parse_config_file(Config *config, const char *file_path, bool must_exist);
bool apply_rule_to_state(Monitor *m, const ConfigMonitorRule *rule,
						 struct wlr_output_state *state);
bool monitor_matches_rule(Monitor *m, const ConfigMonitorRule *rule);
void sync_workspaces_to_tag_num(Monitor *m);

// Helper function to trim whitespace from start and end of a string
void trim_whitespace(char *str) {
	if (str == NULL || *str == '\0')
		return;

	// Trim leading space
	char *start = str;
	while (isspace((unsigned char)*start)) {
		start++;
	}

	// Trim trailing space
	char *end = str + strlen(str) - 1;
	while (end > start && isspace((unsigned char)*end)) {
		end--;
	}

	// Null-terminate the trimmed string
	*(end + 1) = '\0';

	// Move the trimmed part to the beginning if needed
	if (start != str) {
		memmove(str, start, end - start + 2); // +2 to include null terminator
	}
}

// remove comment, support double quote inside "#xxx" or '#xxx' not be treated
// as comment
void remove_comment(char *str) {
	if (str == NULL || *str == '\0')
		return;

	char quote_char = '\0';

	for (char *p = str; *p != '\0'; p++) {
		if (quote_char == '\0') {
			if (*p == '\'' || *p == '"') {
				quote_char = *p;
			} else if (*p == '#' && p > str &&
					   isspace((unsigned char)*(p - 1))) {
				*p = '\0';
				return;
			}
		} else {
			if (*p == quote_char) {
				quote_char = '\0';
			}
		}
	}
}

int32_t parse_double_array(const char *input, double *output,
						   int32_t max_count) {
	char *dup = strdup(input);
	char *token;
	int32_t count = 0;

	// 先清空整个数组
	memset(output, 0, max_count * sizeof(double));

	token = strtok(dup, ",");
	while (token != NULL && count < max_count) {
		trim_whitespace(token);
		char *endptr;
		double val = strtod(token, &endptr);
		if (endptr == token || *endptr != '\0') {
			mango_error(false, WLR_ERROR, "Invalid number in array: %s\n",
						token);
			free(dup);
			return -1;
		}
		if (val < 0.0) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid number in "
					"array (must be non-negative): %s\n",
					token);
			free(dup);
			return -1;
		}
		output[count] = val; // 赋值到当前count位置
		count++;			 // 然后才自增
		token = strtok(NULL, ",");
	}

	free(dup);
	return count;
}

// 清理字符串中的不可见字符（包括 \r, \n, 空格等）
char *sanitize_string(char *str) {
	// 去除首部不可见字符
	while (*str != '\0' && !isprint((unsigned char)*str))
		str++;
	// 去除尾部不可见字符
	char *end = str + strlen(str) - 1;
	while (end > str && !isprint((unsigned char)*end))
		end--;
	*(end + 1) = '\0';
	return str;
}

// 解析bind组合字符串
void parse_bind_flags(const char *str, KeyBinding *kb) {

	// 检查是否以"bind"开头
	if (strncmp(str, "bind", 4) != 0) {
		return;
	}

	const char *suffix = str + 4; // 跳过"bind"

	// 遍历后缀字符
	for (int32_t i = 0; suffix[i] != '\0'; i++) {
		switch (suffix[i]) {
		case 's':
			kb->keysymcode.type = KEY_TYPE_SYM;
			break;
		case 'l':
			kb->islockapply = true;
			break;
		case 'r':
			kb->isreleaseapply = true;
			break;
		case 'p':
			kb->ispassapply = true;
			break;
		case 'c':
			kb->isallowconflict = true;
			break;
		default:
			mango_error(false, WLR_ERROR, "Unknown bind flag: %c\n", suffix[i]);
			break;
		}
	}
}

static void set_binding_keymode(Config *config, char mode[28],
								bool *iscommonmode, bool *isdefaultmode) {
	strcpy(mode, config->keymode);
	if (strcmp(mode, "common") == 0) {
		*iscommonmode = true;
		*isdefaultmode = false;
	} else if (strcmp(mode, "default") == 0) {
		*isdefaultmode = true;
		*iscommonmode = false;
	} else {
		*isdefaultmode = false;
		*iscommonmode = false;
	}
}

int32_t parse_circle_direction(const char *str) {
	// 将输入字符串转换为小写

	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// 根据转换后的小写字符串返回对应的枚举值
	if (strcmp(lowerStr, "next") == 0) {
		return NEXT;
	} else {
		return PREV;
	}
}

int32_t parse_direction(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// 根据转换后的小写字符串返回对应的枚举值
	if (strcmp(lowerStr, "up") == 0) {
		return UP;
	} else if (strcmp(lowerStr, "down") == 0) {
		return DOWN;
	} else if (strcmp(lowerStr, "left") == 0) {
		return LEFT;
	} else if (strcmp(lowerStr, "right") == 0) {
		return RIGHT;
	} else {
		return UNDIR;
	}
}

int32_t parse_force(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// 根据转换后的小写字符串返回对应的枚举值
	if (strcmp(lowerStr, "unforce") == 0) {
		return UNFORCE;
	} else if (strcmp(lowerStr, "force") == 0) {
		return FORCE;
	} else {
		return UNFORCE;
	}
}

int32_t parse_fold_state(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// 根据转换后的小写字符串返回对应的枚举值
	if (strcmp(lowerStr, "fold") == 0) {
		return FOLD;
	} else if (strcmp(lowerStr, "unfold") == 0) {
		return UNFOLD;
	} else {
		return INVALIDFOLD;
	}
}
int64_t parse_color(const char *hex_str) {
	char *endptr;
	int64_t hex_num = strtol(hex_str, &endptr, 16);
	if (*endptr != '\0') {
		return -1;
	}
	return hex_num;
}

// 辅助函数：检查字符串是否以指定的前缀开头（忽略大小写）
static bool starts_with_ignore_case(const char *str, const char *prefix) {
	while (*prefix) {
		if (tolower(*str) != tolower(*prefix)) {
			return false;
		}
		str++;
		prefix++;
	}
	return true;
}

static char *combine_args_until_empty(char *values[], int count) {
	// find the first empty string
	int first_empty = count;
	for (int i = 0; i < count; i++) {
		// check if it's empty: empty string or only contains "0" (initialized)
		if (values[i][0] == '\0' ||
			(strlen(values[i]) == 1 && values[i][0] == '0')) {
			first_empty = i;
			break;
		}
	}

	// 	if there are no valid parameters, return an empty string
	if (first_empty == 0) {
		return strdup("");
	}

	// 	calculate the total length
	size_t total_len = 0;
	for (int i = 0; i < first_empty; i++) {
		total_len += strlen(values[i]);
	}
	// 	plus the number of commas (first_empty-1 commas)
	total_len += (first_empty - 1);

	// 	allocate memory and concatenate
	char *combined = malloc(total_len + 1);
	if (combined == NULL) {
		return strdup("");
	}

	combined[0] = '\0';
	for (int i = 0; i < first_empty; i++) {
		if (i > 0) {
			strcat(combined, ",");
		}
		strcat(combined, values[i]);
	}

	return combined;
}

uint32_t parse_mod(const char *mod_str) {
	if (!mod_str || !*mod_str) {
		return UINT32_MAX;
	}

	uint32_t mod = 0;
	char input_copy[256];
	char *token;
	char *saveptr = NULL;
	bool match_success = false;

	// 复制并转换为小写
	strncpy(input_copy, mod_str, sizeof(input_copy) - 1);
	input_copy[sizeof(input_copy) - 1] = '\0';
	for (char *p = input_copy; *p; p++) {
		*p = tolower(*p);
	}

	// 分割处理每个部分
	token = strtok_r(input_copy, "+", &saveptr);
	while (token != NULL) {
		// 去除前后空白
		trim_whitespace(token);

		// 如果 token 变成空字符串则跳过
		if (*token == '\0') {
			token = strtok_r(NULL, "+", &saveptr);
			continue;
		}

		if (strncmp(token, "code:", 5) == 0) {
			// 处理 code: 形式
			char *endptr;
			long keycode = strtol(token + 5, &endptr, 10);
			if (endptr != token + 5 && (*endptr == '\0' || *endptr == ' ')) {
				switch (keycode) {
				case 133:
				case 134:
					mod |= WLR_MODIFIER_LOGO;
					break;
				case 37:
				case 105:
					mod |= WLR_MODIFIER_CTRL;
					break;
				case 50:
				case 62:
					mod |= WLR_MODIFIER_SHIFT;
					break;
				case 64:
				case 108:
					mod |= WLR_MODIFIER_ALT;
					break;
				default:
					mango_error(false, WLR_ERROR,
								"unknown modifier keycode: "
								"\033[1m\033[31m%s\033[0m\n",
								token);
					break;
				}
			}
		} else {
			if (!strcmp(token, "super") || !strcmp(token, "super_l") ||
				!strcmp(token, "super_r")) {
				mod |= WLR_MODIFIER_LOGO;
				match_success = true;
			}
			if (!strcmp(token, "ctrl") || !strcmp(token, "ctrl_l") ||
				!strcmp(token, "ctrl_r")) {
				mod |= WLR_MODIFIER_CTRL;
				match_success = true;
			}
			if (!strcmp(token, "shift") || !strcmp(token, "shift_l") ||
				!strcmp(token, "shift_r")) {
				mod |= WLR_MODIFIER_SHIFT;
				match_success = true;
			}
			if (!strcmp(token, "alt") || !strcmp(token, "alt_l") ||
				!strcmp(token, "alt_r")) {
				mod |= WLR_MODIFIER_ALT;
				match_success = true;
			}
			if (!strcmp(token, "hyper") || !strcmp(token, "hyper_l") ||
				!strcmp(token, "hyper_r")) {
				mod |= WLR_MODIFIER_MOD3;
				match_success = true;
			}
			if (!strcmp(token, "none")) {
				match_success = true;
			}
		}

		token = strtok_r(NULL, "+", &saveptr);
	}

	if (!match_success) {
		mod = UINT32_MAX;
		mango_error(false, WLR_ERROR,
					"Unknown modifier: "
					"\033[1m\033[31m%s\033[0m\n",
					mod_str);
	}

	return mod;
}

// 定义辅助函数：在 keymap 中查找 keysym 对应的多个 keycode
static int32_t find_keycodes_for_keysym(struct xkb_keymap *keymap,
										xkb_keysym_t sym,
										MultiKeycode *multi_kc) {
	xkb_keycode_t min_keycode = xkb_keymap_min_keycode(keymap);
	xkb_keycode_t max_keycode = xkb_keymap_max_keycode(keymap);

	multi_kc->keycode1 = 0;
	multi_kc->keycode2 = 0;
	multi_kc->keycode3 = 0;

	int32_t found_count = 0;

	for (xkb_keycode_t keycode = min_keycode;
		 keycode <= max_keycode && found_count < 3; keycode++) {
		// 使用布局0和层级0
		const xkb_keysym_t *syms;
		int32_t num_syms =
			xkb_keymap_key_get_syms_by_level(keymap, keycode, 0, 0, &syms);

		for (int32_t i = 0; i < num_syms; i++) {
			if (syms[i] == sym) {
				switch (found_count) {
				case 0:
					multi_kc->keycode1 = keycode;
					break;
				case 1:
					multi_kc->keycode2 = keycode;
					break;
				case 2:
					multi_kc->keycode3 = keycode;
					break;
				}
				found_count++;
				break;
			}
		}
	}

	return found_count;
}

void cleanup_config_keymap(void) {
	if (config.keymap != NULL) {
		xkb_keymap_unref(config.keymap);
		config.keymap = NULL;
	}
	if (config.ctx != NULL) {
		xkb_context_unref(config.ctx);
		config.ctx = NULL;
	}
}

void create_config_keymap(void) {
	// 初始化 xkb 上下文和 keymap

	if (config.ctx == NULL) {
		config.ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	}

	if (config.keymap == NULL) {
		config.keymap = xkb_keymap_new_from_names(
			config.ctx, &xkb_fallback_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}
}

KeySymCode parse_key(const char *key_str, bool isbindsym) {
	KeySymCode kc = {0}; // 初始化为0

	if (config.keymap == NULL || config.ctx == NULL) {
		// 处理错误
		kc.type = KEY_TYPE_SYM;
		kc.keysym = XKB_KEY_NoSymbol;
		return kc;
	}

	// 处理 code: 前缀的情况
	if (strncmp(key_str, "code:", 5) == 0) {
		char *endptr;
		xkb_keycode_t keycode = (xkb_keycode_t)strtol(key_str + 5, &endptr, 10);
		kc.type = KEY_TYPE_CODE;
		kc.keycode.keycode1 = keycode; // 只设置第一个
		kc.keycode.keycode2 = 0;
		kc.keycode.keycode3 = 0;
		return kc;
	}

	// change key string to keysym, case insensitive
	xkb_keysym_t sym =
		xkb_keysym_from_name(key_str, XKB_KEYSYM_CASE_INSENSITIVE);

	if (isbindsym) {
		kc.type = KEY_TYPE_SYM;
		kc.keysym = sym;
		return kc;
	}

	if (sym != XKB_KEY_NoSymbol) {
		// 尝试找到对应的多个 keycode
		int32_t found_count =
			find_keycodes_for_keysym(config.keymap, sym, &kc.keycode);
		if (found_count > 0) {
			kc.type = KEY_TYPE_CODE;
			kc.keysym = sym; // 仍然保存 keysym 供参考
		} else {
			kc.type = KEY_TYPE_SYM;
			kc.keysym = sym;
			// keycode 字段保持为0
		}
	} else {
		// 无法解析的键名
		kc.type = KEY_TYPE_SYM;
		kc.keysym = XKB_KEY_NoSymbol;
		mango_error(false, WLR_ERROR, "Unknown key: \033[1m\033[31m%s\033[0m\n",
					key_str);
		// keycode 字段保持为0
	}

	return kc;
}

uint32_t parse_button(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[20];
	int32_t i = 0;
	while (str[i] && i < 19) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0'; // 确保字符串正确终止

	// 解析 "code:数字" 格式
	if (strncmp(lowerStr, "code:", 5) == 0) {
		const char *numStart = lowerStr + 5; // 跳过 "code:"
		char *endptr;
		unsigned long val = strtoul(numStart, &endptr, 10);

		// 检查是否成功转换且无多余字符，且值未溢出（在 uint32_t 范围内）
		if (endptr != numStart && *endptr == '\0' && val <= UINT32_MAX) {
			return (uint32_t)val;
		} else {
			mango_error(false, WLR_ERROR,
						"Invalid code format: "
						"\033[1m\033[31m%s\033[0m\n",
						str);
			return UINT32_MAX;
		}
	}

	// 根据转换后的小写字符串返回对应的按钮编号
	if (strcmp(lowerStr, "btn_left") == 0) {
		return BTN_LEFT;
	} else if (strcmp(lowerStr, "btn_right") == 0) {
		return BTN_RIGHT;
	} else if (strcmp(lowerStr, "btn_middle") == 0) {
		return BTN_MIDDLE;
	} else if (strcmp(lowerStr, "btn_side") == 0) {
		return BTN_SIDE;
	} else if (strcmp(lowerStr, "btn_extra") == 0) {
		return BTN_EXTRA;
	} else if (strcmp(lowerStr, "btn_forward") == 0) {
		return BTN_FORWARD;
	} else if (strcmp(lowerStr, "btn_back") == 0) {
		return BTN_BACK;
	} else if (strcmp(lowerStr, "btn_task") == 0) {
		return BTN_TASK;
	} else {
		mango_error(false, WLR_ERROR,
					"Unknown button: "
					"\033[1m\033[31m%s\033[0m\n",
					str);
		return UINT32_MAX;
	}
}

int32_t parse_mouse_action(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[20];
	int32_t i = 0;
	while (str[i] && i < 19) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0'; // 确保字符串正确终止

	// 根据转换后的小写字符串返回对应的按钮编号
	if (strcmp(lowerStr, "curmove") == 0) {
		return CurMove;
	} else if (strcmp(lowerStr, "curresize") == 0) {
		return CurResize;
	} else if (strcmp(lowerStr, "curnormal") == 0) {
		return CurNormal;
	} else if (strcmp(lowerStr, "curpressed") == 0) {
		return CurPressed;
	} else {
		return 0;
	}
}

void convert_hex_to_rgba(float *color, uint32_t hex) {
	color[0] = ((hex >> 24) & 0xFF) / 255.0f;
	color[1] = ((hex >> 16) & 0xFF) / 255.0f;
	color[2] = ((hex >> 8) & 0xFF) / 255.0f;
	color[3] = (hex & 0xFF) / 255.0f;
}

uint32_t parse_num_type(char *str) {
	switch (str[0]) {
	case '-':
		return NUM_TYPE_MINUS;
	case '+':
		return NUM_TYPE_PLUS;
	default:
		return NUM_TYPE_DEFAULT;
	}
}

uint32_t parse_tag_mask(char *str) {
	uint32_t mask = 0;
	char *token;
	char *arg_copy = strdup(str);

	if (arg_copy != NULL) {
		char *saveptr = NULL;
		token = strtok_r(arg_copy, "|", &saveptr);

		while (token != NULL) {
			trim_whitespace(token);
			int32_t num = atoi(token);
			if (num == 0 && strcmp(token, "0") == 0) {
				mask |= TAG0_MASK;
			} else if (num > 0 && num <= tag_num_MAX) {
				mask |= (1 << (num - 1));
			}
			token = strtok_r(NULL, "|", &saveptr);
		}

		free(arg_copy);
	}

	// tag0 and normal tags are exclusive; keep tag0 when mixed
	if ((mask & TAG0_MASK) && (mask & TAGMASK))
		mask = TAG0_MASK;

	uint32_t result = 0;

	if (mask) {
		result = mask;
	} else {
		result = atoi(str);
	}

	return result;
}

FuncType parse_func_name(char *func_name, Arg *arg, char *arg_value,
						 char *arg_value2, char *arg_value3, char *arg_value4,
						 char *arg_value5) {

	FuncType func = NULL;
	(*arg).i = 0;
	(*arg).i2 = 0;
	(*arg).f = 0.0f;
	(*arg).f2 = 0.0f;
	(*arg).ui = 0;
	(*arg).ui2 = 0;
	(*arg).v = NULL;
	(*arg).v2 = NULL;
	(*arg).v3 = NULL;

	if (strcmp(func_name, "focusstack") == 0) {
		func = focusstack;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "overcircle") == 0) {
		func = overcircle;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "groupfocus") == 0) {
		func = groupfocus;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "focusdir") == 0) {
		func = focusdir;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "focus_window_or_workspace") == 0) {
		func = focus_window_or_workspace;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "groupjoin") == 0) {
		func = groupjoin;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "groupleave") == 0) {
		func = groupleave;
	} else if (strcmp(func_name, "focusid") == 0) {
		func = focusid;
	} else if (strcmp(func_name, "incnmaster") == 0) {
		func = incnmaster;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "setmfact") == 0) {
		func = setmfact;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "zoom") == 0) {
		func = zoom;
	} else if (strcmp(func_name, "exchange_client") == 0) {
		func = exchange_client;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "exchange_stack_client") == 0) {
		func = exchange_stack_client;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "toggleglobal") == 0) {
		func = toggleglobal;
	} else if (strcmp(func_name, "togglehdr") == 0) {
		/* togglehdr[,on|off|toggle][,<monitor name>|all] */
		func = togglehdr;
		if (strcmp(arg_value, "on") == 0)
			(*arg).i = 1;
		else if (strcmp(arg_value, "off") == 0)
			(*arg).i = 0;
		else
			(*arg).i = -1; // toggle, and the default for an empty argument
		// "not given" is "" from the IPC path and "0" from the keybinding
		// parser -- same rule as combine_args_until_empty().
		bool has_name = arg_value2 && arg_value2[0] != '\0' &&
						!(strlen(arg_value2) == 1 && arg_value2[0] == '0');
		(*arg).v = has_name ? strdup(arg_value2) : NULL;
	} else if (strcmp(func_name, "toggleoverview") == 0) {
		func = toggleoverview;
	} else if (strcmp(func_name, "togglejump") == 0) {
		func = togglejump;
	} else if (strcmp(func_name, "set_proportion") == 0) {
		func = set_proportion;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "switch_proportion_preset") == 0) {
		func = switch_proportion_preset;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "viewtoleft") == 0) {
		func = viewtoleft;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "viewtoright") == 0) {
		func = viewtoright;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "view_insert") == 0) {
		func = view_insert;
		(*arg).i = strcmp(arg_value, "next") == 0 ? NEXT : PREV;
	} else if (strcmp(func_name, "tagsilent") == 0) {
		func = tagsilent;
		(*arg).ui = parse_tag_mask(arg_value);
	} else if (strcmp(func_name, "tagtoleft") == 0) {
		func = tagtoleft;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "tagtoright") == 0) {
		func = tagtoright;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "killclient") == 0) {
		func = killclient;
		(*arg).i = parse_force(arg_value);
	} else if (strcmp(func_name, "centerwin") == 0) {
		func = centerwin;
	} else if (strcmp(func_name, "focuslast") == 0) {
		func = focuslast;
	} else if (strcmp(func_name, "switcher") == 0) {
		func = switcher;
		if (strcmp(arg_value, "all_next") == 0) {
			(*arg).i = NEXT;
			(*arg).i2 = SW_ALL_MON;
		} else if (strcmp(arg_value, "all_prev") == 0) {
			(*arg).i = PREV;
			(*arg).i2 = SW_ALL_MON;
		} else if (strcmp(arg_value, "all_tag_next") == 0) {
			(*arg).i = NEXT;
			(*arg).i2 = SW_ALL_TAG;
		} else if (strcmp(arg_value, "all_tag_prev") == 0) {
			(*arg).i = PREV;
			(*arg).i2 = SW_ALL_TAG;
		} else if (strcmp(arg_value, "prev") == 0) {
			(*arg).i = PREV;
			(*arg).i2 = SW_CURRENT_TAG;
		} else {
			(*arg).i = NEXT;
			(*arg).i2 = SW_CURRENT_TAG;
		}
	} else if (strcmp(func_name, "toggle_trackpad_enable") == 0) {
		func = toggle_trackpad_enable;
	} else if (strcmp(func_name, "setoption") == 0) {
		func = setoption;

		(*arg).v = strdup(arg_value);

		// 收集需要拼接的参数
		const char *non_empty_params[4] = {NULL};
		int32_t param_index = 0;

		if (arg_value2 && arg_value2[0] != '\0')
			non_empty_params[param_index++] = arg_value2;
		if (arg_value3 && arg_value3[0] != '\0')
			non_empty_params[param_index++] = arg_value3;
		if (arg_value4 && arg_value4[0] != '\0')
			non_empty_params[param_index++] = arg_value4;
		if (arg_value5 && arg_value5[0] != '\0')
			non_empty_params[param_index++] = arg_value5;

		// 处理拼接
		if (param_index == 0) {
			(*arg).v2 = strdup("");
		} else {
			// 计算总长度
			size_t len = 0;
			for (int32_t i = 0; i < param_index; i++) {
				len += strlen(non_empty_params[i]);
			}
			len += (param_index - 1) + 1; // 逗号数 + null终止符

			char *temp = malloc(len);
			if (temp) {
				char *cursor = temp;
				for (int32_t i = 0; i < param_index; i++) {
					if (i > 0) {
						*cursor++ = ',';
					}
					size_t param_len = strlen(non_empty_params[i]);
					memcpy(cursor, non_empty_params[i], param_len);
					cursor += param_len;
				}
				*cursor = '\0';
				(*arg).v2 = temp;
			}
		}
	} else if (strcmp(func_name, "setkeymode") == 0) {
		func = setkeymode;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "switch_keyboard_layout") == 0) {
		func = switch_keyboard_layout;
		(*arg).i = CLAMP_INT(atoi(arg_value), 0, 100);
	} else if (strcmp(func_name, "setlayout") == 0) {
		func = setlayout;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "switch_layout") == 0) {
		func = switch_layout;
	} else if (strcmp(func_name, "togglefloating") == 0) {
		func = togglefloating;
	} else if (strcmp(func_name, "togglefullscreen") == 0) {
		func = togglefullscreen;
	} else if (strcmp(func_name, "togglefakefullscreen") == 0) {
		func = togglefakefullscreen;
	} else if (strcmp(func_name, "toggleoverlay") == 0) {
		func = toggleoverlay;
	} else if (strcmp(func_name, "minimized") == 0) {
		func = minimized;
	} else if (strcmp(func_name, "restore_minimized") == 0) {
		func = restore_minimized;
	} else if (strcmp(func_name, "toggle_scratchpad") == 0) {
		func = toggle_scratchpad;
	} else if (strcmp(func_name, "toggle_render_border") == 0) {
		func = toggle_render_border;
	} else if (strcmp(func_name, "focusmon") == 0) {
		func = focusmon;
		(*arg).i = parse_direction(arg_value);
		if ((*arg).i == UNDIR) {
			(*arg).v = strdup(arg_value);
		}
	} else if (strcmp(func_name, "tagmon") == 0) {
		func = tagmon;
		(*arg).i = parse_direction(arg_value);
		(*arg).i2 = atoi(arg_value2);
		if ((*arg).i == UNDIR) {
			(*arg).v = strdup(arg_value);
		};
	} else if (strcmp(func_name, "incgaps") == 0) {
		func = incgaps;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "togglegaps") == 0) {
		func = togglegaps;
	} else if (strcmp(func_name, "chvt") == 0) {
		func = chvt;
		(*arg).ui = atoi(arg_value);
	} else if (strcmp(func_name, "spawn") == 0) {
		func = spawn;
		char *values[] = {arg_value, arg_value2, arg_value3, arg_value4,
						  arg_value5};
		(*arg).v = combine_args_until_empty(values, 5);
	} else if (strcmp(func_name, "spawn_shell") == 0) {
		func = spawn_shell;
		char *values[] = {arg_value, arg_value2, arg_value3, arg_value4,
						  arg_value5};
		(*arg).v = combine_args_until_empty(values, 5);
	} else if (strcmp(func_name, "spawn_on_empty") == 0) {
		func = spawn_on_empty;
		(*arg).v = strdup(arg_value);
		(*arg).ui = parse_tag_mask(arg_value2);
	} else if (strcmp(func_name, "quit") == 0) {
		func = quit;
	} else if (strcmp(func_name, "create_virtual_output") == 0) {
		func = create_virtual_output;
	} else if (strcmp(func_name, "destroy_all_virtual_output") == 0) {
		func = destroy_all_virtual_output;
	} else if (strcmp(func_name, "moveresize") == 0) {
		func = moveresize;
		(*arg).ui = parse_mouse_action(arg_value);
	} else if (strcmp(func_name, "togglemaximizescreen") == 0) {
		func = togglemaximizescreen;
	} else if (strcmp(func_name, "viewtoleft_have_client") == 0) {
		func = viewtoleft_have_client;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "viewtoright_have_client") == 0) {
		func = viewtoright_have_client;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "reload_config") == 0) {
		func = reload_config;
	} else if (strcmp(func_name, "load_config_file") == 0) {
		func = load_config_file;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "tag") == 0) {
		func = tag;
		(*arg).ui = parse_tag_mask(arg_value);
		(*arg).i = atoi(arg_value2);
	} else if (strcmp(func_name, "view") == 0) {
		func = bind_to_view;
		(*arg).ui = parse_tag_mask(arg_value);
		(*arg).i = atoi(arg_value2);
	} else if (strcmp(func_name, "viewcrossmon") == 0) {
		func = viewcrossmon;
		(*arg).ui = parse_tag_mask(arg_value);
		(*arg).v = strdup(arg_value2);
	} else if (strcmp(func_name, "tagcrossmon") == 0) {
		func = tagcrossmon;
		(*arg).ui = parse_tag_mask(arg_value);
		(*arg).v = strdup(arg_value2);
	} else if (strcmp(func_name, "toggletag") == 0) {
		func = toggletag;
		(*arg).ui = parse_tag_mask(arg_value);
	} else if (strcmp(func_name, "toggleview") == 0) {
		func = toggleview;
		(*arg).ui = parse_tag_mask(arg_value);
	} else if (strcmp(func_name, "comboview") == 0) {
		func = comboview;
		(*arg).ui = parse_tag_mask(arg_value);
	} else if (strcmp(func_name, "smartmovewin") == 0) {
		func = smartmovewin;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "smartresizewin") == 0) {
		func = smartresizewin;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "resizewin") == 0) {
		func = resizewin;
		(*arg).ui = parse_num_type(arg_value);
		(*arg).ui2 = parse_num_type(arg_value2);
		(*arg).i = (*arg).ui == NUM_TYPE_DEFAULT ? atoi(arg_value)
												 : atoi(arg_value + 1);
		(*arg).i2 = (*arg).ui2 == NUM_TYPE_DEFAULT ? atoi(arg_value2)
												   : atoi(arg_value2 + 1);
	} else if (strcmp(func_name, "movewin") == 0) {
		func = movewin;
		(*arg).ui = parse_num_type(arg_value);
		(*arg).ui2 = parse_num_type(arg_value2);
		(*arg).i = (*arg).ui == NUM_TYPE_DEFAULT ? atoi(arg_value)
												 : atoi(arg_value + 1);
		(*arg).i2 = (*arg).ui2 == NUM_TYPE_DEFAULT ? atoi(arg_value2)
												   : atoi(arg_value2 + 1);
	} else if (strcmp(func_name, "toggle_named_scratchpad") == 0) {
		func = toggle_named_scratchpad;
		(*arg).v = strdup(arg_value);
		(*arg).v2 = strdup(arg_value2);
		(*arg).v3 = strdup(arg_value3);
	} else if (strcmp(func_name, "toggle_special_tag") == 0) {
		func = toggle_special_tag;
	} else if (strcmp(func_name, "tag_special_tag") == 0) {
		func = tag_special_tag;
	} else if (strcmp(func_name, "tag_special_silent") == 0) {
		func = tag_special_silent;
	} else if (strcmp(func_name, "disable_monitor") == 0) {
		func = disable_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "enable_monitor") == 0) {
		func = enable_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "toggle_monitor") == 0) {
		func = toggle_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "sleep_monitor") == 0) {
		func = sleep_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "wakeup_monitor") == 0) {
		func = wakeup_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "sleep_toggle_monitor") == 0) {
		func = sleep_toggle_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "scroller_stack") == 0) {
		func = scroller_stack;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "toggle_all_floating") == 0) {
		func = toggle_all_floating;
	} else if (strcmp(func_name, "dwindle_toggle_split_direction") == 0) {
		func = dwindle_toggle_split_direction;
	} else if (strcmp(func_name, "dwindle_split_horizontal") == 0) {
		func = dwindle_split_horizontal;
	} else if (strcmp(func_name, "dwindle_split_vertical") == 0) {
		func = dwindle_split_vertical;
	} else if (strcmp(func_name, "dwindle_toggle_current_split") == 0) {
		func = dwindle_toggle_current_split;
	} else {
		return NULL;
	}
	return func;
}

void set_env_without_display() {
	for (int32_t i = 0; i < config.env_count; i++) {
		if (strcmp(config.env[i]->type, "DISPLAY") == 0) {
			continue; // Skip setting DISPLAY
		}
		setenv(config.env[i]->type, config.env[i]->value, 1);
	}
}

void set_env_display() {
	for (int32_t i = 0; i < config.env_count; i++) {
		if (strcmp(config.env[i]->type, "DISPLAY") == 0) {
			setenv("DISPLAY", config.env[i]->value, 1);
		}
	}
}

void run_exec() {
	Arg arg;

	for (int32_t i = 0; i < config.exec_count; i++) {
		arg.v = config.exec[i];
		spawn_shell(&arg);
	}
}

void run_exec_once() {
	Arg arg;

	for (int32_t i = 0; i < config.exec_once_count; i++) {
		arg.v = config.exec_once[i];
		spawn_shell(&arg);
	}
}

bool parse_option(Config *config, char *key, char *value, int line_number) {
	if (strcmp(key, "keymode") == 0) {
		snprintf(config->keymode, sizeof(config->keymode), "%.27s", value);
	} else if (strcmp(key, "animations") == 0) {
		config->animations = atoi(value);
	} else if (strcmp(key, "layer_animations") == 0) {
		config->layer_animations = atoi(value);
	} else if (strcmp(key, "animation_type_open") == 0) {
		snprintf(config->animation_type_open,
				 sizeof(config->animation_type_open), "%.9s",
				 value); // string limit to 9 char
	} else if (strcmp(key, "animation_type_close") == 0) {
		snprintf(config->animation_type_close,
				 sizeof(config->animation_type_close), "%.9s",
				 value); // string limit to 9 char
	} else if (strcmp(key, "layer_animation_type_open") == 0) {
		snprintf(config->layer_animation_type_open,
				 sizeof(config->layer_animation_type_open), "%.9s",
				 value); // string limit to 9 char
	} else if (strcmp(key, "layer_animation_type_close") == 0) {
		snprintf(config->layer_animation_type_close,
				 sizeof(config->layer_animation_type_close), "%.9s",
				 value); // string limit to 9 char
	} else if (strcmp(key, "animation_fade_in") == 0) {
		config->animation_fade_in = atoi(value);
	} else if (strcmp(key, "animation_fade_out") == 0) {
		config->animation_fade_out = atoi(value);
	} else if (strcmp(key, "tag_animation_direction") == 0) {
		config->tag_animation_direction = atoi(value);
	} else if (strcmp(key, "zoom_initial_ratio") == 0) {
		config->zoom_initial_ratio = atof(value);
	} else if (strcmp(key, "zoom_end_ratio") == 0) {
		config->zoom_end_ratio = atof(value);
	} else if (strcmp(key, "fadein_begin_opacity") == 0) {
		config->fadein_begin_opacity = atof(value);
	} else if (strcmp(key, "fadeout_begin_opacity") == 0) {
		config->fadeout_begin_opacity = atof(value);
	} else if (strcmp(key, "animation_duration_move") == 0) {
		config->animation_duration_move = atoi(value);
	} else if (strcmp(key, "animation_duration_open") == 0) {
		config->animation_duration_open = atoi(value);
	} else if (strcmp(key, "animation_duration_tag") == 0) {
		config->animation_duration_tag = atoi(value);
	} else if (strcmp(key, "animation_duration_close") == 0) {
		config->animation_duration_close = atoi(value);
	} else if (strcmp(key, "animation_duration_focus") == 0) {
		config->animation_duration_focus = atoi(value);
	} else if (strcmp(key, "animation_curve_move") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_move, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_move: %s\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_open") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_open, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_open: %s\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_tag") == 0) {
		int32_t num = parse_double_array(value, config->animation_curve_tag, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_tag: %s\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_close") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_close, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_close: %s\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_focus") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_focus, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_focus: %s\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_opafadein") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_opafadein, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_opafadein: %s\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_opafadeout") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_opafadeout, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_opafadeout: %s\n",
						value);
			return false;
		}
	} else if (strcmp(key, "scroller_structs") == 0) {
		config->scroller_structs = atoi(value);
	} else if (strcmp(key, "scroller_default_proportion") == 0) {
		config->scroller_default_proportion = atof(value);
	} else if (strcmp(key, "scroller_default_proportion_single") == 0) {
		config->scroller_default_proportion_single = atof(value);
	} else if (strcmp(key, "scroller_ignore_proportion_single") == 0) {
		config->scroller_ignore_proportion_single = atoi(value);
	} else if (strcmp(key, "scroller_focus_center") == 0) {
		config->scroller_focus_center = atoi(value);
	} else if (strcmp(key, "scroller_prefer_center") == 0) {
		config->scroller_prefer_center = atoi(value);
	} else if (strcmp(key, "scroller_prefer_overspread") == 0) {
		config->scroller_prefer_overspread = atoi(value);
	} else if (strcmp(key, "edge_scroller_pointer_focus") == 0) {
		config->edge_scroller_pointer_focus = atoi(value);
	} else if (strcmp(key, "edge_scroller_focus_allow_speed") == 0) {
		config->edge_scroller_focus_allow_speed = atof(value);
	} else if (strcmp(key, "focus_cross_monitor") == 0) {
		config->focus_cross_monitor = atoi(value);
	} else if (strcmp(key, "focusdir_only_zone_overlap") == 0) {
		config->focusdir_only_zone_overlap = atoi(value);
	} else if (strcmp(key, "exchange_cross_monitor") == 0) {
		config->exchange_cross_monitor = atoi(value);
	} else if (strcmp(key, "scratchpad_cross_monitor") == 0) {
		config->scratchpad_cross_monitor = atoi(value);
	} else if (strcmp(key, "focus_cross_tag") == 0) {
		config->focus_cross_tag = atoi(value);
	} else if (strcmp(key, "view_current_to_back") == 0) {
		config->view_current_to_back = atoi(value);
	} else if (strcmp(key, "blur") == 0) {
		config->blur = atoi(value);
	} else if (strcmp(key, "blur_layer") == 0) {
		config->blur_layer = atoi(value);
	} else if (strcmp(key, "blur_optimized") == 0) {
		config->blur_optimized = atoi(value);
	} else if (strcmp(key, "border_radius") == 0) {
		config->border_radius = atoi(value);
	} else if (strcmp(key, "blur_params_num_passes") == 0) {
		config->blur_params.num_passes = atoi(value);
	} else if (strcmp(key, "blur_params_radius") == 0) {
		config->blur_params.radius = atoi(value);
	} else if (strcmp(key, "blur_params_noise") == 0) {
		config->blur_params.noise = atof(value);
	} else if (strcmp(key, "blur_params_brightness") == 0) {
		config->blur_params.brightness = atof(value);
	} else if (strcmp(key, "blur_params_contrast") == 0) {
		config->blur_params.contrast = atof(value);
	} else if (strcmp(key, "blur_params_saturation") == 0) {
		config->blur_params.saturation = atof(value);
	} else if (strcmp(key, "shadows") == 0) {
		config->shadows = atoi(value);
	} else if (strcmp(key, "shadow_only_floating") == 0) {
		config->shadow_only_floating = atoi(value);
	} else if (strcmp(key, "layer_shadows") == 0) {
		config->layer_shadows = atoi(value);
	} else if (strcmp(key, "shadows_size") == 0) {
		config->shadows_size = atoi(value);
	} else if (strcmp(key, "shadows_blur") == 0) {
		config->shadows_blur = atof(value);
	} else if (strcmp(key, "shadows_position_x") == 0) {
		config->shadows_position_x = atoi(value);
	} else if (strcmp(key, "shadows_position_y") == 0) {
		config->shadows_position_y = atoi(value);
	} else if (strcmp(key, "single_scratchpad") == 0) {
		config->single_scratchpad = atoi(value);
	} else if (strcmp(key, "xwayland_persistence") == 0) {
		config->xwayland_persistence = atoi(value);
	} else if (strcmp(key, "xwayland_ignore_scale") == 0) {
		config->xwayland_ignore_scale = atoi(value);
	} else if (strcmp(key, "syncobj_enable") == 0) {
		config->syncobj_enable = atoi(value);
	} else if (strcmp(key, "tag_carousel") == 0) {
		config->tag_carousel = atoi(value);
	} else if (strcmp(key, "drag_tile_refresh_interval") == 0) {
		config->drag_tile_refresh_interval = atof(value);
	} else if (strcmp(key, "drag_floating_refresh_interval") == 0) {
		config->drag_floating_refresh_interval = atof(value);
	} else if (strcmp(key, "allow_tearing") == 0) {
		config->allow_tearing = atoi(value);
	} else if (strcmp(key, "hdr_depth") == 0) {
		config->hdr_depth = atoi(value);
	} else if (strcmp(key, "allow_shortcuts_inhibit") == 0) {
		config->allow_shortcuts_inhibit = atoi(value);
	} else if (strcmp(key, "allow_lock_transparent") == 0) {
		config->allow_lock_transparent = atoi(value);
	} else if (strcmp(key, "no_border_when_single") == 0) {
		config->no_border_when_single = atoi(value);
	} else if (strcmp(key, "no_radius_when_single") == 0) {
		config->no_radius_when_single = atoi(value);
	} else if (strcmp(key, "snap_distance") == 0) {
		config->snap_distance = atoi(value);
	} else if (strcmp(key, "enable_floating_snap") == 0) {
		config->enable_floating_snap = atoi(value);
	} else if (strcmp(key, "drag_tile_to_tile") == 0) {
		config->drag_tile_to_tile = atoi(value);
	} else if (strcmp(key, "drag_tile_small") == 0) {
		config->drag_tile_small = atoi(value);
	} else if (strcmp(key, "swipe_min_threshold") == 0) {
		config->swipe_min_threshold = atoi(value);
	} else if (strcmp(key, "focused_opacity") == 0) {
		config->focused_opacity = atof(value);
	} else if (strcmp(key, "unfocused_opacity") == 0) {
		config->unfocused_opacity = atof(value);
	} else if (strcmp(key, "xkb_rules_rules") == 0) {
		strncpy(config->xkb_rules_rules, value,
				sizeof(config->xkb_rules_rules) - 1);
		config->xkb_rules_rules[sizeof(config->xkb_rules_rules) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_model") == 0) {
		strncpy(config->xkb_rules_model, value,
				sizeof(config->xkb_rules_model) - 1);
		config->xkb_rules_model[sizeof(config->xkb_rules_model) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_layout") == 0) {
		strncpy(config->xkb_rules_layout, value,
				sizeof(config->xkb_rules_layout) - 1);
		config->xkb_rules_layout[sizeof(config->xkb_rules_layout) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_variant") == 0) {
		strncpy(config->xkb_rules_variant, value,
				sizeof(config->xkb_rules_variant) - 1);
		config->xkb_rules_variant[sizeof(config->xkb_rules_variant) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_options") == 0) {
		strncpy(config->xkb_rules_options, value,
				sizeof(config->xkb_rules_options) - 1);
		config->xkb_rules_options[sizeof(config->xkb_rules_options) - 1] = '\0';
	} else if (strcmp(key, "scroller_proportion_preset") == 0) {
		// 1. 统计 value 中有多少个逗号，确定需要解析的浮点数个数
		int32_t count = 0; // 初始化为 0
		for (const char *p = value; *p; p++) {
			if (*p == ',')
				count++;
		}
		int32_t float_count = count + 1; // 浮点数的数量是逗号数量加 1

		// 2. 动态分配内存，存储浮点数
		// 先释放旧的内存（防止重复设置该选项时内存泄漏）
		if (config->scroller_proportion_preset) {
			free(config->scroller_proportion_preset);
			config->scroller_proportion_preset = NULL;
			config->scroller_proportion_preset_count = 0;
		}
		config->scroller_proportion_preset =
			(float *)malloc(float_count * sizeof(float));
		if (!config->scroller_proportion_preset) {
			mango_error(false, WLR_ERROR,
						"Memory "
						"allocation failed\n");
			return false;
		}

		// 3. 解析 value 中的浮点数
		char *value_copy =
			strdup(value); // 复制 value，因为 strtok 会修改原字符串
		char *token = strtok(value_copy, ",");
		int32_t i = 0;
		float value_set;

		while (token != NULL && i < float_count) {
			if (sscanf(token, "%f", &value_set) != 1) {
				mango_error(false, WLR_ERROR,
							"Invalid float "
							"value in "
							"scroller_proportion_preset: %s\n",
							token);
				free(value_copy);
				free(config->scroller_proportion_preset);
				config->scroller_proportion_preset = NULL;
				return false;
			}

			// Clamp the value between 0.0 and 1.0 (or your desired
			// range)
			config->scroller_proportion_preset[i] =
				CLAMP_FLOAT(value_set, 0.1f, 1.0f);

			token = strtok(NULL, ",");
			i++;
		}

		// 4. 检查解析的浮点数数量是否匹配
		if (i != float_count) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"scroller_proportion_preset format: %s\n",
						value);
			free(value_copy);
			free(config->scroller_proportion_preset);  // 释放已分配的内存
			config->scroller_proportion_preset = NULL; // 防止野指针
			config->scroller_proportion_preset_count = 0;
			return false;
		}
		config->scroller_proportion_preset_count = float_count;

		// 5. 释放临时复制的字符串
		free(value_copy);
	} else if (strcmp(key, "circle_layout") == 0) {
		// 1. 统计 value 中有多少个逗号，确定需要解析的字符串个数
		int32_t count = 0; // 初始化为 0
		for (const char *p = value; *p; p++) {
			if (*p == ',')
				count++;
		}
		int32_t string_count = count + 1; // 字符串的数量是逗号数量加 1

		// 2. 动态分配内存，存储字符串指针
		// 先释放旧的内存（防止重复设置该选项时内存泄漏）
		if (config->circle_layout) {
			for (int32_t j = 0; j < config->circle_layout_count; j++) {
				if (config->circle_layout[j])
					free(config->circle_layout[j]);
			}
			free(config->circle_layout);
			config->circle_layout = NULL;
			config->circle_layout_count = 0;
		}
		config->circle_layout = (char **)malloc(string_count * sizeof(char *));
		if (!config->circle_layout) {
			mango_error(false, WLR_ERROR,
						"Memory "
						"allocation failed\n");
			return false;
		}
		memset(config->circle_layout, 0, string_count * sizeof(char *));

		// 3. 解析 value 中的字符串
		char *value_copy =
			strdup(value); // 复制 value，因为 strtok 会修改原字符串
		char *token = strtok(value_copy, ",");
		int32_t i = 0;
		char *cleaned_token;
		while (token != NULL && i < string_count) {
			// 为每个字符串分配内存并复制内容
			cleaned_token = sanitize_string(token);
			config->circle_layout[i] = strdup(cleaned_token);
			if (!config->circle_layout[i]) {
				mango_error(false, WLR_ERROR,
							"Memory allocation "
							"failed for "
							"string: %s\n",
							token);
				// 释放之前分配的内存
				for (int32_t j = 0; j < i; j++) {
					free(config->circle_layout[j]);
				}
				free(config->circle_layout);
				free(value_copy);
				config->circle_layout = NULL; // 防止野指针
				config->circle_layout_count = 0;
				return false;
			}
			token = strtok(NULL, ",");
			i++;
		}

		// 4. 检查解析的字符串数量是否匹配
		if (i != string_count) {
			mango_error(false, WLR_ERROR,
						"Invalid circle_layout "
						"format: %s\n",
						value);
			// 释放之前分配的内存
			for (int32_t j = 0; j < i; j++) {
				free(config->circle_layout[j]);
			}
			free(config->circle_layout);
			free(value_copy);
			config->circle_layout = NULL; // 防止野指针
			config->circle_layout_count = 0;
			return false;
		}
		config->circle_layout_count = string_count;

		// 5. 释放临时复制的字符串
		free(value_copy);
	} else if (strcmp(key, "new_is_master") == 0) {
		config->new_is_master = atoi(value);
	} else if (strcmp(key, "default_mfact") == 0) {
		config->default_mfact = atof(value);
	} else if (strcmp(key, "default_nmaster") == 0) {
		config->default_nmaster = atoi(value);
	} else if (strcmp(key, "tag_num") == 0) {
		config->tag_num = atoi(value);
	} else if (strcmp(key, "tag_gather") == 0) {
		config->tag_gather = atoi(value);
	} else if (strcmp(key, "center_master_overspread") == 0) {
		config->center_master_overspread = atoi(value);
	} else if (strcmp(key, "center_when_single_stack") == 0) {
		config->center_when_single_stack = atoi(value);
	} else if (strcmp(key, "dwindle_vsplit") == 0) {
		config->dwindle_vsplit = atoi(value);
	} else if (strcmp(key, "dwindle_hsplit") == 0) {
		config->dwindle_hsplit = atoi(value);
	} else if (strcmp(key, "dwindle_preserve_split") == 0) {
		config->dwindle_preserve_split = atoi(value);
	} else if (strcmp(key, "dwindle_smart_split") == 0) {
		config->dwindle_smart_split = atoi(value);
	} else if (strcmp(key, "dwindle_smart_resize") == 0) {
		config->dwindle_smart_resize = atoi(value);
	} else if (strcmp(key, "dwindle_drop_simple_split") == 0) {
		config->dwindle_drop_simple_split = atoi(value);
	} else if (strcmp(key, "dwindle_manual_split") == 0) {
		config->dwindle_manual_split = atoi(value);
	} else if (strcmp(key, "dwindle_split_ratio") == 0) {
		config->dwindle_split_ratio = atof(value);
	} else if (strcmp(key, "hotarea_size") == 0) {
		config->hotarea_size = atoi(value);
	} else if (strcmp(key, "hotarea_corner") == 0) {
		config->hotarea_corner = atoi(value);
	} else if (strcmp(key, "enable_hotarea") == 0) {
		config->enable_hotarea = atoi(value);
	} else if (strcmp(key, "overviewgappi") == 0) {
		config->overviewgappi = atoi(value);
	} else if (strcmp(key, "overviewgappo") == 0) {
		config->overviewgappo = atoi(value);
	} else if (strcmp(key, "overcircle_center_ratio") == 0) {
		config->overcircle_center_ratio = atof(value);
	} else if (strcmp(key, "jump_labels") == 0) {
		if (config->jump_labels)
			free(config->jump_labels);
		config->jump_labels = strdup(value);
	} else if (strcmp(key, "cursor_hide_timeout") == 0) {
		config->cursor_hide_timeout = atoi(value);
	} else if (strcmp(key, "cursor_hide_on_keypress") == 0) {
		config->cursor_hide_on_keypress = atoi(value);
	} else if (strcmp(key, "axis_bind_apply_timeout") == 0) {
		config->axis_bind_apply_timeout = atoi(value);
	} else if (strcmp(key, "focus_on_activate") == 0) {
		config->focus_on_activate = atoi(value);
	} else if (strcmp(key, "smartkill") == 0) {
		config->smartkill = atoi(value);
	} else if (strcmp(key, "numlockon") == 0) {
		config->numlockon = atoi(value);
	} else if (strcmp(key, "idleinhibit_ignore_visible") == 0) {
		config->idleinhibit_ignore_visible = atoi(value);
	} else if (strcmp(key, "sloppyfocus") == 0) {
		config->sloppyfocus = atoi(value);
	} else if (strcmp(key, "warpcursor") == 0) {
		config->warpcursor = atoi(value);
	} else if (strcmp(key, "drag_corner") == 0) {
		config->drag_corner = atoi(value);
	} else if (strcmp(key, "drag_warp_cursor") == 0) {
		config->drag_warp_cursor = atoi(value);
	} else if (strcmp(key, "smartgaps") == 0) {
		config->smartgaps = atoi(value);
	} else if (strcmp(key, "repeat_rate") == 0) {
		config->repeat_rate = atoi(value);
	} else if (strcmp(key, "repeat_delay") == 0) {
		config->repeat_delay = atoi(value);
	} else if (strcmp(key, "disable_trackpad") == 0) {
		config->disable_trackpad = atoi(value);
	} else if (strcmp(key, "touch_enable") == 0) {
		config->touch_enable = atoi(value);
	} else if (strcmp(key, "touch_enable_mouse_emulation") == 0) {
		config->touch_enable_mouse_emulation = atoi(value);
	} else if (strcmp(key, "touch_map_to_mon") == 0) {
		if (config->touch_map_to_mon)
			free(config->touch_map_to_mon);
		config->touch_map_to_mon = value[0] ? strdup(value) : NULL;
	} else if (strcmp(key, "tap_to_click") == 0) {
		config->tap_to_click = atoi(value);
	} else if (strcmp(key, "tap_and_drag") == 0) {
		config->tap_and_drag = atoi(value);
	} else if (strcmp(key, "drag_lock") == 0) {
		config->drag_lock = atoi(value);
	} else if (strcmp(key, "mouse_natural_scrolling") == 0) {
		config->mouse_natural_scrolling = atoi(value);
	} else if (strcmp(key, "trackpad_natural_scrolling") == 0) {
		config->trackpad_natural_scrolling = atoi(value);
	} else if (strcmp(key, "cursor_size") == 0) {
		config->cursor_size = atoi(value);
	} else if (strcmp(key, "cursor_theme") == 0) {
		if (config->cursor_theme)
			free(config->cursor_theme);
		config->cursor_theme = strdup(value);
	} else if (strcmp(key, "group_bar_decorate_font_desc") == 0) {
		if (config->groupbardata.font_desc)
			free((void *)config->groupbardata.font_desc);
		config->groupbardata.font_desc = strdup(value);
	} else if (strcmp(key, "group_bar_decorate_fg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"group_bar_decorate_fg_color "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->groupbardata.fg_color, color);
		}
	} else if (strcmp(key, "group_bar_decorate_bg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"group_bar_decorate_bg_color "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->groupbardata.bg_color, color);
		}
	} else if (strcmp(key, "group_bar_decorate_focus_fg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"group_bar_decorate_focus_fg_color "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->groupbardata.focus_fg_color, color);
		}
	} else if (strcmp(key, "group_bar_decorate_focus_bg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"group_bar_decorate_focus_bg_color "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->groupbardata.focus_bg_color, color);
		}
	} else if (strcmp(key, "group_bar_decorate_border_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"group_bar_decorate_border_color "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->groupbardata.border_color, color);
		}
	} else if (strcmp(key, "group_bar_decorate_border_width") == 0) {
		config->groupbardata.border_width = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "group_bar_decorate_corner_radius") == 0) {
		config->groupbardata.corner_radius = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "group_bar_decorate_padding_x") == 0) {
		config->groupbardata.padding_x = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "group_bar_decorate_padding_y") == 0) {
		config->groupbardata.padding_y = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "jump_label_decorate_font_desc") == 0) {
		if (config->jumplabeldata.font_desc)
			free((void *)config->jumplabeldata.font_desc);
		config->jumplabeldata.font_desc = strdup(value);
	} else if (strcmp(key, "jump_label_decorate_fg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"jump_label_decorate_fg_color "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->jumplabeldata.fg_color, color);
		}
	} else if (strcmp(key, "jump_label_decorate_bg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"jump_label_decorate_bg_color "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->jumplabeldata.bg_color, color);
		}
	} else if (strcmp(key, "jump_label_decorate_focus_fg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"jump_label_decorate_focus_fg_color "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->jumplabeldata.focus_fg_color, color);
		}
	} else if (strcmp(key, "jump_label_decorate_focus_bg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"jump_label_decorate_focus_bg_color "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->jumplabeldata.focus_bg_color, color);
		}
	} else if (strcmp(key, "jump_label_decorate_border_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"jump_label_decorate_border_color "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->jumplabeldata.border_color, color);
		}
	} else if (strcmp(key, "jump_label_decorate_border_width") == 0) {
		config->jumplabeldata.border_width = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "jump_label_decorate_corner_radius") == 0) {
		config->jumplabeldata.corner_radius = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "jump_label_decorate_padding_x") == 0) {
		config->jumplabeldata.padding_x = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "jump_label_decorate_padding_y") == 0) {
		config->jumplabeldata.padding_y = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "mouse_accel_profile") == 0) {
		config->mouse_accel_profile = atoi(value);
	} else if (strcmp(key, "mouse_accel_speed") == 0) {
		config->mouse_accel_speed = atof(value);
	} else if (strcmp(key, "trackpad_accel_profile") == 0) {
		config->trackpad_accel_profile = atoi(value);
	} else if (strcmp(key, "trackpad_accel_speed") == 0) {
		config->trackpad_accel_speed = atof(value);
	} else if (strcmp(key, "mouse_left_handed") == 0) {
		config->mouse_left_handed = atoi(value);
	} else if (strcmp(key, "mouse_middle_button_emulation") == 0) {
		config->mouse_middle_button_emulation = atoi(value);
	} else if (strcmp(key, "mouse_scroll_method") == 0) {
		config->mouse_scroll_method = atoi(value);
	} else if (strcmp(key, "mouse_scroll_button") == 0) {
		config->mouse_scroll_button = atoi(value);
	} else if (strcmp(key, "mouse_click_method") == 0) {
		config->mouse_click_method = atoi(value);
	} else if (strcmp(key, "mouse_send_events_mode") == 0) {
		config->mouse_send_events_mode = atoi(value);
	} else if (strcmp(key, "trackpad_left_handed") == 0) {
		config->trackpad_left_handed = atoi(value);
	} else if (strcmp(key, "trackpad_middle_button_emulation") == 0) {
		config->trackpad_middle_button_emulation = atoi(value);
	} else if (strcmp(key, "trackpad_disable_while_typing") == 0) {
		config->trackpad_disable_while_typing = atoi(value);
	} else if (strcmp(key, "trackpad_scroll_method") == 0) {
		config->trackpad_scroll_method = atoi(value);
	} else if (strcmp(key, "trackpad_scroll_button") == 0) {
		config->trackpad_scroll_button = atoi(value);
	} else if (strcmp(key, "trackpad_click_method") == 0) {
		config->trackpad_click_method = atoi(value);
	} else if (strcmp(key, "trackpad_send_events_mode") == 0) {
		config->trackpad_send_events_mode = atoi(value);
	} else if (strcmp(key, "send_events_mode") == 0) {
		config->send_events_mode = atoi(value);
	} else if (strcmp(key, "button_map") == 0) {
		config->button_map = atoi(value);
	} else if (strcmp(key, "axis_scroll_factor") == 0) {
		config->axis_scroll_factor = atof(value);
	} else if (strcmp(key, "tablet_map_to_mon") == 0) {
		if (config->tablet_map_to_mon)
			free(config->tablet_map_to_mon);
		config->tablet_map_to_mon = strdup(value);
	} else if (strcmp(key, "trackpad_scroll_factor") == 0) {
		config->trackpad_scroll_factor = atof(value);
	} else if (strcmp(key, "gappih") == 0) {
		config->gappih = atoi(value);
	} else if (strcmp(key, "gappiv") == 0) {
		config->gappiv = atoi(value);
	} else if (strcmp(key, "gappoh") == 0) {
		config->gappoh = atoi(value);
	} else if (strcmp(key, "gappov") == 0) {
		config->gappov = atoi(value);
	} else if (strcmp(key, "scratchpad_width_ratio") == 0) {
		config->scratchpad_width_ratio = atof(value);
	} else if (strcmp(key, "scratchpad_height_ratio") == 0) {
		config->scratchpad_height_ratio = atof(value);
	} else if (strcmp(key, "special_dim") == 0) {
		config->special_dim = atof(value);
	} else if (strcmp(key, "special_gappih") == 0) {
		config->special_gappih = atoi(value);
	} else if (strcmp(key, "special_gappiv") == 0) {
		config->special_gappiv = atoi(value);
	} else if (strcmp(key, "special_gappoh") == 0) {
		config->special_gappoh = atoi(value);
	} else if (strcmp(key, "special_gappov") == 0) {
		config->special_gappov = atoi(value);
	} else if (strcmp(key, "borderpx") == 0) {
		config->borderpx = atoi(value);
	} else if (strcmp(key, "group_bar_height") == 0) {
		config->group_bar_height = atoi(value);
	} else if (strcmp(key, "rootcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid rootcolor "
						"format: "
						"%s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->rootcolor, color);
		}

	} else if (strcmp(key, "shadowscolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid shadowscolor "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->shadowscolor, color);
		}
	} else if (strcmp(key, "bordercolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid bordercolor "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->bordercolor, color);
		}
	} else if (strcmp(key, "dropcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid dropcolor "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->dropcolor, color);
		}
	} else if (strcmp(key, "splitcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid splitcolor "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->splitcolor, color);
		}
	} else if (strcmp(key, "focuscolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid focuscolor "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->focuscolor, color);
		}
	} else if (strcmp(key, "maximizescreencolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"maximizescreencolor "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->maximizescreencolor, color);
		}
	} else if (strcmp(key, "urgentcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid urgentcolor "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->urgentcolor, color);
		}
	} else if (strcmp(key, "scratchpadcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"scratchpadcolor "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->scratchpadcolor, color);
		}
	} else if (strcmp(key, "globalcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid globalcolor "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->globalcolor, color);
		}
	} else if (strcmp(key, "overlaycolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid overlaycolor "
						"format: %s\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->overlaycolor, color);
		}
	} else if (strcmp(key, "monitorrule") == 0) {
		config->monitor_rules =
			realloc(config->monitor_rules, (config->monitor_rules_count + 1) *
											   sizeof(ConfigMonitorRule));
		if (!config->monitor_rules) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for monitor rules\n");
			return false;
		}

		ConfigMonitorRule *rule =
			&config->monitor_rules[config->monitor_rules_count];
		memset(rule, 0, sizeof(ConfigMonitorRule));

		// 设置默认值
		rule->name = NULL;
		rule->make = NULL;
		rule->model = NULL;
		rule->serial = NULL;
		rule->rr = 0;
		rule->scale = 1.0f;
		rule->x = INT32_MAX;
		rule->y = INT32_MAX;
		rule->width = -1;
		rule->height = -1;
		rule->refresh = 0.0f;
		rule->vrr = 0;
		rule->hdr = 0;
		rule->hdr_min_lum = 0.0f;
		rule->hdr_max_lum = 0.0f;
		rule->hdr_max_avg_lum = 0.0f;
		rule->hdr_force = 0;
		rule->icc = NULL;
		rule->custom = 0;
		rule->disable = 0;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "name") == 0) {
					rule->name = strdup(val);
				} else if (strcmp(key, "make") == 0) {
					rule->make = strdup(val);
				} else if (strcmp(key, "model") == 0) {
					rule->model = strdup(val);
				} else if (strcmp(key, "serial") == 0) {
					rule->serial = strdup(val);
				} else if (strcmp(key, "rr") == 0) {
					rule->rr = CLAMP_INT(atoi(val), 0, 7);
				} else if (strcmp(key, "scale") == 0) {
					rule->scale = CLAMP_FLOAT(atof(val), 0.001f, 1000.0f);
				} else if (strcmp(key, "x") == 0) {
					rule->x = atoi(val);
				} else if (strcmp(key, "y") == 0) {
					rule->y = atoi(val);
				} else if (strcmp(key, "width") == 0) {
					rule->width = CLAMP_INT(atoi(val), 1, INT32_MAX);
				} else if (strcmp(key, "height") == 0) {
					rule->height = CLAMP_INT(atoi(val), 1, INT32_MAX);
				} else if (strcmp(key, "refresh") == 0) {
					rule->refresh = CLAMP_FLOAT(atof(val), 0.001f, 1000.0f);
				} else if (strcmp(key, "vrr") == 0) {
					rule->vrr = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "hdr") == 0) {
					rule->hdr = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "hdr_min_lum") == 0) {
					// cd/m². OLED blacks sit well below 0.01, so the floor has
					// to allow small fractions -- do not clamp to >= 1.
					rule->hdr_min_lum = CLAMP_FLOAT(atof(val), 0.0f, 10000.0f);
				} else if (strcmp(key, "hdr_max_lum") == 0) {
					rule->hdr_max_lum = CLAMP_FLOAT(atof(val), 0.0f, 10000.0f);
				} else if (strcmp(key, "hdr_max_avg_lum") == 0) {
					rule->hdr_max_avg_lum =
						CLAMP_FLOAT(atof(val), 0.0f, 10000.0f);
				} else if (strcmp(key, "hdr_force") == 0) {
					rule->hdr_force = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "icc") == 0) {
					free(rule->icc);
					rule->icc = strdup(val);
				} else if (strcmp(key, "disable") == 0) {
					rule->disable = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "custom") == 0) {
					rule->custom = CLAMP_INT(atoi(val), 0, 1);
				} else {
					mango_error(false, WLR_ERROR,
								"Unknown "
								"monitor rule "
								"option:\033[1m\033[31m %s\n",
								key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		if (!rule->name && !rule->make && !rule->model && !rule->serial) {
			mango_error(false, WLR_ERROR,
						"Monitor rule "
						"must have at least one of the following "
						"options: name, make, model, serial\n");
			return false;
		}

		config->monitor_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "tagrule") == 0) {
		config->tag_rules =
			realloc(config->tag_rules,
					(config->tag_rules_count + 1) * sizeof(ConfigTagRule));
		if (!config->tag_rules) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for tag rules\n");
			return false;
		}

		ConfigTagRule *rule = &config->tag_rules[config->tag_rules_count];
		memset(rule, 0, sizeof(ConfigTagRule));

		// 设置默认值
		rule->id = 0;
		rule->id_wildcard = false;
		rule->layout_name = NULL;
		rule->monitor_name = NULL;
		rule->monitor_make = NULL;
		rule->monitor_model = NULL;
		rule->monitor_serial = NULL;
		rule->nmaster = 0;
		rule->mfact = 0.0f;
		rule->no_render_border = 0;
		rule->open_as_floating = 0;
		rule->no_hide = 0;
		rule->scroller_default_proportion = 0.0f;
		rule->scroller_default_proportion_single = 0.0f;
		rule->scroller_ignore_proportion_single = -1;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "id") == 0) {
					if (strcmp(val, "*") == 0) {
						rule->id_wildcard = true;
						rule->id = 0;
					} else {
						rule->id_wildcard = false;
						rule->id = CLAMP_INT(atoi(val), 0, LENGTH(tags));
					}
				} else if (strcmp(key, "layout_name") == 0) {
					rule->layout_name = strdup(val);
				} else if (strcmp(key, "monitor_name") == 0) {
					rule->monitor_name = strdup(val);
				} else if (strcmp(key, "monitor_make") == 0) {
					rule->monitor_make = strdup(val);
				} else if (strcmp(key, "monitor_model") == 0) {
					rule->monitor_model = strdup(val);
				} else if (strcmp(key, "monitor_serial") == 0) {
					rule->monitor_serial = strdup(val);
				} else if (strcmp(key, "no_render_border") == 0) {
					rule->no_render_border = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "open_as_floating") == 0) {
					rule->open_as_floating = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "no_hide") == 0) {
					rule->no_hide = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "nmaster") == 0) {
					rule->nmaster = CLAMP_INT(atoi(val), 1, 99);
				} else if (strcmp(key, "mfact") == 0) {
					rule->mfact = CLAMP_FLOAT(atof(val), 0.1f, 0.9f);
				} else if (strcmp(key, "scroller_default_proportion") == 0) {
					rule->scroller_default_proportion =
						CLAMP_FLOAT(atof(val), 0.0f, 1.0f);
				} else if (strcmp(key, "scroller_default_proportion_single") ==
						   0) {
					rule->scroller_default_proportion_single =
						CLAMP_FLOAT(atof(val), 0.0f, 1.0f);
				} else if (strcmp(key, "scroller_ignore_proportion_single") ==
						   0) {
					rule->scroller_ignore_proportion_single =
						CLAMP_INT(atoi(val), 0, 1);
				} else {
					mango_error(false, WLR_ERROR,
								"Unknown "
								"tag rule "
								"option:\033[1m\033[31m %s\n",
								key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		config->tag_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "layerrule") == 0) {
		config->layer_rules =
			realloc(config->layer_rules,
					(config->layer_rules_count + 1) * sizeof(ConfigLayerRule));
		if (!config->layer_rules) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for layer rules\n");
			return false;
		}

		ConfigLayerRule *rule = &config->layer_rules[config->layer_rules_count];
		memset(rule, 0, sizeof(ConfigLayerRule));

		// 设置默认值
		rule->layer_name = NULL;
		rule->animation_type_open = NULL;
		rule->animation_type_close = NULL;
		rule->shield_when_capture = 0;
		rule->noblur = 0;
		rule->noanim = 0;
		rule->noshadow = 0;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "layer_name") == 0) {
					rule->layer_name = strdup(val);
				} else if (strcmp(key, "animation_type_open") == 0) {
					rule->animation_type_open = strdup(val);
				} else if (strcmp(key, "animation_type_close") == 0) {
					rule->animation_type_close = strdup(val);
				} else if (strcmp(key, "shield_when_capture") == 0) {
					rule->shield_when_capture = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "noblur") == 0) {
					rule->noblur = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "noanim") == 0) {
					rule->noanim = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "noshadow") == 0) {
					rule->noshadow = CLAMP_INT(atoi(val), 0, 1);
				} else {
					mango_error(false, WLR_ERROR,
								"Unknown "
								"layer rule "
								"option:\033[1m\033[31m %s\n",
								key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		// 如果没有指定布局名称，则使用默认值
		if (rule->layer_name == NULL) {
			rule->layer_name = strdup("default");
		}

		config->layer_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "windowrule") == 0) {
		config->window_rules =
			realloc(config->window_rules,
					(config->window_rules_count + 1) * sizeof(ConfigWinRule));
		if (!config->window_rules) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for window rules\n");
			return false;
		}

		ConfigWinRule *rule = &config->window_rules[config->window_rules_count];
		memset(rule, 0, sizeof(ConfigWinRule));

		// int32_t rule value, relay to a client property
		rule->isfloating = -1;
		rule->isfullscreen = -1;
		rule->isfakefullscreen = -1;
		rule->isnoborder = -1;
		rule->isnoshadow = -1;
		rule->isnoradius = -1;
		rule->isnoanimation = -1;
		rule->isopensilent = -1;
		rule->istagsilent = -1;
		rule->isnamedscratchpad = -1;
		rule->isunglobal = -1;
		rule->isglobal = -1;
		rule->isoverlay = -1;
		rule->shield_when_capture = -1;
		rule->allow_shortcuts_inhibit = -1;
		rule->ignore_maximize = -1;
		rule->ignore_minimize = -1;
		rule->isnosizehint = -1;
		rule->idleinhibit_when_focus = -1;
		rule->vrr_only_fullscreen = -1;
		rule->force_render = -1;
		rule->activation_bypass = -1;
		rule->isterm = -1;
		rule->allow_csd = -1;
		rule->force_fakemaximize = -1;
		rule->force_tiled_state = -1;
		rule->force_tearing = -1;
		rule->noswallow = -1;
		rule->noblur = -1;
		rule->nofocus = -1;
		rule->nofadein = -1;
		rule->nofadeout = -1;
		rule->no_force_center = -1;

		// string rule value, relay to a client property
		rule->animation_type_open = NULL;
		rule->animation_type_close = NULL;

		// float rule value, relay to a client property
		rule->focused_opacity = 0;
		rule->unfocused_opacity = 0;
		rule->scroller_proportion_single = 0.0f;
		rule->scroller_proportion = 0;

		// special rule value,not directly set to client property
		rule->tags = 0;
		rule->offsetx = 0;
		rule->offsety = 0;
		rule->width = 0;
		rule->height = 0;
		rule->monitor = NULL;
		rule->id = NULL;
		rule->title = NULL;

		rule->globalkeybinding = (KeyBinding){0};

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "isfloating") == 0) {
					rule->isfloating = atoi(val);
				} else if (strcmp(key, "title") == 0) {
					rule->title = strdup(val);
				} else if (strcmp(key, "appid") == 0) {
					rule->id = strdup(val);
				} else if (strcmp(key, "animation_type_open") == 0) {
					rule->animation_type_open = strdup(val);
				} else if (strcmp(key, "animation_type_close") == 0) {
					rule->animation_type_close = strdup(val);
				} else if (strcmp(key, "tags") == 0) {
					rule->tags = parse_tag_mask(val);
				} else if (strcmp(key, "monitor") == 0) {
					rule->monitor = strdup(val);
				} else if (strcmp(key, "offsetx") == 0) {
					rule->offsetx = atoi(val);
				} else if (strcmp(key, "offsety") == 0) {
					rule->offsety = atoi(val);
				} else if (strcmp(key, "nofocus") == 0) {
					rule->nofocus = atoi(val);
				} else if (strcmp(key, "nofadein") == 0) {
					rule->nofadein = atoi(val);
				} else if (strcmp(key, "nofadeout") == 0) {
					rule->nofadeout = atoi(val);
				} else if (strcmp(key, "no_force_center") == 0) {
					rule->no_force_center = atoi(val);
				} else if (strcmp(key, "width") == 0) {
					rule->width = atof(val);
				} else if (strcmp(key, "height") == 0) {
					rule->height = atof(val);
				} else if (strcmp(key, "isnoborder") == 0) {
					rule->isnoborder = atoi(val);
				} else if (strcmp(key, "isnoshadow") == 0) {
					rule->isnoshadow = atoi(val);
				} else if (strcmp(key, "isnoradius") == 0) {
					rule->isnoradius = atoi(val);
				} else if (strcmp(key, "isnoanimation") == 0) {
					rule->isnoanimation = atoi(val);
				} else if (strcmp(key, "isopensilent") == 0) {
					rule->isopensilent = atoi(val);
				} else if (strcmp(key, "istagsilent") == 0) {
					rule->istagsilent = atoi(val);
				} else if (strcmp(key, "isnamedscratchpad") == 0) {
					rule->isnamedscratchpad = atoi(val);
				} else if (strcmp(key, "isunglobal") == 0) {
					rule->isunglobal = atoi(val);
				} else if (strcmp(key, "isglobal") == 0) {
					rule->isglobal = atoi(val);
				} else if (strcmp(key, "scroller_proportion_single") == 0) {
					rule->scroller_proportion_single = atof(val);
				} else if (strcmp(key, "unfocused_opacity") == 0) {
					rule->unfocused_opacity = atof(val);
				} else if (strcmp(key, "focused_opacity") == 0) {
					rule->focused_opacity = atof(val);
				} else if (strcmp(key, "isoverlay") == 0) {
					rule->isoverlay = atoi(val);
				} else if (strcmp(key, "shield_when_capture") == 0) {
					rule->shield_when_capture = atoi(val);
				} else if (strcmp(key, "allow_shortcuts_inhibit") == 0) {
					rule->allow_shortcuts_inhibit = atoi(val);
				} else if (strcmp(key, "ignore_maximize") == 0) {
					rule->ignore_maximize = atoi(val);
				} else if (strcmp(key, "ignore_minimize") == 0) {
					rule->ignore_minimize = atoi(val);
				} else if (strcmp(key, "isnosizehint") == 0) {
					rule->isnosizehint = atoi(val);
				} else if (strcmp(key, "idleinhibit_when_focus") == 0) {
					rule->idleinhibit_when_focus = atoi(val);
				} else if (strcmp(key, "vrr_only_fullscreen") == 0) {
					rule->vrr_only_fullscreen = atoi(val);
				} else if (strcmp(key, "force_render") == 0) {
					rule->force_render = atoi(val);
				} else if (strcmp(key, "activation_bypass") == 0) {
					rule->activation_bypass = atoi(val);
				} else if (strcmp(key, "isterm") == 0) {
					rule->isterm = atoi(val);
				} else if (strcmp(key, "allow_csd") == 0) {
					rule->allow_csd = atoi(val);
				} else if (strcmp(key, "force_fakemaximize") == 0) {
					rule->force_fakemaximize = atoi(val);
				} else if (strcmp(key, "force_tiled_state") == 0) {
					rule->force_tiled_state = atoi(val);
				} else if (strcmp(key, "force_tearing") == 0) {
					rule->force_tearing = atoi(val);
				} else if (strcmp(key, "noswallow") == 0) {
					rule->noswallow = atoi(val);
				} else if (strcmp(key, "noblur") == 0) {
					rule->noblur = atoi(val);
				} else if (strcmp(key, "scroller_proportion") == 0) {
					rule->scroller_proportion = atof(val);
				} else if (strcmp(key, "isfullscreen") == 0) {
					rule->isfullscreen = atoi(val);
				} else if (strcmp(key, "isfakefullscreen") == 0) {
					rule->isfakefullscreen = atoi(val);
				} else if (strcmp(key, "globalkeybinding") == 0) {
					char mod_str[256], keysym_str[256];
					sscanf(val, "%255[^-]-%255[a-zA-Z]", mod_str, keysym_str);
					trim_whitespace(mod_str);
					trim_whitespace(keysym_str);
					rule->globalkeybinding.mod = parse_mod(mod_str);
					rule->globalkeybinding.keysymcode =
						parse_key(keysym_str, false);
					if (rule->globalkeybinding.mod == UINT32_MAX) {
						return false;
					}
					if (rule->globalkeybinding.keysymcode.type ==
							KEY_TYPE_SYM &&
						rule->globalkeybinding.keysymcode.keysym ==
							XKB_KEY_NoSymbol) {
						return false;
					}
				} else {
					mango_error(false, WLR_ERROR,
								"Unknown "
								"window rule "
								"option:\033[1m\033[31m %s\n",
								key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}
		config->window_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "devicerule") == 0) {
		config->device_rules =
			realloc(config->device_rules, (config->device_rules_count + 1) *
											  sizeof(ConfigDeviceRule));
		if (!config->device_rules) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for device rules\n");
			return false;
		}

		ConfigDeviceRule *rule =
			&config->device_rules[config->device_rules_count];
		memset(rule, 0, sizeof(ConfigDeviceRule));

		// 默认值：-1 / UINT32_MAX / 空串 表示未设置，沿用全局配置
		rule->repeat_rate = -1;
		rule->repeat_delay = -1;
		rule->natural_scrolling = -1;
		rule->accel_profile = -1;
		rule->left_handed = -1;
		rule->middle_button_emulation = -1;
		rule->scroll_method = UINT32_MAX;
		rule->scroll_button = UINT32_MAX;
		rule->click_method = UINT32_MAX;
		rule->send_events_mode = UINT32_MAX;
		rule->tap_to_click = -1;
		rule->tap_and_drag = -1;
		rule->drag_lock = -1;
		rule->button_map = UINT32_MAX;
		rule->disable_while_typing = -1;
		rule->accel_speed = NAN;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "name") == 0) {
					rule->name = strdup(val);
				} else if (strcmp(key, "type") == 0) {
					snprintf(rule->type, sizeof(rule->type), "%s", val);
				} else if (strcmp(key, "repeat_rate") == 0) {
					rule->repeat_rate = CLAMP_INT(atoi(val), 0, 1000);
				} else if (strcmp(key, "repeat_delay") == 0) {
					rule->repeat_delay = CLAMP_INT(atoi(val), 0, 10000);
				} else if (strcmp(key, "kb_rules") == 0) {
					snprintf(rule->kb_rules, sizeof(rule->kb_rules), "%s", val);
				} else if (strcmp(key, "kb_model") == 0) {
					snprintf(rule->kb_model, sizeof(rule->kb_model), "%s", val);
				} else if (strcmp(key, "kb_layout") == 0) {
					snprintf(rule->kb_layout, sizeof(rule->kb_layout), "%s",
							 val);
				} else if (strcmp(key, "kb_variant") == 0) {
					snprintf(rule->kb_variant, sizeof(rule->kb_variant), "%s",
							 val);
				} else if (strcmp(key, "kb_options") == 0) {
					snprintf(rule->kb_options, sizeof(rule->kb_options), "%s",
							 val);
				} else if (strcmp(key, "natural_scrolling") == 0) {
					rule->natural_scrolling = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "accel_profile") == 0) {
					rule->accel_profile = CLAMP_INT(atoi(val), 0, 2);
				} else if (strcmp(key, "accel_speed") == 0) {
					rule->accel_speed = CLAMP_FLOAT(atof(val), -1.0, 1.0);
				} else if (strcmp(key, "left_handed") == 0) {
					rule->left_handed = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "middle_button_emulation") == 0) {
					rule->middle_button_emulation = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "scroll_method") == 0) {
					rule->scroll_method = (uint32_t)atoi(val);
				} else if (strcmp(key, "scroll_button") == 0) {
					rule->scroll_button = (uint32_t)atoi(val);
				} else if (strcmp(key, "click_method") == 0) {
					rule->click_method = (uint32_t)atoi(val);
				} else if (strcmp(key, "send_events_mode") == 0) {
					rule->send_events_mode = (uint32_t)atoi(val);
				} else if (strcmp(key, "tap_to_click") == 0) {
					rule->tap_to_click = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "tap_and_drag") == 0) {
					rule->tap_and_drag = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "drag_lock") == 0) {
					rule->drag_lock = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "button_map") == 0) {
					rule->button_map = (uint32_t)atoi(val);
				} else if (strcmp(key, "disable_while_typing") == 0) {
					rule->disable_while_typing = CLAMP_INT(atoi(val), 0, 1);
				} else {
					mango_error(false, WLR_ERROR,
								"Unknown device rule option: %s\n", key);
					parse_error = true;
				}
			} else {
				mango_error(false, WLR_ERROR,
							"Invalid device rule format: %s\n", token);
				parse_error = true;
			}
			token = strtok(NULL, ",");
		}
		config->device_rules_count++;
		return !parse_error;
	} else if (strncmp(key, "env", 3) == 0) {

		char env_type[256], env_value[256];
		if (sscanf(value, "%255[^,],%255[^\n]", env_type, env_value) < 2) {
			mango_error(false, WLR_ERROR,
						"Invalid bind format: "
						"\033[1m\033[31m%s\n",
						value);
			return false;
		}
		trim_whitespace(env_type);
		trim_whitespace(env_value);

		ConfigEnv *env = calloc(1, sizeof(ConfigEnv));

		const char *needle = "~/";

		if (strstr(env_value, needle)) {
			const char *home = getenv("HOME");

			size_t rlen = 1; // replace only ~

			if (!home) {
				free(env);
				mango_error(false, WLR_ERROR,
							"HOME environment "
							"variable not set.\n");
				return false;
			}

			size_t hlen = strlen(home);
			size_t len = strlen(env_value) + 1;

			for (char *p = strstr(env_value, needle); p;
				 p = strstr(p + rlen, needle))
				len += hlen - rlen;

			env->value = malloc(len);
			if (!env->value) {
				free(env);
				mango_error(false, WLR_ERROR,
							"Failed to "
							"allocate memory while expanding $HOME\n");
				return false;
			}

			char *substr;
			char *src = env_value;
			char *dst = env->value;

			while ((substr = strstr(src, needle))) {
				size_t n = substr - src;
				memcpy(dst, src, n);
				dst += n;
				memcpy(dst, home, hlen);
				dst += hlen;
				src = substr + rlen;
			}

			strcpy(dst, src);
		} else {
			env->value = strdup(env_value);
		}

		env->type = strdup(env_type);
		config->env = realloc(config->env,
							  (config->env_count + 1) * sizeof(*config->env));
		if (!config->env) {
			free(env->type);
			free(env->value);
			free(env);
			mango_error(false, WLR_ERROR,
						"Failed to "
						"allocate memory for env\n");
			return false;
		}

		config->env[config->env_count] = env;
		config->env_count++;

	} else if (strncmp(key, "exec", 9) == 0) {
		char **new_exec =
			realloc(config->exec, (config->exec_count + 1) * sizeof(char *));
		if (!new_exec) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for exec\n");
			return false;
		}
		config->exec = new_exec;

		config->exec[config->exec_count] = strdup(value);
		if (!config->exec[config->exec_count]) {
			mango_error(false, WLR_ERROR,
						"Failed to "
						"duplicate exec string\n");
			return false;
		}

		config->exec_count++;

	} else if (strncmp(key, "exec-once", 9) == 0) {

		char **new_exec_once = realloc(
			config->exec_once, (config->exec_once_count + 1) * sizeof(char *));
		if (!new_exec_once) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for exec_once\n");
			return false;
		}
		config->exec_once = new_exec_once;

		config->exec_once[config->exec_once_count] = strdup(value);
		if (!config->exec_once[config->exec_once_count]) {
			mango_error(false, WLR_ERROR,
						"Failed to duplicate "
						"exec_once string\n");
			return false;
		}

		config->exec_once_count++;

	} else if (regex_match("^bind[s|l|r|p|c]*$", key)) {
		config->key_bindings =
			realloc(config->key_bindings,
					(config->key_bindings_count + 1) * sizeof(KeyBinding));
		if (!config->key_bindings) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for key bindings\n");
			return false;
		}

		KeyBinding *binding = &config->key_bindings[config->key_bindings_count];
		memset(binding, 0, sizeof(KeyBinding));
		binding->line_number = line_number;
		binding->file_index = current_file_index;

		char mod_str[256], keysym_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^\n]",
				   mod_str, keysym_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			mango_error(false, WLR_ERROR,
						"Invalid bind format: "
						"\033[1m\033[31m%s\n",
						value);
			return false;
		}
		trim_whitespace(mod_str);
		trim_whitespace(keysym_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		strcpy(binding->mode, config->keymode);
		if (strcmp(binding->mode, "common") == 0) {
			binding->iscommonmode = true;
			binding->isdefaultmode = false;
		} else if (strcmp(binding->mode, "default") == 0) {
			binding->isdefaultmode = true;
			binding->iscommonmode = false;
		} else {
			binding->isdefaultmode = false;
			binding->iscommonmode = false;
		}

		parse_bind_flags(key, binding);
		binding->keysymcode =
			parse_key(keysym_str, binding->keysymcode.type == KEY_TYPE_SYM);
		binding->mod = parse_mod(mod_str);
		binding->arg.i = 0;
		binding->arg.i2 = 0;
		binding->arg.f = 0.0f;
		binding->arg.f2 = 0.0f;
		binding->arg.ui = 0;
		binding->arg.ui2 = 0;
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->arg.tc = NULL;
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);
		if (!binding->func || binding->mod == UINT32_MAX ||
			(binding->keysymcode.type == KEY_TYPE_SYM &&
			 binding->keysymcode.keysym == XKB_KEY_NoSymbol)) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}
			if (!binding->func)
				mango_error(false, WLR_ERROR,
							"Unknown "
							"dispatch in bind: "
							"\033[1m\033[31m%s\n",
							func_name);
			return false;
		} else {
			config->key_bindings_count++;
		}

	} else if (strncmp(key, "mousebind", 9) == 0) {
		config->mouse_bindings =
			realloc(config->mouse_bindings,
					(config->mouse_bindings_count + 1) * sizeof(MouseBinding));
		if (!config->mouse_bindings) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for mouse bindings\n");
			return false;
		}

		MouseBinding *binding =
			&config->mouse_bindings[config->mouse_bindings_count];
		memset(binding, 0, sizeof(MouseBinding));
		set_binding_keymode(config, binding->mode, &binding->iscommonmode,
							&binding->isdefaultmode);
		binding->line_number = line_number;
		binding->file_index = current_file_index;

		char mod_str[256], button_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^\n]",
				   mod_str, button_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			mango_error(false, WLR_ERROR,
						"Invalid mousebind "
						"format: "
						"%s\n",
						value);
			return false;
		}
		trim_whitespace(mod_str);
		trim_whitespace(button_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->mod = parse_mod(mod_str);
		binding->button = parse_button(button_str);
		binding->arg.i = 0;
		binding->arg.i2 = 0;
		binding->arg.f = 0.0f;
		binding->arg.f2 = 0.0f;
		binding->arg.ui = 0;
		binding->arg.ui2 = 0;
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->arg.tc = NULL;

		// TODO: remove this in next version
		if (binding->mod == 0 &&
			(binding->button == BTN_LEFT || binding->button == BTN_RIGHT)) {
			mango_error(false, WLR_ERROR,
						"\033[31m%s\033[33m can't "
						"bind to \033[31m%s\033[33m mod key\033[0m\n",
						button_str, mod_str);
			return false;
		}

		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);
		if (!binding->func || binding->mod == UINT32_MAX ||
			binding->button == UINT32_MAX) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}

			if (!binding->func)
				mango_error(false, WLR_ERROR,
							"Unknown "
							"dispatch in "
							"mousebind: \033[1m\033[31m%s\n",
							func_name);
			return false;
		} else {
			config->mouse_bindings_count++;
		}
	} else if (strncmp(key, "axisbind", 8) == 0) {
		config->axis_bindings =
			realloc(config->axis_bindings,
					(config->axis_bindings_count + 1) * sizeof(AxisBinding));
		if (!config->axis_bindings) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for axis bindings\n");
			return false;
		}

		AxisBinding *binding =
			&config->axis_bindings[config->axis_bindings_count];
		memset(binding, 0, sizeof(AxisBinding));
		set_binding_keymode(config, binding->mode, &binding->iscommonmode,
							&binding->isdefaultmode);
		binding->line_number = line_number;
		binding->file_index = current_file_index;

		char mod_str[256], dir_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^\n]",
				   mod_str, dir_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			mango_error(false, WLR_ERROR,
						"Invalid axisbind "
						"format: %s\n",
						value);
			return false;
		}

		trim_whitespace(mod_str);
		trim_whitespace(dir_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->mod = parse_mod(mod_str);
		binding->dir = parse_direction(dir_str);
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->arg.tc = NULL;
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);

		if (!binding->func || binding->mod == UINT32_MAX) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}
			if (!binding->func)
				mango_error(false, WLR_ERROR,
							"Unknown "
							"dispatch in "
							"axisbind: \033[1m\033[31m%s\n",
							func_name);
			return false;
		} else {
			config->axis_bindings_count++;
		}

	} else if (strncmp(key, "switchbind", 10) == 0) {
		config->switch_bindings = realloc(config->switch_bindings,
										  (config->switch_bindings_count + 1) *
											  sizeof(SwitchBinding));
		if (!config->switch_bindings) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for switch bindings\n");
			return false;
		}

		SwitchBinding *binding =
			&config->switch_bindings[config->switch_bindings_count];
		memset(binding, 0, sizeof(SwitchBinding));
		set_binding_keymode(config, binding->mode, &binding->iscommonmode,
							&binding->isdefaultmode);
		binding->line_number = line_number;
		binding->file_index = current_file_index;

		char fold_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^\n]",
				   fold_str, func_name, arg_value, arg_value2, arg_value3,
				   arg_value4, arg_value5) < 3) {
			mango_error(false, WLR_ERROR,
						"Invalid switchbind "
						"format: %s\n",
						value);
			return false;
		}
		trim_whitespace(fold_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->fold = parse_fold_state(fold_str);
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);

		if (!binding->func) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}

			mango_error(false, WLR_ERROR,
						"Unknown dispatch in "
						"switchbind: "
						"\033[1m\033[31m%s\n",
						func_name);
			return false;
		} else {
			config->switch_bindings_count++;
		}

	} else if (strncmp(key, "gesturebind", 11) == 0) {
		config->gesture_bindings = realloc(
			config->gesture_bindings,
			(config->gesture_bindings_count + 1) * sizeof(GestureBinding));
		if (!config->gesture_bindings) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for axis gesturebind\n");
			return false;
		}

		GestureBinding *binding =
			&config->gesture_bindings[config->gesture_bindings_count];
		memset(binding, 0, sizeof(GestureBinding));
		set_binding_keymode(config, binding->mode, &binding->iscommonmode,
							&binding->isdefaultmode);
		binding->line_number = line_number;
		binding->file_index = current_file_index;

		char mod_str[256], motion_str[256], fingers_count_str[256],
			func_name[256], arg_value[256] = "0\0", arg_value2[256] = "0\0",
							arg_value3[256] = "0\0", arg_value4[256] = "0\0",
							arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^,],%255[^\n]",
				   mod_str, motion_str, fingers_count_str, func_name, arg_value,
				   arg_value2, arg_value3, arg_value4, arg_value5) < 4) {
			mango_error(false, WLR_ERROR,
						"Invalid gesturebind "
						"format: %s\n",
						value);
			return false;
		}

		trim_whitespace(mod_str);
		trim_whitespace(motion_str);
		trim_whitespace(fingers_count_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->mod = parse_mod(mod_str);
		binding->motion = parse_direction(motion_str);
		binding->fingers_count = atoi(fingers_count_str);
		binding->arg.i = 0;
		binding->arg.i2 = 0;
		binding->arg.f = 0.0f;
		binding->arg.f2 = 0.0f;
		binding->arg.ui = 0;
		binding->arg.ui2 = 0;
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->arg.tc = NULL;
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);

		if (!binding->func || binding->mod == UINT32_MAX) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}
			if (!binding->func)
				mango_error(false, WLR_ERROR,
							"Unknown "
							"dispatch in "
							"axisbind: \033[1m\033[31m%s\n",
							func_name);
			return false;
		} else {
			config->gesture_bindings_count++;
		}

	} else if (strncmp(key, "source-optional", 15) == 0) {
		parse_config_file(config, value, false);
	} else if (strncmp(key, "source", 6) == 0) {
		parse_config_file(config, value, true);
	} else {
		mango_error(false, WLR_ERROR,
					"Unknown keyword: "
					"\033[1m\033[31m%s\n",
					key);
		return false;
	}

	return true;
}

bool parse_config_line(Config *config, const char *line, int line_number) {
	char processed_line[512];
	strncpy(processed_line, line, sizeof(processed_line) - 1);
	processed_line[sizeof(processed_line) - 1] = '\0';

	remove_comment(processed_line);

	char key[256], value[256];
	if (sscanf(processed_line, "%255[^=]=%255[^\n]", key, value) != 2) {
		mango_error(false, WLR_ERROR, "Invalid line format: %s", line);
		return false;
	}

	trim_whitespace(key);
	trim_whitespace(value);

	return parse_option(config, key, value, line_number);
}

bool parse_config_file(Config *config, const char *file_path, bool must_exist) {
	FILE *file;
	char full_path[1024];

	if (file_path[0] == '.' && file_path[1] == '/') {
		// Relative path

		if (cli_config_path[0]) {
			char *config_path = strdup(cli_config_path);
			char *config_dir = dirname(config_path);
			snprintf(full_path, sizeof(full_path), "%s/%s", config_dir,
					 file_path + 1);
			free(config_path);
		} else {
			const char *home = getenv("HOME");
			if (!home) {
				mango_error(false, WLR_ERROR,
							"HOME environment "
							"variable not set.\n");
				return false;
			}
			snprintf(full_path, sizeof(full_path), "%s/.config/mango/%s", home,
					 file_path + 1);
		}
		file = fopen(full_path, "r");

	} else if (file_path[0] == '~' &&
			   (file_path[1] == '/' || file_path[1] == '\0')) {
		// Home directory

		const char *home = getenv("HOME");
		if (!home) {
			mango_error(false, WLR_ERROR,
						"HOME environment "
						"variable not set.\n");
			return false;
		}
		snprintf(full_path, sizeof(full_path), "%s%s", home, file_path + 1);
		file = fopen(full_path, "r");

	} else {
		// Absolute path
		file = fopen(file_path, "r");
	}

	// 保存当前文件索引，用于递归恢复
	int saved_file_index = current_file_index;

	// 将文件路径加入全局列表
	file_paths = realloc(file_paths, (file_paths_count + 1) * sizeof(char *));
	file_paths[file_paths_count] =
		strdup(file_path); // 需要 strdup 申请独立内存
	current_file_index = file_paths_count;
	file_paths_count++;

	if (!file) {
		if (must_exist) {
			mango_error(false, WLR_ERROR,
						"Failed to open "
						"config file: %s\n",
						file_path);
			return false;
		} else {
			return true;
		}
	}

	char line[512];
	bool parse_correct = true;
	bool parse_line_correct = true;
	uint32_t line_count = 0;
	while (fgets(line, sizeof(line), file)) {
		line_count++;
		if (line[0] == '#' || line[0] == '\n') {
			continue;
		}
		parse_line_correct = parse_config_line(config, line, line_count);
		if (!parse_line_correct) {
			parse_correct = false;
			mango_error(false, WLR_INFO,
						"\033[1;31m╰─\033[1;33m[Index]\033[0m "
						"\033[1;36m%s\033[0m:\033[1;35m%d\033[0m\n"
						"   \033[1;36m╰─\033[0;33m%s\033[0m\n\n",
						file_path, line_count, line);
		}
	}

	fclose(file);

	current_file_index = saved_file_index;
	return parse_correct;
}

static const char *mod_to_string(uint32_t mod) {
	static char buf[128];
	buf[0] = '\0';
	if (mod & WLR_MODIFIER_LOGO)
		strcat(buf, "Super+");
	if (mod & WLR_MODIFIER_CTRL)
		strcat(buf, "Ctrl+");
	if (mod & WLR_MODIFIER_ALT)
		strcat(buf, "Alt+");
	if (mod & WLR_MODIFIER_SHIFT)
		strcat(buf, "Shift+");
	if (mod & WLR_MODIFIER_MOD3)
		strcat(buf, "Hyper+");
	size_t len = strlen(buf);
	if (len > 0)
		buf[len - 1] = '\0';
	else
		strcpy(buf, "None");
	return buf;
}

static int compare_keybind_by_key_only(const void *a, const void *b) {
	const KeyBinding *ka = (const KeyBinding *)a;
	const KeyBinding *kb = (const KeyBinding *)b;

	if (ka->mod != kb->mod)
		return (ka->mod > kb->mod) ? 1 : -1;

	if (ka->keysymcode.type != kb->keysymcode.type)
		return (ka->keysymcode.type > kb->keysymcode.type) ? 1 : -1;

	if (ka->keysymcode.type == KEY_TYPE_SYM) {
		if (ka->keysymcode.keysym != kb->keysymcode.keysym)
			return (ka->keysymcode.keysym > kb->keysymcode.keysym) ? 1 : -1;
	} else {
		if (ka->keysymcode.keycode.keycode1 != kb->keysymcode.keycode.keycode1)
			return (ka->keysymcode.keycode.keycode1 >
					kb->keysymcode.keycode.keycode1)
					   ? 1
					   : -1;
	}
	return 0;
}

static bool same_key(const KeyBinding *a, const KeyBinding *b) {
	return compare_keybind_by_key_only(a, b) == 0;
}

bool check_key_binding_conflicts(Config *config) {
	int n = config->key_bindings_count;
	if (n < 2)
		return false;

	/* 复制用户定义的绑定（行号 > 0） */
	KeyBinding *binds = malloc(n * sizeof(KeyBinding));
	int count = 0;
	for (int i = 0; i < n; i++) {
		if (config->key_bindings[i].line_number > 0)
			binds[count++] = config->key_bindings[i];
	}
	if (count < 2) {
		free(binds);
		return false;
	}

	/* 只按按键排序，将相同按键的绑定排在一起 */
	qsort(binds, count, sizeof(KeyBinding), compare_keybind_by_key_only);

	bool conflict_found = false;

	for (int i = 0; i < count;) {
		int j = i;
		/* 找出所有按键相同的绑定（区间 [i, j) ） */
		while (j < count && same_key(&binds[i], &binds[j]))
			j++;

		/* 在该区间内检测冲突 */
		for (int a = i; a < j; a++) {
			for (int b = a + 1; b < j; b++) {
				bool same_mode = (strcmp(binds[a].mode, binds[b].mode) == 0);
				bool any_common =
					binds[a].iscommonmode || binds[b].iscommonmode;
				bool allow_conflict =
					binds[a].isallowconflict && binds[b].isallowconflict;

				if ((same_mode || any_common) && !allow_conflict) {

					const char *file_a = (binds[a].file_index >= 0)
											 ? file_paths[binds[a].file_index]
											 : "(built-in)";
					const char *file_b = (binds[b].file_index >= 0)
											 ? file_paths[binds[b].file_index]
											 : "(built-in)";

					conflict_found = true;
					fprintf(stderr,
							"\033[1;33m[WARNING]\033[0m Key binding conflict "
							"in keymode \033[1;36m%s\033[0m:\n"
							"  File \033[1;32m\"%s\"\033[0m, line "
							"\033[1;35m%d\033[0m\n"
							"  File \033[1;32m\"%s\"\033[0m, line "
							"\033[1;35m%d\033[0m\n\n",
							(any_common ? "common" : binds[a].mode), file_a,
							binds[a].line_number, file_b, binds[b].line_number);
				}
			}
		}
		i = j; /* 跳到下一个按键组 */
	}

	free(binds);
	return conflict_found;
}

static bool same_mousebind_key(const void *a, const void *b) {
	const MouseBinding *ma = (const MouseBinding *)a;
	const MouseBinding *mb = (const MouseBinding *)b;
	return ma->mod == mb->mod && ma->button == mb->button;
}

static bool same_axisbind_key(const void *a, const void *b) {
	const AxisBinding *aa = (const AxisBinding *)a;
	const AxisBinding *ab = (const AxisBinding *)b;
	return aa->mod == ab->mod && aa->dir == ab->dir;
}

static bool same_switchbind_key(const void *a, const void *b) {
	const SwitchBinding *sa = (const SwitchBinding *)a;
	const SwitchBinding *sb = (const SwitchBinding *)b;
	return sa->fold == sb->fold;
}

static bool same_gesturebind_key(const void *a, const void *b) {
	const GestureBinding *ga = (const GestureBinding *)a;
	const GestureBinding *gb = (const GestureBinding *)b;
	return ga->mod == gb->mod && ga->motion == gb->motion &&
		   ga->fingers_count == gb->fingers_count;
}

static bool
check_simple_binding_conflicts(void *arr, size_t count, size_t elem_size,
							   bool (*same_key)(const void *, const void *),
							   BindingMetaFunc get_meta, const char *kind) {
	bool conflict_found = false;

	for (size_t i = 0; i < count; i++) {
		for (size_t j = i + 1; j < count; j++) {
			if (!same_key((char *)arr + i * elem_size,
						  (char *)arr + j * elem_size))
				continue;

			BindingConflictMeta ma, mb;
			get_meta((char *)arr + i * elem_size, &ma);
			get_meta((char *)arr + j * elem_size, &mb);

			bool same_mode = (strcmp(ma.mode, mb.mode) == 0);
			bool any_common = ma.iscommonmode || mb.iscommonmode;
			if (same_mode || any_common) {

				const char *file_a = (ma.file_index >= 0)
										 ? file_paths[ma.file_index]
										 : "(built-in)";
				const char *file_b = (mb.file_index >= 0)
										 ? file_paths[mb.file_index]
										 : "(built-in)";

				conflict_found = true;
				fprintf(stderr,
						"\033[1;33m[WARNING]\033[0m %s conflict "
						"in keymode \033[1;36m%s\033[0m:\n"
						"  File \033[1;32m\"%s\"\033[0m, line "
						"\033[1;35m%d\033[0m\n"
						"  File \033[1;32m\"%s\"\033[0m, line "
						"\033[1;35m%d\033[0m\n\n",
						kind, (any_common ? "common" : ma.mode), file_a,
						ma.line_number, file_b, mb.line_number);
			}
		}
	}
	return conflict_found;
}

static void get_mousebind_meta(const void *elem, BindingConflictMeta *meta) {
	const MouseBinding *b = (const MouseBinding *)elem;
	meta->mode = b->mode;
	meta->iscommonmode = b->iscommonmode;
	meta->file_index = b->file_index;
	meta->line_number = b->line_number;
}

static void get_axisbind_meta(const void *elem, BindingConflictMeta *meta) {
	const AxisBinding *b = (const AxisBinding *)elem;
	meta->mode = b->mode;
	meta->iscommonmode = b->iscommonmode;
	meta->file_index = b->file_index;
	meta->line_number = b->line_number;
}

static void get_switchbind_meta(const void *elem, BindingConflictMeta *meta) {
	const SwitchBinding *b = (const SwitchBinding *)elem;
	meta->mode = b->mode;
	meta->iscommonmode = b->iscommonmode;
	meta->file_index = b->file_index;
	meta->line_number = b->line_number;
}

static void get_gesturebind_meta(const void *elem, BindingConflictMeta *meta) {
	const GestureBinding *b = (const GestureBinding *)elem;
	meta->mode = b->mode;
	meta->iscommonmode = b->iscommonmode;
	meta->file_index = b->file_index;
	meta->line_number = b->line_number;
}

bool check_mouse_binding_conflicts(Config *config) {
	return check_simple_binding_conflicts(
		config->mouse_bindings, config->mouse_bindings_count,
		sizeof(MouseBinding), same_mousebind_key, get_mousebind_meta,
		"mousebind");
}

bool check_axis_binding_conflicts(Config *config) {
	return check_simple_binding_conflicts(
		config->axis_bindings, config->axis_bindings_count, sizeof(AxisBinding),
		same_axisbind_key, get_axisbind_meta, "axisbind");
}

bool check_switch_binding_conflicts(Config *config) {
	return check_simple_binding_conflicts(
		config->switch_bindings, config->switch_bindings_count,
		sizeof(SwitchBinding), same_switchbind_key, get_switchbind_meta,
		"switchbind");
}

bool check_gesture_binding_conflicts(Config *config) {
	return check_simple_binding_conflicts(
		config->gesture_bindings, config->gesture_bindings_count,
		sizeof(GestureBinding), same_gesturebind_key, get_gesturebind_meta,
		"gesturebind");
}

void free_circle_layout(Config *config) {
	if (config->circle_layout) {
		// 释放每个字符串
		for (int32_t i = 0; i < config->circle_layout_count; i++) {
			if (config->circle_layout[i]) {
				free(config->circle_layout[i]);	 // 释放单个字符串
				config->circle_layout[i] = NULL; // 防止野指针
			}
		}
		// 释放 circle_layout 数组本身
		free(config->circle_layout);
		config->circle_layout = NULL; // 防止野指针
	}
	config->circle_layout_count = 0; // 重置计数
}

void free_baked_points(void) {
	if (baked_points_move) {
		free(baked_points_move);
		baked_points_move = NULL;
	}
	if (baked_points_open) {
		free(baked_points_open);
		baked_points_open = NULL;
	}
	if (baked_points_close) {
		free(baked_points_close);
		baked_points_close = NULL;
	}
	if (baked_points_tag) {
		free(baked_points_tag);
		baked_points_tag = NULL;
	}
	if (baked_points_focus) {
		free(baked_points_focus);
		baked_points_focus = NULL;
	}
	if (baked_points_opafadein) {
		free(baked_points_opafadein);
		baked_points_opafadein = NULL;
	}
	if (baked_points_opafadeout) {
		free(baked_points_opafadeout);
		baked_points_opafadeout = NULL;
	}
}

void free_config(void) {
	// 释放内存
	int32_t i;

	// 释放 window_rules
	if (config.window_rules) {
		for (int32_t i = 0; i < config.window_rules_count; i++) {
			ConfigWinRule *rule = &config.window_rules[i];
			if (rule->id)
				free((void *)rule->id);
			if (rule->title)
				free((void *)rule->title);
			if (rule->animation_type_open)
				free((void *)rule->animation_type_open);
			if (rule->animation_type_close)
				free((void *)rule->animation_type_close);
			if (rule->monitor)
				free((void *)rule->monitor);
			rule->id = NULL;
			rule->title = NULL;
			rule->animation_type_open = NULL;
			rule->animation_type_close = NULL;
			rule->monitor = NULL;
			// 释放 globalkeybinding 的 arg.v（如果动态分配）
			if (rule->globalkeybinding.arg.v) {
				free((void *)rule->globalkeybinding.arg.v);
			}
		}
		free(config.window_rules);
		config.window_rules = NULL;
		config.window_rules_count = 0;
	}

	// 释放 device_rules
	if (config.device_rules) {
		for (i = 0; i < config.device_rules_count; i++) {
			if (config.device_rules[i].name)
				free(config.device_rules[i].name);
		}
		free(config.device_rules);
		config.device_rules = NULL;
		config.device_rules_count = 0;
	}

	// 释放 key_bindings
	if (config.key_bindings) {
		for (i = 0; i < config.key_bindings_count; i++) {
			if (config.key_bindings[i].arg.v) {
				free((void *)config.key_bindings[i].arg.v);
				config.key_bindings[i].arg.v = NULL;
			}
			if (config.key_bindings[i].arg.v2) {
				free((void *)config.key_bindings[i].arg.v2);
				config.key_bindings[i].arg.v2 = NULL;
			}
			if (config.key_bindings[i].arg.v3) {
				free((void *)config.key_bindings[i].arg.v3);
				config.key_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.key_bindings);
		config.key_bindings = NULL;
		config.key_bindings_count = 0;
	}

	// 释放 mouse_bindings
	if (config.mouse_bindings) {
		for (i = 0; i < config.mouse_bindings_count; i++) {
			if (config.mouse_bindings[i].arg.v) {
				free((void *)config.mouse_bindings[i].arg.v);
				config.mouse_bindings[i].arg.v = NULL;
			}
			if (config.mouse_bindings[i].arg.v2) {
				free((void *)config.mouse_bindings[i].arg.v2);
				config.mouse_bindings[i].arg.v2 = NULL;
			}
			if (config.mouse_bindings[i].arg.v3) {
				free((void *)config.mouse_bindings[i].arg.v3);
				config.mouse_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.mouse_bindings);
		config.mouse_bindings = NULL;
		config.mouse_bindings_count = 0;
	}

	// 释放 axis_bindings
	if (config.axis_bindings) {
		for (i = 0; i < config.axis_bindings_count; i++) {
			if (config.axis_bindings[i].arg.v) {
				free((void *)config.axis_bindings[i].arg.v);
				config.axis_bindings[i].arg.v = NULL;
			}
			if (config.axis_bindings[i].arg.v2) {
				free((void *)config.axis_bindings[i].arg.v2);
				config.axis_bindings[i].arg.v2 = NULL;
			}
			if (config.axis_bindings[i].arg.v3) {
				free((void *)config.axis_bindings[i].arg.v3);
				config.axis_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.axis_bindings);
		config.axis_bindings = NULL;
		config.axis_bindings_count = 0;
	}

	// 释放 switch_bindings
	if (config.switch_bindings) {
		for (i = 0; i < config.switch_bindings_count; i++) {
			if (config.switch_bindings[i].arg.v) {
				free((void *)config.switch_bindings[i].arg.v);
				config.switch_bindings[i].arg.v = NULL;
			}
			if (config.switch_bindings[i].arg.v2) {
				free((void *)config.switch_bindings[i].arg.v2);
				config.switch_bindings[i].arg.v2 = NULL;
			}
			if (config.switch_bindings[i].arg.v3) {
				free((void *)config.switch_bindings[i].arg.v3);
				config.switch_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.switch_bindings);
		config.switch_bindings = NULL;
		config.switch_bindings_count = 0;
	}

	// 释放 gesture_bindings
	if (config.gesture_bindings) {
		for (i = 0; i < config.gesture_bindings_count; i++) {
			if (config.gesture_bindings[i].arg.v) {
				free((void *)config.gesture_bindings[i].arg.v);
				config.gesture_bindings[i].arg.v = NULL;
			}
			if (config.gesture_bindings[i].arg.v2) {
				free((void *)config.gesture_bindings[i].arg.v2);
				config.gesture_bindings[i].arg.v2 = NULL;
			}
			if (config.gesture_bindings[i].arg.v3) {
				free((void *)config.gesture_bindings[i].arg.v3);
				config.gesture_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.gesture_bindings);
		config.gesture_bindings = NULL;
		config.gesture_bindings_count = 0;
	}

	// 释放 tag_rules
	if (config.tag_rules) {
		for (int32_t i = 0; i < config.tag_rules_count; i++) {
			if (config.tag_rules[i].layout_name)
				free((void *)config.tag_rules[i].layout_name);
			if (config.tag_rules[i].monitor_name)
				free((void *)config.tag_rules[i].monitor_name);
			if (config.tag_rules[i].monitor_make)
				free((void *)config.tag_rules[i].monitor_make);
			if (config.tag_rules[i].monitor_model)
				free((void *)config.tag_rules[i].monitor_model);
			if (config.tag_rules[i].monitor_serial)
				free((void *)config.tag_rules[i].monitor_serial);
		}
		free(config.tag_rules);
		config.tag_rules = NULL;
		config.tag_rules_count = 0;
	}

	// 释放 monitor_rules
	if (config.monitor_rules) {
		for (int32_t i = 0; i < config.monitor_rules_count; i++) {
			if (config.monitor_rules[i].name)
				free((void *)config.monitor_rules[i].name);
			if (config.monitor_rules[i].make)
				free((void *)config.monitor_rules[i].make);
			if (config.monitor_rules[i].model)
				free((void *)config.monitor_rules[i].model);
			if (config.monitor_rules[i].serial)
				free((void *)config.monitor_rules[i].serial);
			if (config.monitor_rules[i].icc)
				free((void *)config.monitor_rules[i].icc);
		}
		free(config.monitor_rules);
		config.monitor_rules = NULL;
		config.monitor_rules_count = 0;
	}

	// 释放 layer_rules
	if (config.layer_rules) {
		for (int32_t i = 0; i < config.layer_rules_count; i++) {
			if (config.layer_rules[i].layer_name)
				free((void *)config.layer_rules[i].layer_name);
			if (config.layer_rules[i].animation_type_open)
				free((void *)config.layer_rules[i].animation_type_open);
			if (config.layer_rules[i].animation_type_close)
				free((void *)config.layer_rules[i].animation_type_close);
		}
		free(config.layer_rules);
		config.layer_rules = NULL;
		config.layer_rules_count = 0;
	}

	// 释放 env
	if (config.env) {
		for (int32_t i = 0; i < config.env_count; i++) {
			if (config.env[i]->type) {
				free((void *)config.env[i]->type);
			}
			if (config.env[i]->value) {
				free((void *)config.env[i]->value);
			}
			free(config.env[i]);
		}
		free(config.env);
		config.env = NULL;
		config.env_count = 0;
	}

	// 释放 exec
	if (config.exec) {
		for (i = 0; i < config.exec_count; i++) {
			free(config.exec[i]);
		}
		free(config.exec);
		config.exec = NULL;
		config.exec_count = 0;
	}

	// 释放 exec_once
	if (config.exec_once) {
		for (i = 0; i < config.exec_once_count; i++) {
			free(config.exec_once[i]);
		}
		free(config.exec_once);
		config.exec_once = NULL;
		config.exec_once_count = 0;
	}

	// 释放 scroller_proportion_preset
	if (config.scroller_proportion_preset) {
		free(config.scroller_proportion_preset);
		config.scroller_proportion_preset = NULL;
		config.scroller_proportion_preset_count = 0;
	}

	if (config.cursor_theme) {
		free(config.cursor_theme);
		config.cursor_theme = NULL;
	}

	if (config.jumplabeldata.font_desc) {
		free((void *)config.jumplabeldata.font_desc);
		config.jumplabeldata.font_desc = NULL;
	}

	if (config.groupbardata.font_desc) {
		free((void *)config.groupbardata.font_desc);
		config.groupbardata.font_desc = NULL;
	}

	if (config.tablet_map_to_mon) {
		free(config.tablet_map_to_mon);
		config.tablet_map_to_mon = NULL;
	}

	if (config.touch_map_to_mon) {
		free(config.touch_map_to_mon);
		config.touch_map_to_mon = NULL;
	}

	if (config.jump_labels) {
		free(config.jump_labels);
		config.jump_labels = NULL;
	}

	// 释放 circle_layout
	free_circle_layout(&config);

	// 释放动画资源
	free_baked_points();

	// 清理解析按键用的keymap
	cleanup_config_keymap();
}

void update_global_var(void) { tagmask = ((uint32_t)1 << config.tag_num) - 1; }

void override_config(void) {
	config.animations = CLAMP_INT(config.animations, 0, 1);
	config.layer_animations = CLAMP_INT(config.layer_animations, 0, 1);
	config.tag_animation_direction =
		CLAMP_INT(config.tag_animation_direction, 0, 1);
	config.animation_fade_in = CLAMP_INT(config.animation_fade_in, 0, 1);
	config.animation_fade_out = CLAMP_INT(config.animation_fade_out, 0, 1);
	config.zoom_initial_ratio =
		CLAMP_FLOAT(config.zoom_initial_ratio, 0.1f, 1.0f);
	config.zoom_end_ratio = CLAMP_FLOAT(config.zoom_end_ratio, 0.1f, 1.0f);
	config.fadein_begin_opacity =
		CLAMP_FLOAT(config.fadein_begin_opacity, 0.0f, 1.0f);
	config.fadeout_begin_opacity =
		CLAMP_FLOAT(config.fadeout_begin_opacity, 0.0f, 1.0f);
	config.animation_duration_move =
		CLAMP_INT(config.animation_duration_move, 1, 50000);
	config.animation_duration_open =
		CLAMP_INT(config.animation_duration_open, 1, 50000);
	config.animation_duration_tag =
		CLAMP_INT(config.animation_duration_tag, 1, 50000);
	config.animation_duration_close =
		CLAMP_INT(config.animation_duration_close, 1, 50000);
	config.animation_duration_focus =
		CLAMP_INT(config.animation_duration_focus, 1, 50000);
	config.scroller_default_proportion =
		CLAMP_FLOAT(config.scroller_default_proportion, 0.1f, 1.0f);
	config.scroller_default_proportion_single =
		CLAMP_FLOAT(config.scroller_default_proportion_single, 0.1f, 1.0f);
	config.scroller_ignore_proportion_single =
		CLAMP_INT(config.scroller_ignore_proportion_single, 0, 1);
	config.scroller_focus_center =
		CLAMP_INT(config.scroller_focus_center, 0, 1);
	config.scroller_prefer_center =
		CLAMP_INT(config.scroller_prefer_center, 0, 1);
	config.scroller_prefer_overspread =
		CLAMP_INT(config.scroller_prefer_overspread, 0, 1);
	config.edge_scroller_pointer_focus =
		CLAMP_INT(config.edge_scroller_pointer_focus, 0, 1);
	config.edge_scroller_focus_allow_speed =
		CLAMP_FLOAT(config.edge_scroller_focus_allow_speed, 0.0f, 1000.0f);
	config.scroller_structs = CLAMP_INT(config.scroller_structs, 0, 1000);
	config.default_mfact = CLAMP_FLOAT(config.default_mfact, 0.1f, 0.9f);
	config.default_nmaster = CLAMP_INT(config.default_nmaster, 1, 1000);
	config.tag_num = CLAMP_INT(config.tag_num, 1, tag_num_MAX);
	config.tag_gather = CLAMP_INT(config.tag_gather, 0, 1);
	config.center_master_overspread =
		CLAMP_INT(config.center_master_overspread, 0, 1);
	config.center_when_single_stack =
		CLAMP_INT(config.center_when_single_stack, 0, 1);
	config.new_is_master = CLAMP_INT(config.new_is_master, 0, 1);
	config.dwindle_vsplit = CLAMP_INT(config.dwindle_vsplit, 0, 2);
	config.dwindle_hsplit = CLAMP_INT(config.dwindle_hsplit, 0, 2);
	config.dwindle_preserve_split =
		CLAMP_INT(config.dwindle_preserve_split, 0, 1);
	config.dwindle_smart_split = CLAMP_INT(config.dwindle_smart_split, 0, 1);
	config.dwindle_smart_resize = CLAMP_INT(config.dwindle_smart_resize, 0, 1);
	config.dwindle_drop_simple_split =
		CLAMP_INT(config.dwindle_drop_simple_split, 0, 1);
	config.dwindle_manual_split = CLAMP_INT(config.dwindle_manual_split, 0, 1);
	config.dwindle_split_ratio =
		CLAMP_FLOAT(config.dwindle_split_ratio, 0.05f, 0.95f);
	config.hotarea_size = CLAMP_INT(config.hotarea_size, 1, 1000);
	config.hotarea_corner = CLAMP_INT(config.hotarea_corner, 0, 3);
	config.enable_hotarea = CLAMP_INT(config.enable_hotarea, 0, 1);
	config.overviewgappi = CLAMP_INT(config.overviewgappi, 0, 1000);
	config.overviewgappo = CLAMP_INT(config.overviewgappo, 0, 1000);
	config.overcircle_center_ratio =
		CLAMP_FLOAT(config.overcircle_center_ratio, 0.1f, 0.9f);
	config.xwayland_persistence = CLAMP_INT(config.xwayland_persistence, 0, 1);
	config.xwayland_ignore_scale =
		CLAMP_INT(config.xwayland_ignore_scale, 0, 1);
	config.syncobj_enable = CLAMP_INT(config.syncobj_enable, 0, 1);
	config.drag_tile_refresh_interval =
		CLAMP_FLOAT(config.drag_tile_refresh_interval, 1.0f, 16.0f);
	config.drag_floating_refresh_interval =
		CLAMP_FLOAT(config.drag_floating_refresh_interval, 0.0f, 1000.0f);
	config.drag_tile_to_tile = CLAMP_INT(config.drag_tile_to_tile, 0, 1);
	config.drag_tile_small = CLAMP_INT(config.drag_tile_small, 0, 1);
	config.allow_tearing = CLAMP_INT(config.allow_tearing, 0, 2);
	config.hdr_depth = CLAMP_INT(config.hdr_depth, 0, 2);
	config.allow_shortcuts_inhibit =
		CLAMP_INT(config.allow_shortcuts_inhibit, 0, 1);
	config.allow_lock_transparent =
		CLAMP_INT(config.allow_lock_transparent, 0, 1);
	config.axis_bind_apply_timeout =
		CLAMP_INT(config.axis_bind_apply_timeout, 0, 1000);
	config.focus_on_activate = CLAMP_INT(config.focus_on_activate, 0, 1);
	config.smartkill = CLAMP_INT(config.smartkill, 0, 1);
	config.idleinhibit_ignore_visible =
		CLAMP_INT(config.idleinhibit_ignore_visible, 0, 1);
	config.sloppyfocus = CLAMP_INT(config.sloppyfocus, 0, 1);
	config.warpcursor = CLAMP_INT(config.warpcursor, 0, 1);
	config.drag_corner = CLAMP_INT(config.drag_corner, 0, 4);
	config.drag_warp_cursor = CLAMP_INT(config.drag_warp_cursor, 0, 1);
	config.focus_cross_monitor = CLAMP_INT(config.focus_cross_monitor, 0, 1);
	config.focusdir_only_zone_overlap =
		CLAMP_INT(config.focusdir_only_zone_overlap, 0, 1);
	config.exchange_cross_monitor =
		CLAMP_INT(config.exchange_cross_monitor, 0, 1);
	config.scratchpad_cross_monitor =
		CLAMP_INT(config.scratchpad_cross_monitor, 0, 1);
	config.focus_cross_tag = CLAMP_INT(config.focus_cross_tag, 0, 1);
	config.view_current_to_back = CLAMP_INT(config.view_current_to_back, 0, 1);
	config.enable_floating_snap = CLAMP_INT(config.enable_floating_snap, 0, 1);
	config.snap_distance = CLAMP_INT(config.snap_distance, 0, 99999);
	config.cursor_size = CLAMP_INT(config.cursor_size, 4, 512);
	config.no_border_when_single =
		CLAMP_INT(config.no_border_when_single, 0, 1);
	config.no_radius_when_single =
		CLAMP_INT(config.no_radius_when_single, 0, 1);
	config.cursor_hide_timeout =
		CLAMP_INT(config.cursor_hide_timeout, 0, 36000);
	config.cursor_hide_on_keypress =
		CLAMP_INT(config.cursor_hide_on_keypress, 0, 1);
	config.single_scratchpad = CLAMP_INT(config.single_scratchpad, 0, 1);
	config.repeat_rate = CLAMP_INT(config.repeat_rate, 1, 1000);
	config.repeat_delay = CLAMP_INT(config.repeat_delay, 1, 20000);
	config.numlockon = CLAMP_INT(config.numlockon, 0, 1);
	config.disable_trackpad = CLAMP_INT(config.disable_trackpad, 0, 1);
	config.touch_enable = CLAMP_INT(config.touch_enable, 0, 1);
	config.touch_enable_mouse_emulation =
		CLAMP_INT(config.touch_enable_mouse_emulation, 0, 1);
	config.tap_to_click = CLAMP_INT(config.tap_to_click, 0, 1);
	config.tap_and_drag = CLAMP_INT(config.tap_and_drag, 0, 1);
	config.drag_lock = CLAMP_INT(config.drag_lock, 0, 1);
	config.trackpad_natural_scrolling =
		CLAMP_INT(config.trackpad_natural_scrolling, 0, 1);
	config.swipe_min_threshold = CLAMP_INT(config.swipe_min_threshold, 1, 1000);
	config.mouse_natural_scrolling =
		CLAMP_INT(config.mouse_natural_scrolling, 0, 1);
	config.mouse_accel_profile = CLAMP_INT(config.mouse_accel_profile, 0, 2);
	config.mouse_accel_speed =
		CLAMP_FLOAT(config.mouse_accel_speed, -1.0f, 1.0f);
	config.trackpad_accel_profile =
		CLAMP_INT(config.trackpad_accel_profile, 0, 2);
	config.trackpad_accel_speed =
		CLAMP_FLOAT(config.trackpad_accel_speed, -1.0f, 1.0f);
	config.send_events_mode = CLAMP_INT(config.send_events_mode, 0, 2);
	config.button_map = CLAMP_INT(config.button_map, 0, 1);
	config.mouse_left_handed = CLAMP_INT(config.mouse_left_handed, 0, 1);
	config.mouse_middle_button_emulation =
		CLAMP_INT(config.mouse_middle_button_emulation, 0, 1);
	config.mouse_scroll_method = CLAMP_INT(config.mouse_scroll_method, 0, 4);
	config.mouse_scroll_button =
		CLAMP_INT(config.mouse_scroll_button, 272, 279);
	config.mouse_click_method = CLAMP_INT(config.mouse_click_method, 0, 2);
	config.mouse_send_events_mode =
		CLAMP_INT(config.mouse_send_events_mode, 0, 2);
	config.trackpad_left_handed = CLAMP_INT(config.trackpad_left_handed, 0, 1);
	config.trackpad_middle_button_emulation =
		CLAMP_INT(config.trackpad_middle_button_emulation, 0, 1);
	config.trackpad_disable_while_typing =
		CLAMP_INT(config.trackpad_disable_while_typing, 0, 1);
	config.trackpad_scroll_method =
		CLAMP_INT(config.trackpad_scroll_method, 0, 4);
	config.trackpad_scroll_button =
		CLAMP_INT(config.trackpad_scroll_button, 272, 279);
	config.trackpad_click_method =
		CLAMP_INT(config.trackpad_click_method, 0, 2);
	config.trackpad_send_events_mode =
		CLAMP_INT(config.trackpad_send_events_mode, 0, 2);
	config.axis_scroll_factor =
		CLAMP_FLOAT(config.axis_scroll_factor, 0.1f, 10.0f);
	config.trackpad_scroll_factor =
		CLAMP_FLOAT(config.trackpad_scroll_factor, 0.1f, 10.0f);
	config.gappih = CLAMP_INT(config.gappih, 0, 1000);
	config.gappiv = CLAMP_INT(config.gappiv, 0, 1000);
	config.gappoh = CLAMP_INT(config.gappoh, 0, 1000);
	config.gappov = CLAMP_INT(config.gappov, 0, 1000);
	config.scratchpad_width_ratio =
		CLAMP_FLOAT(config.scratchpad_width_ratio, 0.1f, 1.0f);
	config.scratchpad_height_ratio =
		CLAMP_FLOAT(config.scratchpad_height_ratio, 0.1f, 1.0f);
	config.special_dim = CLAMP_FLOAT(config.special_dim, 0.0f, 1.0f);
	config.special_gappih = CLAMP_INT(config.special_gappih, 0, 1000);
	config.special_gappiv = CLAMP_INT(config.special_gappiv, 0, 1000);
	config.special_gappoh = CLAMP_INT(config.special_gappoh, 0, 1000);
	config.special_gappov = CLAMP_INT(config.special_gappov, 0, 1000);
	config.borderpx = CLAMP_INT(config.borderpx, 0, 200);
	config.group_bar_height = CLAMP_INT(config.group_bar_height, 0, 500);
	config.smartgaps = CLAMP_INT(config.smartgaps, 0, 1);
	config.blur = CLAMP_INT(config.blur, 0, 1);
	config.blur_layer = CLAMP_INT(config.blur_layer, 0, 1);
	config.blur_optimized = CLAMP_INT(config.blur_optimized, 0, 1);
	config.border_radius = CLAMP_INT(config.border_radius, 0, 100);
	config.blur_params.num_passes =
		CLAMP_INT(config.blur_params.num_passes, 0, 10);
	config.blur_params.radius = CLAMP_INT(config.blur_params.radius, 0, 100);
	config.blur_params.noise = CLAMP_FLOAT(config.blur_params.noise, 0, 1);
	config.blur_params.brightness =
		CLAMP_FLOAT(config.blur_params.brightness, 0, 1);
	config.blur_params.contrast =
		CLAMP_FLOAT(config.blur_params.contrast, 0, 1);
	config.blur_params.saturation =
		CLAMP_FLOAT(config.blur_params.saturation, 0, 1);
	config.shadows = CLAMP_INT(config.shadows, 0, 1);
	config.shadow_only_floating = CLAMP_INT(config.shadow_only_floating, 0, 1);
	config.layer_shadows = CLAMP_INT(config.layer_shadows, 0, 1);
	config.shadows_size = CLAMP_INT(config.shadows_size, 0, 100);
	config.shadows_blur = CLAMP_INT(config.shadows_blur, 0, 100);
	config.shadows_position_x =
		CLAMP_INT(config.shadows_position_x, -1000, 1000);
	config.shadows_position_y =
		CLAMP_INT(config.shadows_position_y, -1000, 1000);
	config.focused_opacity = CLAMP_FLOAT(config.focused_opacity, 0.0f, 1.0f);
	config.unfocused_opacity =
		CLAMP_FLOAT(config.unfocused_opacity, 0.0f, 1.0f);

	config.groupbardata.border_width =
		CLAMP_INT(config.groupbardata.border_width, 0, 100);
	config.groupbardata.corner_radius =
		CLAMP_INT(config.groupbardata.corner_radius, 0, 100);
	config.groupbardata.padding_x =
		CLAMP_INT(config.groupbardata.padding_x, 0, 100);
	config.groupbardata.padding_y =
		CLAMP_INT(config.groupbardata.padding_y, 0, 100);

	config.jumplabeldata.border_width =
		CLAMP_INT(config.jumplabeldata.border_width, 0, 100);
	config.jumplabeldata.corner_radius =
		CLAMP_INT(config.jumplabeldata.corner_radius, 0, 100);
	config.jumplabeldata.padding_x =
		CLAMP_INT(config.jumplabeldata.padding_x, 0, 100);
	config.jumplabeldata.padding_y =
		CLAMP_INT(config.jumplabeldata.padding_y, 0, 100);

	update_global_var();
}

void set_value_default() {
	config.animations = 1;
	config.layer_animations = 0;
	config.animation_fade_in = 1;
	config.animation_fade_out = 1;
	config.tag_animation_direction = HORIZONTAL;
	config.zoom_initial_ratio = 0.4f;
	config.zoom_end_ratio = 0.8f;
	config.fadein_begin_opacity = 0.5f;
	config.fadeout_begin_opacity = 0.5f;
	config.animation_duration_move = 500;
	config.animation_duration_open = 400;
	config.animation_duration_tag = 300;
	config.animation_duration_close = 300;
	config.animation_duration_focus = 0;

	config.axis_bind_apply_timeout = 100;
	config.focus_on_activate = 1;
	config.smartkill = 0;
	config.new_is_master = 1;
	config.default_mfact = 0.55f;
	config.default_nmaster = 1;
	config.tag_num = 9;
	config.tag_gather = 0;
	config.center_master_overspread = 0;
	config.center_when_single_stack = 1;

	config.dwindle_vsplit = 1;
	config.dwindle_hsplit = 1;
	config.dwindle_preserve_split = 0;
	config.dwindle_smart_split = 0;
	config.dwindle_smart_resize = 0;
	config.dwindle_drop_simple_split = 1;
	config.dwindle_manual_split = 0;
	config.dwindle_split_ratio = 0.5f;

	config.log_level = WLR_ERROR;
	config.numlockon = 0;
	config.capslock = 0;
	config.hotarea_size = 10;
	config.hotarea_corner = BOTTOM_LEFT;
	config.enable_hotarea = 0;
	config.smartgaps = 0;
	config.sloppyfocus = 1;
	config.gappih = 5;
	config.gappiv = 5;
	config.gappoh = 10;
	config.gappov = 10;
	config.scratchpad_width_ratio = 0.8f;
	config.scratchpad_height_ratio = 0.9f;
	config.special_dim = 0.5f;
	config.special_gappih = 10;
	config.special_gappiv = 10;
	config.special_gappoh = 20;
	config.special_gappov = 20;

	config.scroller_structs = 20;
	config.scroller_default_proportion = 0.9f;
	config.scroller_default_proportion_single = 1.0f;
	config.scroller_ignore_proportion_single = 1;
	config.scroller_focus_center = 0;
	config.scroller_prefer_center = 0;
	config.scroller_prefer_overspread = 1;
	config.edge_scroller_pointer_focus = 1;
	config.edge_scroller_focus_allow_speed = 0.0f;
	config.focus_cross_monitor = 0;
	config.focusdir_only_zone_overlap = 0;
	config.exchange_cross_monitor = 0;
	config.scratchpad_cross_monitor = 0;
	config.focus_cross_tag = 0;
	config.axis_scroll_factor = 1.0;
	config.trackpad_scroll_factor = 1.0;
	config.view_current_to_back = 0;
	config.single_scratchpad = 1;
	config.xwayland_persistence = 1;
	config.xwayland_ignore_scale = 0;
	config.syncobj_enable = 1;
	config.tag_carousel = 0;
	config.drag_tile_refresh_interval = 8.0f;
	config.drag_floating_refresh_interval = 8.0f;
	config.allow_tearing = TEARING_DISABLED;
	config.hdr_depth = MANGO_RENDER_BIT_DEPTH_10;
	config.allow_shortcuts_inhibit = SHORTCUTS_INHIBIT_ENABLE;
	config.allow_lock_transparent = 0;
	config.no_border_when_single = 0;
	config.no_radius_when_single = 0;
	config.snap_distance = 30;
	config.drag_tile_to_tile = 1;
	config.drag_tile_small = 1;
	config.enable_floating_snap = 0;
	config.swipe_min_threshold = 1;

	config.idleinhibit_ignore_visible = 0;

	config.borderpx = 4;
	config.group_bar_height = 50;
	config.overviewgappi = 5;
	config.overviewgappo = 30;
	config.overcircle_center_ratio = 0.5f;
	config.cursor_hide_timeout = 0;
	config.cursor_hide_on_keypress = 0;

	config.warpcursor = 1;
	config.drag_corner = 3;
	config.drag_warp_cursor = 1;

	config.repeat_rate = 25;
	config.repeat_delay = 600;

	config.disable_trackpad = 0;
	config.touch_enable = 1;
	config.touch_enable_mouse_emulation = 0;
	config.tap_to_click = 1;
	config.tap_and_drag = 1;
	config.drag_lock = 1;
	config.mouse_natural_scrolling = 0;
	config.cursor_size = 24;
	config.trackpad_natural_scrolling = 0;
	config.mouse_accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	config.mouse_accel_speed = 0.0;
	config.trackpad_accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	config.trackpad_accel_speed = 0.0;
	config.send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	config.button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
	config.mouse_left_handed = 0;
	config.mouse_middle_button_emulation = 0;
	config.mouse_scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
	config.mouse_scroll_button = 274;
	config.mouse_click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
	config.mouse_send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	config.trackpad_left_handed = 0;
	config.trackpad_middle_button_emulation = 0;
	config.trackpad_disable_while_typing = 1;
	config.trackpad_scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
	config.trackpad_scroll_button = 274;
	config.trackpad_click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
	config.trackpad_send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

	config.blur = 0;
	config.blur_layer = 0;
	config.blur_optimized = 1;
	config.border_radius = 0;
	config.blur_params.num_passes = 1;
	config.blur_params.radius = 5;
	config.blur_params.noise = 0.02f;
	config.blur_params.brightness = 0.9f;
	config.blur_params.contrast = 0.9f;
	config.blur_params.saturation = 1.2f;
	config.shadows = 0;
	config.shadow_only_floating = 1;
	config.layer_shadows = 0;
	config.shadows_size = 10;
	config.shadows_blur = 15.0f;
	config.shadows_position_x = 0;
	config.shadows_position_y = 0;
	config.focused_opacity = 1.0f;
	config.unfocused_opacity = 1.0f;

	config.shadowscolor[0] = 0.0f;
	config.shadowscolor[1] = 0.0f;
	config.shadowscolor[2] = 0.0f;
	config.shadowscolor[3] = 1.0f;

	config.animation_curve_move[0] = 0.46;
	config.animation_curve_move[1] = 1.0;
	config.animation_curve_move[2] = 0.29;
	config.animation_curve_move[3] = 0.99;
	config.animation_curve_open[0] = 0.46;
	config.animation_curve_open[1] = 1.0;
	config.animation_curve_open[2] = 0.29;
	config.animation_curve_open[3] = 0.99;
	config.animation_curve_tag[0] = 0.46;
	config.animation_curve_tag[1] = 1.0;
	config.animation_curve_tag[2] = 0.29;
	config.animation_curve_tag[3] = 0.99;
	config.animation_curve_close[0] = 0.46;
	config.animation_curve_close[1] = 1.0;
	config.animation_curve_close[2] = 0.29;
	config.animation_curve_close[3] = 0.99;
	config.animation_curve_focus[0] = 0.46;
	config.animation_curve_focus[1] = 1.0;
	config.animation_curve_focus[2] = 0.29;
	config.animation_curve_focus[3] = 0.99;
	config.animation_curve_opafadein[0] = 0.46;
	config.animation_curve_opafadein[1] = 1.0;
	config.animation_curve_opafadein[2] = 0.29;
	config.animation_curve_opafadein[3] = 0.99;
	config.animation_curve_opafadeout[0] = 0.5;
	config.animation_curve_opafadeout[1] = 0.5;
	config.animation_curve_opafadeout[2] = 0.5;
	config.animation_curve_opafadeout[3] = 0.5;

	config.groupbardata.fg_color[0] = 0xc4 / 255.0f;
	config.groupbardata.fg_color[1] = 0x93 / 255.0f;
	config.groupbardata.fg_color[2] = 0x9d / 255.0f;
	config.groupbardata.fg_color[3] = 1.0f;
	config.groupbardata.bg_color[0] = 0x32 / 255.0f;
	config.groupbardata.bg_color[1] = 0x32 / 255.0f;
	config.groupbardata.bg_color[2] = 0x32 / 255.0f;
	config.groupbardata.bg_color[3] = 1.0f;
	config.groupbardata.focus_fg_color[0] = 0xed / 255.0f;
	config.groupbardata.focus_fg_color[1] = 0xa6 / 255.0f;
	config.groupbardata.focus_fg_color[2] = 0xb4 / 255.0f;
	config.groupbardata.focus_fg_color[3] = 1.0f;
	config.groupbardata.focus_bg_color[0] = 0x4e / 255.0f;
	config.groupbardata.focus_bg_color[1] = 0x45 / 255.0f;
	config.groupbardata.focus_bg_color[2] = 0x3c / 255.0f;
	config.groupbardata.focus_bg_color[3] = 1.0f;
	config.groupbardata.border_color[0] = 0x8b / 255.0f;
	config.groupbardata.border_color[1] = 0xaa / 255.0f;
	config.groupbardata.border_color[2] = 0x9b / 255.0f;
	config.groupbardata.border_color[3] = 1.0f;
	config.groupbardata.border_width = 4;
	config.groupbardata.corner_radius = 5;
	config.groupbardata.padding_x = 0;
	config.groupbardata.padding_y = 0;

	config.jumplabeldata.fg_color[0] = 0xc4 / 255.0f;
	config.jumplabeldata.fg_color[1] = 0x93 / 255.0f;
	config.jumplabeldata.fg_color[2] = 0x9d / 255.0f;
	config.jumplabeldata.fg_color[3] = 1.0f;
	config.jumplabeldata.bg_color[0] = 0x32 / 255.0f;
	config.jumplabeldata.bg_color[1] = 0x32 / 255.0f;
	config.jumplabeldata.bg_color[2] = 0x32 / 255.0f;
	config.jumplabeldata.bg_color[3] = 1.0f;
	config.jumplabeldata.focus_fg_color[0] = 0xed / 255.0f;
	config.jumplabeldata.focus_fg_color[1] = 0xa6 / 255.0f;
	config.jumplabeldata.focus_fg_color[2] = 0xb4 / 255.0f;
	config.jumplabeldata.focus_fg_color[3] = 1.0f;
	config.jumplabeldata.focus_bg_color[0] = 0x4e / 255.0f;
	config.jumplabeldata.focus_bg_color[1] = 0x45 / 255.0f;
	config.jumplabeldata.focus_bg_color[2] = 0x3c / 255.0f;
	config.jumplabeldata.focus_bg_color[3] = 1.0f;
	config.jumplabeldata.border_color[0] = 0x8b / 255.0f;
	config.jumplabeldata.border_color[1] = 0xaa / 255.0f;
	config.jumplabeldata.border_color[2] = 0x9b / 255.0f;
	config.jumplabeldata.border_color[3] = 1.0f;
	config.jumplabeldata.border_width = 4;
	config.jumplabeldata.corner_radius = 5;
	config.jumplabeldata.padding_x = 10;
	config.jumplabeldata.padding_y = 10;

	config.rootcolor[0] = 0x32 / 255.0f;
	config.rootcolor[1] = 0x32 / 255.0f;
	config.rootcolor[2] = 0x32 / 255.0f;
	config.rootcolor[3] = 1.0f;
	config.bordercolor[0] = 0x44 / 255.0f;
	config.bordercolor[1] = 0x44 / 255.0f;
	config.bordercolor[2] = 0x44 / 255.0f;
	config.bordercolor[3] = 1.0f;
	config.dropcolor[0] = 0xd5 / 255.0f;
	config.dropcolor[1] = 0x89 / 255.0f;
	config.dropcolor[2] = 0x9d / 255.0f;
	config.dropcolor[3] = 0.5f;
	config.splitcolor[0] = 0xeb / 255.0f;
	config.splitcolor[1] = 0x44 / 255.0f;
	config.splitcolor[2] = 0x1e / 255.0f;
	config.splitcolor[3] = 1.0f;
	config.focuscolor[0] = 0xc6 / 255.0f;
	config.focuscolor[1] = 0x6b / 255.0f;
	config.focuscolor[2] = 0x25 / 255.0f;
	config.focuscolor[3] = 1.0f;
	config.maximizescreencolor[0] = 0x89 / 255.0f;
	config.maximizescreencolor[1] = 0xaa / 255.0f;
	config.maximizescreencolor[2] = 0x61 / 255.0f;
	config.maximizescreencolor[3] = 1.0f;
	config.urgentcolor[0] = 0xad / 255.0f;
	config.urgentcolor[1] = 0x40 / 255.0f;
	config.urgentcolor[2] = 0x1f / 255.0f;
	config.urgentcolor[3] = 1.0f;
	config.scratchpadcolor[0] = 0x51 / 255.0f;
	config.scratchpadcolor[1] = 0x6c / 255.0f;
	config.scratchpadcolor[2] = 0x93 / 255.0f;
	config.scratchpadcolor[3] = 1.0f;
	config.globalcolor[0] = 0xb1 / 255.0f;
	config.globalcolor[1] = 0x53 / 255.0f;
	config.globalcolor[2] = 0xa7 / 255.0f;
	config.globalcolor[3] = 1.0f;
	config.overlaycolor[0] = 0x14 / 255.0f;
	config.overlaycolor[1] = 0xa5 / 255.0f;
	config.overlaycolor[2] = 0x7c / 255.0f;
	config.overlaycolor[3] = 1.0f;
}

void set_default_key_bindings(Config *config) {
	KeyBinding *b = NULL;

	// 计算默认按键绑定的数量
	size_t default_key_bindings_count =
		sizeof(default_key_bindings) / sizeof(KeyBinding);

	// 重新分配内存以容纳新的默认按键绑定
	config->key_bindings =
		realloc(config->key_bindings,
				(config->key_bindings_count + default_key_bindings_count) *
					sizeof(KeyBinding));
	if (!config->key_bindings) {
		return;
	}

	// 将默认按键绑定复制到配置的按键绑定数组中
	for (size_t i = 0; i < default_key_bindings_count; i++) {
		config->key_bindings[config->key_bindings_count + i] =
			default_key_bindings[i];
		b = &config->key_bindings[config->key_bindings_count + i];
		b->iscommonmode = true;
		b->islockapply = true;
		b->line_number = 0;
		strcpy(b->mode, "common");
	}

	// 更新按键绑定的总数
	config->key_bindings_count += default_key_bindings_count;
}

bool parse_config(void) {

	char filename[1024];

	free_config();

	memset(&config, 0, sizeof(config));

	// 重新将xkb_rules指针指向静态数组
	config.xkb_rules.layout = config.xkb_rules_layout;
	config.xkb_rules.variant = config.xkb_rules_variant;
	config.xkb_rules.options = config.xkb_rules_options;
	config.xkb_rules.rules = config.xkb_rules_rules;
	config.xkb_rules.model = config.xkb_rules_model;

	// 初始化动态数组的指针为NULL，避免野指针
	config.window_rules = NULL;
	config.window_rules_count = 0;
	config.monitor_rules = NULL;
	config.monitor_rules_count = 0;
	config.device_rules = NULL;
	config.device_rules_count = 0;
	config.key_bindings = NULL;
	config.key_bindings_count = 0;
	config.mouse_bindings = NULL;
	config.mouse_bindings_count = 0;
	config.axis_bindings = NULL;
	config.axis_bindings_count = 0;
	config.switch_bindings = NULL;
	config.switch_bindings_count = 0;
	config.gesture_bindings = NULL;
	config.gesture_bindings_count = 0;
	config.env = NULL;
	config.env_count = 0;
	config.exec = NULL;
	config.exec_count = 0;
	config.exec_once = NULL;
	config.exec_once_count = 0;
	config.scroller_proportion_preset = NULL;
	config.scroller_proportion_preset_count = 0;
	config.circle_layout = NULL;
	config.circle_layout_count = 0;
	config.tag_rules = NULL;
	config.tag_rules_count = 0;
	config.cursor_theme = NULL;
	config.jumplabeldata.font_desc = NULL;
	config.groupbardata.font_desc = NULL;
	config.tablet_map_to_mon = NULL;
	config.touch_map_to_mon = NULL;
	config.jump_labels = NULL;
	strcpy(config.keymode, "default");

	create_config_keymap();

	if (cli_config_path[0]) {
		snprintf(filename, sizeof(filename), "%s", cli_config_path);
	} else {
		// 获取当前用户家目录
		const char *homedir = getenv("HOME");
		if (!homedir) {
			// 如果获取失败，则无法继续
			return false;
		}
		// 构建日志文件路径
		snprintf(filename, sizeof(filename), "%s/.config/mango/config.conf",
				 homedir);

		// 检查文件是否存在
		if (access(filename, F_OK) != 0) {
			// 如果文件不存在，则使用 /etc/mango/config.conf
			snprintf(filename, sizeof(filename), "%s/mango/config.conf",
					 SYSCONFDIR);
		}
	}

	bool parse_correct = true;
	bool keybindings_conflict = false;
	set_value_default();
	parse_correct = parse_config_file(&config, filename, true);
	set_default_key_bindings(&config);
	override_config();

	keybindings_conflict = check_key_binding_conflicts(&config);
	keybindings_conflict |= check_mouse_binding_conflicts(&config);
	keybindings_conflict |= check_axis_binding_conflicts(&config);
	keybindings_conflict |= check_switch_binding_conflicts(&config);
	keybindings_conflict |= check_gesture_binding_conflicts(&config);

	// 释放文件路径列表
	if (file_paths) {
		for (int i = 0; i < file_paths_count; i++) {
			free(file_paths[i]);
		}
		free(file_paths);
		file_paths = NULL;
		file_paths_count = 0;
	}

	return parse_correct || keybindings_conflict;
}

void reset_blur_params(void) {
	if (config.blur) {
		Monitor *m = NULL;
		wl_list_for_each(m, &mons, link) {
			if (m->blur != NULL) {
				wlr_scene_node_destroy(&m->blur->node);
			}
			m->blur = wlr_scene_optimized_blur_create(&scene->tree, 0, 0);
			wlr_scene_node_reparent(&m->blur->node, layers[LyrBlur]);
			wlr_scene_optimized_blur_set_size(m->blur, m->m.width, m->m.height);
			wlr_scene_set_blur_data(
				scene, config.blur_params.num_passes, config.blur_params.radius,
				config.blur_params.noise, config.blur_params.brightness,
				config.blur_params.contrast, config.blur_params.saturation);
		}
	} else {
		Monitor *m = NULL;
		wl_list_for_each(m, &mons, link) {

			if (m->blur) {
				wlr_scene_node_destroy(&m->blur->node);
				m->blur = NULL;
			}
		}
	}
}

void reapply_monitor_rules(void) {
	ConfigMonitorRule *mr;
	Monitor *m = NULL;
	int32_t ji;
	int32_t mx, my;

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled)
			continue;

		for (ji = config.monitor_rules_count - 1; ji >= 0; ji--) {
			mr = &config.monitor_rules[ji];

			if (monitor_matches_rule(m, mr)) {
				mx = mr->x == INT32_MAX ? m->m.x : mr->x;
				my = mr->y == INT32_MAX ? m->m.y : mr->y;

				apply_rule_to_state(m, mr, &m->pending);

				wlr_output_layout_add(output_layout, m->wlr_output, mx, my);
				break;
			}
		}

		if (m->prefer_disable) {
			wlr_output_state_set_enabled(&m->pending, false);
		} else {
			wlr_output_state_set_enabled(&m->pending, true);
		}

		if (m->hdr_enable) {
			output_state_setup_hdr(m, false, &m->pending);
		} else {
			output_enable_hdr(m, &m->pending, false, false);
		}

		if (!(mango_scene_output_commit(m->scene_output, &m->pending))) {
			if (m->hdr_enable) {
				output_state_setup_hdr(m, true, &m->pending);
			}
		}
		/* scale/mode 变化后强制调度一帧，确保 wl_output 事件发送 */
		wlr_output_schedule_frame(m->wlr_output);
		wlr_output_effective_resolution(m->wlr_output, &m->m.width,
										&m->m.height);
	}

	updatemons(NULL, NULL);
}

void set_xcursor_env() {
	if (config.cursor_size > 0) {
		char size_str[16];
		snprintf(size_str, sizeof(size_str), "%d", config.cursor_size);
		setenv("XCURSOR_SIZE", size_str, 1);
	} else {
		setenv("XCURSOR_SIZE", "24", 1);
	}

	if (config.cursor_theme) {
		setenv("XCURSOR_THEME", config.cursor_theme, 1);
	}
}

void reapply_cursor_style(void) {
	if (hide_cursor_source) {
		wl_event_source_timer_update(hide_cursor_source, 0);
		wl_event_source_remove(hide_cursor_source);
		hide_cursor_source = NULL;
	}

	wlr_cursor_unset_image(cursor);

	wlr_cursor_set_surface(cursor, NULL, 0, 0);

	if (cursor_mgr) {
		wlr_xcursor_manager_destroy(cursor_mgr);
		cursor_mgr = NULL;
	}

	set_xcursor_env();

	cursor_mgr =
		wlr_xcursor_manager_create(config.cursor_theme, config.cursor_size);

	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		wlr_xcursor_manager_load(cursor_mgr, m->wlr_output->scale);
	}

	wlr_cursor_set_xcursor(cursor, cursor_mgr, "left_ptr");

	hide_cursor_source = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
												 hidecursor, cursor);
	if (cursor_hidden) {
		wlr_cursor_unset_image(cursor);
	} else {
		wl_event_source_timer_update(hide_cursor_source,
									 config.cursor_hide_timeout * 1000);
	}
}

void reapply_rootbg(void) {
	wlr_scene_rect_set_color(root_bg, config.rootcolor);
}

void reapply_property(void) {
	Client *c = NULL;

	// reset border width when config change
	wl_list_for_each(c, &clients, link) {
		if (c && !c->iskilling) {
			if (!c->isnoborder && !c->isfullscreen) {
				c->bw = config.borderpx;
			}
			client_set_group_config(c);
		}
	}
}

void reapply_keyboard(void) {
	InputDevice *id;
	KeyboardGroup *g;
	ConfigDeviceRule *rule;
	bool want_standalone;

	wlr_keyboard_set_repeat_info(&kb_group->wlr_group->keyboard,
								 config.repeat_rate, config.repeat_delay);
	wl_list_for_each(id, &inputdevices, link) {
		if (id->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
			continue;
		}

		rule = find_device_rule(id->wlr_device);
		want_standalone = rule && device_rule_has_keyboard_settings(rule);

		if (want_standalone && !id->standalone) {
			/* 命中规则：拆出为独立键盘 */
			struct wlr_keyboard *kb = (struct wlr_keyboard *)id->device_data;
			if (!kb)
				continue;
			wlr_keyboard_group_remove_keyboard(kb_group->wlr_group, kb);
			id->standalone = true;
			create_standalone_keyboard(id, kb, rule);
		} else if (!want_standalone && id->standalone) {
			/* 规则失效：并回默认键盘组 */
			g = (KeyboardGroup *)id->device_data;
			struct wlr_keyboard *kb = g ? g->keyboard : NULL;
			if (g)
				destroy_standalone_keyboard(&g->destroy, NULL);
			id->standalone = false;
			id->device_data = kb;
			if (kb) {
				wlr_keyboard_set_keymap(kb, kb_group->keyboard->keymap);
				wlr_keyboard_notify_modifiers(kb, 0, 0, locked_mods, 0);
				wlr_keyboard_group_add_keyboard(kb_group->wlr_group, kb);
				wlr_keyboard_set_repeat_info(kb, config.repeat_rate,
											 config.repeat_delay);
			}
		} else if (want_standalone) {
			g = (KeyboardGroup *)id->device_data;
			if (g)
				standalone_keyboard_apply_config(g, rule);
		} else {
			wlr_keyboard_set_repeat_info((struct wlr_keyboard *)id->device_data,
										 config.repeat_rate,
										 config.repeat_delay);
		}
	}
}

void reapply_pointer(void) {
	InputDevice *id;
	struct libinput_device *device;
	wl_list_for_each(id, &inputdevices, link) {

		if (id->wlr_device->type != WLR_INPUT_DEVICE_POINTER) {
			continue;
		}

		device = id->libinput_device;
		if (wlr_input_device_is_libinput(id->wlr_device) && device) {
			configure_pointer(id->wlr_device, device);
		}
	}
}

void reapply_master(void) {

	int32_t i;
	Monitor *m = NULL;
	for (i = 0; i < PERTAG_SLOTS; i++) {
		wl_list_for_each(m, &mons, link) {
			if (!m->wlr_output->enabled) {
				continue;
			}
			m->pertag->nmasters[i] = config.default_nmaster;
			m->pertag->mfacts[i] = config.default_mfact;
			m->gappih = config.gappih;
			m->gappiv = config.gappiv;
			m->gappoh = config.gappoh;
			m->gappov = config.gappov;
			m->special_gappih = config.special_gappih;
			m->special_gappiv = config.special_gappiv;
			m->special_gappoh = config.special_gappoh;
			m->special_gappov = config.special_gappov;
		}
	}
}

// Reset a pertag slot to defaults.
static void tag_slot_set_defaults(Monitor *m, uint32_t tag) {
	m->pertag->nmasters[tag] = config.default_nmaster;
	m->pertag->mfacts[tag] = config.default_mfact;
	m->pertag->ltidxs[tag] = &layouts[0];
	m->pertag->scroller_default_proportion[tag] =
		config.scroller_default_proportion;
	m->pertag->scroller_default_proportion_single[tag] =
		config.scroller_default_proportion_single;
	m->pertag->scroller_ignore_proportion_single[tag] =
		config.scroller_ignore_proportion_single;
}

// Does this tag rule match the monitor?
static bool tag_rule_matches_monitor(const ConfigTagRule *tr, Monitor *m) {
	if (tr->monitor_name != NULL &&
		!regex_match(tr->monitor_name, m->wlr_output->name))
		return false;
	if (tr->monitor_make != NULL &&
		(m->wlr_output->make == NULL ||
		 strcmp(tr->monitor_make, m->wlr_output->make) != 0))
		return false;
	if (tr->monitor_model != NULL &&
		(m->wlr_output->model == NULL ||
		 strcmp(tr->monitor_model, m->wlr_output->model) != 0))
		return false;
	if (tr->monitor_serial != NULL &&
		(m->wlr_output->serial == NULL ||
		 strcmp(tr->monitor_serial, m->wlr_output->serial) != 0))
		return false;
	return true;
}

// Apply one tag rule to a slot (caller checks coverage).
static void tag_rule_apply_to_slot(Monitor *m, const ConfigTagRule *tr,
								   uint32_t tag) {
	int32_t jk;

	for (jk = 0; jk < LENGTH(layouts); jk++) {
		if (tr->layout_name && strcmp(layouts[jk].name, tr->layout_name) == 0)
			m->pertag->ltidxs[tag] = &layouts[jk];
	}

	if (tr->no_hide >= 0)
		m->pertag->no_hide[tag] = tr->no_hide;
	if (tr->nmaster >= 1)
		m->pertag->nmasters[tag] = tr->nmaster;
	if (tr->mfact > 0.0f)
		m->pertag->mfacts[tag] = tr->mfact;
	if (tr->no_render_border >= 0)
		m->pertag->no_render_border[tag] = tr->no_render_border;
	if (tr->open_as_floating >= 0)
		m->pertag->open_as_floating[tag] = tr->open_as_floating;
	if (tr->scroller_default_proportion > 0.0f)
		m->pertag->scroller_default_proportion[tag] =
			tr->scroller_default_proportion;
	if (tr->scroller_default_proportion_single > 0.0f)
		m->pertag->scroller_default_proportion_single[tag] =
			tr->scroller_default_proportion_single;
	if (tr->scroller_ignore_proportion_single >= 0)
		m->pertag->scroller_ignore_proportion_single[tag] =
			tr->scroller_ignore_proportion_single;
}

void parse_tagrule(Monitor *m) {
	int32_t i;
	Client *c = NULL;

	// Set defaults for every tag.
	for (i = 0; i <= config.tag_num; i++)
		tag_slot_set_defaults(m, i);
	// dedicated state slot for the all-tags view
	tag_slot_set_defaults(m, PERTAG_ALL_TAGS_IDX);

	for (i = 0; i < config.tag_rules_count; i++) {
		const ConfigTagRule *tr = &config.tag_rules[i];

		if (tag_rule_matches_monitor(tr, m) &&
			(tr->id_wildcard || tr->id <= config.tag_num)) {
			int32_t tag_id_start = tr->id_wildcard ? 0 : tr->id;
			int32_t tag_id_end = tr->id_wildcard ? config.tag_num : tr->id;
			int32_t ti;

			for (ti = tag_id_start; ti <= tag_id_end; ti++)
				tag_rule_apply_to_slot(m, tr, ti);
		}
	}

	for (i = 1; i <= config.tag_num; i++) {
		wl_list_for_each(c, &clients, link) {
			if ((c->tags & (1 << (i - 1)) & TAGMASK) && ISTILED(c)) {
				if (m->pertag->mfacts[i] > 0.0f)
					c->master_mfact_per = m->pertag->mfacts[i];
			}
		}
	}
}

void reapply_tagrule(void) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		parse_tagrule(m);
	}
}

void reset_option(void) {
	init_baked_points();
	handlecursoractivity();
	reset_keyboard_layout();
	reset_blur_params();
	set_env_without_display();
	set_env_display();
	run_exec();

	reapply_cursor_style();
	reapply_property();
	reapply_rootbg();
	reapply_keyboard();
	reapply_pointer();
	reapply_master();

	reapply_tagrule();
	reapply_monitor_rules();

	arrange(selmon, false, false);
}

void reset_tag(int old_tag_num) {

	if (config.tag_num != old_tag_num) {
		uint32_t last_tag_mask = (uint32_t)1 << (config.tag_num - 1);
		Client *c = NULL;
		Monitor *m = NULL;

		wl_list_for_each(c, &clients, link) {
			if (c->tags & ~tagmask) {
				c->tags = last_tag_mask;
				c->oldtags = last_tag_mask;
			}
		}

		wl_list_for_each(m, &mons, link) {
			if (!m->wlr_output->enabled)
				continue;
			m->tagset[0] &= tagmask;
			m->tagset[1] &= tagmask;
			if (m->tagset[m->seltags] == 0)
				m->tagset[m->seltags] = last_tag_mask;
			if (m->pertag->curtag > (uint32_t)config.tag_num)
				m->pertag->curtag = config.tag_num;
			if (m->pertag->prevtag > (uint32_t)config.tag_num)
				m->pertag->prevtag = config.tag_num;
			sync_workspaces_to_tag_num(m);
		}
	}
}

void reload_config(const Arg *arg) {
	int old_tag_num = config.tag_num;
	parse_config();
	reset_tag(old_tag_num);
	reset_option();
	printstatus(IPC_WATCH_ARRANGGE);
	return;
}
