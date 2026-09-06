#include "mango/config/preset.h"

/* Preset tag names (matching the tag_num_MAX configurable tags). */
const char *tags[tag_num_MAX] = {
	"1",  "2",	"3",  "4",	"5",  "6",	"7",  "8",	"9",  "10", "11",
	"12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22",
	"23", "24", "25", "26", "27", "28", "29", "30", "31",
};

const struct xkb_rule_names xkb_fallback_rules = {
	.layout = "us",
	.variant = NULL,
	.model = NULL,
	.rules = NULL,
	.options = NULL,
};
