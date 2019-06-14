#include "plugin/file/job_queue.h"
#include "plugin/file/job.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

void __wrap_run_job(const job* const j, state_db_pool* db_pool)
{
}

void simple(void** arg)
{

}

int main()
{
    const struct CMUnitTest tests[] =
    {
    };

//    yella_load_settings_doc();
//    yella_destroy_settings_doc();
//    yella_settings_set_dir(u"file", u"data-dir", u"state-db-test-data");
//    yella_remove_all(yella_settings_get_dir(u"file", u"data-dir"));
//    yella_settings_set_uint(u"file", u"max-spool-dbs", 10);
//    yella_settings_set_byte_size(u"agent", u"max-message-size", u"1MB");
//    yella_settings_set_uint(u"file", u"send-latency-seconds", 1);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
