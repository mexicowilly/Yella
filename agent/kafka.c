#include "agent/kafka.h"
#include "agent/spool.h"
#include "common/sglib.h"
#include "common/uds.h"
#include "common/settings.h"
#include "common/text_util.h"
#include "common/thread.h"
#include <librdkafka/rdkafka.h>
#include <unicode/ustring.h>
#include <chucho/log.h>
#include <cjson/cJSON.h>
#include <syslog.h>
#include <stdlib.h>
#include <assert.h>

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
    bool should_stop;
    spool* sp;
    yella_thread* spool_thread;
    bool disconnected;
    yella_mutex* guard;
    yella_condition_variable* conn_condition;
    void* handler_udata;
} kafka;

// This has to be static, because the logger function doesn't have a user data parameter
static chucho_logger_t* lgr;

#define YELLA_KEY_COMPARE(x, y) u_strcmp((x->key), (y->key))

SGLIB_DEFINE_RBTREE_PROTOTYPES(topic, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(topic, left, right, color, YELLA_KEY_COMPARE);

static bool kafka_should_stop(kafka* kf)
{
    bool result;

    yella_lock_mutex(kf->guard);
    result = kf->should_stop;
    yella_unlock_mutex(kf->guard);
    return result;
}

static void consumer_main(void* udata)
{
    kafka* kf;
    rd_kafka_message_t* msg;

    kf = (kafka*)udata;
    while (!kafka_should_stop(kf))
    {
        msg = rd_kafka_consumer_poll(kf->consumer, 500);
        if (msg->err == RD_KAFKA_RESP_ERR_NO_ERROR)
        {
            if (kf->handler != NULL)
                kf->handler(msg->payload, msg->len, kf->handler_udata);
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

static void log_kafka_conf(rd_kafka_conf_t* conf)
{
    const char** arr;
    size_t sz;
    size_t i;
    int32_t longest;
    int32_t cur_len;
    int32_t diff;
    int32_t j;
    UChar** uarr;
    uds log;
    char* utf8_log;

    if (chucho_logger_permits(lgr, CHUCHO_INFO))
    {
        longest = 0;
        arr = rd_kafka_conf_dump(conf, &sz);
        uarr = malloc(sizeof(UChar) * sz);
        for (i = 0; i < sz; i++)
        {
            uarr[i] = yella_from_utf8(arr[i]);
            cur_len = u_strlen(uarr[i]);
            if (cur_len > longest)
                longest = cur_len;
        }
        log = udsnew(u"Kafka configuration:");
        log = udscat(log, YELLA_NL);
        for (i = 0; i < sz; i += 2)
        {
            log = udscat(log, u"  ");
            log = udscat(log, uarr[i]);
            cur_len = u_strlen(uarr[i]);
            diff = longest - cur_len;
            for (j = 0; j < diff; j++)
                log = udscat(log, u" ");
            log = udscatprintf(log, u": %S%S", uarr[i + 1], YELLA_NL);
        }
        cur_len = u_strlen(YELLA_NL);
        udsrange(log, 0, -cur_len);
        utf8_log = yella_to_utf8(log);
        CHUCHO_C_INFO(lgr, utf8_log);
        free(utf8_log);
        udsfree(log);
        for (i = 0; i < sz; i++)
            free(uarr[i]);
        rd_kafka_conf_dump_free(arr, sz);
    }
}

static void message_delivered(rd_kafka_t* rdk, const rd_kafka_message_t* msg, void* udata)
{
    kafka* kf;

    if (msg->err != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
        if (msg->err == RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN)
        {
            kf = (kafka*)udata;
            yella_lock_mutex(kf->guard);
            kf->disconnected = true;
            yella_unlock_mutex(kf->guard);
        }
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
    while (!kafka_should_stop(kf))
        rd_kafka_poll(kf->producer, 500);
}

static void spool_main(void* udata)
{
    kafka* kf;
    message_part* popped;
    size_t count_popped;
    yella_rc rc;
    int i;
    bool ready;

    kf = (kafka*)udata;
    while (!kafka_should_stop(kf))
    {
        yella_lock_mutex(kf->guard);
        if (!kf->disconnected || yella_wait_milliseconds_for_condition_variable(kf->conn_condition, kf->guard, 500))
        {
            ready = !kf->disconnected;
            yella_unlock_mutex(kf->guard);
            if (ready && spool_pop(kf->sp, 500, &popped, &count_popped) == YELLA_NO_ERROR)
            {
                assert(count_popped == 2);
                send_transient_kafka_message(kf, (UChar *) popped[0].data, popped[1].data, popped[1].size);
                for (i = 0; i < count_popped; i++)
                    free(popped[i].data);
                free(popped);
            }
        }
        else
        {
            yella_unlock_mutex(kf->guard);
        }
    }
}

static int statistics_delivered(rd_kafka_t* rdk, char* buf, size_t len, void* udata)
{
    cJSON* json;
    cJSON* brokers;
    cJSON* broker;
    cJSON* state;
    char* val;
    kafka* kf;

    kf = (kafka*)udata;
    assert(buf[len] == 0);
    json = cJSON_Parse(buf);
    if (json == NULL)
    {
        CHUCHO_C_ERROR(lgr, "Unparseable statistics: %s", buf);
    }
    else
    {
        brokers = cJSON_GetObjectItemCaseSensitive(json, "brokers");
        if (brokers == NULL)
        {
            CHUCHO_C_ERROR(lgr, "No 'brokers' found in statistics: %s", buf);
        }
        else
        {
            cJSON_ArrayForEach(broker, brokers)
            {
                state = cJSON_GetObjectItemCaseSensitive(broker, "state");
                if (state == NULL)
                {
                    CHUCHO_C_ERROR(lgr, "No 'state' found in broker %s: %s", cJSON_GetStringValue(broker), buf);
                }
                else
                {
                    val = cJSON_GetStringValue(state);
                    if (val == NULL)
                    {
                        CHUCHO_C_ERROR(lgr, "'state' is not a string in broker %s: %s", cJSON_GetStringValue(broker),
                                       buf);
                    }
                    else if (strcmp(val, "UP") == 0)
                    {
                        yella_lock_mutex(kf->guard);
                        kf->disconnected = false;
                        yella_signal_condition_variable(kf->conn_condition);
                        yella_unlock_mutex(kf->guard);
                        break;
                    }
                }
            }
        }
        cJSON_Delete(json);
    }
    return 0;
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
    result->sp = create_spool();
    if (result->sp == NULL)
    {
        CHUCHO_C_FATAL(lgr, "Unable to create spool");
        chucho_release_logger(lgr);
        free(result);
        return NULL;
    }
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
    snprintf(int_str, sizeof(int_str), "%" PRIu64, *yella_settings_get_uint(u"agent", u"connection-interval-seconds") * 1000);
    res = rd_kafka_conf_set(conf, "statistics.interval.ms", int_str, err_msg, sizeof(err_msg));
    if (res != RD_KAFKA_CONF_OK)
        CHUCHO_C_ERROR(lgr, "Unable to set statistics.interval.ms value '%s': %s", int_str, err_msg);
    rd_kafka_conf_set_stats_cb(conf, statistics_delivered);
    rd_kafka_conf_set_opaque(conf, result);
    log_kafka_conf(conf);
    result->consumer = rd_kafka_new(RD_KAFKA_CONSUMER, rd_kafka_conf_dup(conf), err_msg, sizeof(err_msg));
    if (result->consumer == NULL)
    {
        CHUCHO_C_FATAL(lgr, "Unable to create Kafka consumer: %s", err_msg);
        rd_kafka_conf_destroy(conf);
        destroy_spool(result->sp);
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
        destroy_spool(result->sp);
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
        destroy_spool(result->sp);
        chucho_release_logger(lgr);
        free(result);
        return NULL;
    }
    utf8 = yella_to_utf8(yella_settings_get_text(u"agent", u"brokers"));
    rd_kafka_brokers_add(result->producer, utf8);
    free(utf8);
    result->guard = yella_create_mutex();
    result->conn_condition = yella_create_condition_variable();
    result->consumer_thread = yella_create_thread(consumer_main, result);
    result->producer_thread = yella_create_thread(producer_main, result);
    result->spool_thread = yella_create_thread(spool_main, result);
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
    yella_lock_mutex(kf->guard);
    kf->should_stop = true;
    yella_unlock_mutex(kf->guard);
    yella_join_thread(kf->consumer_thread);
    yella_destroy_thread(kf->consumer_thread);
    yella_join_thread(kf->spool_thread);
    yella_destroy_thread(kf->spool_thread);
    yella_join_thread(kf->producer_thread);
    yella_destroy_thread(kf->producer_thread);
    rd_kafka_destroy(kf->consumer);
    rd_kafka_destroy(kf->producer);
    destroy_spool(kf->sp);
    yella_destroy_condition_variable(kf->conn_condition);
    yella_destroy_mutex(kf->guard);
    free(kf);
    chucho_release_logger(lgr);
}

bool send_kafka_message(kafka* kf, const UChar* const tpc, void* msg, size_t len)
{
    message_part parts[2];
    bool result;

    result = true;
    yella_lock_mutex(kf->guard);
    if (!spool_empty_of_messages(kf->sp) || kf->disconnected)
    {
        parts[0].data = (uint8_t*)tpc;
        parts[0].size = (u_strlen(tpc) + 1) * sizeof(UChar);
        parts[1].data = msg;
        parts[1].size = len;
        result = spool_push(kf->sp, parts, 2) == YELLA_NO_ERROR;
        yella_unlock_mutex(kf->guard);
    }
    else
    {
        yella_unlock_mutex(kf->guard);
        result = send_transient_kafka_message(kf, tpc, msg, len);
    }
    return result;
}

bool send_transient_kafka_message(kafka* kf, const UChar* const tpc, void* msg, size_t len)
{
    topic to_find;
    topic* found;
    char* utf8;
    int rc;

    to_find.key = (UChar*)tpc;
    yella_lock_mutex(kf->guard);
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
    yella_unlock_mutex(kf->guard);
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

void set_kafka_message_handler(kafka* kf, kafka_message_handler hndlr, void* udata)
{
    kf->handler = hndlr;
    kf->handler_udata = udata;
}

