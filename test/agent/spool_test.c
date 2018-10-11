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
#include "common/thread.h"
#include "agent/spool.h"
#include <chucho/configuration.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static void destroy_parts(yella_message_part* parts, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
        free(parts[i].data);
    free(parts);
}

static yella_message_part make_part(const char* const text)
{
    yella_message_part p = { (uint8_t*)text, strlen(text) + 1 };
    return p;
}

typedef struct thread_arg
{
    yella_spool* sp;
    size_t milliseconds_delay;
    size_t count;
} thread_arg;

static void empty(void** data)
{
    yella_spool* sp;
    yella_rc rc;
    yella_message_part* popped;
    size_t count_popped;

    sp = yella_create_spool();
    assert_non_null(sp);
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_int_equal(rc, YELLA_TIMED_OUT);
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_int_equal(rc, YELLA_TIMED_OUT);
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_int_equal(rc, YELLA_TIMED_OUT);
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_int_equal(rc, YELLA_TIMED_OUT);
    yella_destroy_spool(sp);
}

static void full_speed_main(void* data)
{
    thread_arg* targ = (thread_arg*)data;
    size_t i;
    yella_message_part parts[2];

    parts[0].data = malloc(sizeof(size_t));
    parts[0].size = sizeof(size_t);
    parts[1].data = malloc(sizeof(size_t));
    parts[1].size = sizeof(size_t);
    for (i = 0; i < targ->count; i++)
    {
        memcpy(parts[0].data, &i, sizeof(i));
        memcpy(parts[1].data, &i, sizeof(i));
        yella_spool_push(targ->sp, parts, 2);
        yella_sleep_this_thread(targ->milliseconds_delay);
    }
    free(parts[0].data);
    free(parts[1].data);
}

static char* stats_to_json(const yella_spool_stats* stats)
{
    int req;
    char* buf = malloc(2048);

    req = snprintf(buf, 2048, "{ \"max_partition_size\": %zu, \"max_partitions\": %zu, \"current_size\": %zu, \"largest_size\": %zu, \"files_created\": %zu, \"files_destroyed\": %zu, \"bytes_culled\": %zu, \"events_read\": %zu, \"events_written\": %zu, \"smallest_event_size\": %zu, \"largest_event_size\": %zu, \"average_event_size\": %zu, \"cull_events\": %zu }",
                   stats->max_partition_size,
                   stats->max_partitions,
                   stats->current_size,
                   stats->largest_size,
                   stats->files_created,
                   stats->files_destroyed,
                   stats->bytes_culled,
                   stats->events_read,
                   stats->events_written,
                   stats->smallest_event_size,
                   stats->largest_event_size,
                   stats->average_event_size,
                   stats->cull_events);
    buf = realloc(buf, req + 1);
    return buf;
}

static void cull(void** targ)
{
    yella_spool* sp;
    thread_arg thr_arg;
    yella_thread* thr;
    int total_popped_events;
    yella_rc rc;
    yella_message_part* popped;
    size_t count_popped;
    yella_spool_stats stats;
    char* tstats;

    yella_settings_set_uint("agent", "max-spool-partition-size", 1024 * 1024);
    yella_settings_set_uint("agent", "max-spool-partitions", 2);
    sp = yella_create_spool();
    assert_non_null(sp);
    thr_arg.milliseconds_delay = 0;
    thr_arg.count = 1000000;
    thr_arg.sp = sp;
    thr = yella_create_thread(full_speed_main, &thr_arg);
    assert_non_null(thr);
    yella_sleep_this_thread(3000);
    total_popped_events = 0;
    do
    {
        rc = yella_spool_pop(sp, 250, &popped, &count_popped);
        if (rc == YELLA_NO_ERROR)
        {
            assert_int_equal(count_popped, 2);
            destroy_parts(popped, 2);
            ++total_popped_events;
        }
    } while (rc == YELLA_NO_ERROR);
    yella_join_thread(thr);
    yella_destroy_thread(thr);
    stats = yella_spool_get_stats(sp);
    yella_destroy_spool(sp);
    assert_true(stats.bytes_culled > 0);
    assert_true(total_popped_events < 1000000);
    tstats = stats_to_json(&stats);
    print_message("Stats: %s\n", tstats);
    free(tstats);
}

static void full_speed(void** targ)
{
    yella_spool* sp;
    thread_arg thr_arg;
    yella_thread* thr;
    size_t i;
    yella_rc rc;
    yella_message_part* popped;
    size_t count_popped;
    size_t found;
    yella_spool_stats stats;
    char* tstats;

    sp = yella_create_spool();
    assert_non_null(sp);
    thr_arg.milliseconds_delay = 0;
    thr_arg.count = 1000000;
    thr_arg.sp = sp;
    thr = yella_create_thread(full_speed_main, &thr_arg);
    assert_non_null(thr);
    for (i = 0; i < 1000000; i++)
    {
        rc = yella_spool_pop(sp, 5000, &popped, &count_popped);
        assert_int_equal(rc, YELLA_NO_ERROR);
        assert_int_equal(count_popped, 2);
        assert_int_equal(popped[0].size, sizeof(size_t));
        memcpy(&found, popped[0].data, sizeof(found));
        assert_int_equal(found, i);
        assert_int_equal(popped[1].size, sizeof(size_t));
        memcpy(&found, popped[1].data, sizeof(found));
        assert_int_equal(found, i);
        destroy_parts(popped, 2);
    }
    yella_join_thread(thr);
    yella_destroy_thread(thr);
    stats = yella_spool_get_stats(sp);
    yella_destroy_spool(sp);
    tstats = stats_to_json(&stats);
    print_message("Stats: %s\n", tstats);
    free(tstats);
}

