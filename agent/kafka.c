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

kafka* create_kafka(void)
{
    kafka* result;
    rd_kafka_conf_t* global_conf;
    char int_str[32];

    result = calloc(1, sizeof(kafka));
    lgr = chucho_get_logger("yella.kafka");
    global_conf = rd_kafka_conf_new();
    snprintf(int_str, sizeof(int_str), "%i", LOG_DEBUG);
    rd_kafka_conf_set(global_conf, "log_level", int_str, NULL, 0);
    rd_kafka_conf_set_log_cb(global_conf, kafka_log);
    snprintf(int_str, sizeof(int_str), "%" PRIu64, *yella_settings_get_uint(u"agent", u"latency-milliseconds"));
    rd_kafka_conf_set(global_conf, "queue.buffering.max.ms", int_str, NULL, 0);
    rd_kafka_conf_set(global_conf, "compression.codec", "lz4", NULL, 0);
    return result;
}
