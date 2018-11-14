#include "agent/kafka.h"
#include "common/settings.h"
#include <chucho/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static void simple(void** arg)
{
    kafka* kf;
    yella_uuid* id;

    id = yella_create_uuid();
    kf = create_kafka(id);
    assert_true(send_transient_kafka_message(kf, u"kafka.test", "hello", 6));
    destroy_kafka(kf);
    yella_destroy_uuid(id);
}

static int set_up(void** arg)
{
    yella_initialize_settings();
    yella_load_settings_doc();
    yella_settings_set_text(u"agent", u"brokers", u"10.0.0.2:9092");
    yella_settings_set_uint(u"agent", u"latency-milliseconds", 0);
    yella_settings_set_text(u"agent", u"spool-dir", u"test-kafka-spool");
    yella_settings_set_uint(u"agent", u"max-spool-partition-size", 1000000);
    yella_settings_set_uint(u"agent", u"max-spool-partitions", 100);
    yella_settings_set_text(u"agent", u"compression-type", u"lz4");
    yella_settings_set_text(u"agent", u"agent-topic", u"kafka.test");
    yella_settings_set_uint(u"agent", u"connection-interval-seconds", 5);
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
        cmocka_unit_test(simple)
    };

    return cmocka_run_group_tests(tests, set_up, tear_down);
}
