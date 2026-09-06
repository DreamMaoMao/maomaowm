#ifndef __LAYOUT_OVERVIEW_H__
#define __LAYOUT_OVERVIEW_H__ 1

#include "mango/mango.h"

typedef struct {
	float x, y, w, h;
} OvPlacedRect;

typedef struct {
	float x, y;
} OvPoint;

typedef struct {
	Client *c;
	float orig_w;
	float orig_h;
	float area;
} OvLayoutItem;
/* Declarations */
int compare_layout_items(const void *a, const void *b);
bool try_place(OvPlacedRect *placed, int placed_cnt, float w, float h,
			   float gap, float avail_w, float avail_h, OvPlacedRect *out,
			   OvPoint *cands, OvPoint *feas);
void overview_scale(Monitor *m);
// Overview layout: focused window centered (about half screen width), remaining
// windows split on both sides.
void overview_layout_column(Monitor *m, Client **items, int cnt, float x,
							float top, float col_w, float col_h, float gap);
void overview_scale_tab(Monitor *m);
void create_jump_hints(Monitor *m);
void begin_jump_mode(Monitor *m);
void finish_jump_mode(Monitor *m);
void overview(Monitor *m);

#endif
