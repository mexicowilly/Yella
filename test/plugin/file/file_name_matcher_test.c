#include "plugin/file/file_name_matcher.h"
#include "common/macro_util.h"
#include "common/text_util.h"
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

static void check_it(const expected* ex, size_t sz)
{
    int i;
    char* utf8_n;
    char* utf8_p;
    const char* b;

    for (i = 0; i < sz; i++)
    {
        if (file_name_matches(ex[i].name, ex[i].pattern) != ex[i].result)
        {
            utf8_n = yella_to_utf8(ex[i].name);
            utf8_p = yella_to_utf8(ex[i].pattern);
            b = ex[i].result ? "true" : "false";
            fail_msg("'%s' '%s' != %s", utf8_n, utf8_p, b);
            free(utf8_p);
            free(utf8_n);
        }
    }
}

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
        { u"/my/dog", u"/?\?/?\??", true },
        { u"/my/dog", u"?\??\??\??", false }
    };

    check_it(tests, YELLA_ARRAY_SIZE(tests));
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

    check_it(tests, YELLA_ARRAY_SIZE(tests));
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

    check_it(tests, YELLA_ARRAY_SIZE(tests));
}

static void star_star(void** arg)
{
    int i;
    expected tests[] =
    {
        { u"/my/dog", u"**", true },
        { u"/my/dog/has/fleas", u"/my/**", true },
        { u"/my/dog/has/fleas", u"/my/**/fleas", true }
    };

    check_it(tests, YELLA_ARRAY_SIZE(tests));
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(question),
        cmocka_unit_test(simple),
        cmocka_unit_test(star),
        cmocka_unit_test(star_star)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
