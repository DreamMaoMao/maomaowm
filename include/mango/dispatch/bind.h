#ifndef __BIND_H__
#define __BIND_H__ 1

#include "mango/common/types.h"
#include <stdint.h>

/* The generic argument carried by a binding action. */
typedef struct Arg {
	int32_t i;
	int32_t i2;
	float f;
	float f2;
	char *v;
	char *v2;
	char *v3;
	uint32_t ui;
	uint32_t ui2;
	Client *tc;
} Arg;

enum { PREV, NEXT };
enum { FORCE, UNFORCE };

void minimize_window(const Arg *arg);
void restore_minimized(const Arg *arg);
void toggle_scratchpad(const Arg *arg);
void focus_direction(const Arg *arg);
void focus_window_or_workspace(const Arg *arg);
void group_join(const Arg *arg);
void group_leave(const Arg *arg);
void toggle_overview(const Arg *arg);
void switcher(const Arg *arg);
void toggle_hdr(const Arg *arg);
void toggle_jump(const Arg *arg);
void set_proportion(const Arg *arg);
void switch_proportion_preset(const Arg *arg);
void zoom(const Arg *arg);
void tag_silent(const Arg *arg);
void tag_to_left(const Arg *arg);
void tag_to_right(const Arg *arg);
void tag_cross_monitor(const Arg *arg);
void view_to_left(const Arg *arg);
void view_to_right(const Arg *arg);
void view_insert(const Arg *arg);
void view_to_left_have_client(const Arg *arg);
void view_to_right_have_client(const Arg *arg);
void view_cross_monitor(const Arg *arg);
void toggle_floating(const Arg *arg);
void toggle_fullscreen(const Arg *arg);
void toggle_maximize_screen(const Arg *arg);
void toggle_gaps(const Arg *arg);
void tag_monitor(const Arg *arg);
void spawn(const Arg *arg);
void spawn_shell(const Arg *arg);
void spawn_on_empty(const Arg *arg);
void set_key_mode(const Arg *arg);
void switch_keyboard_layout(const Arg *arg);
void set_layout(const Arg *arg);
void switch_layout(const Arg *arg);
void set_master_factor(const Arg *arg);
void quit(const Arg *arg);
void move_resize(const Arg *arg);
void exchange_client(const Arg *arg);
void exchange_stack_client(const Arg *arg);
void kill_client(const Arg *arg);
void toggle_global(const Arg *arg);
void inc_nmaster(const Arg *arg);
void focus_monitor(const Arg *arg);
void focus_stack(const Arg *arg);
void over_circle(const Arg *arg);
void group_focus(const Arg *arg);
void change_vt(const Arg *arg);
void reload_config(const Arg *arg);
void load_config_file(const Arg *arg);
void smart_move_window(const Arg *arg);
void smart_resize_window(const Arg *arg);
void center_window(const Arg *arg);
void bind_to_view(const Arg *arg);
void toggle_tag(const Arg *arg);
void toggle_view(const Arg *arg);
void tag(const Arg *arg);
void combo_view(const Arg *arg);
void increase_gaps(const Arg *arg);
void increase_inner_gap(const Arg *arg);
void increase_inner_horizontal_gap(const Arg *arg);
void increase_inner_vertical_gap(const Arg *arg);
void increase_outer_gap(const Arg *arg);
void increase_outer_horizontal_gap(const Arg *arg);
void increase_outer_vertical_gap(const Arg *arg);
void reset_gaps(const Arg *arg);
void toggle_fake_fullscreen(const Arg *arg);
void toggle_overlay(const Arg *arg);
void move_window(const Arg *arg);
void resize_window(const Arg *arg);
void toggle_named_scratchpad(const Arg *arg);
void toggle_render_border(const Arg *arg);
void create_virtual_output(const Arg *arg);
void destroy_all_virtual_output(const Arg *arg);
void focus_last(const Arg *arg);
void toggle_trackpad_enable(const Arg *arg);
void setoption(const Arg *arg);
void disable_monitor(const Arg *arg);
void enable_monitor(const Arg *arg);
void toggle_monitor(const Arg *arg);
void sleep_monitor(const Arg *arg);
void wakeup_monitor(const Arg *arg);
void sleep_toggle_monitor(const Arg *arg);
void scroller_stack(const Arg *arg);
void toggle_all_floating(const Arg *arg);
void dwindle_toggle_split_direction(const Arg *arg);
void dwindle_split_horizontal(const Arg *arg);
void dwindle_split_vertical(const Arg *arg);
void dwindle_toggle_current_split(const Arg *arg);
void focus_by_id(const Arg *arg);

void toggle_special_tag(const Arg *arg);
void tag_special_tag(const Arg *arg);
void toggle_special_tag_mon(Monitor *m);
void tag_special_silent(const Arg *arg);
#endif
