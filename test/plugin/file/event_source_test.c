#include "plugin/file/event_source.h"
#include "common/settings.h"
#include "common/file.h"
#include "common/text_util.h"
#include <unicode/ustdio.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

typedef struct excpected
{
    uds config_name;
    uds file_name;
    yella_event* pause_evt;
} expected;

typedef struct test_data
{
    event_source* esrc;
    uds dir_name;
    yella_mutex* guard;
    yella_condition_variable* cond;
    yella_ptr_vector* exp;
} test_data;

static void check_exp(yella_ptr_vector* exp)
{
    unsigned i;
    char* c;
    char* f;
    expected* cur;

    for (i = 0; i < yella_ptr_vector_size(exp); i++)
    {
        cur = yella_ptr_vector_at(exp, i);
        c = yella_to_utf8(cur->config_name);
        f = yella_to_utf8(cur->file_name);
        print_message("Unexpected: config '%s', file '%s'\n", c, f);
        free(c);
        free(f);
    }
    assert_int_equal(i, 0);
}

static void expected_destructor(void* elem, void* udata)
{
    expected* exp;

    exp = elem;
    udsfree(exp->config_name);
    udsfree(exp->file_name);
}

static void file_changed(const UChar* const config_name, const UChar* const fname, void* udata)
{
    test_data* td;
    char* c;
    char* f;
    size_t i;
    expected* cur;
    bool got_one;

    td = udata;
    c = yella_to_utf8(config_name);
    f = yella_to_utf8(fname);
    print_message("Received '%s': '%s'\n", c, f);
    free(c);
    free(f);
    got_one = false;
    yella_lock_mutex(td->guard);
    for (i = 0; i < yella_ptr_vector_size(td->exp); i++)
    {
        cur = yella_ptr_vector_at(td->exp, i);
        if (u_strcmp(cur->config_name, config_name) == 0 && u_strcmp(cur->file_name, fname) == 0)
        {
            if (cur->pause_evt != NULL)
                pause_event_source(td->esrc);
            yella_erase_ptr_vector_at(td->exp, i);
            if (yella_ptr_vector_size(td->exp) == 0)
                yella_signal_condition_variable(td->cond);
            got_one = true;
            break;
        }
    }
    yella_unlock_mutex(td->guard);
    assert_true(got_one);
}

static void touch_file(const UChar* const fname)
{
    UFILE* f;
    char* utf8;

    utf8 = yella_to_utf8(fname);
    print_message("Touching file '%s'\n", utf8);
    free(utf8);
    f = u_fopen_u(fname, "w", NULL, NULL);
    u_fclose(f);
}

static void clear(void** arg)
{
    test_data* td;
    event_source_spec* spec;
    expected* exp;
    uds fname;

    td = *arg;
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"config 1");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"one");
    yella_push_back_ptr_vector(spec->includes, fname);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(fname);
    yella_push_back_ptr_vector(td->exp, exp);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"config 2");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"two");
    yella_push_back_ptr_vector(spec->includes, fname);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(fname);
    yella_push_back_ptr_vector(td->exp, exp);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    yella_sleep_this_thread_milliseconds(250);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 0))->file_name);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 1))->file_name);
    yella_lock_mutex(td->guard);
    assert_true(yella_wait_milliseconds_for_condition_variable(td->cond, td->guard, 2000));
    check_exp(td->exp);
    yella_unlock_mutex(td->guard);
    clear_event_source_specs(td->esrc);
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"one");
    yella_remove_file(fname);
    udsfree(fname);
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"two");
    yella_remove_file(fname);
    udsfree(fname);
    yella_sleep_this_thread_milliseconds(500);
}

static void dir(void** arg)
{
    test_data* td;
    event_source_spec* spec;
    UFILE* f;
    expected* exp;
    uds inc;

    td = *arg;
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"iguanas");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    inc = udsdup(td->dir_name);
    inc = udscat(inc, u"*");
    yella_push_back_ptr_vector(spec->includes, inc);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(td->dir_name);
    exp->file_name = udscat(exp->file_name, u"one");
    yella_push_back_ptr_vector(td->exp, exp);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(td->dir_name);
    exp->file_name = udscat(exp->file_name, u"two");
    yella_push_back_ptr_vector(td->exp, exp);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    yella_sleep_this_thread_milliseconds(250);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 0))->file_name);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 1))->file_name);
    yella_lock_mutex(td->guard);
    assert_true(yella_wait_milliseconds_for_condition_variable(td->cond, td->guard, 2000));
    check_exp(td->exp);
    yella_unlock_mutex(td->guard);
}

