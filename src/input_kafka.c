#include <stdlib.h>
#include <string.h>
#include <bsd/string.h>
#include <librdkafka/rdkafka.h>

#include "unklog.h"

static int  kafka_start(struct input *, input_dispatch_t, void *);
static int  kafka_stop(struct input *);
static void kafka_log(const rd_kafka_t *, int, const char *, const char *);
static void kafka_rebalance(rd_kafka_t *, rd_kafka_resp_err_t,
                            rd_kafka_topic_partition_list_t *, void *);

struct kafka_state {
    rd_kafka_conf_t                 *conf;
    rd_kafka_topic_conf_t           *tconf;
    rd_kafka_topic_partition_list_t *topics;
    rd_kafka_t                      *rd;
};

void
kafka_log(const rd_kafka_t *rd, int level, const char *fac, const char *buf)
{
    log_print(level, 0, "%s: kafka message: %s", fac, buf);
}

void
kafka_rebalance(rd_kafka_t *rd,
                rd_kafka_resp_err_t err,
                rd_kafka_topic_partition_list_t *partitions,
                void *opaque)
{
    log_info("kafka_rebalance: consumer group rebalanced");
    switch (err) {
    case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
        log_info("kafka_rebalance: new assignment");
        rd_kafka_assign(rd, partitions);
        break;
    case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
        log_info("kafka_rebalance: partitions revoked");
        rd_kafka_assign(rd, NULL);
        break;
    default:
        log_error("kafka_rebalance: bad state");
        rd_kafka_assign(rd, NULL);
        break;
    }
}

void
kafka_handle(struct input *in, rd_kafka_message_t *msg, input_dispatch_t fn, void *p)
{
    if (msg->err) {
        if (msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
            log_debug("kafka_handle: reached end of partition %ld",
                      msg->partition);
        } else {
            log_error("kafka_handle: kafka error");
        }
        return;
    }
    metric_inc(&in->count);
    (void)fn((const char *)msg->payload, msg->len, p);
}

int
kafka_start(struct input *in, input_dispatch_t fn, void *p)
{
    struct kafka_state  *k;
    struct option       *opt;
    char                 estr[512];
    rd_kafka_message_t  *msg;
    char                *topic = NULL;

    log_trace("kafka_start: enter");
    if ((k = calloc(1, sizeof(*k))) == NULL) {
        log_sys_fatal("kafka_start: out of memory");
    }
    in->state = k;

    if ((k->conf = rd_kafka_conf_new()) == NULL)
        log_sys_fatal("kafka_start: out of memory");

    if ((k->tconf = rd_kafka_topic_conf_new()) == NULL)
        log_sys_fatal("kafka_start: out of memory");

    /*
     * Regardless of our configuration, we need to use
     * broker storage for offsets.
     */
    if (rd_kafka_topic_conf_set(k->tconf,
                                "offset.store.method",
                                "broker",
                                estr,
                                sizeof(estr)) != RD_KAFKA_CONF_OK)
        log_fatal("kafka_start: cannot set offset store method: %s", estr);

    rd_kafka_conf_set_log_cb(k->conf, kafka_log);

    log_trace("kafka_start: applying options");

    TAILQ_FOREACH(opt, &in->options, entry) {

        if (strcasecmp(opt->key, "topic") == 0) {
            topic = opt->val;
            log_debug("kafka_start: setting topic to: %s", topic);
            continue;
        }

        if (topic == NULL) {
            log_debug("kafka_start: applying global option: %s => %s", opt->key, opt->val);
            if (rd_kafka_conf_set(k->conf, opt->key, opt->val, estr, sizeof(estr)) != RD_KAFKA_CONF_OK)
                log_fatal("kafka_start: invalid configuration option: %s=%s: %s",
                          opt->key, opt->val, estr);
        } else {
            log_debug("kafka_start: applying topic option: %s => %s", opt->key, opt->val);
            if (rd_kafka_topic_conf_set(k->tconf, opt->key, opt->val, estr, sizeof(estr)) != RD_KAFKA_CONF_OK)
                log_fatal("kafka_start: invalid configuration option: %s=%s: %s",
                          opt->key, opt->val, estr);
        }
    }

    if (topic == NULL) {
        topic = "logs";
    }

    rd_kafka_conf_set_default_topic_conf(k->conf, k->tconf);
    rd_kafka_conf_set_rebalance_cb(k->conf, kafka_rebalance);

    if ((k->rd = rd_kafka_new(RD_KAFKA_CONSUMER, k->conf, estr, sizeof(estr))) == NULL)
        log_fatal("kafka_start: cannot create consumer: %s", estr);

    rd_kafka_set_log_level(k->rd, LOG_DEBUG);

    if ((k->topics = rd_kafka_topic_partition_list_new(1)) == NULL)
        log_fatal("kafka_start: cannot create topic partitions list");

    rd_kafka_topic_partition_list_add(k->topics, topic, -1);

    rd_kafka_subscribe(k->rd, k->topics);
    log_trace("kafka_start: polling log messages");
    while (in->flags & INPUT_RUN) {
        if ((msg = rd_kafka_consumer_poll(k->rd, 300)) == NULL)
            continue;
        kafka_handle(in, msg, fn, p);
        rd_kafka_message_destroy(msg);
    }
    rd_kafka_unsubscribe(k->rd);
    log_info("kafka_start: stopped subscription");
    log_trace("kafka_start: success");
    return 0;
}

int
kafka_stop(struct input *in)
{

    struct kafka_state  *k = in->state;

    in->flags &= ~INPUT_RUN;
    rd_kafka_consumer_close(k->rd);
    (void)rd_kafka_wait_destroyed(1000);
    return 0;
}

struct input_impl kafka_input = {
    kafka_start,
    kafka_stop
};
