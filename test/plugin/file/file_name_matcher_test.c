#include "plugin/file/file_name_matcher.h"
#include "common/macro_util.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

typedef struct expected
{
    const UChar* name;
    const UChar* pattern;
    bool result;
} expected;

static void simple(void** arg)
{
    int i;
    expected tests[] =
    {
        { u"/my/dog", u"/my/dog", true },
        { u"/my/dog", u"/my/cat", false },
        { u"/my/dog", u"/my/do", false },
        { u"/my/dog", u"/my/dogs", false },
        { u"", u"", true },
        { u"", u"funky monkey", false },
        { u"funky monkey", u"", false }
    };

    for (i = 0; i < YELLA_ARRAY_SIZE(tests); i++)
        assert_true(file_name_matches(tests[i].name, tests[i].pattern) == tests[i].result);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(simple)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
