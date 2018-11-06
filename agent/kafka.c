#include "agent/kafka.h"
#include "common/sglib.h"
#include "common/uds.h"
#include "common/settings.h"
#include "common/text_util.h"
#include "common/thread.h"
#include <librdkafka/rdkafka.h>
#include <unicode/ustring.h>
#include <chucho/log.h>
#include <syslog.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdatomic.h>

typedef struct topic
{
    uds key;
    rd_kafka_topic_t* topic;
    char color;
    struct topic* left;
    struct topic* right;
} topic;

typedef struct kafka
{
    rd_kafka_t* consumer;
    rd_kafka_t* producer;
    topic* topics;
    kafka_message_handler handler;
    yella_thread* producer_thread;
    yella_thread* consumer_thread;
    atomic_bool should_stop;
} kafka;

// This has to be static, because the logger function doesn't have a user data parameter
static chucho_logger_t* lgr;

#define YELLA_KEY_COMPARE(x, y) u_strcmp((x->key), (y->key))

SGLIB_DEFINE_RBTREE_PROTOTYPES(topic, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(topic, left, right, color, YELLA_KEY_COMPARE);

static void consumer_main(void* udata)
{
    kafka* kf;
    rd_kafka_message_t* msg;

    kf = (kafka*)udata;
    while (!kf->should_stop)
    {
        msg = rd_kafka_consumer_poll(kf->consumer, 500);
        if (msg->err == RD_KAFKA_RESP_ERR_NO_ERROR)
        {
            if (kf->handler != NULL)
                kf->handler(msg->payload, msg->len);
        }
        else if (msg->err != RD_KAFKA_RESP_ERR__PARTITION_EOF &&
                 msg->err != RD_KAFKA_RESP_ERR__TIMED_OUT)
        {
            CHUCHO_C_ERROR(lgr,
                           "Error consuming Kafka topic %s: %s",
                           rd_kafka_topic_name(msg->rkt),
                           (const char*)msg->payload);
        }
    }
}

static void kafka_log(const rd_kafka_t* rk, int level, const char* fac, const char* buf)
{
    chucho_level_t clvl;

    if (level == LOG_INFO)
        clvl = CHUCHO_INFO;
    else if (level == LOG_DEBUG)
        clvl = CHUCHO_DEBUG;
    else if (level <= LOG_NOTICE && level >= LOG_WARNING)
        clvl = CHUCHO_WARN;
    else if (level <= LOG_ERR && level >= LOG_ALERT)
        clvl = CHUCHO_ERROR;
    else
        clvl = CHUCHO_FATAL;
    if (clvl == CHUCHO_INFO)
        CHUCHO_C_INFO(lgr, "(%s) %s", fac, buf);
    else if (clvl == CHUCHO_DEBUG)
        CHUCHO_C_DEBUG(lgr, "(%s) %s", fac, buf);
    else if (clvl == CHUCHO_WARN)
        CHUCHO_C_WARN(lgr, "(%s) %s", fac, buf);
    else if (clvl == CHUCHO_ERROR)
        CHUCHO_C_ERROR(lgr, "(%s) %s", fac, buf);
    else
        CHUCHO_C_FATAL(lgr, "(%s) %s", fac, buf);
}

static void message_delivered(rd_kafka_t* rdk, const rd_kafka_message_t* msg, void* udata)
{
    if (msg->err)
    {
        CHUCHO_C_ERROR(lgr,
                       "Message delivery failed to topic %s: %s",
                       rd_kafka_topic_name(msg->rkt),
                       rd_kafka_err2str(msg->err));
    }
}

static void producer_main(void* udata)
{
    kafka* kf;

    kf = (kafka*)udata;
    while (!kf->should_stop)
        rd_kafka_poll(kf->producer, 500);
}

kafka* create_kafka(const yella_uuid* const id)
{
    kafka* result;
    rd_kafka_conf_t* conf;
    char int_str[32];
    char* utf8;
    const UChar* st;
    rd_kafka_conf_res_t res;
    char err_msg[1024];
    rd_kafka_topic_partition_list_t* topics;
    rd_kafka_resp_err_t rc;

    if (yella_settings_get_text(u"agent", u"brokers") == NULL)
    {
        CHUCHO_C_FATAL("yella.kafka", "No brokers have been set");
        return NULL;
    }
    result = calloc(1, sizeof(kafka));
    lgr = chucho_get_logger("yella.kafka");
    conf = rd_kafka_conf_new();
    snprintf(int_str, sizeof(int_str), "%i", LOG_DEBUG);
    res = rd_kafka_conf_set(conf, "log_level", int_str, err_msg, sizeof(err_msg));
    if (res != RD_KAFKA_CONF_OK)
        CHUCHO_C_ERROR(lgr, "Unable to set log_level: %s", err_msg);
    rd_kafka_conf_set_log_cb(conf, kafka_log);
    st = yella_settings_get_text(u"agent", u"kafka-debug-contexts");
    if (st != NULL)
    {
        utf8 = yella_to_utf8(st);
        res = rd_kafka_conf_set(conf, "debug", utf8, err_msg, sizeof(err_msg));
        if (res != RD_KAFKA_CONF_OK)
            CHUCHO_C_ERROR(lgr, "Unable to set debug value '%s': %s", utf8, err_msg);
        free(utf8);
    }
    snprintf(int_str, sizeof(int_str), "%" PRIu64, *yella_settings_get_uint(u"agent", u"latency-milliseconds"));
    res = rd_kafka_conf_set(conf, "queue.buffering.max.ms", int_str, err_msg, sizeof(err_msg));
    if (res != RD_KAFKA_CONF_OK)
        CHUCHO_C_ERROR(lgr, "Unable to set queue.buffering.max.ms value '%s': %s", int_str, err_msg);
    utf8 = yella_to_utf8(yella_settings_get_text(u"agent", u"compression-type"));
    res = rd_kafka_conf_set(conf, "compression.codec", utf8, err_msg, sizeof(err_msg));
    if (res != RD_KAFKA_CONF_OK)
        CHUCHO_C_ERROR(lgr, "Unable to set compression.codec value '%s': %s", utf8, err_msg);
    free(utf8);
    utf8 = yella_to_utf8(id->text);
    res = rd_kafka_conf_set(conf, "group.id", utf8, err_msg, sizeof(err_msg));
    if (res != RD_KAFKA_CONF_OK)
        CHUCHO_C_ERROR(lgr, "Unable to set group.id value '%s': %s", utf8, err_msg);
    free(utf8);
    rd_kafka_conf_set_dr_msg_cb(conf, message_delivered);
    rd_kafka_conf_set_opaque(conf, result);
    result->consumer = rd_kafka_new(RD_KAFKA_CONSUMER, rd_kafka_conf_dup(conf), err_msg, sizeof(err_msg));
    if (result->consumer == NULL)
    {
        CHUCHO_C_FATAL(lgr, "Unable to create Kafka consumer: %s", err_msg);
        rd_kafka_conf_destroy(conf);
        chucho_release_logger(lgr);
        free(result);
        return NULL;
    }
    rd_kafka_poll_set_consumer(result->consumer);
    utf8 = yella_to_utf8(yella_settings_get_text(u"agent", u"brokers"));
    rd_kafka_brokers_add(result->consumer, utf8);
    free(utf8);
    topics = rd_kafka_topic_partition_list_new(1);
    utf8 = yella_to_utf8(yella_settings_get_text(u"agent", u"agent-topic"));
    rd_kafka_topic_partition_list_add(topics, utf8, RD_KAFKA_PARTITION_UA);
    rc = rd_kafka_subscribe(result->consumer, topics);
    rd_kafka_topic_partition_list_destroy(topics);
    if (rc != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
        CHUCHO_C_FATAL(lgr, "Unable to subscribe to topic: %s", utf8);
        free(utf8);
        rd_kafka_destroy(result->consumer);
        chucho_release_logger(lgr);
        free(result);
        return NULL;
    }
    free(utf8);
    result->producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, err_msg, sizeof(err_msg));
    if (result->producer == NULL)
    {
        CHUCHO_C_FATAL(lgr, "Unable to create Kafka producer: %s", err_msg);
        rd_kafka_destroy(result->consumer);
        rd_kafka_conf_destroy(conf);
        chucho_release_logger(lgr);
        free(result);
        return NULL;
    }
    utf8 = yella_to_utf8(yella_settings_get_text(u"agent", u"brokers"));
    rd_kafka_brokers_add(result->producer, utf8);
    free(utf8);
    result->producer_thread = yella_create_thread(producer_main, result);
    result->consumer_thread = yella_create_thread(consumer_main, result);
    return result;
}

