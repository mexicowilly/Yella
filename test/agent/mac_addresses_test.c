#include "agent/mac_addresses.h"
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

static void print_mac_addrs(void** targ)
{
    yella_mac_addresses* addrs = yella_get_mac_addresses();
    size_t i;

    assert_non_null(addrs);
    assert_true(addrs->count > 0);
    for (i = 0; i < addrs->count; i++)
        print_message("%s\n", addrs->addrs[i].text);
    yella_destroy_mac_addresses(addrs);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(print_mac_addrs)
    };

#if defined(YELLA_POSIX)
    setenv("CMOCKA_TEST_ABORT", "1", 1);
#endif
    return cmocka_run_group_tests(tests, NULL, NULL);
}
