#include "plugin/file/file_name_matcher.h"
#include "common/macro_util.h"
#include "common/text_util.h"
#include <stdio.h>
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

static void escaped(void** arg)
{
    int i;
    expected tests[] =
    {
        { u"/my/d?g", u"/my/d\\?g", true },
        { u"/my/*g", u"/my/\\*g", true },
        { u"/my/*g", u"/my/\\*?", true },
        { u"/my/\\og", u"/my/\\\\og", true }
    };

    check_it(tests, YELLA_ARRAY_SIZE(tests));
}

static void first_unescaped_special(void** arg)
{
    int i;
    const UChar* found;
    struct
    {
        const UChar* pattern;
        const UChar* result;
    } tests[] =
    {
        { u"*", u"*" },
        { u"dogs?*", u"?*" },
        { u"d\\*gs?end", u"?end" },
        { u"d\\\\*gs?end", u"*gs?end" },
        { u"\\d*gs?end", u"*gs?end" },
        { u"\\*dogs?end", u"?end" },
        { u"dogs\\?end", NULL },
        { u"\\\\\\*dogs?end", u"?end" },
        { u"/my/dog/has/fleas", NULL },
        { u"\\\\\\\\*dogs?end", u"*dogs?end" },
        { u"dogsend\\?", NULL },
        { u"dogsend\\\\?", u"?" }
    };

    for (i = 0; i < YELLA_ARRAY_SIZE(tests); i++)
    {
        found = first_unescaped_special_char(tests[i].pattern);
        assert_true((found == NULL && tests[i].result == NULL) ||
                    u_strcmp(found, tests[i].result) == 0);
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
        { u"/my/dog/has", u"/my/*/has not", false },
        { u"", u"*", true }
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
        { u"/my/dog/has/fleas", u"/my/**/fleas", true },
        { u"/my/dog/has/fleas", u"**/fleas", true },
        { u"/my/dog/has/fleas", u"/my**/fleas", true },
        { u"/my/dog/has/fleas", u"/my/**/*", true },
        { u"/my/dog/has/fleas", u"/my/**/*s", true },
        { u"/my/dog/has/fleas", u"**fleas", false },
        { u"/my/dog/has/fleas", u"/**fleas", false },
        { u"/my/dog/has/fleas", u"/**/f?ea*", true },
        { u"/my/dog/has/fleas", u"/m?/**/f?ea*", true },
        { u"fleas", u"**/fleas", true }
    };

    check_it(tests, YELLA_ARRAY_SIZE(tests));
}

static void unescape(void** arg)
{
    int i;
    uds unesc;
    struct
    {
        const UChar* pattern;
        const UChar* result;
    } tests[] =
    {
        { u"/my/dog", u"/my/dog" },
        { u"\\*/my/dog", u"*/my/dog" },
        { u"/my/dog\\\\", u"/my/dog\\" },
        { u"/my\\/dog", u"/my/dog" },
        { u"/my/dog\\", u"/my/dog" }
    };

    for (i = 0; i < YELLA_ARRAY_SIZE(tests); i++)
    {
        unesc = unescape_pattern(tests[i].pattern);
        assert_true(unesc != NULL && u_strcmp(unesc, tests[i].result) == 0);
        udsfree(unesc);
    }
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(escaped),
        cmocka_unit_test(first_unescaped_special),
        cmocka_unit_test(question),
        cmocka_unit_test(simple),
        cmocka_unit_test(star),
        cmocka_unit_test(star_star),
        cmocka_unit_test(unescape)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
