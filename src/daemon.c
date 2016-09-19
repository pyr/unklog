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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/wait.h>

#include <bsd/string.h>
#include <bsd/stdlib.h>

#include <unistd.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "unklog.h"

static void usage(void);
static void daemon_signal(uv_signal_t *, int);
static void daemon_run(struct unklog *);
static void daemon_init(struct unklog *);
static void daemon_shutdown(struct unklog *);

void
usage(void)
{
    fprintf(stderr,
            "usage: unklog [-fn] [-c <config>] [-l <logfile>] [-d <level>]\n");
    exit(1);
}

void
daemon_shutdown(struct unklog *uk)
{
    log_warn("daemon_shutdown: stopping all inputs");
    input_stop(uk);

    log_warn("daemon_shutdown: stopping all outputs");
    output_stop(uk);

    log_debug("daemon_shutdown: stopping event loop");
    uv_stop(&uk->loop);
    _exit(0);
}

void
daemon_signal(uv_signal_t *sig, int signo)
{
    struct unklog    *uk = sig->data;

    daemon_shutdown(uk);
}

void
daemon_run(struct unklog *uk)
{
    uk->tick.data = uk;
    uk->sighup.data = uk;
    uk->sigterm.data = uk;
    uk->sigint.data = uk;
    uv_timer_start(&uk->tick, metric_flush, 0, 5000);
    uv_signal_start(&uk->sighup, daemon_signal, SIGHUP);
    uv_signal_start(&uk->sigterm, daemon_signal, SIGTERM);
    uv_signal_start(&uk->sigint, daemon_signal, SIGINT);
    if (uk->mrun)
        metric_start(uk);
    uv_run(&uk->loop, UV_RUN_DEFAULT);
}

void
daemon_init(struct unklog *uk)
{
    bzero(uk, sizeof (*uk));
    metric_counter_init(&uk->count);
    uk->uptime = time(NULL);
    uv_mutex_init(&uk->mlock);

    TAILQ_INIT(&uk->inputs);
    TAILQ_INIT(&uk->outputs);

    log_init(LOG_INFO, NULL);
    uv_loop_init(&uk->loop);
    uv_timer_init(&uk->loop, &uk->tick);
    uv_signal_init(&uk->loop, &uk->sighup);
    uv_signal_init(&uk->loop, &uk->sigterm);
    uv_signal_init(&uk->loop, &uk->sigint);
}

int
main(int argc, char *argv[])
{
    int              c;
    int              level;
    int              daemonize;
    int              validate_config;
    char            *logpath;
    char            *cfgpath;
    struct unklog    uk;

    level = LOG_INFO;
    logpath = NULL;
    cfgpath = NULL;
    daemonize = 1;
    validate_config = 0;

    daemon_init(&uk);

    while ((c = getopt(argc, argv, "c:d:fl:n")) != -1) {
        switch (c) {
        case 'c':
            if (cfgpath != NULL)
                free(cfgpath);
            cfgpath = strdup(optarg);
            if (cfgpath == NULL)
                log_sys_fatal("main: out of memory");
            break;
        case 'd':
            if (strcasecmp(optarg, "trace") == 0) {
                level = LOG_TRACE;
            } else if (strcasecmp(optarg, "debug") == 0) {
                level = LOG_DEBUG;
            } else if (strcasecmp(optarg, "info") == 0) {
                level = LOG_INFO;
            } else if (strcasecmp(optarg, "warn") == 0) {
                level = LOG_WARNING;
            } else if (strcasecmp(optarg, "error") == 0) {
                level = LOG_ERR;
            } else {
                log_fatal("main: invalid log level");
            }
            uk.cli_flags |= CLI_LOG;
            break;
        case 'f':
            daemonize = 0;
            break;
        case 'l':
            if (logpath != NULL)
                free(logpath);
            logpath = strdup(optarg);
            if (logpath == NULL)
                log_sys_fatal("main: out of memory");
            uk.cli_flags |= CLI_LOG;
            break;
        case 'n':
            validate_config = 1;
            break;
        default:
            usage();
        }
    }

    argc -= optind;
    argv += optind;

    if (argc)
        usage();

    if (uk.cli_flags & CLI_LOG)
        log_init(level, logpath);

    if (logpath != NULL)
        free(logpath);

    log_info("main: parsing configuration: %s", cfgpath);

    config_parse(&uk, cfgpath);
    if (cfgpath != NULL)
        free(cfgpath);

    if (validate_config) {
        printf("configuration is valid\n");
        exit(0);
    }

    if (daemonize) {
        if (daemon(1, 1) == -1) {
            log_sys_fatal("main: failed to daemonize");
        }
    }

    log_info("main: starting workload");

    output_start(&uk);
    input_start(&uk);
    daemon_run(&uk);

    log_info("main: daemon has finished, exiting");
    daemon_shutdown(&uk);
    log_info("main: shutdown complete, bye");
    return 0;
}
