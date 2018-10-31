#include "agent/kafka.h"
#include "common/sglib.h"
#include "common/uds.h"
#include "common/settings.h"
#include "common/text_util.h"
#include <librdkafka/rdkafka.h>
#include <unicode/ustring.h>
#include <chucho/log.h>
#include <syslog.h>
#include <stdlib.h>
#include <inttypes.h>

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
} kafka;

// This has to be static, because the logger function doesn't have a user data parameter
static chucho_logger_t* lgr;

#define YELLA_KEY_COMPARE(x, y) u_strcmp((x->key), (y->key))

SGLIB_DEFINE_RBTREE_PROTOTYPES(topic, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(topic, left, right, color, YELLA_KEY_COMPARE);

static void message_delivered(rd_kafka_t* rdk, const rd_kafka_message_t* msg, void* udata)
{

}

static void message_received(rd_kafka_message_t* msg, void* udata)
{

}

static void kafka_log(const rd_kafka_t *rk, int level, const char *fac, const char *buf)
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

kafka* create_kafka(const yella_uuid* const id)
{
    kafka* result;
    rd_kafka_conf_t* conf;
    char int_str[32];
    char* utf8;
    const UChar* st;
    rd_kafka_conf_res_t res;
    char err_msg[1024];

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
    rd_kafka_conf_set_consume_cb(conf, message_received);
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
    utf8 = yella_to_utf8(yella_settings_get_text(u"agent", u"brokers"));
    rd_kafka_brokers_add(result->consumer, utf8);
    free(utf8);
    return result;
}

void destroy_kafka(kafka* kf)
{
    struct sglib_topic_iterator titor;
    topic* tpc;

    for (tpc = sglib_topic_it_init(&titor, kf->topics);
         tpc != NULL;
         sglib_topic_it_next(&titor))
    {
        udsfree(tpc->key);
        rd_kafka_topic_destroy(tpc->topic);
        free(tpc);
    }
    rd_kafka_destroy(kf->consumer);
    rd_kafka_destroy(kf->producer);
    free(kf);
}
