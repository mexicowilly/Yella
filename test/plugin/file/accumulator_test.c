#include "plugin/file/accumulator.h"
#include "common/text_util.h"
#include "common/settings.h"
#include "common/file.h"
#include "common/thread.h"
#include "plugin/plugin.h"
#include "file_reader.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>
#include <stdatomic.h>
#include <unicode/ucal.h>
#include <chucho/log.h>

typedef struct msg_rec
{
    UDate tm;
    size_t sz;
} msg_rec;

typedef struct test_data
{
    accumulator* acc;
    yella_agent_api api;
    chucho_logger_t* lgr;
    yella_ptr_vector* recs;
    yella_mutex* guard;
} test_data;

static void receive_message(void* agent, yella_message_header* mhdr, uint8_t* msg, size_t sz)
{
    test_data* td;
    msg_rec* rec;

    td = agent;
    yella_log_mhdr(mhdr, td->lgr);
    rec = malloc(sizeof(struct msg_rec));
    rec->tm = mhdr->time;
    rec->sz = sz;
    yella_lock_mutex(td->guard);
    yella_push_back_ptr_vector(td->recs, rec);
    yella_unlock_mutex(td->guard);
}

static int set_up(void** arg)
{
    test_data* td;

    td = malloc(sizeof(test_data));
    td->api.send_message = receive_message;
    td->lgr = chucho_get_logger("accumulator-test");
    td->recs = yella_create_ptr_vector();
    td->guard = yella_create_mutex();
    td->acc = create_accumulator(td, &td->api);
    *arg = td;
    return 0;
}

static int tear_down(void** arg)
{
    test_data* td;

    td = *arg;
    destroy_accumulator(td->acc);
    yella_destroy_ptr_vector(td->recs);
    yella_destroy_mutex(td->guard);
    chucho_release_logger(td->lgr);
    free(td);
    return 0;
}

static void latency(void** arg)
{
    test_data* td;
    element* elem;
    attribute* attr;

    td = *arg;
    elem = create_element(u"jumpy lumpy");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem, attr);
    add_accumulator_message(td->acc,
                            u"funky",
                            u"config me",
                            element_name(elem),
                            elem,
                            yella_fb_file_condition_CHANGED);
    destroy_element(elem);
    yella_sleep_this_thread(*yella_settings_get_uint(u"file", u"send-latency-seconds") * 1000 * 3);
    yella_lock_mutex(td->guard);
    assert_int_equal(yella_ptr_vector_size(td->recs), 1);
    yella_unlock_mutex(td->guard);
}

static void size(void** arg)
{
    test_data* td;
    uint64_t max_sz;
    element* elem;
    attribute* attr;
    UDate latency_time;
    bool done;
    msg_rec* rec;
    size_t count;

    td = *arg;
    /* Add 2% extra because the message will only be sent once the max size is exceeded */
    max_sz = *yella_settings_get_byte_size(u"agent", u"max-message-size");
    max_sz += max_sz * .02;
    latency_time = ucal_getNow() + (*yella_settings_get_uint(u"file", u"send-latency-seconds") * 1000);
    elem = create_element(u"jumpy lumpy");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem, attr);
    count = 0;
    do
    {
        add_accumulator_message(td->acc,
                                u"funky",
                                u"config me",
                                element_name(elem),
                                elem,
                                yella_fb_file_condition_CHANGED);
        ++count;
        yella_lock_mutex(td->guard);
        done = yella_ptr_vector_size(td->recs) == 1;
        yella_unlock_mutex(td->guard);
    } while (!done);
    destroy_element(elem);
    CHUCHO_C_INFO(td->lgr, "Added %zu messages to accumulator", count);
    assert_true(ucal_getNow() <= latency_time);
    yella_lock_mutex(td->guard);
    rec = yella_ptr_vector_at(td->recs, 0);
    assert_true(rec->sz < max_sz);
    yella_unlock_mutex(td->guard);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup_teardown(size, set_up, tear_down),
        cmocka_unit_test_setup_teardown(latency, set_up, tear_down)
    };

    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_byte_size(u"agent", u"max-message-size", u"1MB");
    yella_settings_set_uint(u"file", u"send-latency-seconds", 1);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
