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

typedef struct test_data
{
    event_source* esrc;
    uds dir_name;
} test_data;

static void file_changed(const UChar* const config_name, const UChar* const fname, void* udata)
{
    test_data* td;
    char* c;
    char* f;

    td = udata;
    c = yella_to_utf8(config_name);
    f = yella_to_utf8(fname);
    print_message("Received '%s': '%s'\n", c, f);
    free(c);
    free(f);
}

static void one_file(void** arg)
{
    test_data* td;
    event_source_spec* spec;
    UFILE* f;

    td = *arg;
    spec = malloc(sizeof(event_source_spec));
    spec->name = udsnew(u"doggies");
    spec->includes = yella_create_uds_ptr_vector();
    spec->excludes = yella_create_uds_ptr_vector();
    yella_push_back_ptr_vector(spec->includes, udscat(td->dir_name, u"one"));
    add_or_replace_event_source_spec(td->esrc, spec);
    yella_sleep_this_thread(250);
    f = u_fopen_u(yella_ptr_vector_at(spec->includes, 0), "w", NULL, NULL);
    u_fclose(f);
    yella_sleep_this_thread(2000);
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
    *arg = td;
    return 0;
}

static int tear_down(void** arg)
{
    test_data* td;

    td = *arg;
    destroy_event_source(td->esrc);
    free(td);
    return 0;
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup_teardown(one_file, set_up, tear_down)
    };

    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_uint(u"file", u"fs-monitor-latency-seconds", 1);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