static void pick_up(void** targ)
{
    yella_spool* sp;
    thread_arg thr_arg;
    yella_thread* thr;
    yella_message_part part;
    yella_rc rc;
    size_t i;
    yella_message_part* popped;
    size_t count_popped;
    size_t found;
    yella_spool_stats stats;
    char* tstats;

    sp = yella_create_spool();
    assert_non_null(sp);
    thr_arg.milliseconds_delay = 0;
    thr_arg.count = 100000;
    thr_arg.sp = sp;
    thr = yella_create_thread(full_speed_main, &thr_arg);
    yella_join_thread(thr);
    yella_destroy_thread(thr);
    for (i = 0; i < 50000; i++)
    {
        rc = yella_spool_pop(sp, 250, &popped, &count_popped);
        assert_int_equal(rc, YELLA_NO_ERROR);
        assert_int_equal(count_popped, 2);
        assert_int_equal(popped[0].size, sizeof(size_t));
        memcpy(&found, popped[0].data, sizeof(found));
        assert_int_equal(found, i);
        assert_int_equal(popped[1].size, sizeof(size_t));
        memcpy(&found, popped[1].data, sizeof(found));
        assert_int_equal(found, i);
        destroy_parts(popped, 2);
    }
    part = make_part("My dog has fleas");
    rc = yella_spool_push(sp, &part, 1);
    assert_int_equal(rc, YELLA_NO_ERROR);
    yella_destroy_spool(sp);
    sp = yella_create_spool();
    assert_non_null(sp);
    for (; i < 100000; i++)
    {
        rc = yella_spool_pop(sp, 250, &popped, &count_popped);
        assert_int_equal(rc, YELLA_NO_ERROR);
        assert_int_equal(count_popped, 2);
        assert_int_equal(popped[0].size, sizeof(size_t));
        memcpy(&found, popped[0].data, sizeof(found));
        assert_int_equal(found, i);
        assert_int_equal(popped[1].size, sizeof(size_t));
        memcpy(&found, popped[1].data, sizeof(found));
        assert_int_equal(found, i);
        destroy_parts(popped, 2);
    }
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_int_equal(rc, YELLA_NO_ERROR);
    assert_int_equal(count_popped, 1);
    assert_string_equal((char*)popped->data, "My dog has fleas");
    destroy_parts(popped, 1);
    rc = yella_spool_pop(sp, 250, &popped, &count_popped);
    assert_int_equal(rc, YELLA_TIMED_OUT);
    stats = yella_spool_get_stats(sp);
    yella_destroy_spool(sp);
    tstats = stats_to_json(&stats);
    print_message("Stats: %s\n", tstats);
    free(tstats);
}

static void simple(void** targ)
{
    yella_spool* sp;
    yella_message_part one[] = { make_part("This is one") };
    yella_message_part two[] = { make_part("One of two"), make_part("Two of two") };
    yella_message_part three[] = { make_part("One of three"), make_part("Two of three"), make_part("Three of three") };
    yella_rc rc;
    yella_message_part* popped;
    size_t count_popped;
    yella_spool_stats stats;
    char* tstats;

    sp = yella_create_spool();
    assert_non_null(sp);
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
    stats = yella_spool_get_stats(sp);
    tstats = stats_to_json(&stats);
    print_message("Stats: %s\n", tstats);
    free(tstats);
    yella_destroy_spool(sp);
}

static int clean_settings(void** arg)
{
    yella_destroy_settings();
    return 0;
}

static int init_test(void **arg)
{
    yella_remove_all(yella_settings_get_text("agent", "spool-dir"));
    yella_settings_set_uint("agent", "max-spool-partition-size", 1024 * 1024);
    yella_settings_set_uint("agent", "max-spool-partitions", 100);
    return 0;
}

static int init_settings(void** arg)
{
    chucho_cnf_set_fallback(
"chucho::logger:\n"
"    name: <root>\n"
"    level: trace\n"
"    chucho::cout_writer:\n"
"        chucho::pattern_formatter:\n"
"            pattern: '%-5p %5r %b:%L] %m%n'\n");
    yella_initialize_settings();
    yella_settings_set_text("agent", "spool-dir", "test-spool");
    return 0;
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup_teardown(simple, init_test, NULL),
        cmocka_unit_test_setup_teardown(full_speed, init_test, NULL),
        cmocka_unit_test_setup_teardown(cull, init_test, NULL),
        cmocka_unit_test_setup_teardown(pick_up, init_test, NULL),
        cmocka_unit_test_setup_teardown(empty, init_test, NULL)
    };

    return cmocka_run_group_tests(tests, init_settings, clean_settings);
}