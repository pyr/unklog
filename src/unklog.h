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

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#define OUTPUT_MAX  64
#define INPUT_MAX   64
#define KEY_MAX     64
#define VAL_MAX     512
#define URL_MAX     512
#define METRIC_MAX  32
#define SLOTS_MAX   13

#define DEFAULT_CONFIG "/etc/unklog.conf"

#include <sys/queue.h>
#include <sys/syslog.h>
#include <limits.h>
#include <uv.h>
#include <time.h>

#ifndef LOG_TRACE
#define LOG_TRACE 10
#endif

#ifndef LOG_FATAL
#define LOG_FATAL LOG_CRIT
#endif

struct output;
struct input;

typedef int     (*output_start_t)(struct output *);
typedef int     (*output_stop_t)(struct output *);
typedef int     (*output_payload_t)(struct output *, const char *, const char *, size_t);

typedef int     (*input_dispatch_t)(const char *, size_t, void *);
typedef int     (*input_start_t)(struct input *, input_dispatch_t, void *);
typedef int     (*input_stop_t)(struct input *);

#define METRIC_2MS      0
#define METRIC_5MS      1
#define METRIC_10MS     2
#define METRIC_20MS     3
#define METRIC_50MS     4
#define METRIC_100MS    5
#define METRIC_200MS    6
#define METRIC_500MS    7
#define METRIC_1S       8
#define METRIC_2S       9
#define METRIC_5S       10
#define METRIC_10S      11
#define METRIC_SLOW     12
#define METRIC_SLOTMAX  13

struct metric_counter {
    uint64_t            metric;
};

struct metric_meter {
    uint64_t            max;
    uint32_t            slots[SLOTS_MAX];
};

struct option {
    TAILQ_ENTRY(option) entry;
    char                key[KEY_MAX];
    char                val[VAL_MAX];
};
TAILQ_HEAD(option_list, option);

struct payload {
    STAILQ_ENTRY(payload)    entry;
    char                    *buf;
    char                    *type;
    size_t                   len;
};
STAILQ_HEAD(payload_list, payload);

struct output_impl {
    output_start_t      start;
    output_stop_t       stop;
    output_payload_t    payload;
};

struct input_impl {
    input_start_t   start;
    input_stop_t    stop;
};

struct input {
    TAILQ_ENTRY(input)       entry;
#define INPUT_RUN            0x01
    uint8_t                  flags;
    struct unklog           *uk;
    char                    *cmdline;
    uv_thread_t              thread;
    char                     name[INPUT_MAX];
    void                    *state;
    input_dispatch_t         dispatch;
    void                    *dispatch_state;
    struct input_impl       *impl;
    struct option_list       options;
    struct metric_counter    count;
};
TAILQ_HEAD(input_list, input);

struct output {
    TAILQ_ENTRY(output)      entry;
#define OUTPUT_RUN           0x01
    uint8_t                  flags;
    uv_thread_t              thread;
    char                     name[OUTPUT_MAX];
    char                    *cmdline;
    void                    *state;
    struct output_impl      *impl;
    struct option_list       options;
    struct payload_list      payloads;
    uv_mutex_t               lock;
    uv_cond_t                signal;
    struct metric_counter    count;
    struct metric_counter    errors;
    struct metric_meter      meter;
};
TAILQ_HEAD(output_list, output);

struct unklog {
#define CLI_LOG              0x01
    uint8_t                  cli_flags;
    struct input_list        inputs;
    struct output_list       outputs;
    uv_timer_t               tick;
    uv_signal_t              sigint;
    uv_signal_t              sighup;
    uv_signal_t              sigterm;
    uv_loop_t                loop;
    size_t                   incount;
    size_t                   outcount;
    struct metric_counter    count;
    time_t                   uptime;
    uv_tcp_t                 proxy;
    uv_buf_t                *mbufs;
    size_t                   mcount;
    int                      mrun;
    char                     maddr[URL_MAX];
    int                      mport;
    uv_mutex_t               mlock;
};

/* input_kafka.c */
extern struct input_impl kafka_input;

/* output_es.c */
extern struct output_impl es_output;

/* output_exec.c */
extern struct output_impl exec_output;

/* input.c */
void    input_start(struct unklog *);
void    input_stop(struct unklog *);

/* output.c */
void    output_start(struct unklog *);
void    output_stop(struct unklog *);

/* dispatch.c */
int dispatch_payload(const char *, size_t, void *);

/* config.c */
void    config_parse(struct unklog *, const char *);

/* metric.c */
void    metric_counter_init(struct metric_counter *);
void    metric_meter_init(struct metric_meter *);
void    metric_inc(struct metric_counter *);
void    metric_meter(struct metric_meter *, clock_t);
void    metric_flush(uv_timer_t *);
void    metric_start(struct unklog *);

/* log.c */
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
void log_print(int, int, const char *, ...);