void destroy_kafka(kafka* kf)
{
    struct sglib_topic_iterator titor;
    topic* tpc;

    rd_kafka_flush(kf->producer, 500);
    for (tpc = sglib_topic_it_init(&titor, kf->topics);
         tpc != NULL;
         sglib_topic_it_next(&titor))
    {
        udsfree(tpc->key);
        rd_kafka_topic_destroy(tpc->topic);
        free(tpc);
    }
    kf->should_stop = true;
    yella_join_thread(kf->producer_thread);
    yella_destroy_thread(kf->producer_thread);
    yella_join_thread(kf->consumer_thread);
    yella_destroy_thread(kf->consumer_thread);
    rd_kafka_destroy(kf->consumer);
    rd_kafka_destroy(kf->producer);
    free(kf);
}

bool send_kafka_message(kafka* kf, const UChar* const tpc, void* msg, size_t len)
{
    topic to_find;
    topic* found;
    char* utf8;
    int rc;

    to_find.key = (UChar*)tpc;
    found = sglib_topic_find_member(kf->topics, &to_find);
    if (found == NULL)
    {
        found = malloc(sizeof(topic));
        found->key = udsnew(tpc);
        utf8 = yella_to_utf8(tpc);
        found->topic = rd_kafka_topic_new(kf->producer, utf8, NULL);
        if (found->topic == NULL)
        {
            CHUCHO_C_ERROR(lgr, "Failed to create topic %s: %s", utf8, rd_kafka_err2str(rd_kafka_last_error()));
            free(utf8);
            return false;
        }
        free(utf8);
        sglib_topic_add(&kf->topics, found);
    }
    while (true)
    {
        rc = rd_kafka_produce(found->topic,
                              RD_KAFKA_PARTITION_UA,
                              RD_KAFKA_MSG_F_COPY,
                              msg,
                              len,
                              NULL,
                              0,
                              kf);
        if (rc == 0)
        {
            break;
        }
        else
        {
            CHUCHO_C_ERROR(lgr,
                           "Error sending to topic %s: %s",
                           rd_kafka_topic_name(found->topic),
                           rd_kafka_err2str(rd_kafka_last_error()));
            if (rd_kafka_last_error() != RD_KAFKA_RESP_ERR__QUEUE_FULL)
                return false;
            /* In case queue is full, let our poller run a litle and then retry. */
            yella_sleep_this_thread(100);
        }
    }
    return true;
}

void set_kafka_message_handler(kafka* kf, kafka_message_handler hndlr)
{
    kf->handler = hndlr;
}

