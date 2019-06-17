#include "plugin/file/job_queue.h"
#include "plugin/file/job.h"
#include "common/file.h"
#include "common/settings.h"
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

void __wrap_run_job(const job* const j, state_db_pool* db_pool)
{
}

static int set_up(void** arg)
{
    test_data* td;

    yella_remove_all(yella_settings_get_dir(u"file", u"data-dir"));
    td = calloc(1, sizeof(test_data));
    td->pool = create_state_db_pool();
    td->jq = create_job_queue(td->pool);
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

void one(void** arg)
{
    test_data* td;
    size_t sz;
    job* j;
    job_queue_stats stats;

    td = *arg;
    j = create_job(u"one", u"monkey boy", td->acc);
    sz = push_job_queue(td->jq, j);
    assert_int_equal(sz, 1);
    log_job_queue_stats(td->jq, td->lgr);
    stats = get_job_queue_stats(td->jq);
    assert_int_equal(stats.jobs_run, 1);
    assert_int_equal(stats.jobs_pushed, 1);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup_teardown(one, set_up, tear_down)
    };

    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_dir(u"file", u"data-dir", u"job-queue-test-data");
    yella_settings_set_uint(u"file", u"max-spool-dbs", 10);
    yella_settings_set_byte_size(u"agent", u"max-message-size", u"1MB");
    yella_settings_set_uint(u"file", u"send-latency-seconds", 1);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
