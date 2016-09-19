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

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <bsd/string.h>
#include "unklog.h"
#define MBUF_MAX 128

void
metric_connect(uv_stream_t *server, int status)
{
    int              res;
    struct unklog   *uk = server->data;
    uv_tcp_t         client;
    uv_write_t       wreq;

    if (status == -1) {
        log_warn("metric_connect: connection error");
        return;
    }

    bzero(&client, sizeof(client));
    uv_tcp_init(&uk->loop, &client);

    res = uv_accept(server, (uv_stream_t *)&client);
    if (res != 0) {
        (void)uv_close((uv_handle_t *)&client, NULL);
        return;
    }
    bzero(&wreq, sizeof(wreq));
    pthread_mutex_lock(&uk->mlock);
    (void)uv_write(&wreq, (uv_stream_t *)&client, uk->mbufs, uk->mcount, NULL);
    pthread_mutex_unlock(&uk->mlock);
    (void)uv_close((uv_handle_t *)&client, NULL);
}

void
metric_start(struct unklog *uk)
{
    struct sockaddr_in sin;
    int res;

    if (uk->mrun == 0) {
        return;
    }
    /*
     * The callback have been set-up, now we can set-up our listener.
     */
    (void)uv_ip4_addr(uk->maddr, uk->mport, &sin);
    uv_tcp_init(&uk->loop, &uk->proxy);
    uv_tcp_bind(&uk->proxy, (struct sockaddr *)&sin, 0);

    uk->proxy.data = uk;
    res = uv_listen((uv_stream_t *)&uk->proxy, 128, metric_connect);
    if (res != 0) {
        log_fatal("metric_start: failed to listen");
    }
}


void
metric_inc(struct metric_counter *m)
{
    m->metric++;
}

void
metric_meter(struct metric_meter *m, clock_t init)
{
    uint64_t duration = (clock() - init) / (CLOCKS_PER_SEC / 1000);

    if (duration > m->max)
        m->max = duration;

    if (duration <= 2.0) {
        m->slots[METRIC_2MS]++;
    } else if (duration > 2.0 && duration <= 5.0) {
        m->slots[METRIC_5MS]++;
    } else if (duration > 5.0 && duration <= 10.0) {
        m->slots[METRIC_10MS]++;
    } else if (duration > 10.0 && duration <= 20.0) {
        m->slots[METRIC_20MS]++;
    } else if (duration > 20.0 && duration <= 50.0) {
        m->slots[METRIC_50MS]++;
    } else if (duration > 50.0 && duration <= 100.0) {
        m->slots[METRIC_100MS]++;
    } else if (duration > 100.0 && duration <= 200.0) {
        m->slots[METRIC_200MS]++;
    } else if (duration > 200.0 && duration <= 500.0) {
        m->slots[METRIC_500MS]++;
    } else if (duration > 500.0 && duration <= 1000.0) {
        m->slots[METRIC_1S]++;
    } else if (duration > 1000.0 && duration <= 2000.0) {
        m->slots[METRIC_2S]++;
    } else if (duration > 2000.0 && duration <= 5000.0) {
        m->slots[METRIC_5S]++;
    } else if (duration > 5000.0 && duration <= 10000.0) {
        m->slots[METRIC_10S]++;
    } else {
        m->slots[METRIC_SLOW]++;
    }
}

void
metric_counter_init(struct metric_counter *m)
{
    m->metric = 0;
}

void
metric_meter_init(struct metric_meter *m)
{
    int i;
    m->max = 0;
    for (i = 0; i < METRIC_SLOTMAX; i++) {
        m->slots[i] = 0;
    }
}

void
metric_format(struct unklog *uk, uv_buf_t *buf, struct metric_counter *m)
{
    char    *s;

    asprintf(&s, "global.uptime %ld\nglobal.count %ld\n",
             uk->uptime, m->metric);
    buf->base = s;
    buf->len = strlen(s);
}

void
metric_format_in(uv_buf_t *buf, char *pfx, struct metric_counter *m)
{
    char    *s;

    asprintf(&s, "in.%s.count %ld\n", pfx, m->metric);
    buf->base = s;
    buf->len = strlen(s);
}

void
metric_format_out(struct unklog *uk, uv_buf_t *buf, char *pfx,
                  struct metric_counter *m, struct metric_counter *err,
                  struct metric_meter *mtr)
{
    int      i;
    char     meters[512];
    char     numbuf[16];
    char    *s;
    uint64_t    lag;

    bzero(meters, sizeof(meters));
    for (i = 0; i < METRIC_SLOTMAX; i++) {
        bzero(numbuf, sizeof(numbuf));
        snprintf(numbuf, sizeof(numbuf), " %d", mtr->slots[i]);
        (void)strlcat(meters, numbuf, sizeof(meters));
    }
    lag = uk->count.metric - m->metric;
    bzero(numbuf, sizeof(numbuf));
    snprintf(numbuf, sizeof(numbuf), " max:%ld", mtr->max);
    (void)strlcat(meters, numbuf, sizeof(meters));
    asprintf(&s, "out.%s.count %ld\nout.%s.errs %ld\nout.%s.lag %ld\nout.%s.meter%s\n",
             pfx,  m->metric, pfx, err->metric, pfx, lag, pfx, meters);
    buf->base = s;
    buf->len = strlen(s);
}

void
metric_flush(uv_timer_t *t)
{
    int              i;
    struct unklog   *uk = t->data;
    struct input    *in;
    struct output   *out;

    uv_mutex_lock(&uk->mlock);
    if (uk->mbufs != NULL) {
        for (i = 0; i < uk->mcount; i++)
            free(uk->mbufs[i].base);
        free(uk->mbufs);
        uk->mbufs = NULL;
        uk->mcount = 0;
    }
    uk->mcount = 1 + uk->incount + uk->outcount;
    if ((uk->mbufs = calloc(uk->mcount, sizeof(*uk->mbufs))) == NULL)
        log_sys_fatal("metric_flush: out of memory");

    metric_format(uk, &uk->mbufs[0], &uk->count);

    i = 1;
    TAILQ_FOREACH(in, &uk->inputs, entry) {
        metric_format_in(&uk->mbufs[i++], in->name, &in->count);
    }

    TAILQ_FOREACH(out, &uk->outputs, entry) {
        metric_format_out(uk, &uk->mbufs[i++], out->name, &out->count, &out->errors, &out->meter);
    }
    uv_mutex_unlock(&uk->mlock);
}
