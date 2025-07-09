#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

// 整数版本 - 截断小数部分
#define CLAMP_INT(x, min, max)                                                 \
	((int)(x) < (int)(min) ? (int)(min)                                        \
						   : ((int)(x) > (int)(max) ? (int)(max) : (int)(x)))

// 浮点数版本 - 保留小数部分
#define CLAMP_FLOAT(x, min, max)                                               \
	((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

enum { NUM_TYPE_MINUS, NUM_TYPE_PLUS, NUM_TYPE_DEFAULT };

enum { KEY_TYPE_SYM, KEY_TYPE_CODE };

typedef struct {
	xkb_keysym_t keysym;
	uint32_t keycode;
	int type;
} KeySymCode;

typedef struct {
	uint32_t mod;
	KeySymCode keysymcode;
	void (*func)(const Arg *);
	Arg arg;
} KeyBinding;

typedef struct {
	const char *id;
	const char *title;
	unsigned int tags;
	int isfloating;
	int isfullscreen;
	float scroller_proportion;
	const char *animation_type_open;
	const char *animation_type_close;
	int isnoborder;
	int isopensilent;
	int isnamedscratchpad;
	int isunglobal;
	int isglobal;
	int isoverlay;
	int monitor;
	int offsetx;
	int offsety;
	int width;
	int height;
	int nofadein;
	int nofadeout;
	int no_force_center;
	int isterm;
	int noswallow;
	int scratchpad_width;
	int scratchpad_height;
	float focused_opacity;
	float unfocused_opacity;
	uint32_t passmod;
	xkb_keysym_t keysym;
	KeyBinding globalkeybinding;
} ConfigWinRule;

typedef struct {
	const char *name;	// 显示器名称
	float mfact;		// 主区域比例
	int nmaster;		// 主区域窗口数量
	const char *layout; // 布局名称（字符串）
	int rr;				// 旋转和翻转（假设为整数）
	float scale;		// 显示器缩放比例
	int x, y;			// 显示器位置
	int isterm;
	int noswallow;
} ConfigMonitorRule;

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
	unsigned int mod;
	unsigned int button;
	void (*func)(const Arg *);
	Arg arg;
} MouseBinding;

typedef struct {
	unsigned int mod;
	unsigned int dir;
	void (*func)(const Arg *);
	Arg arg;
} AxisBinding;

typedef struct {
	unsigned int mod;
	unsigned int motion;
	unsigned int fingers_count;
	void (*func)(const Arg *);
	Arg arg;
} GestureBinding;

typedef struct {
	int id;			   // 标签ID (1-9)
	char *layout_name; // 布局名称
	char *monitor_name;
	int no_render_border;
} ConfigTagRule;

typedef struct {
	char *layer_name; // 布局名称
	int noanim;
} ConfigLayerRule;

typedef struct {
	int animations;
	int layer_animations;
	char animation_type_open[10];
	char animation_type_close[10];
	int animation_fade_in;
	int animation_fade_out;
	int tag_animation_direction;
	float zoom_initial_ratio;
	float fadein_begin_opacity;
	float fadeout_begin_opacity;
	uint32_t animation_duration_move;
	uint32_t animation_duration_open;
	uint32_t animation_duration_tag;
	uint32_t animation_duration_close;
	double animation_curve_move[4];
	double animation_curve_open[4];
	double animation_curve_tag[4];
	double animation_curve_close[4];

	int scroller_structs;
	float scroller_default_proportion;
	float scroller_default_proportion_single;
	int scroller_focus_center;
	int scroller_prefer_center;
	int focus_cross_monitor;
	int focus_cross_tag;
	int no_border_when_single;
	int snap_distance;
	int enable_floating_snap;
	int drag_tile_to_tile;
	unsigned int swipe_min_threshold;
	float focused_opacity;
	float unfocused_opacity;
	float *scroller_proportion_preset;
	int scroller_proportion_preset_count;

	char **circle_layout;
	int circle_layout_count;

	unsigned int new_is_master;
	float default_mfact;
	float default_smfact;
	unsigned int default_nmaster;

	unsigned int hotarea_size;
	unsigned int enable_hotarea;
	unsigned int ov_tab_mode;
	int overviewgappi;
	int overviewgappo;
	unsigned int cursor_hide_timeout;

	unsigned int axis_bind_apply_timeout;
	unsigned int focus_on_activate;
	int inhibit_regardless_of_visibility;
	int sloppyfocus;
	int warpcursor;

	/* keyboard */
	int repeat_rate;
	int repeat_delay;
	unsigned int numlockon;

	/* Trackpad */
	int disable_trackpad;
	int tap_to_click;
	int tap_and_drag;
	int drag_lock;
	int mouse_natural_scrolling;
	int trackpad_natural_scrolling;
	int disable_while_typing;
	int left_handed;
	int middle_button_emulation;
	unsigned int accel_profile;
	double accel_speed;
	unsigned int scroll_method;
	unsigned int scroll_button;
	unsigned int click_method;
	unsigned int send_events_mode;
	unsigned int button_map;

	int smartgaps;
	unsigned int gappih;
	unsigned int gappiv;
	unsigned int gappoh;
	unsigned int gappov;
	unsigned int borderpx;
	float scratchpad_width_ratio;
	float scratchpad_height_ratio;
	float rootcolor[4];
	float bordercolor[4];
	float focuscolor[4];
	float maxmizescreencolor[4];
	float urgentcolor[4];
	float scratchpadcolor[4];
	float globalcolor[4];
	float overlaycolor[4];

	char autostart[3][256];

	ConfigTagRule *tag_rules; // 动态数组
	int tag_rules_count;	  // 数量

	ConfigLayerRule *layer_rules; // 动态数组
	int layer_rules_count;		  // 数量

	ConfigWinRule *window_rules;
	int window_rules_count;

	ConfigMonitorRule *monitor_rules; // 动态数组
	int monitor_rules_count;		  // 条数

	KeyBinding *key_bindings;
	int key_bindings_count;

	MouseBinding *mouse_bindings;
	int mouse_bindings_count;

	AxisBinding *axis_bindings;
	int axis_bindings_count;

	GestureBinding *gesture_bindings;
	int gesture_bindings_count;

	char **exec;
	int exec_count;

	char **exec_once;
	int exec_once_count;

	char *cursor_theme;
	unsigned int cursor_size;

	int single_scratchpad;
	int xwayland_persistence;
	int syncobj_enable;

	struct xkb_rule_names xkb_rules;
} Config;

typedef void (*FuncType)(const Arg *);
Config config;

void parse_config_file(Config *config, const char *file_path);

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

int parse_double_array(const char *input, double *output, int max_count) {
	char *dup = strdup(input); // 复制一份用于修改
	char *token;
	int count = 0;

	token = strtok(dup, ",");
	while (token != NULL && count < max_count) {
		trim_whitespace(token); // 对每一个分割后的 token 去除前后空格
		char *endptr;
		double val = strtod(token, &endptr);
		if (endptr == token || *endptr != '\0') {
			fprintf(stderr, "Error: Invalid number in array: %s\n", token);
			free(dup);
			return -1; // 解析失败
		}
		output[count++] = val;
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

int parse_circle_direction(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[10];
	int i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// 根据转换后的小写字符串返回对应的枚举值
	if (strcmp(lowerStr, "next") == 0) {
		return 1;
	} else {
		return -1;
	}
}

int parse_direction(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[10];
	int i = 0;
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

long int parse_color(const char *hex_str) {
	char *endptr;
	long int hex_num = strtol(hex_str, &endptr, 16);
	if (*endptr != '\0') {
		return -1;
	}
	return hex_num;
}

xkb_keysym_t normalize_keysym(xkb_keysym_t sym) {
	// 首先转换为小写（主要影响字母键）
	sym = xkb_keysym_to_lower(sym);

	// 将数字小键盘键转换为普通数字键
	switch (sym) {
	// 小键盘数字转换
	case XKB_KEY_KP_0:
		return XKB_KEY_0;
	case XKB_KEY_KP_1:
		return XKB_KEY_1;
	case XKB_KEY_KP_2:
		return XKB_KEY_2;
	case XKB_KEY_KP_3:
		return XKB_KEY_3;
	case XKB_KEY_KP_4:
		return XKB_KEY_4;
	case XKB_KEY_KP_5:
		return XKB_KEY_5;
	case XKB_KEY_KP_6:
		return XKB_KEY_6;
	case XKB_KEY_KP_7:
		return XKB_KEY_7;
	case XKB_KEY_KP_8:
		return XKB_KEY_8;
	case XKB_KEY_KP_9:
		return XKB_KEY_9;

	// 将 Shift+数字 的符号转换回基础数字
	case XKB_KEY_exclam:
		return XKB_KEY_1; // !
	case XKB_KEY_at:
		return XKB_KEY_2; // @
	case XKB_KEY_numbersign:
		return XKB_KEY_3; // #
	case XKB_KEY_dollar:
		return XKB_KEY_4; // $
	case XKB_KEY_percent:
		return XKB_KEY_5; // %
	case XKB_KEY_asciicircum:
		return XKB_KEY_6; // ^
	case XKB_KEY_ampersand:
		return XKB_KEY_7; // &
	case XKB_KEY_asterisk:
		return XKB_KEY_8; // *
	case XKB_KEY_parenleft:
		return XKB_KEY_9; // (
	case XKB_KEY_parenright:
		return XKB_KEY_0; // )

	// 其他布局可能需要的变体（如欧洲键盘）
	case XKB_KEY_quotedbl:
		return XKB_KEY_2; // "
	case XKB_KEY_section:
		return XKB_KEY_6; // §
	case XKB_KEY_degree:
		return XKB_KEY_0; // °

	default:
		return sym;
	}
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

uint32_t parse_mod(const char *mod_str) {
	if (!mod_str || !*mod_str) {
		return 0;
	}

	uint32_t mod = 0;
	char input_copy[256];
	char *token;
	char *saveptr = NULL;

	// 复制并转换为小写
	strncpy(input_copy, mod_str, sizeof(input_copy) - 1);
	input_copy[sizeof(input_copy) - 1] = '\0';
	for (char *p = input_copy; *p; p++) {
		*p = tolower(*p);
	}

	// 分割处理每个部分
	token = strtok_r(input_copy, "+", &saveptr);
	while (token != NULL) {
		// 去除空白
		while (*token == ' ' || *token == '\t')
			token++;

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
				}
			}
		} else {
			// 完整的 modifier 检查（保留原始所有检查项）
			if (strstr(token, "super") || strstr(token, "super_l") ||
				strstr(token, "super_r")) {
				mod |= WLR_MODIFIER_LOGO;
			}
			if (strstr(token, "ctrl") || strstr(token, "ctrl_l") ||
				strstr(token, "ctrl_r")) {
				mod |= WLR_MODIFIER_CTRL;
			}
			if (strstr(token, "shift") || strstr(token, "shift_l") ||
				strstr(token, "shift_r")) {
				mod |= WLR_MODIFIER_SHIFT;
			}
			if (strstr(token, "alt") || strstr(token, "alt_l") ||
				strstr(token, "alt_r")) {
				mod |= WLR_MODIFIER_ALT;
			}
			if (strstr(token, "hyper") || strstr(token, "hyper_l") ||
				strstr(token, "hyper_r")) {
				mod |= WLR_MODIFIER_MOD3;
			}
		}

		token = strtok_r(NULL, "+", &saveptr);
	}

	return mod;
}

KeySymCode parse_key(const char *key_str) {
	KeySymCode kc;

	// 处理 code: 前缀的情况
	if (strncmp(key_str, "code:", 5) == 0) {
		char *endptr;
		xkb_keycode_t keycode = (xkb_keycode_t)strtol(key_str + 5, &endptr, 10);
		kc.type = KEY_TYPE_CODE;
		kc.keycode = keycode;
		return kc;
	}

	// 普通键名直接转换
	xkb_keysym_t sym = xkb_keysym_from_name(key_str, XKB_KEYSYM_NO_FLAGS);
	kc.type = KEY_TYPE_SYM;
	kc.keycode = 0;
	kc.keysym = sym;
	return kc;
}

int parse_button(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[20];
	int i = 0;
	while (str[i] && i < 19) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0'; // 确保字符串正确终止

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
		return 0;
	}
}

int parse_mouse_action(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[20];
	int i = 0;
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

void convert_hex_to_rgba(float *color, unsigned long int hex) {
	color[0] = ((hex >> 24) & 0xFF) / 255.0f;
	color[1] = ((hex >> 16) & 0xFF) / 255.0f;
	color[2] = ((hex >> 8) & 0xFF) / 255.0f;
	color[3] = (hex & 0xFF) / 255.0f;
}

unsigned int parse_num_type(char *str) {
	switch (str[0]) {
	case '-':
		return NUM_TYPE_MINUS;
	case '+':
		return NUM_TYPE_PLUS;
	default:
		return NUM_TYPE_DEFAULT;
	}
}

FuncType parse_func_name(char *func_name, Arg *arg, char *arg_value,
						 char *arg_value2, char *arg_value3, char *arg_value4,
						 char *arg_value5) {

	FuncType func = NULL;
	(*arg).v = NULL;
	(*arg).v2 = NULL;
	(*arg).v3 = NULL;

	if (strcmp(func_name, "focusstack") == 0) {
		func = focusstack;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "focusdir") == 0) {
		func = focusdir;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "incnmaster") == 0) {
		func = incnmaster;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "setmfact") == 0) {
		func = setmfact;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "setsmfact") == 0) {
		func = setsmfact;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "zoom") == 0) {
		func = zoom;
	} else if (strcmp(func_name, "exchange_client") == 0) {
		func = exchange_client;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "toggleglobal") == 0) {
		func = toggleglobal;
	} else if (strcmp(func_name, "toggleoverview") == 0) {
		func = toggleoverview;
	} else if (strcmp(func_name, "set_proportion") == 0) {
		func = set_proportion;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "increase_proportion") == 0) {
		func = increase_proportion;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "switch_proportion_preset") == 0) {
		func = switch_proportion_preset;
	} else if (strcmp(func_name, "viewtoleft") == 0) {
		func = viewtoleft;
	} else if (strcmp(func_name, "viewtoright") == 0) {
		func = viewtoright;
	} else if (strcmp(func_name, "tagsilent") == 0) {
		func = tagsilent;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "tagtoleft") == 0) {
		func = tagtoleft;
	} else if (strcmp(func_name, "tagtoright") == 0) {
		func = tagtoright;
	} else if (strcmp(func_name, "killclient") == 0) {
		func = killclient;
	} else if (strcmp(func_name, "focuslast") == 0) {
		func = focuslast;
	} else if (strcmp(func_name, "setlayout") == 0) {
		func = setlayout;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "switch_layout") == 0) {
		func = switch_layout;
	} else if (strcmp(func_name, "switch_keyboard_layout") == 0) {
		func = switch_keyboard_layout;
	} else if (strcmp(func_name, "togglefloating") == 0) {
		func = togglefloating;
	} else if (strcmp(func_name, "togglefullscreen") == 0) {
		func = togglefullscreen;
	} else if (strcmp(func_name, "togglefakefullscreen") == 0) {
		func = togglefakefullscreen;
	} else if (strcmp(func_name, "toggleoverlay") == 0) {
		func = toggleoverlay;
	} else if (strcmp(func_name, "minized") == 0) {
		func = minized;
	} else if (strcmp(func_name, "restore_minized") == 0) {
		func = restore_minized;
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
		(*arg).ui = atoi(arg_value2);
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
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "spawn_on_empty") == 0) {
		func = spawn_on_empty;
		(*arg).v = strdup(arg_value); // 注意：之后需要释放这个内存
		(*arg).ui = 1 << (atoi(arg_value2) - 1);
	} else if (strcmp(func_name, "quit") == 0) {
		func = quit;
	} else if (strcmp(func_name, "create_virtual_output") == 0) {
		func = create_virtual_output;
	} else if (strcmp(func_name, "destroy_all_virtual_output") == 0) {
		func = destroy_all_virtual_output;
	} else if (strcmp(func_name, "moveresize") == 0) {
		func = moveresize;
		(*arg).ui = parse_mouse_action(arg_value);
	} else if (strcmp(func_name, "togglemaxmizescreen") == 0) {
		func = togglemaxmizescreen;
	} else if (strcmp(func_name, "viewtoleft_have_client") == 0) {
		func = viewtoleft_have_client;
	} else if (strcmp(func_name, "viewtoright_have_client") == 0) {
		func = viewtoright_have_client;
	} else if (strcmp(func_name, "reload_config") == 0) {
		func = reload_config;
	} else if (strcmp(func_name, "tag") == 0) {
		func = tag;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "view") == 0) {
		func = bind_to_view;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "toggletag") == 0) {
		func = toggletag;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "toggleview") == 0) {
		func = toggleview;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
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
		(*arg).v3 = strdup(arg_value5);
		(*arg).ui = arg_value3 ? atoi(arg_value3) : 0;
		(*arg).ui2 = arg_value4 ? atoi(arg_value4) : 0;
	} else {
		return NULL;
	}
	return func;
}

