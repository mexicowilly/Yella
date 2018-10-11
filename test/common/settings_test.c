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
                          const char* const section,
                          const yella_setting_desc* desc,
                          size_t count)
{
    yella_rc rc;

    write_config("load_settings_test.yaml", config);
    yella_settings_set_text("agent", "config-file", "load_settings_test.yaml");
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
    assert_string_equal(yella_settings_get_text("agent", "config-file"), "/etc/yella.yaml");
    assert_string_equal(yella_settings_get_text("agent", "data-dir"), "/var/lib/yella");
    assert_string_equal(yella_settings_get_text("agent", "spool-dir"), "/var/spool/yella");
    assert_string_equal(yella_settings_get_text("agent", "plugin-dir"), "/usr/local/plugin");
#endif
}

static void get_set(void** arg)
{
    const uint64_t* val;
    const char* str;

    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_uint("doggy", "int-test", 67);
    val = yella_settings_get_uint("doggy", "int-test");
    assert_non_null(val);
    assert_int_equal(*val, 67);
    val = yella_settings_get_uint("agent", "config-file");
    assert_null(val);
    val = yella_settings_get_uint("plummers", "doggy");
    assert_null(val);
    yella_settings_set_text("sumin", "text-test", "whoa");
    str = yella_settings_get_text("sumin", "text-test");
    assert_non_null(str);
    assert_string_equal(str, "whoa");
    str = yella_settings_get_text("iguanas", "int-test");
    assert_null(str);
    str = yella_settings_get_text("lumpy", "lumpy-garbage");
    assert_null(str);
}

static void load_1(void** arg)
{
    const char* val;
    yella_setting_desc desc[] =
    {
        { "load-1-mine-1", YELLA_SETTING_VALUE_TEXT },
        { "load-1-mine-2", YELLA_SETTING_VALUE_TEXT }
    };

    load_preamble("agent:\n"
                  "    load-1-not-mine: howdy\n"
                  "    load-1-mine-1: whaddup\n"
                  "    load-1-more-not-mine: bye\n"
                  "    load-1-mine-2: yummy",
                  "agent",
                  desc,
                  2);
    val = yella_settings_get_text("agent", "load-1-mine-1");
    assert_non_null(val);
    assert_string_equal(val, "whaddup");
    val = yella_settings_get_text("agent", "load-1-mine-2");
    assert_non_null(val);
    assert_string_equal(val, "yummy");
    assert_null(yella_settings_get_text("agent", "non-mine"));
    assert_null(yella_settings_get_text("agent", "more-non-mine"));
}

static void load_2(void** arg)
{
    const char* val;
    yella_setting_desc desc;

    desc.key = "load-2-mine";
    desc.type = YELLA_SETTING_VALUE_TEXT;
    load_preamble("- chucho::logger:\n"
                  "    - name: will\n"
                  "    - chucho::cout_writer:\n"
                  "        - chucho::pattern_formatter:\n"
                  "            - pattern: '%m'\n"
                  "- agent:\n"
                  "    load-2-mine: doggy",
                  "agent",
                  &desc,
                  1);
    val = yella_settings_get_text("agent", "load-2-mine");
    assert_non_null(val);
    assert_string_equal(val, "doggy");
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
    yella_settings_set_text("agent", "config-file", "too_big.yaml");
    rc = yella_load_settings_doc();
    remove("too_big.yaml");
    assert_int_equal(rc, YELLA_TOO_BIG);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(initial_settings),
        cmocka_unit_test(get_set),
        cmocka_unit_test(load_1),
        cmocka_unit_test(load_2),
        cmocka_unit_test(too_big)
    };
    return cmocka_run_group_tests(tests, set_up, tear_down);
}
