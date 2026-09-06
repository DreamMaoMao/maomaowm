#ifndef __COMMON_UTIL_H__
#define __COMMON_UTIL_H__ 1

/* See LICENSE.dwm file for copyright and license details. */
#include <stdint.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

/* Generic helpers shared by every module. */
#define MANGO_MAX(A, B) ((A) > (B) ? (A) : (B))
#define MANGO_MIN(A, B) ((A) < (B) ? (A) : (B))
#define GEZERO(A) ((A) >= 0 ? (A) : 0)

#define LENGTH(X) (sizeof X / sizeof X[0])
#define END(A) ((A) + LENGTH(A))
#define LISTEN(E, L, H) wl_signal_add((E), ((L)->notify = (H), (L)))
#define LISTEN_STATIC(E, H)                                                    \
	do {                                                                       \
		struct wl_listener *_l = ecalloc(1, sizeof(*_l));                      \
		_l->notify = (H);                                                      \
		wl_signal_add((E), _l);                                                \
	} while (0)

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
int32_t fd_set_nonblock(int32_t fd);
int32_t regex_match(const char *pattern_mb, const char *str_mb);
void wl_list_append(struct wl_list *list, struct wl_list *object);
uint32_t get_now_in_ms(void);
uint32_t timespec_to_ms(struct timespec *ts);
char *join_strings(char *arr[], const char *sep);
char *join_strings_with_suffix(char *arr[], const char *suffix,
							   const char *sep);
char *string_printf(const char *fmt, ...);
void wl_list_swap(struct wl_list *l1, struct wl_list *l2);
void wl_list_safe_reinsert_prev(struct wl_list *l1, struct wl_list *l2);
void wl_list_safe_reinsert_next(struct wl_list *l1, struct wl_list *l2);

#endif
