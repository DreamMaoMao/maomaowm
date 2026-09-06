#ifndef __CONFIG_PRESET_H__
#define __CONFIG_PRESET_H__ 1
#include <xkbcommon/xkbcommon.h>

#define MODKEY WLR_MODIFIER_ALT

/* 最大可配置的 tag 数量：决定静态数组大小（Pertag 等），同时受 uint32_t
 * 位宽限制 */
#define tag_num_MAX 31
extern const char *tags[tag_num_MAX];
extern const struct xkb_rule_names xkb_fallback_rules;

#endif
