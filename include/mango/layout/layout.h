#ifndef __LAYOUT_LAYOUT_H__
#define __LAYOUT_LAYOUT_H__ 1

#include "mango/common/types.h"
#include <stdint.h>
#include "mango/layout/horizontal.h"
#include "mango/layout/vertical.h"
#include "mango/layout/scroll.h"
#include "mango/layout/dwindle.h"
#include "mango/layout/overview.h"

/* layout(s) */
typedef struct Layout {
	const char *symbol;
	void (*arrange)(Monitor *);
	const char *name;
	uint32_t id;
} Layout;

extern Layout overviewlayout;

enum {
	TILE,
	SCROLLER,
	GRID,
	MONOCLE,
	DECK,
	CENTER_TILE,
	VERTICAL_SCROLLER,
	VERTICAL_TILE,
	VERTICAL_GRID,
	VERTICAL_DECK,
	RIGHT_TILE,
	DWINDLE,
	FAIR,
	VERTICAL_FAIR,
};

extern Layout layouts[14];

#endif
