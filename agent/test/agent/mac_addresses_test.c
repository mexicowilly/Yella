#include "agent/mac_addresses.h"
#include "common/text_util.h"
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

static void print_mac_addrs(void** targ)
{
    yella_mac_addresses* addrs;
    size_t i;
    chucho_logger_t* lgr;
    char* utf8;

    lgr = chucho_get_logger("mac_addrs");
    addrs = yella_get_mac_addresses(lgr);
    assert_non_null(addrs);
    assert_true(addrs->count > 0);
    for (i = 0; i < addrs->count; i++)
    {
        utf8 = yella_to_utf8(addrs->addrs[i].text);
        print_message("%s\n", utf8);
        free(utf8);
    }
    yella_destroy_mac_addresses(addrs);
    chucho_release_logger(lgr);
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