void run_exec() {
	Arg arg;

	for (int i = 0; i < config.exec_count; i++) {
		arg.v = config.exec[i];
		spawn(&arg);
	}
}

void run_exec_once() {
	Arg arg;

	for (int i = 0; i < config.exec_once_count; i++) {
		arg.v = config.exec_once[i];
		spawn(&arg);
	}
}

void parse_config_line(Config *config, const char *line) {
	char key[256], value[256];
	if (sscanf(line, "%[^=]=%[^\n]", key, value) != 2) {
		// fprintf(stderr, "Error: Invalid line format: %s\n", line);
		return;
	}

	// Then trim each part separately
	trim_whitespace(key);
	trim_whitespace(value);

	if (strcmp(key, "animations") == 0) {
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
	} else if (strcmp(key, "animation_fade_in") == 0) {
		config->animation_fade_in = atoi(value);
	} else if (strcmp(key, "animation_fade_out") == 0) {
		config->animation_fade_out = atoi(value);
	} else if (strcmp(key, "tag_animation_direction") == 0) {
		config->tag_animation_direction = atoi(value);
	} else if (strcmp(key, "zoom_initial_ratio") == 0) {
		config->zoom_initial_ratio = atof(value);
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
	} else if (strcmp(key, "animation_curve_move") == 0) {
		int num = parse_double_array(value, config->animation_curve_move, 4);
		if (num != 4) {
			fprintf(stderr, "Error: Failed to parse animation_curve_move: %s\n",
					value);
		}
	} else if (strcmp(key, "animation_curve_open") == 0) {
		int num = parse_double_array(value, config->animation_curve_open, 4);
		if (num != 4) {
			fprintf(stderr, "Error: Failed to parse animation_curve_open: %s\n",
					value);
		}
	} else if (strcmp(key, "animation_curve_tag") == 0) {
		int num = parse_double_array(value, config->animation_curve_tag, 4);
		if (num != 4) {
			fprintf(stderr, "Error: Failed to parse animation_curve_tag: %s\n",
					value);
		}
	} else if (strcmp(key, "animation_curve_close") == 0) {
		int num = parse_double_array(value, config->animation_curve_close, 4);
		if (num != 4) {
			fprintf(stderr,
					"Error: Failed to parse animation_curve_close: %s\n",
					value);
		}
	} else if (strcmp(key, "scroller_structs") == 0) {
		config->scroller_structs = atoi(value);
	} else if (strcmp(key, "scroller_default_proportion") == 0) {
		config->scroller_default_proportion = atof(value);
	} else if (strcmp(key, "scroller_default_proportion_single") == 0) {
		config->scroller_default_proportion_single = atof(value);
	} else if (strcmp(key, "scroller_focus_center") == 0) {
		config->scroller_focus_center = atoi(value);
	} else if (strcmp(key, "scroller_prefer_center") == 0) {
		config->scroller_prefer_center = atoi(value);
	} else if (strcmp(key, "focus_cross_monitor") == 0) {
		config->focus_cross_monitor = atoi(value);
	} else if (strcmp(key, "focus_cross_tag") == 0) {
		config->focus_cross_tag = atoi(value);
	} else if (strcmp(key, "focus_cross_tag") == 0) {
		config->focus_cross_tag = atoi(value);
	} else if (strcmp(key, "single_scratchpad") == 0) {
		config->single_scratchpad = atoi(value);
	} else if (strcmp(key, "xwayland_persistence") == 0) {
		config->xwayland_persistence = atoi(value);
	} else if (strcmp(key, "syncobj_enable") == 0) {
		config->syncobj_enable = atoi(value);
	} else if (strcmp(key, "no_border_when_single") == 0) {
		config->no_border_when_single = atoi(value);
	} else if (strcmp(key, "snap_distance") == 0) {
		config->snap_distance = atoi(value);
	} else if (strcmp(key, "enable_floating_snap") == 0) {
		config->enable_floating_snap = atoi(value);
	} else if (strcmp(key, "drag_tile_to_tile") == 0) {
		config->drag_tile_to_tile = atoi(value);
	} else if (strcmp(key, "swipe_min_threshold") == 0) {
		config->swipe_min_threshold = atoi(value);
	} else if (strcmp(key, "focused_opacity") == 0) {
		config->focused_opacity = atof(value);
	} else if (strcmp(key, "unfocused_opacity") == 0) {
		config->unfocused_opacity = atof(value);
	} else if (strcmp(key, "xkb_rules_rules") == 0) {
		strncpy(xkb_rules_rules, value, sizeof(xkb_rules_rules) - 1);
		xkb_rules_rules[sizeof(xkb_rules_rules) - 1] =
			'\0'; // 确保字符串以 null 结尾
	} else if (strcmp(key, "xkb_rules_model") == 0) {
		strncpy(xkb_rules_model, value, sizeof(xkb_rules_model) - 1);
		xkb_rules_model[sizeof(xkb_rules_model) - 1] =
			'\0'; // 确保字符串以 null 结尾
	} else if (strcmp(key, "xkb_rules_layout") == 0) {
		strncpy(xkb_rules_layout, value, sizeof(xkb_rules_layout) - 1);
		xkb_rules_layout[sizeof(xkb_rules_layout) - 1] =
			'\0'; // 确保字符串以 null 结尾
	} else if (strcmp(key, "xkb_rules_variant") == 0) {
		strncpy(xkb_rules_variant, value, sizeof(xkb_rules_variant) - 1);
		xkb_rules_variant[sizeof(xkb_rules_variant) - 1] =
			'\0'; // 确保字符串以 null 结尾
	} else if (strcmp(key, "xkb_rules_options") == 0) {
		strncpy(xkb_rules_options, value, sizeof(xkb_rules_options) - 1);
		xkb_rules_options[sizeof(xkb_rules_options) - 1] =
			'\0'; // 确保字符串以 null 结尾
	} else if (strcmp(key, "scroller_proportion_preset") == 0) {
		// 1. 统计 value 中有多少个逗号，确定需要解析的浮点数个数
		int count = 0; // 初始化为 0
		for (const char *p = value; *p; p++) {
			if (*p == ',')
				count++;
		}
		int float_count = count + 1; // 浮点数的数量是逗号数量加 1

		// 2. 动态分配内存，存储浮点数
		config->scroller_proportion_preset =
			(float *)malloc(float_count * sizeof(float));
		if (!config->scroller_proportion_preset) {
			fprintf(stderr, "Error: Memory allocation failed\n");
			return;
		}

		// 3. 解析 value 中的浮点数
		char *value_copy =
			strdup(value); // 复制 value，因为 strtok 会修改原字符串
		char *token = strtok(value_copy, ",");
		int i = 0;
		float value_set;

		while (token != NULL && i < float_count) {
			if (sscanf(token, "%f", &value_set) != 1) {
				fprintf(stderr,
						"Error: Invalid float value in "
						"scroller_proportion_preset: %s\n",
						token);
				free(value_copy);
				free(config->scroller_proportion_preset);
				config->scroller_proportion_preset = NULL;
				return;
			}

			// Clamp the value between 0.0 and 1.0 (or your desired range)
			config->scroller_proportion_preset[i] =
				CLAMP_FLOAT(value_set, 0.1f, 1.0f);

			token = strtok(NULL, ",");
			i++;
		}

		// 4. 检查解析的浮点数数量是否匹配
		if (i != float_count) {
			fprintf(stderr,
					"Error: Invalid scroller_proportion_preset format: %s\n",
					value);
			free(value_copy);
			free(config->scroller_proportion_preset);  // 释放已分配的内存
			config->scroller_proportion_preset = NULL; // 防止野指针
			config->scroller_proportion_preset_count = 0;
			return;
		}
		config->scroller_proportion_preset_count = float_count;

		// 5. 释放临时复制的字符串
		free(value_copy);
	} else if (strcmp(key, "circle_layout") == 0) {
		// 1. 统计 value 中有多少个逗号，确定需要解析的字符串个数
		int count = 0; // 初始化为 0
		for (const char *p = value; *p; p++) {
			if (*p == ',')
				count++;
		}
		int string_count = count + 1; // 字符串的数量是逗号数量加 1

		// 2. 动态分配内存，存储字符串指针
		config->circle_layout = (char **)malloc(string_count * sizeof(char *));
		memset(config->circle_layout, 0, string_count * sizeof(char *));
		if (!config->circle_layout) {
			fprintf(stderr, "Error: Memory allocation failed\n");
			return;
		}

		// 3. 解析 value 中的字符串
		char *value_copy =
			strdup(value); // 复制 value，因为 strtok 会修改原字符串
		char *token = strtok(value_copy, ",");
		int i = 0;
		char *cleaned_token;
		while (token != NULL && i < string_count) {
			// 为每个字符串分配内存并复制内容
			cleaned_token = sanitize_string(token);
			config->circle_layout[i] = strdup(cleaned_token);
			if (!config->circle_layout[i]) {
				fprintf(stderr,
						"Error: Memory allocation failed for string: %s\n",
						token);
				// 释放之前分配的内存
				for (int j = 0; j < i; j++) {
					free(config->circle_layout[j]);
				}
				free(config->circle_layout);
				free(value_copy);
				config->circle_layout = NULL; // 防止野指针
				config->circle_layout_count = 0;
				return;
			}
			token = strtok(NULL, ",");
			i++;
		}

		// 4. 检查解析的字符串数量是否匹配
		if (i != string_count) {
			fprintf(stderr, "Error: Invalid circle_layout format: %s\n", value);
			// 释放之前分配的内存
			for (int j = 0; j < i; j++) {
				free(config->circle_layout[j]);
			}
			free(config->circle_layout);
			free(value_copy);
			config->circle_layout = NULL; // 防止野指针
			config->circle_layout_count = 0;
			return;
		}
		config->circle_layout_count = string_count;

		// 5. 释放临时复制的字符串
		free(value_copy);
	} else if (strcmp(key, "new_is_master") == 0) {
		config->new_is_master = atoi(value);
	} else if (strcmp(key, "default_mfact") == 0) {
		config->default_mfact = atof(value);
	} else if (strcmp(key, "default_smfact") == 0) {
		config->default_smfact = atof(value);
	} else if (strcmp(key, "default_nmaster") == 0) {
		config->default_nmaster = atoi(value);
	} else if (strcmp(key, "hotarea_size") == 0) {
		config->hotarea_size = atoi(value);
	} else if (strcmp(key, "enable_hotarea") == 0) {
		config->enable_hotarea = atoi(value);
	} else if (strcmp(key, "ov_tab_mode") == 0) {
		config->ov_tab_mode = atoi(value);
	} else if (strcmp(key, "overviewgappi") == 0) {
		config->overviewgappi = atoi(value);
	} else if (strcmp(key, "overviewgappo") == 0) {
		config->overviewgappo = atoi(value);
	} else if (strcmp(key, "cursor_hide_timeout") == 0) {
		config->cursor_hide_timeout = atoi(value);
	} else if (strcmp(key, "axis_bind_apply_timeout") == 0) {
		config->axis_bind_apply_timeout = atoi(value);
	} else if (strcmp(key, "focus_on_activate") == 0) {
		config->focus_on_activate = atoi(value);
	} else if (strcmp(key, "numlockon") == 0) {
		config->numlockon = atoi(value);
	} else if (strcmp(key, "inhibit_regardless_of_visibility") == 0) {
		config->inhibit_regardless_of_visibility = atoi(value);
	} else if (strcmp(key, "sloppyfocus") == 0) {
		config->sloppyfocus = atoi(value);
	} else if (strcmp(key, "warpcursor") == 0) {
		config->warpcursor = atoi(value);
	} else if (strcmp(key, "smartgaps") == 0) {
		config->smartgaps = atoi(value);
	} else if (strcmp(key, "repeat_rate") == 0) {
		config->repeat_rate = atoi(value);
	} else if (strcmp(key, "repeat_delay") == 0) {
		config->repeat_delay = atoi(value);
	} else if (strcmp(key, "disable_trackpad") == 0) {
		config->disable_trackpad = atoi(value);
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
		config->cursor_theme = strdup(value);
	} else if (strcmp(key, "disable_while_typing") == 0) {
		config->disable_while_typing = atoi(value);
	} else if (strcmp(key, "left_handed") == 0) {
		config->left_handed = atoi(value);
	} else if (strcmp(key, "middle_button_emulation") == 0) {
		config->middle_button_emulation = atoi(value);
	} else if (strcmp(key, "accel_profile") == 0) {
		config->accel_profile = atoi(value);
	} else if (strcmp(key, "accel_speed") == 0) {
		config->accel_speed = atof(value);
	} else if (strcmp(key, "scroll_method") == 0) {
		config->scroll_method = atoi(value);
	} else if (strcmp(key, "scroll_button") == 0) {
		config->scroll_button = atoi(value);
	} else if (strcmp(key, "click_method") == 0) {
		config->click_method = atoi(value);
	} else if (strcmp(key, "send_events_mode") == 0) {
		config->send_events_mode = atoi(value);
	} else if (strcmp(key, "button_map") == 0) {
		config->button_map = atoi(value);
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
	} else if (strcmp(key, "borderpx") == 0) {
		config->borderpx = atoi(value);
	} else if (strcmp(key, "rootcolor") == 0) {
		long int color = parse_color(value);
		if (color == -1) {
			fprintf(stderr, "Error: Invalid rootcolor format: %s\n", value);
		} else {
			convert_hex_to_rgba(config->rootcolor, color);
		}

	} else if (strcmp(key, "bordercolor") == 0) {
		long int color = parse_color(value);
		if (color == -1) {
			fprintf(stderr, "Error: Invalid bordercolor format: %s\n", value);
		} else {
			convert_hex_to_rgba(config->bordercolor, color);
		}
	} else if (strcmp(key, "focuscolor") == 0) {
		long int color = parse_color(value);
		if (color == -1) {
			fprintf(stderr, "Error: Invalid focuscolor format: %s\n", value);
		} else {
			convert_hex_to_rgba(config->focuscolor, color);
		}
	} else if (strcmp(key, "maxmizescreencolor") == 0) {
		long int color = parse_color(value);
		if (color == -1) {
			fprintf(stderr, "Error: Invalid maxmizescreencolor format: %s\n",
					value);
		} else {
			convert_hex_to_rgba(config->maxmizescreencolor, color);
		}
	} else if (strcmp(key, "urgentcolor") == 0) {
		long int color = parse_color(value);
		if (color == -1) {
			fprintf(stderr, "Error: Invalid urgentcolor format: %s\n", value);
		} else {
			convert_hex_to_rgba(config->urgentcolor, color);
		}
	} else if (strcmp(key, "scratchpadcolor") == 0) {
		long int color = parse_color(value);
		if (color == -1) {
			fprintf(stderr, "Error: Invalid scratchpadcolor format: %s\n",
					value);
		} else {
			convert_hex_to_rgba(config->scratchpadcolor, color);
		}
	} else if (strcmp(key, "globalcolor") == 0) {
		long int color = parse_color(value);
		if (color == -1) {
			fprintf(stderr, "Error: Invalid globalcolor format: %s\n", value);
		} else {
			convert_hex_to_rgba(config->globalcolor, color);
		}
	} else if (strcmp(key, "overlaycolor") == 0) {
		long int color = parse_color(value);
		if (color == -1) {
			fprintf(stderr, "Error: Invalid overlaycolor format: %s\n", value);
		} else {
			convert_hex_to_rgba(config->overlaycolor, color);
		}
	} else if (strcmp(key, "autostart") == 0) {
		if (sscanf(value, "%[^,],%[^,],%[^,]", config->autostart[0],
				   config->autostart[1], config->autostart[2]) != 3) {
			fprintf(stderr, "Error: Invalid autostart format: %s\n", value);
		}
		trim_whitespace(config->autostart[0]);
		trim_whitespace(config->autostart[1]);
		trim_whitespace(config->autostart[2]);
	} else if (strcmp(key, "tagrule") == 0) {
		config->tag_rules =
			realloc(config->tag_rules,
					(config->tag_rules_count + 1) * sizeof(ConfigTagRule));
		if (!config->tag_rules) {
			fprintf(stderr, "Error: Failed to allocate memory for tag rules\n");
			return;
		}

		ConfigTagRule *rule = &config->tag_rules[config->tag_rules_count];
		memset(rule, 0, sizeof(ConfigTagRule));

		// 设置默认值
		rule->id = 0;
		rule->layout_name = NULL;
		rule->monitor_name = NULL;

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
					rule->id = CLAMP_INT(atoi(val), 1, LENGTH(tags));
				} else if (strcmp(key, "layout_name") == 0) {
					rule->layout_name = strdup(val);
				} else if (strcmp(key, "monitor_name") == 0) {
					rule->monitor_name = strdup(val);
				} else if (strcmp(key, "no_render_border") == 0) {
					rule->no_render_border = CLAMP_INT(atoi(val), 0, 1);
				}
			}
			token = strtok(NULL, ",");
		}

		config->tag_rules_count++;
	} else if (strcmp(key, "layerrule") == 0) {
		config->layer_rules =
			realloc(config->layer_rules,
					(config->layer_rules_count + 1) * sizeof(ConfigLayerRule));
		if (!config->layer_rules) {
			fprintf(stderr,
					"Error: Failed to allocate memory for layer rules\n");
			return;
		}

		ConfigLayerRule *rule = &config->layer_rules[config->layer_rules_count];
		memset(rule, 0, sizeof(ConfigLayerRule));

		// 设置默认值
		rule->layer_name = NULL;
		rule->noanim = 0;

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
				} else if (strcmp(key, "noanim") == 0) {
					rule->noanim = CLAMP_INT(atoi(val), 0, 1);
				}
			}
			token = strtok(NULL, ",");
		}

		// 如果没有指定布局名称，则使用默认值
		if (rule->layer_name == NULL) {
			rule->layer_name = strdup("default");
		}

		config->layer_rules_count++;
	} else if (strcmp(key, "windowrule") == 0) {
		config->window_rules =
			realloc(config->window_rules,
					(config->window_rules_count + 1) * sizeof(ConfigWinRule));
		if (!config->window_rules) {
			fprintf(stderr,
					"Error: Failed to allocate memory for window rules\n");
			return;
		}

		ConfigWinRule *rule = &config->window_rules[config->window_rules_count];
		memset(rule, 0, sizeof(ConfigWinRule));

		rule->isfloating = -1;
		rule->isfullscreen = -1;
		rule->isnoborder = -1;
		rule->isopensilent = -1;
		rule->isnamedscratchpad = -1;
		rule->isunglobal = -1;
		rule->isglobal = -1;
		rule->isoverlay = -1;
		rule->isterm = -1;
		rule->noswallow = -1;
		rule->monitor = -1;
		rule->offsetx = 0;
		rule->offsety = 0;
		rule->nofadein = -1;
		rule->nofadeout = -1;
		rule->no_force_center = -1;
		rule->scratchpad_width = 0;
		rule->scratchpad_height = 0;
		rule->focused_opacity = 0;
		rule->unfocused_opacity = 0;
		rule->width = 0;
		rule->height = 0;
		rule->animation_type_open = NULL;
		rule->animation_type_close = NULL;
		rule->scroller_proportion = 0;
		rule->id = NULL;
		rule->title = NULL;
		rule->tags = 0;
		rule->globalkeybinding = (KeyBinding){0};

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
					rule->tags = 1 << (atoi(val) - 1);
				} else if (strcmp(key, "monitor") == 0) {
					rule->monitor = atoi(val);
				} else if (strcmp(key, "offsetx") == 0) {
					rule->offsetx = atoi(val);
				} else if (strcmp(key, "offsety") == 0) {
					rule->offsety = atoi(val);
				} else if (strcmp(key, "nofadein") == 0) {
					rule->nofadein = atoi(val);
				} else if (strcmp(key, "nofadeout") == 0) {
					rule->nofadeout = atoi(val);
				} else if (strcmp(key, "no_force_center") == 0) {
					rule->no_force_center = atoi(val);
				} else if (strcmp(key, "scratchpad_width") == 0) {
					rule->scratchpad_width = atoi(val);
				} else if (strcmp(key, "scratchpad_height") == 0) {
					rule->scratchpad_height = atoi(val);
				} else if (strcmp(key, "width") == 0) {
					rule->width = atoi(val);
				} else if (strcmp(key, "height") == 0) {
					rule->height = atoi(val);
				} else if (strcmp(key, "isnoborder") == 0) {
					rule->isnoborder = atoi(val);
				} else if (strcmp(key, "isopensilent") == 0) {
					rule->isopensilent = atoi(val);
				} else if (strcmp(key, "isnamedscratchpad") == 0) {
					rule->isnamedscratchpad = atoi(val);
				} else if (strcmp(key, "isunglobal") == 0) {
					rule->isunglobal = atoi(val);
				} else if (strcmp(key, "isglobal") == 0) {
					rule->isglobal = atoi(val);
				} else if (strcmp(key, "unfocused_opacity") == 0) {
					rule->unfocused_opacity = atof(val);
				} else if (strcmp(key, "focused_opacity") == 0) {
					rule->focused_opacity = atof(val);
				} else if (strcmp(key, "isoverlay") == 0) {
					rule->isoverlay = atoi(val);
				} else if (strcmp(key, "isterm") == 0) {
					rule->isterm = atoi(val);
				} else if (strcmp(key, "noswallow") == 0) {
					rule->noswallow = atoi(val);
				} else if (strcmp(key, "scroller_proportion") == 0) {
					rule->scroller_proportion = atof(val);
				} else if (strcmp(key, "isfullscreen") == 0) {
					rule->isfullscreen = atoi(val);
				} else if (strcmp(key, "globalkeybinding") == 0) {
					char mod_str[256], keysym_str[256];
					sscanf(val, "%[^-]-%[a-zA-Z]", mod_str, keysym_str);
					trim_whitespace(mod_str);
					trim_whitespace(keysym_str);
					rule->globalkeybinding.mod = parse_mod(mod_str);
					rule->globalkeybinding.keysymcode = parse_key(keysym_str);
				}
			}
			token = strtok(NULL, ",");
		}
		config->window_rules_count++;
	} else if (strcmp(key, "monitorrule") == 0) {
		config->monitor_rules =
			realloc(config->monitor_rules, (config->monitor_rules_count + 1) *
											   sizeof(ConfigMonitorRule));
		if (!config->monitor_rules) {
			fprintf(stderr,
					"Error: Failed to allocate memory for monitor rules\n");
			return;
		}

		ConfigMonitorRule *rule =
			&config->monitor_rules[config->monitor_rules_count];
		memset(rule, 0, sizeof(ConfigMonitorRule));

		// 临时存储每个字段的原始字符串
		char raw_name[256], raw_layout[256];
		char raw_mfact[256], raw_nmaster[256], raw_rr[256];
		char raw_scale[256], raw_x[256], raw_y[256];

		// 先读取所有字段为字符串
		int parsed = sscanf(value,
							"%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255["
							"^,],%255[^,],%255s",
							raw_name, raw_mfact, raw_nmaster, raw_layout,
							raw_rr, raw_scale, raw_x, raw_y);

		if (parsed == 8) {
			// 修剪每个字段的空格
			trim_whitespace(raw_name);
			trim_whitespace(raw_mfact);
			trim_whitespace(raw_nmaster);
			trim_whitespace(raw_layout);
			trim_whitespace(raw_rr);
			trim_whitespace(raw_scale);
			trim_whitespace(raw_x);
			trim_whitespace(raw_y);

			// 转换修剪后的字符串为特定类型
			rule->name = strdup(raw_name);
			rule->layout = strdup(raw_layout);
			rule->mfact = atof(raw_mfact);
			rule->nmaster = atoi(raw_nmaster);
			rule->rr = atoi(raw_rr);
			rule->scale = atof(raw_scale);
			rule->x = atoi(raw_x);
			rule->y = atoi(raw_y);

			if (!rule->name || !rule->layout) {
				if (rule->name)
					free((void *)rule->name);
				if (rule->layout)
					free((void *)rule->layout);
				fprintf(stderr,
						"Error: Failed to allocate memory for monitor rule\n");
				return;
			}

			config->monitor_rules_count++;
		} else {
			fprintf(stderr, "Error: Invalid monitorrule format: %s\n", value);
		}
	} else if (strncmp(key, "env", 3) == 0) {

		char env_type[256], env_value[256];
		if (sscanf(value, "%[^,],%[^\n]", env_type, env_value) < 2) {
			fprintf(stderr, "Error: Invalid bind format: %s\n", value);
			return;
		}
		trim_whitespace(env_type);
		trim_whitespace(env_value);
		setenv(env_type, env_value, 1);

	} else if (strncmp(key, "exec", 9) == 0) {
		char **new_exec =
			realloc(config->exec, (config->exec_count + 1) * sizeof(char *));
		if (!new_exec) {
			fprintf(stderr, "Error: Failed to allocate memory for exec\n");
			return;
		}
		config->exec = new_exec;

		config->exec[config->exec_count] = strdup(value);
		if (!config->exec[config->exec_count]) {
			fprintf(stderr, "Error: Failed to duplicate exec string\n");
			return;
		}

		config->exec_count++;

	} else if (strncmp(key, "exec-once", 9) == 0) {

		char **new_exec_once = realloc(
			config->exec_once, (config->exec_once_count + 1) * sizeof(char *));
		if (!new_exec_once) {
			fprintf(stderr, "Error: Failed to allocate memory for exec_once\n");
			return;
		}
		config->exec_once = new_exec_once;

		config->exec_once[config->exec_once_count] = strdup(value);
		if (!config->exec_once[config->exec_once_count]) {
			fprintf(stderr, "Error: Failed to duplicate exec_once string\n");
			return;
		}

		config->exec_once_count++;

	} else if (strncmp(key, "bind", 4) == 0) {
		config->key_bindings =
			realloc(config->key_bindings,
					(config->key_bindings_count + 1) * sizeof(KeyBinding));
		if (!config->key_bindings) {
			fprintf(stderr,
					"Error: Failed to allocate memory for key bindings\n");
			return;
		}

		KeyBinding *binding = &config->key_bindings[config->key_bindings_count];
		memset(binding, 0, sizeof(KeyBinding));

		char mod_str[256], keysym_str[256], func_name[256],
			arg_value[256] = "none", arg_value2[256] = "none",
			arg_value3[256] = "none", arg_value4[256] = "none",
			arg_value5[256] = "none";
		if (sscanf(value, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^\n]",
				   mod_str, keysym_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			fprintf(stderr, "Error: Invalid bind format: %s\n", value);
			return;
		}
		trim_whitespace(mod_str);
		trim_whitespace(keysym_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->mod = parse_mod(mod_str);
		binding->keysymcode = parse_key(keysym_str);
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
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
			fprintf(stderr, "Error: Unknown function in bind: %s\n", func_name);
		} else {
			config->key_bindings_count++;
		}

	} else if (strncmp(key, "mousebind", 9) == 0) {
		config->mouse_bindings =
			realloc(config->mouse_bindings,
					(config->mouse_bindings_count + 1) * sizeof(MouseBinding));
		if (!config->mouse_bindings) {
			fprintf(stderr,
					"Error: Failed to allocate memory for mouse bindings\n");
			return;
		}

		MouseBinding *binding =
			&config->mouse_bindings[config->mouse_bindings_count];
		memset(binding, 0, sizeof(MouseBinding));

		char mod_str[256], button_str[256], func_name[256],
			arg_value[256] = "none", arg_value2[256] = "none",
			arg_value3[256] = "none", arg_value4[256] = "none",
			arg_value5[256] = "none";
		if (sscanf(value, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^\n]",
				   mod_str, button_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			fprintf(stderr, "Error: Invalid mousebind format: %s\n", value);
			return;
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
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
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
			fprintf(stderr, "Error: Unknown function in mousebind: %s\n",
					func_name);
		} else {
			config->mouse_bindings_count++;
		}
	} else if (strncmp(key, "axisbind", 8) == 0) {
		config->axis_bindings =
			realloc(config->axis_bindings,
					(config->axis_bindings_count + 1) * sizeof(AxisBinding));
		if (!config->axis_bindings) {
			fprintf(stderr,
					"Error: Failed to allocate memory for axis bindings\n");
			return;
		}

		AxisBinding *binding =
			&config->axis_bindings[config->axis_bindings_count];
		memset(binding, 0, sizeof(AxisBinding));

		char mod_str[256], dir_str[256], func_name[256],
			arg_value[256] = "none", arg_value2[256] = "none",
			arg_value3[256] = "none", arg_value4[256] = "none",
			arg_value5[256] = "none";
		if (sscanf(value, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^\n]",
				   mod_str, dir_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			fprintf(stderr, "Error: Invalid axisbind format: %s\n", value);
			return;
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
			fprintf(stderr, "Error: Unknown function in axisbind: %s\n",
					func_name);
		} else {
			config->axis_bindings_count++;
		}

	} else if (strncmp(key, "gesturebind", 11) == 0) {
		config->gesture_bindings = realloc(
			config->gesture_bindings,
			(config->gesture_bindings_count + 1) * sizeof(GestureBinding));
		if (!config->gesture_bindings) {
			fprintf(stderr,
					"Error: Failed to allocate memory for axis gesturebind\n");
			return;
		}

		GestureBinding *binding =
			&config->gesture_bindings[config->gesture_bindings_count];
		memset(binding, 0, sizeof(GestureBinding));

		char mod_str[256], motion_str[256], fingers_count_str[256],
			func_name[256], arg_value[256] = "none", arg_value2[256] = "none",
							arg_value3[256] = "none", arg_value4[256] = "none",
							arg_value5[256] = "none";
		if (sscanf(value,
				   "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^\n]",
				   mod_str, motion_str, fingers_count_str, func_name, arg_value,
				   arg_value2, arg_value3, arg_value4, arg_value5) < 4) {
			fprintf(stderr, "Error: Invalid gesturebind format: %s\n", value);
			return;
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
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
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
			fprintf(stderr, "Error: Unknown function in axisbind: %s\n",
					func_name);
		} else {
			config->gesture_bindings_count++;
		}

	} else if (strncmp(key, "source", 6) == 0) {
		parse_config_file(config, value);
	} else {
		fprintf(stderr, "Error: Unknown key: %s\n", key);
	}
}

