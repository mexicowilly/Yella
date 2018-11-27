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

static void question(void** arg)
{
    int i;
    expected tests[] =
    {
        { u"/my/dog", u"/m?/dog", true },
        { u"/my/dog", u"/my?dog", false },
        { u"/my/dog", u"/my/do??", false },
        { u"/my/dog", u"?my/dog", false },
        { u"/my/dog", u"/my/do?", true },
        { u"/my/dog", u"?my/do?", false },
        { u"/my/dog", u"/??/???", true },
        { u"/my/dog", u"???????", false }
    };

    for (i = 0; i < YELLA_ARRAY_SIZE(tests); i++)
        assert_true(file_name_matches(tests[i].name, tests[i].pattern) == tests[i].result);

}

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
        { u"funky monkey", u"", false },
        { u"/my/dog", u"/your/dog", false }
    };

    for (i = 0; i < YELLA_ARRAY_SIZE(tests); i++)
        assert_true(file_name_matches(tests[i].name, tests[i].pattern) == tests[i].result);
}

static void star(void** arg)
{
    int i;
    expected tests[] =
    {
        { u"/my/dog", u"/my/*", true },
        { u"/my/dog", u"/*/*", true },
        { u"/my/dog", u"*", false },
        { u"/my/dog", u"*my/dog", false },
        { u"/my/dog", u"/my/*g", true },
        { u"/my/dog", u"/my/*t", false },
        { u"/my/dog", u"/my/*gg", false },
        { u"/my/dog", u"/my/d*g", true },
        { u"/my/dog", u"/*/dog", true },
        { u"/my/dog", u"*/dog", false },
        { u"/my/dog/has", u"/my/*/has", true },
        { u"/my/dog/has", u"/my/*", false },
        { u"/my/dog/has", u"/my/*/has not", false }
    };

    for (i = 0; i < YELLA_ARRAY_SIZE(tests); i++)
        assert_true(file_name_matches(tests[i].name, tests[i].pattern) == tests[i].result);
}

static void star_star(void** arg)
{
    int i;
    expected tests[] =
    {
//        { u"/my/dog", u"**", true },
//        { u"/my/dog/has/fleas", u"/my/**", true }
        { u"/my/dog/has/fleas", u"/my/**/fleas", true }
    };

    for (i = 0; i < YELLA_ARRAY_SIZE(tests); i++)
        assert_true(file_name_matches(tests[i].name, tests[i].pattern) == tests[i].result);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
//        cmocka_unit_test(question),
//        cmocka_unit_test(simple),
//        cmocka_unit_test(star)
        cmocka_unit_test(star_star)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
