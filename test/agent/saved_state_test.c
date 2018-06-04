#include "agent/saved_state.h"
#include "common/settings.h"
#include "common/file.h"
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

static void load_new(void** targ)
{
    yella_saved_state* st = yella_load_saved_state();

    assert_non_null(st);
    yella_destroy_saved_state(st);
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
        cmocka_unit_test(load_new)
    };

#if defined(YELLA_POSIX)
    setenv("CMOCKA_TEST_ABORT", "1", 1);
#endif
    yella_settings_set_text("data-dir", "saved-state-data");
    return cmocka_run_group_tests(tests, set_up, tear_down);
}

