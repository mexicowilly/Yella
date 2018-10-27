#include "agent/saved_state.h"
#include "common/settings.h"
#include "common/file.h"
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

static void load_new_and_save(void** targ)
{
    yella_saved_state* st;
    yella_saved_state* st2;
    yella_rc rc;
    int i;
    chucho_logger_t* lgr = NULL;

    lgr = chucho_get_logger("saved_state");
    st = yella_load_saved_state(lgr);
    assert_non_null(st);
    assert_int_equal(st->boot_count, 1);
    rc = yella_save_saved_state(st, lgr);
    assert_int_equal(rc, YELLA_NO_ERROR);
    st2 = yella_load_saved_state(lgr);
    assert_non_null(st2);
    assert_int_equal(st2->boot_count, 2);
    assert_memory_equal(st2->id->id, st->id->id, sizeof(st2->id->id));
    assert_int_equal(st2->mac_addresses->count, st->mac_addresses->count);
    for (i = 0; i < st2->mac_addresses->count; i++)
    {
        assert_memory_equal(st2->mac_addresses->addrs[i].addr,
                            st->mac_addresses->addrs[i].addr,
                            sizeof(st->mac_addresses->addrs[i].addr));
    }
    yella_destroy_saved_state(st);
    yella_destroy_saved_state(st2);
    chucho_release_logger(lgr);
}

static int set_up(void** arg)
{
    yella_initialize_settings();
    yella_settings_set_text(u"agent", u"data-dir", u"saved-state-data");
    yella_remove_all(yella_settings_get_text(u"agent", u"data-dir"));
    yella_ensure_dir_exists(yella_settings_get_text(u"agent", u"data-dir"));
    return 0;
}

static int tear_down(void** arg)
{
    yella_remove_all(yella_settings_get_text(u"agent", u"data-dir"));
    yella_destroy_settings();
    return 0;
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(load_new_and_save)
    };

#if defined(YELLA_POSIX)
    setenv("CMOCKA_TEST_ABORT", "1", 1);
#endif
    return cmocka_run_group_tests(tests, set_up, tear_down);
}

