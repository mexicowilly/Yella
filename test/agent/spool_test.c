/*
 * Copyright 2016 Will Mason
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "common/settings.h"
#include "common/file.h"
#include "agent/spool.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static void simple()
{
    yella_spool* sp = yella_create_spool(1);

    yella_destroy_spool(sp);
}

static int clean_spool(void** arg)
{
    yella_remove_all(yella_settings_get_text("spool-dir"));
    return 0;
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup_teardown(simple, clean_spool, clean_spool)
    };

#if defined(YELLA_POSIX)
    setenv("CMOCKA_TEST_ABORT", "1", 1);
#endif
    yella_settings_set_text("spool-dir", "test-spool");
    yella_settings_set_uint("max-spool-partition", 1024 * 1024);
    yella_settings_set_uint("max-total-spool", 100 * 1024 * 1024);
    return cmocka_run_group_tests(tests, NULL, NULL);
}