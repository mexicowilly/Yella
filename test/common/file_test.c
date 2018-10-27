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
#include <unicode/ustring.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

static void base_name(void** arg)
{
    uds r;

    r = yella_base_name(u"/one/two/three");
    assert_true(u_strcmp(r, u"three") == 0);
    udsfree(r);
    r = yella_base_name(u"/one/two/three/");
    assert_true(u_strcmp(r, u"three") == 0);
    udsfree(r);
    r = yella_base_name(u"/one/two/three///");
    assert_true(u_strcmp(r, u"three") == 0);
    udsfree(r);
    r = yella_base_name(u"/one/two////////three///");
    assert_true(u_strcmp(r, u"three") == 0);
    udsfree(r);
    r = yella_base_name(u"");
    assert_true(u_strcmp(r, u".") == 0);
    udsfree(r);
    r = yella_base_name(u"one");
    assert_true(u_strcmp(r, u"one") == 0);
    udsfree(r);
#if defined(YELLA_POSIX)
    r = yella_base_name(u"/");
    assert_true(u_strcmp(r, u"/") == 0);
    udsfree(r);
    r = yella_base_name(u"//////");
    assert_true(u_strcmp(r, u"/") == 0);
    udsfree(r);
#endif
}

static void dir_name(void** arg)
{
    uds r;

    r = yella_dir_name(u"/one/two/three");
    assert_true(u_strcmp(r, u"/one/two") == 0);
    udsfree(r);
    r = yella_dir_name(u"/one/two/three/");
    assert_true(u_strcmp(r, u"/one/two") == 0);
    udsfree(r);
    r = yella_dir_name(u"/one/two/three/////////");
    assert_true(u_strcmp(r, u"/one/two") == 0);
    udsfree(r);
    r = yella_dir_name(u"/one/two/////three/////////");
    assert_true(u_strcmp(r, u"/one/two") == 0);
    udsfree(r);
    r = yella_dir_name(u"/one/two/////three");
    assert_true(u_strcmp(r, u"/one/two") == 0);
    udsfree(r);
    r = yella_dir_name(u"one/two");
    assert_true(u_strcmp(r, u"one") == 0);
    udsfree(r);
    r = yella_dir_name(u"one/////two/////////////");
    assert_true(u_strcmp(r, u"one") == 0);
    udsfree(r);
    r = yella_dir_name(u"");
    assert_true(u_strcmp(r, u".") == 0);
    udsfree(r);
    r = yella_dir_name(u"one");
    assert_true(u_strcmp(r, u".") == 0);
    udsfree(r);
    r = yella_dir_name(u"/one");
    assert_true(u_strcmp(r, u"/") == 0);
    udsfree(r);
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
