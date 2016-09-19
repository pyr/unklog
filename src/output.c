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

#include <stdlib.h>
#include "unklog.h"

static void output_dispose(struct payload *);
static void output_pop(void *);
static void output_create(struct unklog *, struct output *);

void
output_dispose(struct payload *p)
{
    free(p->buf);
    free(p->type);
    free(p);
}

void
output_pop(void *p)
{
    struct output   *out = p;
    struct payload  *payload;
    clock_t          start;

    log_trace("output_pop: enter");
    log_info("output_pop: starting worker thread for output %s", out->name);

    while (out->flags & OUTPUT_RUN) {
        uv_mutex_lock(&out->lock);
        while (STAILQ_EMPTY(&out->payloads) && (out->flags & OUTPUT_RUN)) {
            uv_cond_wait(&out->signal, &out->lock);
        }
        if (!(out->flags & OUTPUT_RUN)) {
            log_info("output_pop: signaled to stop, quitting");
            return;
        }
        payload = STAILQ_FIRST(&out->payloads);
        STAILQ_REMOVE_HEAD(&out->payloads, entry);
        uv_mutex_unlock(&out->lock);

        metric_inc(&out->count);
        start = clock();
        if (out->impl->payload(out, payload->type, payload->buf, payload->len) != 0) {
            metric_inc(&out->errors);
            log_warn("output_pop: could not process payload");
        }
        output_dispose(payload);
        metric_meter(&out->meter, start);
    }
    log_trace("output_pop: leaving");
}

void
output_create(struct unklog *uk, struct output *out)
{
    log_trace("output_create: enter");
    uv_mutex_init(&out->lock);
    STAILQ_INIT(&out->payloads);
    out->impl->start(out);
    out->flags |= OUTPUT_RUN;
    if (uv_thread_create(&out->thread, output_pop, out) != 0)
        log_fatal("output_create: could not start worker for output %s", out->name);
    log_trace("output_create: leave");
}

void
output_stop(struct unklog *uk)
{
    struct output   *out;

    log_trace("output_stop: enter");
    TAILQ_FOREACH(out, &uk->outputs, entry) {
        uv_mutex_lock(&out->lock);
        out->impl->stop(out);
        uv_cond_signal(&out->signal);
        uv_mutex_unlock(&out->lock);
    }
    log_trace("output_stop: leave");
}

void
output_start(struct unklog *uk)
{
    struct output   *out;

    log_trace("output_start: enter");
    TAILQ_FOREACH(out, &uk->outputs, entry) {
        output_create(uk, out);
    }
    log_trace("output_start: leave");
}
