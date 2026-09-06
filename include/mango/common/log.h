#ifndef MANGO_LOG_H
#define MANGO_LOG_H

#include <stdbool.h>
#include <wlr/util/log.h>

/**
 * This is just a wrapper around wlr_log_init.
 */
void mango_log_init(enum wlr_log_importance verbosity, wlr_log_func_t callback);

/**
 * Unified error/log output interface (internal implementation, use the
 * mango_error macro instead).
 * @param log if true, the message is sent through the wlroots logging system
 *            (a [file:line] prefix is automatically prepended); if false, the
 *            message is printed directly to stderr for the user.
 * @param verbosity the importance of the message
 * @param file source file name (automatically passed by the mango_error macro)
 * @param line source line number (automatically passed by the mango_error
 * macro)
 * @param fmt the format string, works like printf
 */
void mango_error_impl(bool log, enum wlr_log_importance verbosity,
					  const char *file, int line, const char *fmt, ...)
	__attribute__((format(printf, 5, 6)));

/** Prepend the source file/line info to log messages; usage is the same as the
 * original mango_error. */
#define mango_error(log, verbosity, fmt, ...)                                  \
	mango_error_impl((log), (verbosity), __FILE__, __LINE__, (fmt),            \
					 ##__VA_ARGS__)

#endif