static void duplicates(void** arg)
{
    test_data* td;
    event_source_spec* spec;
    UFILE* f;
    expected* exp;
    uds inc;

    td = *arg;
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"iguanas");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    inc = udsdup(td->dir_name);
    inc = udscat(inc, u"*");
    yella_push_back_ptr_vector(spec->includes, inc);
    yella_push_back_ptr_vector(spec->includes, udsdup(inc));
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(td->dir_name);
    exp->file_name = udscat(exp->file_name, u"one");
    yella_push_back_ptr_vector(td->exp, exp);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(td->dir_name);
    exp->file_name = udscat(exp->file_name, u"two");
    yella_push_back_ptr_vector(td->exp, exp);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    yella_sleep_this_thread_milliseconds(250);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 0))->file_name);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 1))->file_name);
    yella_lock_mutex(td->guard);
    assert_true(yella_wait_milliseconds_for_condition_variable(td->cond, td->guard, 2000));
    check_exp(td->exp);
    yella_unlock_mutex(td->guard);

}

static void exclude(void** arg)
{
    test_data* td;
    event_source_spec* spec;
    UFILE* f;
    expected* exp;
    uds inc;
    uds ex;

    td = *arg;
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"monkees");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    inc = udsdup(td->dir_name);
    inc = udscat(inc, u"*");
    yella_push_back_ptr_vector(spec->includes, inc);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(td->dir_name);
    exp->file_name = udscat(exp->file_name, u"one");
    yella_push_back_ptr_vector(td->exp, exp);
    ex = udsdup(td->dir_name);
    ex = udscat(ex, u"two");
    yella_push_back_ptr_vector(spec->excludes, ex);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    yella_sleep_this_thread_milliseconds(250);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 0))->file_name);
    touch_file(ex);
    yella_lock_mutex(td->guard);
    assert_true(yella_wait_milliseconds_for_condition_variable(td->cond, td->guard, 2000));
    check_exp(td->exp);
    yella_unlock_mutex(td->guard);
}

static void multiple_configs(void** arg)
{
    test_data* td;
    event_source_spec* spec;
    expected* exp;
    uds fname;

    td = *arg;
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"config 1");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"one");
    yella_push_back_ptr_vector(spec->includes, fname);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(fname);
    yella_push_back_ptr_vector(td->exp, exp);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"config 2");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"two");
    yella_push_back_ptr_vector(spec->includes, fname);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(fname);
    yella_push_back_ptr_vector(td->exp, exp);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    yella_sleep_this_thread_milliseconds(250);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 0))->file_name);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 1))->file_name);
    yella_lock_mutex(td->guard);
    assert_true(yella_wait_milliseconds_for_condition_variable(td->cond, td->guard, 2000));
    check_exp(td->exp);
    yella_unlock_mutex(td->guard);

}

static void one_file(void** arg)
{
    test_data* td;
    event_source_spec* spec;
    expected* exp;
    uds fname;

    td = *arg;
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"doggies");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"one");
    yella_push_back_ptr_vector(spec->includes, fname);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(yella_ptr_vector_at(spec->includes, 0));
    yella_push_back_ptr_vector(td->exp, exp);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    yella_sleep_this_thread_milliseconds(250);
    touch_file(yella_ptr_vector_at(spec->includes, 0));
    yella_lock_mutex(td->guard);
    assert_true(yella_wait_milliseconds_for_condition_variable(td->cond, td->guard, 2000));
    check_exp(td->exp);
    yella_unlock_mutex(td->guard);
}

