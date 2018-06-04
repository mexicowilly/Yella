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

static void destroy_parts(yella_msg_part* parts, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
        free(parts[i].data);
    free(parts);
}

static yella_msg_part make_part(const char* const text)
{
    yella_msg_part p = { (uint8_t*)text, strlen(text) + 1 };
    return p;
}

static void simple()
{
    yella_spool* sp;
    yella_msg_part one[] = { make_part("This is one") };
    yella_msg_part two[] = { make_part("One of two"), make_part("Two of two") };
    yella_msg_part three[] = { make_part("One of three"), make_part("Two of three"), make_part("Three of three") };
    yella_rc rc;
    yella_msg_part* popped;
    size_t count_popped;

    sp = yella_create_spool();
    rc = yella_spool_push(sp, one, 1);
    assert_true(rc == YELLA_NO_ERROR);
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_int_equal(rc, YELLA_NO_ERROR);
    assert_int_equal(count_popped, 1);
    assert_int_equal(popped->size, 12);
    assert_string_equal(popped->data, "This is one");
    destroy_parts(popped, count_popped);
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_int_equal(rc, YELLA_TIMED_OUT);
    rc = yella_spool_push(sp, two, 2);
    assert_true(rc == YELLA_NO_ERROR);
    rc = yella_spool_push(sp, three, 3);
    assert_true(rc == YELLA_NO_ERROR);
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_true(rc == YELLA_NO_ERROR);
    assert_int_equal(count_popped, 2);
    assert_int_equal(popped[0].size, 11);
    assert_string_equal(popped[0].data, "One of two");
    assert_int_equal(popped[1].size, 11);
    assert_string_equal(popped[1].data, "Two of two");
    destroy_parts(popped, count_popped);
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_int_equal(rc, YELLA_NO_ERROR);
    assert_int_equal(count_popped, 3);
    assert_int_equal(popped[0].size, 13);
    assert_string_equal(popped[0].data, "One of three");
    assert_int_equal(popped[1].size, 13);
    assert_string_equal(popped[1].data, "Two of three");
    assert_int_equal(popped[2].size, 15);
    assert_string_equal(popped[2].data, "Three of three");
    destroy_parts(popped, count_popped);
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_int_equal(rc, YELLA_TIMED_OUT);
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
        cmocka_unit_test_setup_teardown(simple, clean_spool, NULL)
    };

#if defined(YELLA_POSIX)
    setenv("CMOCKA_TEST_ABORT", "1", 1);
#endif
    yella_settings_set_text("spool-dir", "test-spool");
    yella_settings_set_uint("max-spool-partition", 1024 * 1024);
    yella_settings_set_uint("max-total-spool", 100 * 1024 * 1024);
    return cmocka_run_group_tests(tests, NULL, NULL);
}