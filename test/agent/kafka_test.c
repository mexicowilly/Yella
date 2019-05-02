#include "agent/kafka.h"
#include "common/settings.h"
#include "common/thread.h"
#include <chucho/log.h>
#include <chucho/configuration.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static void message_handler(void* msg, size_t len, void* udata)
{
    printf("Got message '%s'\n", (char*)msg);
    assert_string_equal((char*)msg, (char*)udata);
}

static void consume(void** arg)
{
    kafka* kf;
    yella_uuid* id;
    char* msg;

    id = yella_create_uuid();
    kf = create_kafka(id);
    set_kafka_message_handler(kf, message_handler, "consume-test");
    yella_sleep_this_thread(5000);
    msg = strdup("consume-test");
    assert_true(send_kafka_message(kf, u"kafka.test", msg, strlen(msg) + 1));
    printf("Just sent messasge 'consume-test'\n");
    yella_sleep_this_thread(5000);
    destroy_kafka(kf);
    yella_destroy_uuid(id);

}

static void how_fast(void** arg)
{
    kafka* kf;
    yella_uuid* id;
    int i;
    char int_str[32];
    int len;
    char* msg;

    id = yella_create_uuid();
    kf = create_kafka(id);
    yella_sleep_this_thread(5000);
    for (i = 0; i < 1000000; i++)
    {
        len = snprintf(int_str, sizeof(int_str), "%i", i);
        msg = strdup(int_str);
        assert_true(send_kafka_message(kf, u"kafka.test", msg, strlen(msg) + 1));
    }
    destroy_kafka(kf);
    yella_destroy_uuid(id);
}

static void produce(void **arg)
{
    kafka* kf;
    yella_uuid* id;
    char* msg;

    id = yella_create_uuid();
    kf = create_kafka(id);
    yella_sleep_this_thread(5000);
    msg = strdup("goodbye");
    assert_true(send_kafka_message(kf, u"kafka.test", msg, strlen(msg) + 1));
    destroy_kafka(kf);
    yella_destroy_uuid(id);
}

static int set_up(void** arg)
{
    chucho_cnf_set_fallback(
            "chucho::logger:\n"
            "    name: <root>\n"
            "    level: info\n"
            "    chucho::cout_writer:\n"
            "        chucho::pattern_formatter:\n"
            "            pattern: '%p %d{%H:%M:%S.%q} %c: %m%n'"
            );
    yella_initialize_settings();
    yella_load_settings_doc();
    yella_settings_set_text(u"agent", u"brokers", u"10.0.2.5:9092");
    yella_settings_set_uint(u"agent", u"latency-milliseconds", 0);
    yella_settings_set_text(u"agent", u"spool-dir", u"test-kafka-spool");
    yella_settings_set_uint(u"agent", u"max-spool-partition-size", 1000000);
    yella_settings_set_uint(u"agent", u"max-spool-partitions", 100);
    yella_settings_set_text(u"agent", u"compression-type", u"lz4");
    yella_settings_set_text(u"agent", u"agent-recipient", u"kafka.test");
    yella_settings_set_uint(u"agent", u"connection-interval-seconds", 1);
    yella_settings_set_text(u"agent", u"kafka-debug-contexts", u"all");
    return 0;
}

static int tear_down(void** arg)
{
    yella_destroy_settings();
    return 0;
}

int main()
{
    const struct CMUnitTest tests[] =
    {
//        cmocka_unit_test(how_fast)
//        cmocka_unit_test(produce)
//        cmocka_unit_test(consume)
    };

    return cmocka_run_group_tests(tests, set_up, tear_down);
}
