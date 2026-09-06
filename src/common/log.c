#define _GNU_SOURCE
#include "mango/common/log.h"
#include "mango/common/util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void mango_log_init(enum wlr_log_importance verbosity,
					wlr_log_func_t callback) {
	wlr_log_init(verbosity, callback);
}

void mango_error_impl(bool log, enum wlr_log_importance verbosity,
					  const char *file, int line, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	if (!log) {
		if (verbosity == WLR_ERROR) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[0m ");
			vfprintf(stderr, fmt, args);
		} else if (verbosity == WLR_INFO) {
			vfprintf(stderr, fmt, args);
		} else if (verbosity == WLR_DEBUG) {
			fprintf(stderr, "\033[1;32m[DEBUG]:\033[0m ");
			vfprintf(stderr, fmt, args);
		}
	} else {
		char *prefixed = string_printf("[%s:%d] %s", file, line, fmt);
		if (prefixed) {
			_wlr_vlog(verbosity, prefixed, args);
			free(prefixed);
		} else {
			_wlr_vlog(verbosity, fmt, args);
		}
	}
	va_end(args);
}
