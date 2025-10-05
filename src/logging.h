/**
 *  @file logging.h
 *
 *  @brief Handle Errors and logging for the emulator
 *
 *  @author E Warren (ethanwarren768@gmail.com)
 *  @date 2025-10-04
 */
#ifndef ERROR_H
#define ERROR_H

#include <stdarg.h>
#include <stdio.h>

// LOG_TO_FILE (1) or LOG_TO_CONSOLE (0)
typedef unsigned char LogMode;
#define LOG_TO_FILE (LogMode) 1
#define LOG_TO_CONSOLE (LogMode) 0
#define MAX_MSG_LEN 255


void init_log(void);
/**
 *  Aborts the emulator and print an error message specified by the varg to the
 *  Error stream.
 *
 *  @param[in] msg
 *			the error mesage to abort with
 *	@param[in] ...
 *			a varg list to be formated into msg
 */
void abort_e(const char *msg, ...);

/**
 * Prints an error to the log stream
 *
 * @param[in] msg
 *			the message to be printed
 * @param[in] ...
 *		a varg list to format the msg with
 */
void print_error(const char *msg, ...);


/**
 * Prints an warning to the log stram
 *
 * @param[in] msg
 *			the message to be printed
 * @param[in] ...
 *		a varg list to format the msg with
 */
void print_warning(const char *msg, ...);


/**
 * Prints an info message to the log stream
 *
 * @param[in] format
 *        the message format string
 * @param[in] ...
 *        a varg list to format the msg with
 */
void print_info(const char *format, ...);

/**
 * Prints a debug message to the log stream
 *
 * @param[in] format
 *        the message format string
 * @param[in] ...
 *        a varg list to format the msg with
 */
void print_debug(const char *format, ...);

/**
 * Sets the log stream to specified mode
 *
 * @param[in] mode
 *		the mode the program must use for logging
 * @return 
 *		if the stream was set successfully
 *
 */
int set_logstream(LogMode mode);

/**
 * Closes the logstream, only nessisary if using LOG_TO_FILE mode
 *
 */
void close_logstream(void);
#endif //ERROR_H
