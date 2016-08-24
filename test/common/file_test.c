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


#include "common/file.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

static void base_name(void** arg)
{
    char* r;

    r = yella_base_name("/one/two/three");
    assert_string_equal(r, "three");
    free(r);
    r = yella_base_name("/one/two/three/");
    assert_string_equal(r, "three");
    free(r);
    r = yella_base_name("/one/two/three///");
    assert_string_equal(r, "three");
    free(r);
    r = yella_base_name("/one/two////////three///");
    assert_string_equal(r, "three");
    free(r);
    r = yella_base_name("");
    assert_string_equal(r, ".");
    free(r);
    r = yella_base_name("one");
    assert_string_equal(r, "one");
    free(r);
#if defined(YELLA_POSIX)
    r = yella_base_name("/");
    assert_string_equal(r, "/");
    free(r);
    r = yella_base_name("//////");
    assert_string_equal(r, "/");
    free(r);
#endif
}

static void dir_name(void** arg)
{
    char* r;

    r = yella_dir_name("/one/two/three");
    assert_string_equal(r, "/one/two");
    free(r);
    r = yella_dir_name("/one/two/three/");
    assert_string_equal(r, "/one/two");
    free(r);
    r = yella_dir_name("/one/two/three/////////");
    assert_string_equal(r, "/one/two");
    free(r);
    r = yella_dir_name("/one/two/////three/////////");
    assert_string_equal(r, "/one/two");
    free(r);
    r = yella_dir_name("/one/two/////three");
    assert_string_equal(r, "/one/two");
    free(r);
    r = yella_dir_name("one/two");
    assert_string_equal(r, "one");
    free(r);
    r = yella_dir_name("one/////two/////////////");
    assert_string_equal(r, "one");
    free(r);
    r = yella_dir_name("");
    assert_string_equal(r, ".");
    free(r);
    r = yella_dir_name("one");
    assert_string_equal(r, ".");
    free(r);
    r = yella_dir_name("/one");
    assert_string_equal(r, "/");
    free(r);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(base_name),
        cmocka_unit_test(dir_name)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
