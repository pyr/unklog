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

#include "unklog.h"

static void input_run(void *);
static void input_create(struct unklog *, struct input *);

void
input_run(void *p)
{
    struct input    *in = p;
    struct unklog   *uk = in->uk;

    log_trace("input_start: enter");
    in->impl->start(in, dispatch_payload, uk);
    log_trace("input_start: leave");
}

void
input_create(struct unklog *uk, struct input *in)
{
    log_trace("input_create: enter");
    in->flags |= INPUT_RUN;
    metric_counter_init(&in->count);
    if (uv_thread_create(&in->thread, input_run, in) != 0)
        log_fatal("input_create: could not start worker for output %s", in->name);
    log_trace("input_create: leave");
}

void
input_stop(struct unklog *uk)
{
    struct input    *in;

    log_trace("input_stop: enter");
    TAILQ_FOREACH(in, &uk->inputs, entry) {
        in->impl->stop(in);
    }
    log_trace("input_stop: leave");
}

void
input_start(struct unklog *uk)
{
    struct input    *in;

    log_trace("input_stop: enter");
    TAILQ_FOREACH(in, &uk->inputs, entry) {
        in->uk = uk;
        input_create(uk, in);
    }
    log_trace("input_stop: leave");
}