void parse_config_file(Config *config, const char *file_path) {
	FILE *file;
	// 检查路径是否以 ~/ 开头
	if (file_path[0] == '~' && (file_path[1] == '/' || file_path[1] == '\0')) {
		const char *home = getenv("HOME");
		if (!home) {
			fprintf(stderr, "Error: HOME environment variable not set.\n");
			return;
		}

		// 构建完整路径（家目录 + / + 原路径去掉 ~）
		char full_path[1024];
		snprintf(full_path, sizeof(full_path), "%s%s", home, file_path + 1);

		file = fopen(full_path, "r");
		if (!file) {
			perror("Error opening file");
			return;
		}
	} else {
		file = fopen(file_path, "r");
		if (!file) {
			perror("Error opening file");
			return;
		}
	}

	char line[512];
	while (fgets(line, sizeof(line), file)) {
		if (line[0] == '#' || line[0] == '\n')
			continue;
		parse_config_line(config, line);
	}

	fclose(file);
}

void free_circle_layout(Config *config) {
	if (config->circle_layout) {
		// 释放每个字符串
		for (int i = 0; i < config->circle_layout_count; i++) {
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
}

void free_config(void) {
	// 释放内存
	int i;

	// 释放 window_rules
	if (config.window_rules) {
		for (int i = 0; i < config.window_rules_count; i++) {
			ConfigWinRule *rule = &config.window_rules[i];
			free((void *)rule->id);
			free((void *)rule->title);
			free((void *)rule->animation_type_open);
			free((void *)rule->animation_type_close);
			// 释放 globalkeybinding 的 arg.v（如果动态分配）
			if (rule->globalkeybinding.arg.v) {
				free((void *)rule->globalkeybinding.arg.v);
			}
		}
		free(config.window_rules);
		config.window_rules = NULL;
		config.window_rules_count = 0;
	}

	// 释放 monitor_rules
	if (config.monitor_rules) {
		for (int i = 0; i < config.monitor_rules_count; i++) {
			ConfigMonitorRule *rule = &config.monitor_rules[i];
			free((void *)rule->name);
			free((void *)rule->layout);
		}
		free(config.monitor_rules);
		config.monitor_rules = NULL;
		config.monitor_rules_count = 0;
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
		for (int i = 0; i < config.tag_rules_count; i++) {
			if (config.tag_rules[i].layout_name)
				free((void *)config.tag_rules[i].layout_name);
			if (config.tag_rules[i].monitor_name)
				free((void *)config.tag_rules[i].monitor_name);
		}
		free(config.tag_rules);
		config.tag_rules = NULL;
		config.tag_rules_count = 0;
	}

	// 释放 layer_rules
	if (config.layer_rules) {
		for (int i = 0; i < config.layer_rules_count; i++) {
			free((void *)config.layer_rules[i].layer_name);
		}
		free(config.layer_rules);
		config.layer_rules = NULL;
		config.layer_rules_count = 0;
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

	// 释放 circle_layout
	free_circle_layout(&config);

	// 释放动画资源
	free_baked_points();
}

void override_config(void) {
	// 动画启用
	animations = CLAMP_INT(config.animations, 0, 1);
	layer_animations = CLAMP_INT(config.layer_animations, 0, 1);

	// 标签动画方向
	tag_animation_direction = CLAMP_INT(config.tag_animation_direction, 0, 1);

	// 动画淡入淡出设置
	animation_fade_in = CLAMP_INT(config.animation_fade_in, 0, 1);
	animation_fade_out = CLAMP_INT(config.animation_fade_out, 0, 1);
	zoom_initial_ratio = CLAMP_FLOAT(config.zoom_initial_ratio, 0.1f, 1.0f);
	fadein_begin_opacity = CLAMP_FLOAT(config.fadein_begin_opacity, 0.0f, 1.0f);
	fadeout_begin_opacity =
		CLAMP_FLOAT(config.fadeout_begin_opacity, 0.0f, 1.0f);

	// 打开关闭动画类型
	animation_type_open = config.animation_type_open;
	animation_type_close = config.animation_type_close;

	// 动画时间限制在合理范围(1-50000ms)
	animation_duration_move =
		CLAMP_INT(config.animation_duration_move, 1, 50000);
	animation_duration_open =
		CLAMP_INT(config.animation_duration_open, 1, 50000);
	animation_duration_tag = CLAMP_INT(config.animation_duration_tag, 1, 50000);
	animation_duration_close =
		CLAMP_INT(config.animation_duration_close, 1, 50000);

	// 滚动布局设置
	scroller_default_proportion =
		CLAMP_FLOAT(config.scroller_default_proportion, 0.1f, 1.0f);
	scroller_default_proportion_single =
		CLAMP_FLOAT(config.scroller_default_proportion_single, 0.1f, 1.0f);
	scroller_focus_center = CLAMP_INT(config.scroller_focus_center, 0, 1);
	scroller_prefer_center = CLAMP_INT(config.scroller_prefer_center, 0, 1);
	scroller_structs = CLAMP_INT(config.scroller_structs, 0, 1000);

	// 主从布局设置
	default_mfact = CLAMP_FLOAT(config.default_mfact, 0.1f, 0.9f);
	default_smfact = CLAMP_FLOAT(config.default_smfact, 0.1f, 0.9f);
	default_nmaster = CLAMP_INT(config.default_nmaster, 1, 1000);
	new_is_master = CLAMP_INT(config.new_is_master, 0, 1);

	// 概述模式设置
	hotarea_size = CLAMP_INT(config.hotarea_size, 1, 1000);
	enable_hotarea = CLAMP_INT(config.enable_hotarea, 0, 1);
	ov_tab_mode = CLAMP_INT(config.ov_tab_mode, 0, 1);
	overviewgappi = CLAMP_INT(config.overviewgappi, 0, 1000);
	overviewgappo = CLAMP_INT(config.overviewgappo, 0, 1000);

	// 杂项设置
	xwayland_persistence = CLAMP_INT(config.xwayland_persistence, 0, 1);
	syncobj_enable = CLAMP_INT(config.syncobj_enable, 0, 1);
	axis_bind_apply_timeout =
		CLAMP_INT(config.axis_bind_apply_timeout, 0, 1000);
	focus_on_activate = CLAMP_INT(config.focus_on_activate, 0, 1);
	inhibit_regardless_of_visibility =
		CLAMP_INT(config.inhibit_regardless_of_visibility, 0, 1);
	sloppyfocus = CLAMP_INT(config.sloppyfocus, 0, 1);
	warpcursor = CLAMP_INT(config.warpcursor, 0, 1);
	focus_cross_monitor = CLAMP_INT(config.focus_cross_monitor, 0, 1);
	focus_cross_tag = CLAMP_INT(config.focus_cross_tag, 0, 1);
	enable_floating_snap = CLAMP_INT(config.enable_floating_snap, 0, 1);
	snap_distance = CLAMP_INT(config.snap_distance, 0, 99999);
	cursor_size = CLAMP_INT(config.cursor_size, 4, 512);
	no_border_when_single = CLAMP_INT(config.no_border_when_single, 0, 1);
	cursor_hide_timeout =
		CLAMP_INT(config.cursor_hide_timeout, 0, 36000); // 0-10小时
	drag_tile_to_tile = CLAMP_INT(config.drag_tile_to_tile, 0, 1);
	single_scratchpad = CLAMP_INT(config.single_scratchpad, 0, 1);

	// 键盘设置
	repeat_rate = CLAMP_INT(config.repeat_rate, 1, 1000);
	repeat_delay = CLAMP_INT(config.repeat_delay, 1, 20000);
	numlockon = CLAMP_INT(config.numlockon, 0, 1);

	// 触控板设置
	disable_trackpad = CLAMP_INT(config.disable_trackpad, 0, 1);
	tap_to_click = CLAMP_INT(config.tap_to_click, 0, 1);
	tap_and_drag = CLAMP_INT(config.tap_and_drag, 0, 1);
	drag_lock = CLAMP_INT(config.drag_lock, 0, 1);
	trackpad_natural_scrolling =
		CLAMP_INT(config.trackpad_natural_scrolling, 0, 1);
	disable_while_typing = CLAMP_INT(config.disable_while_typing, 0, 1);
	left_handed = CLAMP_INT(config.left_handed, 0, 1);
	middle_button_emulation = CLAMP_INT(config.middle_button_emulation, 0, 1);
	swipe_min_threshold = CLAMP_INT(config.swipe_min_threshold, 1, 1000);

	// 鼠标设置
	mouse_natural_scrolling = CLAMP_INT(config.mouse_natural_scrolling, 0, 1);
	accel_profile = CLAMP_INT(config.accel_profile, 0, 2);
	accel_speed = CLAMP_FLOAT(config.accel_speed, -1.0f, 1.0f);
	scroll_method = CLAMP_INT(config.scroll_method, 0, 4);
	scroll_button = CLAMP_INT(config.scroll_button, 272, 276);
	click_method = CLAMP_INT(config.click_method, 0, 2);
	send_events_mode = CLAMP_INT(config.send_events_mode, 0, 2);
	button_map = CLAMP_INT(config.button_map, 0, 1);

	// 外观设置
	gappih = CLAMP_INT(config.gappih, 0, 1000);
	gappiv = CLAMP_INT(config.gappiv, 0, 1000);
	gappoh = CLAMP_INT(config.gappoh, 0, 1000);
	gappov = CLAMP_INT(config.gappov, 0, 1000);
	scratchpad_width_ratio =
		CLAMP_FLOAT(config.scratchpad_width_ratio, 0.1f, 1.0f);
	scratchpad_height_ratio =
		CLAMP_FLOAT(config.scratchpad_height_ratio, 0.1f, 1.0f);
	borderpx = CLAMP_INT(config.borderpx, 0, 200);
	smartgaps = CLAMP_INT(config.smartgaps, 0, 1);

	focused_opacity = CLAMP_FLOAT(config.focused_opacity, 0.0f, 1.0f);
	unfocused_opacity = CLAMP_FLOAT(config.unfocused_opacity, 0.0f, 1.0f);

	// 复制颜色数组
	memcpy(rootcolor, config.rootcolor, sizeof(rootcolor));
	memcpy(bordercolor, config.bordercolor, sizeof(bordercolor));
	memcpy(focuscolor, config.focuscolor, sizeof(focuscolor));
	memcpy(maxmizescreencolor, config.maxmizescreencolor,
		   sizeof(maxmizescreencolor));
	memcpy(urgentcolor, config.urgentcolor, sizeof(urgentcolor));
	memcpy(scratchpadcolor, config.scratchpadcolor, sizeof(scratchpadcolor));
	memcpy(globalcolor, config.globalcolor, sizeof(globalcolor));
	memcpy(overlaycolor, config.overlaycolor, sizeof(overlaycolor));

	// 复制动画曲线
	memcpy(animation_curve_move, config.animation_curve_move,
		   sizeof(animation_curve_move));
	memcpy(animation_curve_open, config.animation_curve_open,
		   sizeof(animation_curve_open));
	memcpy(animation_curve_tag, config.animation_curve_tag,
		   sizeof(animation_curve_tag));
	memcpy(animation_curve_close, config.animation_curve_close,
		   sizeof(animation_curve_close));
}

void set_value_default() {
	/* animaion */
	config.animations = animations;					// 是否启用动画
	config.layer_animations = layer_animations;		// 是否启用layer动画
	config.animation_fade_in = animation_fade_in;	// Enable animation fade in
	config.animation_fade_out = animation_fade_out; // Enable animation fade out
	config.tag_animation_direction = tag_animation_direction; // 标签动画方向
	config.zoom_initial_ratio = zoom_initial_ratio; // 动画起始窗口比例
	config.fadein_begin_opacity =
		fadein_begin_opacity; // Begin opac window ratio for animations
	config.fadeout_begin_opacity = fadeout_begin_opacity;
	config.animation_duration_move =
		animation_duration_move; // Animation move speed
	config.animation_duration_open =
		animation_duration_open; // Animation open speed
	config.animation_duration_tag =
		animation_duration_tag; // Animation tag speed
	config.animation_duration_close =
		animation_duration_close; // Animation tag speed

	/* appearance */
	config.axis_bind_apply_timeout =
		axis_bind_apply_timeout; // 滚轮绑定动作的触发的时间间隔
	config.focus_on_activate =
		focus_on_activate;					// 收到窗口激活请求是否自动跳转聚焦
	config.new_is_master = new_is_master;	// 新窗口是否插在头部
	config.default_mfact = default_mfact;	// master 窗口比例
	config.default_smfact = default_smfact; // 第一个stack比例
	config.default_nmaster = default_nmaster; // 默认master数量

	config.numlockon = numlockon; // 是否打开右边小键盘

	config.ov_tab_mode = ov_tab_mode;		// alt tab切换模式
	config.hotarea_size = hotarea_size;		// 热区大小,10x10
	config.enable_hotarea = enable_hotarea; // 是否启用鼠标热区
	config.smartgaps =
		smartgaps; /* 1 means no outer gap when there is only one window */
	config.sloppyfocus = sloppyfocus; /* focus follows mouse */
	config.gappih = gappih;			  /* horiz inner gap between windows */
	config.gappiv = gappiv;			  /* vert inner gap between windows */
	config.gappoh =
		gappoh; /* horiz outer gap between windows and screen edge */
	config.gappov = gappov; /* vert outer gap between windows and screen edge */
	config.scratchpad_width_ratio = scratchpad_width_ratio;
	config.scratchpad_height_ratio = scratchpad_height_ratio;

	config.scroller_structs = scroller_structs;
	config.scroller_default_proportion = scroller_default_proportion;
	config.scroller_default_proportion_single =
		scroller_default_proportion_single;
	config.scroller_focus_center = scroller_focus_center;
	config.scroller_prefer_center = scroller_prefer_center;
	config.focus_cross_monitor = focus_cross_monitor;
	config.focus_cross_tag = focus_cross_tag;
	config.single_scratchpad = single_scratchpad;
	config.xwayland_persistence = xwayland_persistence;
	config.syncobj_enable = syncobj_enable;
	config.no_border_when_single = no_border_when_single;
	config.snap_distance = snap_distance;
	config.drag_tile_to_tile = drag_tile_to_tile;
	config.enable_floating_snap = enable_floating_snap;
	config.swipe_min_threshold = swipe_min_threshold;

	config.inhibit_regardless_of_visibility =
		inhibit_regardless_of_visibility; /* 1 means idle inhibitors will
									  disable idle tracking even if it's surface
									  isn't visible
									*/

	config.borderpx = borderpx;
	config.overviewgappi = overviewgappi; /* overview时 窗口与边缘 缝隙大小 */
	config.overviewgappo = overviewgappo; /* overview时 窗口与窗口 缝隙大小 */
	config.cursor_hide_timeout = cursor_hide_timeout;

	config.warpcursor = warpcursor; /* Warp cursor to focused client */

	config.repeat_rate = repeat_rate;
	config.repeat_delay = repeat_delay;

	/* Trackpad */
	config.disable_trackpad = disable_trackpad;
	config.tap_to_click = tap_to_click;
	config.tap_and_drag = tap_and_drag;
	config.drag_lock = drag_lock;
	config.mouse_natural_scrolling = mouse_natural_scrolling;
	config.cursor_size = cursor_size;
	config.trackpad_natural_scrolling = trackpad_natural_scrolling;
	config.disable_while_typing = disable_while_typing;
	config.left_handed = left_handed;
	config.middle_button_emulation = middle_button_emulation;
	config.accel_profile = accel_profile;
	config.accel_speed = accel_speed;
	config.scroll_method = scroll_method;
	config.scroll_button = scroll_button;
	config.click_method = click_method;
	config.send_events_mode = send_events_mode;
	config.button_map = button_map;

	config.focused_opacity = focused_opacity;
	config.unfocused_opacity = unfocused_opacity;

	memcpy(config.animation_curve_move, animation_curve_move,
		   sizeof(animation_curve_move));
	memcpy(config.animation_curve_open, animation_curve_open,
		   sizeof(animation_curve_open));
	memcpy(config.animation_curve_tag, animation_curve_tag,
		   sizeof(animation_curve_tag));
	memcpy(config.animation_curve_close, animation_curve_close,
		   sizeof(animation_curve_close));

	memcpy(config.rootcolor, rootcolor, sizeof(rootcolor));
	memcpy(config.bordercolor, bordercolor, sizeof(bordercolor));
	memcpy(config.focuscolor, focuscolor, sizeof(focuscolor));
	memcpy(config.maxmizescreencolor, maxmizescreencolor,
		   sizeof(maxmizescreencolor));
	memcpy(config.urgentcolor, urgentcolor, sizeof(urgentcolor));
	memcpy(config.scratchpadcolor, scratchpadcolor, sizeof(scratchpadcolor));
	memcpy(config.globalcolor, globalcolor, sizeof(globalcolor));
	memcpy(config.overlaycolor, overlaycolor, sizeof(overlaycolor));
}

void set_default_key_bindings(Config *config) {
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
	}

	// 更新按键绑定的总数
	config->key_bindings_count += default_key_bindings_count;
}

void parse_config(void) {

	char filename[1024];

	free_config();

	// 重置config结构体，确保所有指针初始化为NULL
	memset(&config, 0, sizeof(config));

	// 初始化动态数组的指针为NULL，避免野指针
	config.window_rules = NULL;
	config.window_rules_count = 0;
	config.monitor_rules = NULL;
	config.monitor_rules_count = 0;
	config.key_bindings = NULL;
	config.key_bindings_count = 0;
	config.mouse_bindings = NULL;
	config.mouse_bindings_count = 0;
	config.axis_bindings = NULL;
	config.axis_bindings_count = 0;
	config.gesture_bindings = NULL;
	config.gesture_bindings_count = 0;
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

	// 获取 MAOMAOCONFIG 环境变量
	const char *maomaoconfig = getenv("MAOMAOCONFIG");

	// 如果 MAOMAOCONFIG 环境变量不存在或为空，则使用 HOME 环境变量
	if (!maomaoconfig || maomaoconfig[0] == '\0') {
		// 获取当前用户家目录
		const char *homedir = getenv("HOME");
		if (!homedir) {
			// 如果获取失败，则无法继续
			return;
		}
		// 构建日志文件路径
		snprintf(filename, sizeof(filename), "%s/.config/maomao/config.conf",
				 homedir);

		// 检查文件是否存在
		if (access(filename, F_OK) != 0) {
			// 如果文件不存在，则使用 /etc/maomao/config.conf
			snprintf(filename, sizeof(filename), "%s/maomao/config.conf",
					 SYSCONFDIR);
		}
	} else {
		// 使用 MAOMAOCONFIG 环境变量作为配置文件夹路径
		snprintf(filename, sizeof(filename), "%s/config.conf", maomaoconfig);
	}

	set_value_default();
	parse_config_file(&config, filename);
	set_default_key_bindings(&config);
	override_config();
}

void reload_config(const Arg *arg) {
	Client *c;
	Monitor *m;
	int i, jk;
	Keyboard *kb;
	char *rule_monitor_name = NULL;
	parse_config();
	init_baked_points();
	handlecursoractivity();
	reset_keyboard_layout();
	run_exec();

	// reset border width when config change
	wl_list_for_each(c, &clients, link) {
		if (c && !c->iskilling) {
			if (c->bw && !c->isnoborder) {
				c->bw = borderpx;
			}
		}
	}

	// reset keyboard repeat rate when config change
	wl_list_for_each(kb, &keyboards, link) {
		wlr_keyboard_set_repeat_info(kb->wlr_keyboard, repeat_rate,
									 repeat_delay);
	}

	// reset master status when config change
	for (i = 0; i <= LENGTH(tags); i++) {
		wl_list_for_each(m, &mons, link) {
			if (!m->wlr_output->enabled) {
				continue;
			}
			m->pertag->nmasters[i] = default_nmaster;
			m->pertag->mfacts[i] = default_mfact;
			m->pertag->smfacts[i] = default_smfact;
			m->gappih = gappih;
			m->gappiv = gappiv;
			m->gappoh = gappoh;
			m->gappov = gappov;
		}
	}

	// reset tag status by tag rules
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}

		// apply tag rule
		for (i = 1; i <= config.tag_rules_count; i++) {
			rule_monitor_name = config.tag_rules[i - 1].monitor_name;
			if (regex_match(rule_monitor_name, m->wlr_output->name) ||
				!rule_monitor_name) {
				for (jk = 0; jk < LENGTH(layouts); jk++) {
					if (config.tag_rules_count > 0 &&
						strcmp(layouts[jk].name,
							   config.tag_rules[i - 1].layout_name) == 0) {
						m->pertag->ltidxs[config.tag_rules[i - 1].id] =
							&layouts[jk];
					}
				}
			}
		}
	}

	arrange(selmon, false);
}
