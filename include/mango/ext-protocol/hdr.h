#ifndef __EXT_PROTOCOL_HDR_H__
#define __EXT_PROTOCOL_HDR_H__ 1

#include "mango/mango.h"
#include "mango/config/parse_config.h"
#include <stdint.h>
#include <drm_fourcc.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

extern uint32_t output_formats_8bit[10];
extern uint32_t output_formats_10bit[8];

/* Declarations */

bool output_set_render_format(Monitor *m, uint32_t candidates[], size_t count,
							  struct wlr_output_state *state);
bool output_format_in_candidates(uint32_t format, uint32_t candidates[],
								 size_t count);
enum render_bit_depth bit_depth_from_format(uint32_t render_format);
bool output_supports_hdr(const Monitor *m, const char **reason);
void output_enable_hdr(Monitor *m, struct wlr_output_state *os, bool enabled,
					   bool silent);
void output_state_setup_hdr(Monitor *m, bool silent,
							struct wlr_output_state *state);
/* togglehdr[,on|off|toggle][,<monitor name>|all] -- apply to one output */
bool togglehdr_output(Monitor *target, bool want);
void togglehdr(const Arg *arg);

#endif