static void pause_resume(void** arg)
{
    test_data* td;
    event_source_spec* spec;
    expected* exp;
    uds fname;

    td = *arg;
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"doggies");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"one");
    yella_push_back_ptr_vector(spec->includes, fname);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(yella_ptr_vector_at(spec->includes, 0));
    exp->pause_evt = yella_create_event();
    yella_push_back_ptr_vector(td->exp, exp);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    yella_sleep_this_thread_milliseconds(250);
    touch_file(yella_ptr_vector_at(spec->includes, 0));
    yella_lock_mutex(td->guard);
    assert_true(yella_wait_milliseconds_for_condition_variable(td->cond, td->guard, 2000));
    check_exp(td->exp);
    yella_unlock_mutex(td->guard);
    wait_for_event_source_pause(td->esrc);
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"one");
    yella_remove_file(fname);
    resume_event_source(td->esrc);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(yella_ptr_vector_at(spec->includes, 0));
    yella_push_back_ptr_vector(td->exp, exp);
    touch_file(yella_ptr_vector_at(spec->includes, 0));
    yella_lock_mutex(td->guard);
    assert_true(yella_wait_milliseconds_for_condition_variable(td->cond, td->guard, 2000));
    check_exp(td->exp);
    yella_unlock_mutex(td->guard);
}

static void remove_one(void** arg)
{
    test_data* td;
    event_source_spec* spec;
    expected* exp;
    uds fname;

    td = *arg;
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"config 1");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"one");
    yella_push_back_ptr_vector(spec->includes, fname);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(fname);
    yella_push_back_ptr_vector(td->exp, exp);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"config 2");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"two");
    yella_push_back_ptr_vector(spec->includes, fname);
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsdup(spec->name);
    exp->file_name = udsdup(fname);
    yella_push_back_ptr_vector(td->exp, exp);
    add_or_replace_event_source_specs(td->esrc, &spec, 1);
    yella_sleep_this_thread_milliseconds(250);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 0))->file_name);
    touch_file(((expected*)yella_ptr_vector_at(td->exp, 1))->file_name);
    yella_lock_mutex(td->guard);
    assert_true(yella_wait_milliseconds_for_condition_variable(td->cond, td->guard, 2000));
    check_exp(td->exp);
    yella_unlock_mutex(td->guard);
    remove_event_source_spec(td->esrc, u"config 1");
    exp = calloc(1, sizeof(expected));
    exp->config_name = udsnew(u"config 2");
    exp->file_name = udsdup(fname);
    yella_push_back_ptr_vector(td->exp, exp);
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"one");
    yella_remove_file(fname);
    udsfree(fname);
    fname = udsdup(td->dir_name);
    fname = udscat(fname, u"two");
    yella_remove_file(fname);
    udsfree(fname);
    yella_lock_mutex(td->guard);
    assert_true(yella_wait_milliseconds_for_condition_variable(td->cond, td->guard, 2000));
    check_exp(td->exp);
    yella_unlock_mutex(td->guard);
}

static int set_up(void** arg)
{
    test_data* td;
    UChar* cur_dir;
    const UChar* sep;

    td = malloc(sizeof(test_data));
    td->esrc = create_event_source(file_changed, td);
    assert_non_null(td->esrc);
    cur_dir = yella_getcwd();
    if (u_strlen(cur_dir) > 0 && cur_dir[u_strlen(cur_dir) - 1] != YELLA_DIR_SEP[0])
        sep = YELLA_DIR_SEP;
    else
        sep = u"";
    td->dir_name = udscatprintf(udsempty(), u"%S%Sevent-source-test-data%S", cur_dir, sep, YELLA_DIR_SEP);
    free(cur_dir);
    yella_remove_all(td->dir_name);
    yella_ensure_dir_exists(td->dir_name);
    td->guard = yella_create_mutex();
    td->cond = yella_create_condition_variable();
    td->exp = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(td->exp, expected_destructor, NULL);
    *arg = td;
    return 0;
}

static int tear_down(void** arg)
{
    test_data* td;

    td = *arg;
    destroy_event_source(td->esrc);
    yella_destroy_condition_variable(td->cond);
    yella_destroy_mutex(td->guard);
    yella_destroy_ptr_vector(td->exp);
    free(td);
    return 0;
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup_teardown(one_file, set_up, tear_down),
        cmocka_unit_test_setup_teardown(dir, set_up, tear_down),
        cmocka_unit_test_setup_teardown(exclude, set_up, tear_down),
        cmocka_unit_test_setup_teardown(duplicates, set_up, tear_down),
        cmocka_unit_test_setup_teardown(multiple_configs, set_up, tear_down),
        cmocka_unit_test_setup_teardown(remove_one, set_up, tear_down),
        cmocka_unit_test_setup_teardown(clear, set_up, tear_down),
        cmocka_unit_test_setup_teardown(pause_resume, set_up, tear_down)
    };

    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_uint(u"file", u"fs-monitor-latency-seconds", 1);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
