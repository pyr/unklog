/*
 * Copyright (c) 2016 Pierre-Yves Ritschard <pyr@spootnik.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/syslog.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef LOG_TRACE
#define LOG_TRACE 10
#endif

#ifndef LOG_FATAL
#define LOG_FATAL LOG_CRIT
#endif

void log_init(int, const char *);

void log_trace(const char *, ...);
void log_debug(const char *, ...);
void log_info(const char *, ...);
void log_warn(const char *, ...);
void log_error(const char *, ...);
void log_fatal(const char *, ...);

void log_sys_warn(const char *, ...);
void log_sys_error(const char *, ...);
void log_sys_fatal(const char *, ...);

void log_vprint(int, int, const char *, va_list);
void log_print(int, int, const char *, ...);

static char *log_code(int);
static int   log_level = LOG_DEBUG;
static FILE *stream = NULL;

void
log_close()
{
    if ((stream == NULL) ||
        (stream == stdout) ||
        (stream == stderr))
        return;

    fflush(stream);
    fclose(stream);
    stream = stderr;
}

void
log_init(int n_level, const char *path)
{

    log_close();
    if ((n_level < LOG_EMERG) || (n_level > LOG_TRACE)) {
        log_fatal("invalid log level");
    }
    log_level = n_level;
    if ((path == NULL) ||  (strcasecmp(path, "stderr") == 0)) {
        stream = stderr;
    } else if (strcasecmp(path, "stdout") == 0) {
        stream = stdout;
    } else if ((stream = fopen(path, "a")) == NULL) {
        stream = stderr;
        log_fatal("log_init: cannot open log");
    }
}

char *
log_code(int level) {
    switch (level) {
    case LOG_TRACE:
        return "trace";
    case LOG_DEBUG:
        return "debug";
    case LOG_NOTICE:
        return "notice";
    case LOG_INFO:
        return "info";
    case LOG_WARNING:
        return "warning";
    case LOG_ERR:
        return "error";
    case LOG_FATAL:
        return "fatal";
    case LOG_EMERG:
        return "emerg";
    }
    return "unknown";
}

void
log_vprint(int level, int sys, const char *fmt, va_list ap)
{
    int      res;
    char    *nfmt;
    char    *code;

    if (level > log_level)
        return;

    code = log_code(level);

    if (sys) {
        res = asprintf(&nfmt, "[%s] %s: %s\n",
                       code,
                       (fmt == NULL) ? "errno" : fmt,
                       strerror(errno));
    } else {
        res = asprintf(&nfmt, "[%s] %s\n", code, fmt);
    }

    if (stream == NULL)
        stream = stderr;

    if (res == -1) {
        fprintf(stream, "[%s] ", code);
        vfprintf(stream, fmt, ap);
        fprintf(stream, ": %s\n", strerror(errno));
    } else {
        vfprintf(stream, nfmt, ap);
        free(nfmt);
    }
    fflush(stream);
}

void
log_print(int level, int sys, const char *fmt, ...)
{
    va_list ap;

    if (level > log_level)
        return;

    va_start(ap, fmt);
    log_vprint(level, sys, fmt, ap);
    va_end(ap);
}


void
log_sys_warn(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_vprint(LOG_WARNING, 1, fmt, ap);
    va_end(ap);
}

void
log_sys_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_vprint(LOG_ERR, 1, fmt, ap);
    va_end(ap);
}

void
log_sys_fatal(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_vprint(LOG_FATAL, 1, fmt, ap);
    va_end(ap);
    exit(1);
}

void
log_trace(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_vprint(LOG_TRACE, 0, fmt, ap);
    va_end(ap);
}

void
log_debug(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_vprint(LOG_DEBUG, 0, fmt, ap);
    va_end(ap);
}

void
log_info(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_vprint(LOG_INFO, 0, fmt, ap);
    va_end(ap);
}

void
log_warn(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_vprint(LOG_WARNING, 0, fmt, ap);
    va_end(ap);
}

void
log_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_vprint(LOG_ERR, 0, fmt, ap);
    va_end(ap);
}

void
log_fatal(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_vprint(LOG_FATAL, 0, fmt, ap);
    va_end(ap);
    exit(1);
}
