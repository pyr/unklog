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
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>
#include <stdlib.h>
#include <bsd/string.h>
#include "unklog.h"

#define MAX_ARGS 10

static void     config_apply(struct unklog *, char *, int, const char *[]);
static void     config_apply_stats(struct unklog *, char * , int, const char *[]);
static void     config_apply_input(struct unklog *, char * , int, const char *[]);
static void     config_apply_output(struct unklog *, char *, int, const char *[]);
static void     config_apply_log(struct unklog *, char *, int, const char *[]);
static void     config_apply_unknown(struct unklog *, char *, int, const char *[]);
static void     config_parse_line(struct unklog *, char *);

void
config_apply_stats(struct unklog *uk, char *cmdline, int argc, const char *argv[])
{
    uk->mport = 6789;
    (void)strlcpy(uk->maddr, "localhost", sizeof(uk->maddr));
    uk->mrun = 1;

    if (argc >= 1) {
        (void)strlcpy(uk->maddr, argv[0], sizeof(uk->maddr));
    }

    if (argc >= 2) {
        uk->mport = atoi(argv[1]);
    }
    log_info("config_apply_stats: setting up statistics on %s:%d", uk->maddr, uk->mport);
}

void
config_apply_input(struct unklog *uk, char *cmdline, int argc, const char *argv[])
{
    struct input    *in;
    struct option   *opt;
    int              i;
    size_t           off;

    if ((in = calloc(1, sizeof(*in))) == NULL)
        log_sys_fatal("config_apply_input: out of memory");
    in->cmdline = cmdline;

    if (strcasecmp(argv[0], "kafka") == 0) {
        in->impl = &kafka_input;
        (void)strlcpy(in->name, "kafka", sizeof(in->name));
    } else {
        log_fatal("config_apply_input: unsupported input method: %s", argv[0]);
    }

    TAILQ_INIT(&in->options);
    for (i = 1; i < argc; i++) {
        if ((opt = calloc(1, sizeof(*opt))) == NULL)
            log_sys_fatal("config_apply_input: out of memory");
        off = strcspn(argv[i], "=");
        off++;
        (void)strlcpy(opt->key, argv[i], off);
        (void)strlcpy(opt->val, argv[i] + off, sizeof(opt->val));
        log_trace("config_apply_input: %s => %s", opt->key, opt->val);
        TAILQ_INSERT_TAIL(&in->options, opt, entry);
    }
    TAILQ_INSERT_TAIL(&uk->inputs, in, entry);
}

void
config_apply_output(struct unklog *uk, char *cmdline, int argc, const char *argv[])
{
    struct output   *out;
    struct option   *opt;
    int              i;
    size_t           off;

    if ((out = calloc(1, sizeof(*out))) == NULL)
        log_sys_fatal("config_apply_output: out of memory");
    out->cmdline = cmdline;
    log_debug("config_apply_output: commandline: %s", cmdline);

    if (strcasecmp(argv[0], "elasticsearch") == 0) {
        out->impl = &es_output;
    } else if (strcasecmp(argv[0], "exec") == 0) {
        out->impl = &exec_output;
    } else {
        log_fatal("config_apply_output: unsupported output method: %s", argv[0]);
    }
    TAILQ_INIT(&out->options);
    for (i = 1; i < argc; i++) {
        if ((opt = calloc(1, sizeof(*opt))) == NULL)
            log_sys_fatal("config_apply_output: out of memory");
        off = strcspn(argv[i], "=");
        off++;
        (void)strlcpy(opt->key, argv[i], off);
        (void)strlcpy(opt->val, argv[i] + off, sizeof(opt->val));
        TAILQ_INSERT_TAIL(&out->options, opt, entry);
    }
    TAILQ_INSERT_TAIL(&uk->outputs, out, entry);
}

void
config_apply_log(struct unklog *uk, char *cmdline, int argc, const char *argv[])
{
    int level = LOG_INFO;

    if (uk->cli_flags & CLI_LOG)
        return;

    if (strcasecmp("trace", argv[0]) == 0) {
        level = LOG_TRACE;
    } else if (strcasecmp("debug", argv[0]) == 0) {
        level = LOG_DEBUG;
    } else if (strcasecmp("info", argv[0]) == 0) {
        level = LOG_INFO;

    } else if (strcasecmp("warn", argv[0]) == 0) {
        level = LOG_WARNING;
    } else if (strcasecmp("error", argv[0]) == 0) {
        level = LOG_WARNING;
    } else {
        log_fatal("config_apply_log: invalid log level: %s", argv[0]);
    }
    log_init(level, argv[1]);
}

void
config_apply_unknown(struct unklog *uk, char *cmdline, int argc, const char *argv[])
{
    log_fatal("config_apply_unknown: unknown directive");
}


void
config_apply(struct unklog *uk, char *cmdline, int argc, const char *argv[])
{
    int          i;
    struct {
        char    *opcode;
        void    (*apply)(struct unklog *, char *, int, const char *[]);
        int      argcount;
    }            commands[] = {
        { "input",      config_apply_input,     1 },
        { "log",        config_apply_log,       2 },
        { "output",     config_apply_output,    1 },
        { "stats",      config_apply_stats,     0 },
        { NULL,         config_apply_unknown,   0 }
    };

    for (i = 0;
         commands[i].opcode && strcasecmp(commands[i].opcode, argv[0]) != 0;
         i++)
        ;

    argc--;
    argv++;

    if (argc < commands[i].argcount)
        log_fatal("config_apply: missing arguments for %s",
                  commands[i].opcode);

    commands[i].apply(uk, cmdline, argc, argv);
}

void
config_parse_line(struct unklog *uk, char *line)
{
    size_t       len;
    int          argc;
    char        *end;
    char        *cmdline = NULL;
    const char  *argv[MAX_ARGS];

    line += strspn(line, " \t\n");
    line[strcspn(line, "#")] = '\0';
    end = line + strlen(line);

    if (*line == '\0') {
        return;
    }

    argc = 0;
    bzero(argv, sizeof(argv));
    do {
        if (argc >= MAX_ARGS)
            log_fatal("config_parse_line: too many arguments");
        argv[argc] = line;
        if (argc == 1) {
            cmdline = strdup(line);
            if (cmdline == NULL) {
                log_sys_fatal("config_parse_line: out of memory");
            }
        }
        len = strcspn(line, " \t\r");
        line[len] = '\0';
        line += len + 1;
        line += strspn(line, " \t\r");
        argc++;
        if (line >= end)
            break;
    } while (1);

    config_apply(uk, cmdline, argc, argv);
}

void
config_parse(struct unklog *uk, const char *path)
{
    size_t           len;
    ssize_t          br;
    FILE            *fd;
    char            *line = NULL;
    struct input    *in;
    struct output   *out;

    if ((fd = fopen((path==NULL)?DEFAULT_CONFIG:path, "r")) == NULL)
        log_sys_fatal("config_parse: cannot open config");

    while ((br = getline(&line, &len, fd)) != -1) {
        line[strcspn(line, "\r\n")] = '\0';
        config_parse_line(uk, line);
        len = 0;
        free(line);
        line = NULL;
    }

    uk->incount = 0;
    TAILQ_FOREACH(in, &uk->inputs, entry) {
        uk->incount++;
    }
    uk->outcount = 0;
    TAILQ_FOREACH(out, &uk->outputs, entry) {
        uk->outcount++;
    }
    log_trace("config_parse: parsed config");
}
