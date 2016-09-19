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

#include <stdio.h>
#include "unklog.h"

static int  exec_start(struct output *);
static int  exec_stop(struct output *);
static int  exec_payload(struct output *, const char *, const char *, size_t);

int
exec_start(struct output *out)
{
    FILE        *stream;

    log_trace("exec_start: enter");
    if ((stream = popen(out->cmdline, "we")) == NULL)
        log_sys_fatal("exec_start: cannot open stream");
    (void)snprintf(out->name, sizeof(out->name), "exec");
    out->state = stream;
    log_trace("exec_start: success");
    return 0;
}

int
exec_payload(struct output *out, const char *type, const char *buf, size_t len)
{

    FILE    *stream = out->state;

    if (stream == NULL) {
        if ((stream = popen(out->cmdline, "we")) == NULL)
            log_sys_fatal("exec_start: cannot open stream");
    }

    log_trace("exec_payload: enter");
    if (fprintf(stream, "%s\n", buf) < 0) {
        (void)pclose(stream);
        out->state = NULL;
    }
    log_trace("exec_payload: success");
    return 0;
}

int
exec_stop(struct output *out)
{
    FILE    *stream = out->state;

    log_trace("exec_stop: enter");
    if (stream == NULL) {
        log_trace("es_stop: success");
        return 0;
    }
    log_trace("exec_stop: success");
    (void)pclose(stream);
    return 0;
}

struct output_impl exec_output = {
    exec_start,
    exec_stop,
    exec_payload
};
