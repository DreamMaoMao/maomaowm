#ifndef __CONFIG_PRESET_H__
#define __CONFIG_PRESET_H__ 1
#include <xkbcommon/xkbcommon.h>

#define MODKEY WLR_MODIFIER_ALT

/*
 * Maximum configurable tag count: determines static array sizes (e.g. Pertag)
 * and is limited by the uint32_t bit width.
 */
#define tag_num_MAX 31
extern const char *tags[tag_num_MAX];
extern const struct xkb_rule_names xkb_fallback_rules;

#endif
