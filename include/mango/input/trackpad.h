#ifndef __INPUT_TRACKPAD_H__
#define __INPUT_TRACKPAD_H__ 1

#include "mango/common/types.h"
#include <stdbool.h>
#include <stdint.h>

struct wlr_pointer;
struct wl_listener;
struct wlr_pointer_swipe_end_event;

bool pointer_is_trackpad(struct wlr_pointer *pointer);
bool check_trackpad_disabled(struct wlr_pointer *pointer);
bool trackpad_enabled(void);
int32_t pointer_process_swipe_end(struct wlr_pointer_swipe_end_event *event);
void handle_cursor_swipe_begin(struct wl_listener *listener, void *data);
void handle_cursor_swipe_update(struct wl_listener *listener, void *data);
void handle_cursor_swipe_end(struct wl_listener *listener, void *data);
void handle_cursor_pinch_begin(struct wl_listener *listener, void *data);
void handle_cursor_pinch_update(struct wl_listener *listener, void *data);
void handle_cursor_pinch_end(struct wl_listener *listener, void *data);
void handle_cursor_hold_begin(struct wl_listener *listener, void *data);
void handle_cursor_hold_end(struct wl_listener *listener, void *data);

#endif
