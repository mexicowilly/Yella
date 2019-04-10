#include "plugin/file/state_db_pool.h"
#include "common/settings.h"
#include "common/file.h"
#include "common/uds_util.h"
#include "common/thread.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>
#include <unicode/ustring.h>

typedef struct pool_test
{
    state_db_pool* pool;
    /* This is uds */
    yella_ptr_vector* configs;
} pool_test;

static int set_up(void** arg)
{
    pool_test* pt;

    yella_remove_all(yella_settings_get_text(u"file", u"data-dir"));
    pt = malloc(sizeof(pool_test));
    pt->pool = create_state_db_pool();
    pt->configs = yella_create_uds_ptr_vector();
    *arg = pt;
    return 0;
}

static int tear_down(void** arg)
{
    pool_test* pt;
    size_t i;

    pt = *arg;
    /* This has to be done this way, because destroy_state_db_pool will retain
     * all the DB files, and remove_state_db_from_pool does not retain files. */
    assert_int_equal(state_db_pool_size(pt->pool), yella_ptr_vector_size(pt->configs));
    for (i = 0; i < yella_ptr_vector_size(pt->configs); i++)
        remove_state_db_from_pool(pt->pool, yella_ptr_vector_at(pt->configs, i));
    assert_int_equal(state_db_pool_size(pt->pool), 0);
    destroy_state_db_pool(pt->pool);
    yella_destroy_ptr_vector(pt->configs);
    free(pt);
    return 0;
}

static void get(void** arg)
{
    pool_test* pt;
    state_db* db;

    pt = *arg;
    db = get_state_db_from_pool(pt->pool, u"doggies");
    assert_non_null(db);
    yella_push_back_ptr_vector(pt->configs, udsnew(u"doggies"));
    assert_int_equal(u_strcmp(state_db_name(db), u"doggies"), 0);
}

static void exceed_max(void** arg)
{
    pool_test* pt;
    state_db* db;

    pt = *arg;
    db = get_state_db_from_pool(pt->pool, u"one");
    assert_non_null(db);
    assert_int_equal(state_db_pool_size(pt->pool), 1);
    assert_int_equal(u_strcmp(state_db_name(db), u"one"), 0);
    yella_sleep_this_thread(100);
    db = get_state_db_from_pool(pt->pool, u"two");
    assert_non_null(db);
    assert_int_equal(state_db_pool_size(pt->pool), 2);
    assert_int_equal(u_strcmp(state_db_name(db), u"two"), 0);
    yella_push_back_ptr_vector(pt->configs, udsnew(u"two"));
    yella_sleep_this_thread(100);
    db = get_state_db_from_pool(pt->pool, u"three");
    assert_non_null(db);
    assert_int_equal(state_db_pool_size(pt->pool), 2);
    assert_int_equal(u_strcmp(state_db_name(db), u"three"), 0);
    remove_state_db_from_pool(pt->pool, u"three");
    assert_int_equal(state_db_pool_size(pt->pool), 1);
    db = get_state_db_from_pool(pt->pool, u"one");
    assert_non_null(db);
    assert_int_equal(state_db_pool_size(pt->pool), 2);
    assert_int_equal(u_strcmp(state_db_name(db), u"one"), 0);
    yella_push_back_ptr_vector(pt->configs, udsnew(u"one"));
}

static void repeated(void** arg)
{
    pool_test* pt;
    state_db* db;
    int i;

    pt = *arg;
    for (i = 0; i < 3; i++)
    {
        db = get_state_db_from_pool(pt->pool, u"one");
        assert_non_null(db);
        assert_int_equal(state_db_pool_size(pt->pool), 1);
        assert_int_equal(u_strcmp(state_db_name(db), u"one"), 0);
    }
    yella_push_back_ptr_vector(pt->configs, udsnew(u"one"));
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup_teardown(get, set_up, tear_down),
        cmocka_unit_test_setup_teardown(exceed_max, set_up, tear_down),
        cmocka_unit_test_setup_teardown(repeated, set_up, tear_down)
    };

    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_text(u"file", u"data-dir", u"state-db-pool-test-data");
    yella_settings_set_uint(u"file", u"max-spool-dbs", 2);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
