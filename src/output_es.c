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
#include <time.h>
#include <bsd/string.h>
#include <curl/curl.h>
#include "unklog.h"

static void es_curl_error(const char *, const char *, CURLcode, const char *, int);
static size_t   es_write(void *, size_t, size_t, void *);
static int  es_start(struct output *);
static int  es_stop(struct output *);
static int  es_payload(struct output *, const char *, const char *, size_t);

struct es_state {
    CURL        *curl;
    char         url[URL_MAX];
    char         ebuf[CURL_ERROR_SIZE];
    struct tm    stamp;
    char         daybuf[9];
    int          verbose;
};

void
es_curl_error(const char *ctx, const char *op, CURLcode code, const char *ebuf, int die)
{
    if (ebuf != NULL && strlen(ebuf)) {
        if (die) {
            log_fatal("%s: curl error during %s (%ld): %s",
                      ctx, op, code, ebuf);
        } else {
            log_error("%s: curl error during %s (%ld): %s",
                      ctx, op, code, ebuf);
        }
    } else {
        if (die) {
            log_fatal("%s: curl error during %s (%ld)",
                      ctx, op, code);
        } else {
            log_error("%s: curl error during %s (%ld)",
                      ctx, op, code);
        }
    }
}

size_t
es_write(void *contents, size_t sz, size_t nmemb, void *p)
{
    return sz * nmemb;
}

int
es_start(struct output *out)
{
    struct es_state     *es;
    struct option       *opt;
    time_t               now;
    CURLcode             res;

    log_trace("es_start: enter");
    if ((es = calloc(1, sizeof(*es))) == NULL)
        log_sys_fatal("es_start: out of memory");

    out->state = es;

    TAILQ_FOREACH(opt, &out->options, entry) {
        if (strcasecmp(opt->key, "url") == 0) {
            (void)strlcpy(es->url, opt->val, sizeof(es->url));
            log_info("es_start: using url: %s", es->url);
        } else if (strcasecmp(opt->key, "verbose") == 0) {
            es->verbose = 1;
            log_info("es_start: setting verbose mode on");
        } else {
            log_fatal("es_config: unknown option: %s", opt->key);
        }
    }
    if (strlen(es->url) == 0) {
        log_fatal("es_config: need url to connect to");
    }
    if (strlen(out->name) == 0) {
        (void)strlcpy(out->name, "es", sizeof(out->name));
    }

    if ((es->curl = curl_easy_init()) == NULL)
        log_fatal("es_config: cannot create curl handle");

    if ((res = curl_easy_setopt(es->curl, CURLOPT_ERRORBUFFER, es->ebuf)) != CURLE_OK) {
        es_curl_error("es_config", "errobuf", res, NULL, 1);
    }
    if ((res = curl_easy_setopt(es->curl, CURLOPT_VERBOSE, es->verbose?1L:0L)) != CURLE_OK) {
        es_curl_error("es_config", "verbose", res, es->ebuf, 1);
    }
    if ((res = curl_easy_setopt(es->curl, CURLOPT_POST, 1L)) != CURLE_OK) {
        es_curl_error("es_config", "post", res, es->ebuf, 1);
    }
    if ((res = curl_easy_setopt(es->curl, CURLOPT_WRITEFUNCTION, es_write)) != CURLE_OK) {
        es_curl_error("es_config", "writefn", res, es->ebuf, 1);
    }
    if ((res = curl_easy_setopt(es->curl, CURLOPT_TCP_KEEPALIVE, 1L)) != CURLE_OK) {
        es_curl_error("es_config", "keepalive", res, es->ebuf, 1);
    }
    if ((res = curl_easy_setopt(es->curl, CURLOPT_TCP_KEEPIDLE, 300L)) != CURLE_OK) {
        es_curl_error("es_config", "keepidle", res, es->ebuf, 1);
    }
    if ((res = curl_easy_setopt(es->curl, CURLOPT_TCP_KEEPINTVL, 60L)) != CURLE_OK) {
        es_curl_error("es_config", "keepinterval", res, es->ebuf, 1);
    }
    now = time(NULL);
    (void)gmtime_r(&now, &es->stamp);
    strftime(es->daybuf, sizeof(es->daybuf), "%Y%m%d", &es->stamp);
    out->state = es;
    log_trace("es_start: success");
    return 0;
}

int
es_payload(struct output *out, const char *type, const char *buf, size_t len)
{
    struct es_state     *es = out->state;
    CURLcode             res;
    char                 url[URL_MAX];
    time_t               now;
    struct tm            stamp;

    log_trace("es_payload: enter");
    bzero(es->ebuf, sizeof(es->ebuf));
    if ((res = curl_easy_setopt(es->curl, CURLOPT_VERBOSE, es->verbose?1L:0L)) != CURLE_OK) {
        es_curl_error("es_config", "verbose", res, es->ebuf, 1);
    }
    if ((res = curl_easy_setopt(es->curl, CURLOPT_POSTFIELDS, buf)) != CURLE_OK) {
        es_curl_error("es_payload", "postfields", res, es->ebuf, 0);
        return -1;
    }
    if ((res = curl_easy_setopt(es->curl, CURLOPT_POSTFIELDSIZE, strlen(buf))) != CURLE_OK) {
        es_curl_error("es_payload", "postfieldsize", res, es->ebuf, 0);
        return -1;
    }
    now = time(NULL);
    (void)gmtime_r(&now, &stamp);

    if (stamp.tm_year > es->stamp.tm_year ||
        stamp.tm_mon > es->stamp.tm_mon ||
        stamp.tm_mday > es->stamp.tm_mday) {
        strftime(es->daybuf, sizeof(es->daybuf), "%Y%m%d", &stamp);
    }
    memcpy(&es->stamp, &stamp, sizeof(es->stamp));

    snprintf(url,
             sizeof(url),
             "%s/logstash-%s/%s",
             es->url,
             es->daybuf,
             type);

    if ((res = curl_easy_setopt(es->curl, CURLOPT_URL, url)) != CURLE_OK) {
        es_curl_error("es_payload", "url", res, es->ebuf, 0);
        return -1;
    }
    res = curl_easy_perform(es->curl);
    if (res != CURLE_OK) {
        es_curl_error("es_payload", "perform", res, es->ebuf, 0);
        return -1;
    }
    log_trace("es_payload: success");
    return 0;
}

int
es_stop(struct output *out)
{
    struct es_state *es = out->state;

    log_trace("es_stop: enter");
    if (es->curl != NULL)
        curl_easy_cleanup(es->curl);
    log_trace("es_stop: success");
    return 0;

}

struct output_impl es_output = {
    es_start,
    es_stop,
    es_payload
};
