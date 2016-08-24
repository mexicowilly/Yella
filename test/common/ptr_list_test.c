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

#include "common/ptr_list.h"
#include "common/text_util.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static void simple(void** arg)
{
    yella_ptr_list* list;
    char* text;
    yella_ptr_list_iterator* itor;
    char* p;

    text = yella_text_dup("two");
    list = yella_create_ptr_list(text);
    assert_non_null(list);
    assert_int_equal(yella_ptr_list_size(list), 1);
    yella_push_front_ptr_list(list, yella_text_dup("one"));
    assert_int_equal(yella_ptr_list_size(list), 2);
    yella_push_back_ptr_list(list, yella_text_dup("three"));
    assert_int_equal(yella_ptr_list_size(list), 3);
    itor = yella_create_ptr_list_iterator(list);
    assert_non_null(itor);
    p = yella_ptr_list_iterator_next(itor);
    assert_non_null(p);
    assert_string_equal(p, "one");
    p = yella_ptr_list_iterator_next(itor);
    assert_non_null(p);
    assert_string_equal(p, "two");
    p = yella_ptr_list_iterator_next(itor);
    assert_non_null(p);
    assert_string_equal(p, "three");
    p = yella_ptr_list_iterator_next(itor);
    assert_null(p);
    yella_destroy_ptr_list_iterator(itor);
    yella_reverse_ptr_list(list);
    itor = yella_create_ptr_list_iterator(list);
    assert_non_null(itor);
    p = yella_ptr_list_iterator_next(itor);
    assert_non_null(p);
    assert_string_equal(p, "three");
    p = yella_ptr_list_iterator_next(itor);
    assert_non_null(p);
    assert_string_equal(p, "two");
    p = yella_ptr_list_iterator_next(itor);
    assert_non_null(p);
    assert_string_equal(p, "one");
    p = yella_ptr_list_iterator_next(itor);
    assert_null(p);
    yella_destroy_ptr_list_iterator(itor);
    yella_destroy_ptr_list(list);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(simple)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
