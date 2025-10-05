/**
 *  @file logging.c
 *
 *  @brief Handle Errors and logging for the emulator
 *
 *  @author E Warren (ethanwarren768@gmail.com)
 *  @date 2025-10-04
 */

#include "logging.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *logf = NULL;
static FILE *errf = NULL;
static LogMode curr_mode = LOG_TO_CONSOLE;

void init_log(void)
{
	if (logf == NULL) {
		logf = stdout;
	}
	if (errf == NULL) {
		errf = stderr;
	}
}

void _msg_format(char *buffer, const char *msg, va_list args)
{
	vsnprintf(buffer, MAX_MSG_LEN, msg, args);
}

void abort_e(const char *msg, ...)
{

	char buff[MAX_MSG_LEN];
	va_list args;
	va_start(args, msg);
	_msg_format(buff, msg, args);
	va_end(args);

	fprintf(errf, "FATAL: %s\n", buff);
	fflush(errf);
	exit(1);
}

void print_error(const char *msg, ...)
{

	char buff[MAX_MSG_LEN];
	va_list args;
	va_start(args, msg);
	_msg_format(buff, msg, args);
	va_end(args);

	fprintf(errf, "ERROR: %s\n", buff);
	fflush(errf);
}

void print_warning(const char *msg, ...)
{

	char buff[MAX_MSG_LEN];
	va_list args;
	va_start(args, msg);
	_msg_format(buff, msg, args);
	va_end(args);

	fprintf(logf, "WARN:  %s\n", buff);
	fflush(logf);
}

void print_trace(const char *msg, ...)
{

	char buff[MAX_MSG_LEN];
	va_list args;
	va_start(args, msg);
	_msg_format(buff, msg, args);
	va_end(args);

	fprintf(logf, "TRACE:  %s\n", buff);
	fflush(logf);
}

void print_debug(const char *format, ...)
{

	char buff[MAX_MSG_LEN];
	va_list args;
	va_start(args, format);
	_msg_format(buff, format, args);
	va_end(args);

	fprintf(logf, "DEBUG: %s\n", buff);
	fflush(logf);
}

int set_logstream(LogMode mode)
{
	if (curr_mode == mode) {
		return 1;
	}

	close_logstream();

	if (mode == LOG_TO_CONSOLE) {
		logf = stdout;
		errf = stderr;
		curr_mode = mode;
		return 1;
	} else if (mode == LOG_TO_FILE) {
		// Create logs directory if it doesn't exist
		if (system("mkdir -p logs")) {

		}
		// Add timestamp to filename
		time_t now = time(NULL);
		struct tm *t = localtime(&now);
		char filename[128];
		snprintf(filename, sizeof(filename),
				 "logs/nes_%04d%02d%02d_%02d%02d%02d.log",
				 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
				 t->tm_hour, t->tm_min, t->tm_sec);

		logf = fopen(filename, "w");
		if (!logf) {
			// Fallback to console if file opening fails
			logf = stdout;
			errf = stderr;
			return 0;
		}
		errf = logf;
		curr_mode = mode;

		// Write header to log file
		fprintf(logf, "NES Emulator Log - Started at %s", ctime(&now));
		fflush(logf);
		return 1;
	} else {
		return 0;
	}
}

void close_logstream(void)
{
	if (logf && logf != stdout && logf != stderr) {
		fclose(logf);
	}
	logf = NULL;
	errf = NULL;
}
