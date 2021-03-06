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
#include <yajl/yajl_tree.h>
#include "unklog.h"

int
dispatch_payload(const char *buf, size_t len, void *p)
{
    struct unklog   *uk = p;
    yajl_val         node;
    yajl_val         type;
    char             ebuf[512];
    const char      *path[] = {"type", NULL};
    struct payload  *payload;
    struct output   *out;

    log_trace("dispatch_payload: enter");
    node = yajl_tree_parse(buf, ebuf, sizeof(ebuf));

    if (node == NULL) {
        log_warn("dispatch_payload: bad message: %s", ebuf);
        return -1;
    }

    metric_inc(&uk->count);
    type = yajl_tree_get(node, path, yajl_t_string);
    if (type == NULL) {
        log_warn("dispatch_payload: no type in message");
        yajl_tree_free(node);
        return -1;
    }
    TAILQ_FOREACH(out, &uk->outputs, entry) {
        if ((payload = calloc(1, sizeof(*payload))) == NULL) {
            log_sys_error("dispatch_payload: out of memory");
            continue;
        }
        if ((payload->buf = strdup(buf)) == NULL) {
            log_sys_error("dispatch_payload: out of memory");
            free(payload);
            continue;
        }
        if ((payload->type = strdup(YAJL_GET_STRING(type))) == NULL) {
            log_sys_error("dispatch_payload: out of memory");
            free(payload->buf);
            free(payload);
            continue;
        }
        payload->len = strlen(buf);
        uv_mutex_lock(&out->lock);
        STAILQ_INSERT_TAIL(&out->payloads, payload, entry);
        uv_cond_signal(&out->signal);
        uv_mutex_unlock(&out->lock);
    }
    log_trace("dispatch_payload: success");
    return 0;
}
