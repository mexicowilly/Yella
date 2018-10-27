/*
 * Copyright 2016 Will Mason
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "common/settings.h"
#include <unicode/ustring.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>

static void write_config(const char* const fname, const char* const content)
{
    FILE* f;
    size_t written;
    size_t expected;

    f = fopen(fname, "w");
    assert_non_null(f);
    expected = strlen(content);
    written = fwrite(content, 1, expected, f);
    fclose(f);
    assert_int_equal(written, expected);
}

static void load_preamble(const char* const config,
                          const UChar* const section,
                          const yella_setting_desc* desc,
                          size_t count)
{
    yella_rc rc;

    write_config("load_settings_test.yaml", config);
    yella_settings_set_text(u"agent", u"config-file", u"load_settings_test.yaml");
    rc = yella_load_settings_doc();
    remove("load_settings_test.yaml");
    assert_int_equal(rc, YELLA_NO_ERROR);
    yella_retrieve_settings(section, desc, count);
    yella_destroy_settings_doc();
}

static int set_up(void** arg)
{
    yella_initialize_settings();
    return 0;
}

static int tear_down(void** arg)
{
    yella_destroy_settings();
    return 0;
}

static void initial_settings(void** arg)
{
#if defined(YELLA_POSIX)
    assert_true(u_strcmp(yella_settings_get_text(u"agent", u"config-file"), u"/etc/yella.yaml") == 0);
    assert_true(u_strcmp(yella_settings_get_text(u"agent", u"data-dir"), u"/var/lib/yella") == 0);
    assert_true(u_strcmp(yella_settings_get_text(u"agent", u"spool-dir"), u"/var/spool/yella") == 0);
    assert_true(u_strcmp(yella_settings_get_text(u"agent", u"plugin-dir"), u"/usr/local/plugin") == 0);
#endif
}

static void get_set(void** arg)
{
    const uint64_t* val;
    const UChar* str;

    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_uint(u"doggy", u"int-test", 67);
    val = yella_settings_get_uint(u"doggy", u"int-test");
    assert_non_null(val);
    assert_int_equal(*val, 67);
    val = yella_settings_get_uint(u"agent", u"config-file");
    assert_null(val);
    val = yella_settings_get_uint(u"plummers", u"doggy");
    assert_null(val);
    yella_settings_set_text(u"sumin", u"text-test", u"whoa");
    str = yella_settings_get_text(u"sumin", u"text-test");
    assert_non_null(str);
    assert_true(u_strcmp(str, u"whoa") == 0);
    str = yella_settings_get_text(u"iguanas", u"int-test");
    assert_null(str);
    str = yella_settings_get_text(u"lumpy", u"lumpy-garbage");
    assert_null(str);
}

static void load_1(void** arg)
{
    const UChar* val;
    yella_setting_desc desc[] =
    {
        { u"load-1-mine-1", YELLA_SETTING_VALUE_TEXT },
        { u"load-1-mine-2", YELLA_SETTING_VALUE_TEXT }
    };

    load_preamble("agent:\n"
                  "    load-1-not-mine: howdy\n"
                  "    load-1-mine-1: whaddup\n"
                  "    load-1-more-not-mine: bye\n"
                  "    load-1-mine-2: yummy",
                  u"agent",
                  desc,
                  2);
    val = yella_settings_get_text(u"agent", u"load-1-mine-1");
    assert_non_null(val);
    assert_true(u_strcmp(val, u"whaddup") == 0);
    val = yella_settings_get_text(u"agent", u"load-1-mine-2");
    assert_non_null(val);
    assert_true(u_strcmp(val, u"yummy") == 0);
    assert_null(yella_settings_get_text(u"agent", u"non-mine"));
    assert_null(yella_settings_get_text(u"agent", u"more-non-mine"));
}

static void load_2(void** arg)
{
    const UChar* val;
    yella_setting_desc desc;

    desc.key = u"load-2-mine";
    desc.type = YELLA_SETTING_VALUE_TEXT;
    load_preamble("- chucho::logger:\n"
                  "    - name: will\n"
                  "    - chucho::cout_writer:\n"
                  "        - chucho::pattern_formatter:\n"
                  "            - pattern: '%m'\n"
                  "- agent:\n"
                  "    load-2-mine: doggy",
                  u"agent",
                  &desc,
                  1);
    val = yella_settings_get_text(u"agent", u"load-2-mine");
    assert_non_null(val);
    assert_true(u_strcmp(val, u"doggy") == 0);
}

static void too_big(void** arg)
{
    FILE* f;
    char ch = '#';
    int i;
    yella_rc rc;

    f = fopen("too_big.yaml", "w");
    for (i = 0; i < 100 * 1024 + 1; i++)
        fwrite(&ch, 1, 1, f);
    fclose(f);
    yella_settings_set_text(u"agent", u"config-file", u"too_big.yaml");
    rc = yella_load_settings_doc();
    remove("too_big.yaml");
    assert_int_equal(rc, YELLA_TOO_BIG);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup_teardown(initial_settings, set_up, tear_down),
        cmocka_unit_test_setup_teardown(get_set, set_up, tear_down),
        cmocka_unit_test_setup_teardown(load_1, set_up, tear_down),
        cmocka_unit_test_setup_teardown(load_2, set_up, tear_down),
        cmocka_unit_test_setup_teardown(too_big, set_up, tear_down)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
