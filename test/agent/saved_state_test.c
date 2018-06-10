#include "agent/saved_state.h"
#include "common/settings.h"
#include "common/file.h"
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

static void load_new_and_save(void** targ)
{
    yella_saved_state* st = yella_load_saved_state();
    yella_saved_state* st2;
    yella_rc rc;
    int i;

    assert_non_null(st);
    rc = yella_save_saved_state(st);
    assert_int_equal(rc, YELLA_NO_ERROR);
    st2 = yella_load_saved_state();
    assert_non_null(st2);
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
}

static int set_up(void** arg)
{
    yella_remove_all(yella_settings_get_text("data-dir"));
    yella_ensure_dir_exists(yella_settings_get_text("data-dir"));
    return 0;
}

static int tear_down(void** arg)
{
    yella_remove_all(yella_settings_get_text("data-dir"));
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
    yella_settings_set_text("data-dir", "saved-state-data");
    return cmocka_run_group_tests(tests, set_up, tear_down);
}

