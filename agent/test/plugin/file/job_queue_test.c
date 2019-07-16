#include "plugin/file/job_queue.h"
#include "plugin/file/job.h"
#include "common/file.h"
#include "common/settings.h"
#include "common/thread.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>
#include <chucho/log.h>

typedef struct test_data
{
    state_db_pool* pool;
    job_queue* jq;
    accumulator* acc;
    yella_agent_api api;
    chucho_logger_t* lgr;
} test_data;

static job* get_job(test_data* td)
{
    job* j;

    j = create_job(u"one", u"monkey boy", td->acc);
    yella_push_back_ptr_vector(j->includes, udsnew(yella_settings_get_dir(u"file", u"data-dir")));
    j->attr_types = malloc(sizeof(attribute_type) * 2);
    j->attr_type_count = 2;
    j->attr_types[0] = ATTR_TYPE_FILE_TYPE;
    j->attr_types[1] = ATTR_TYPE_SHA256;
    return j;
}

static void a_lot(void** arg)
{
    test_data* td;
    size_t sz;
    job_queue_stats stats;
    size_t count;

    td = *arg;
    count = 0;
    do
    {
        sz = push_job_queue(td->jq, get_job(td));
        if (++count % 10000 == 0)
            print_message("Count %zu\n", count);
    } while (sz < 50000);
    log_job_queue_stats(td->jq, td->lgr);
    stats = get_job_queue_stats(td->jq);
    assert_int_equal(stats.max_size, 50000);
    yella_sleep_this_thread_milliseconds(2000);
    log_job_queue_stats(td->jq, td->lgr);
    stats = get_job_queue_stats(td->jq);
    assert_int_equal(stats.jobs_run, stats.jobs_pushed);
}

static void cb(void* udata)
{
    yella_signal_event((yella_event*)udata);
}

static void empty_callback(void** arg)
{
    yella_event* evt;
    test_data* td;
    size_t sz;
    job_queue_stats stats;
    size_t count;

    td = *arg;
    evt = yella_create_event();
    count = 0;
    do
    {
        sz = push_job_queue(td->jq, get_job(td));
        if (++count % 10000 == 0)
            print_message("Count %zu\n", count);
    } while (sz < 50000);
    set_job_queue_empty_callback(td->jq, cb, evt);
    log_job_queue_stats(td->jq, td->lgr);
    yella_wait_for_event(evt);
    stats = get_job_queue_stats(td->jq);
    assert_int_equal(stats.jobs_pushed, stats.jobs_run);
    log_job_queue_stats(td->jq, td->lgr);
    yella_destroy_event(evt);
}

static void one(void** arg)
{
    test_data* td;
    size_t sz;
    job* j;
    job_queue_stats stats;

    td = *arg;
    j = get_job(td);
    sz = push_job_queue(td->jq, j);
    yella_sleep_this_thread_milliseconds(1000);
    assert_int_equal(sz, 1);
    log_job_queue_stats(td->jq, td->lgr);
    stats = get_job_queue_stats(td->jq);
    assert_int_equal(stats.jobs_run, 1);
    assert_int_equal(stats.jobs_pushed, 1);
}

static void send_message(void* agent, yella_parcel* pcl)
{
    print_message("In send_message\n");
}

static int set_up(void** arg)
{
    test_data* td;

    yella_remove_all(yella_settings_get_dir(u"file", u"data-dir"));
    td = calloc(1, sizeof(test_data));
    td->pool = create_state_db_pool();
    td->jq = create_job_queue(td->pool);
    td->api.send_message = send_message;
    td->acc = create_accumulator(td, &td->api);
    td->lgr = chucho_get_logger("job-queue-test");
    *arg = td;
    return 0;
}

static int tear_down(void** arg)
{
    test_data* td;
    size_t i;

    td = *arg;
    destroy_job_queue(td->jq);
    destroy_state_db_pool(td->pool);
    destroy_accumulator(td->acc);
    chucho_release_logger(td->lgr);
    free(td);
    return 0;
}

int main()
{
    int rc;

    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup_teardown(one, set_up, tear_down),
        cmocka_unit_test_setup_teardown(a_lot, set_up, tear_down),
        cmocka_unit_test_setup_teardown(empty_callback, set_up, tear_down)
    };

    yella_initialize_settings();
    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_dir(u"file", u"data-dir", u"job-queue-test-data");
    yella_settings_set_uint(u"file", u"max-spool-dbs", 10);
    yella_settings_set_byte_size(u"agent", u"max-message-size", u"1MB");
    yella_settings_set_uint(u"file", u"send-latency-seconds", 1);
    rc = cmocka_run_group_tests(tests, NULL, NULL);
    yella_destroy_settings();
    return rc;
}
