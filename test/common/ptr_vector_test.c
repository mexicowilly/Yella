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

#include "common/ptr_vector.h"
#include "common/text_util.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

static void simple(void** arg)
{
    yella_ptr_vector* vec;

    vec = yella_create_ptr_vector();
    assert_non_null(vec);
    assert_int_equal(yella_ptr_vector_size(vec), 0);
    yella_push_back_ptr_vector(vec, yella_text_dup("two"));
    assert_int_equal(yella_ptr_vector_size(vec), 1);
    assert_string_equal(yella_ptr_vector_at(vec, 0), "two");
    yella_push_front_ptr_vector(vec, yella_text_dup("one"));
    assert_int_equal(yella_ptr_vector_size(vec), 2);
    assert_string_equal(yella_ptr_vector_at(vec, 0), "one");
    assert_string_equal(yella_ptr_vector_at(vec, 1), "two");
    yella_push_back_ptr_vector(vec, yella_text_dup("three"));
    assert_int_equal(yella_ptr_vector_size(vec), 3);
    assert_string_equal(yella_ptr_vector_at(vec, 0), "one");
    assert_string_equal(yella_ptr_vector_at(vec, 1), "two");
    assert_string_equal(yella_ptr_vector_at(vec, 2), "three");
    yella_clear_ptr_vector(vec);
    assert_int_equal(yella_ptr_vector_size(vec), 0);
    assert_null(yella_ptr_vector_at(vec, 0));
    assert_null(yella_ptr_vector_at(vec, 1));
    assert_null(yella_ptr_vector_at(vec, 2));
    yella_destroy_ptr_vector(vec);
}

static void erase(void** arg)
{
    yella_ptr_vector* vec;

    vec = yella_create_ptr_vector();
    yella_push_back_ptr_vector(vec, yella_text_dup("one"));
    yella_push_back_ptr_vector(vec, yella_text_dup("two"));
    yella_push_back_ptr_vector(vec, yella_text_dup("three"));
    yella_push_back_ptr_vector(vec, yella_text_dup("four"));
    yella_push_back_ptr_vector(vec, yella_text_dup("five"));
    assert_int_equal(yella_ptr_vector_size(vec), 5);
    assert_string_equal(yella_ptr_vector_at(vec, 0), "one");
    assert_string_equal(yella_ptr_vector_at(vec, 1), "two");
    assert_string_equal(yella_ptr_vector_at(vec, 2), "three");
    assert_string_equal(yella_ptr_vector_at(vec, 3), "four");
    assert_string_equal(yella_ptr_vector_at(vec, 4), "five");
    yella_erase_ptr_vector_at(vec, 2);
    assert_int_equal(yella_ptr_vector_size(vec), 4);
    assert_string_equal(yella_ptr_vector_at(vec, 0), "one");
    assert_string_equal(yella_ptr_vector_at(vec, 1), "two");
    assert_string_equal(yella_ptr_vector_at(vec, 2), "four");
    assert_string_equal(yella_ptr_vector_at(vec, 3), "five");
    yella_erase_ptr_vector_at(vec, 0);
    assert_int_equal(yella_ptr_vector_size(vec), 3);
    assert_string_equal(yella_ptr_vector_at(vec, 0), "two");
    assert_string_equal(yella_ptr_vector_at(vec, 1), "four");
    assert_string_equal(yella_ptr_vector_at(vec, 2), "five");
    yella_erase_ptr_vector_at(vec, 2);
    assert_int_equal(yella_ptr_vector_size(vec), 2);
    assert_string_equal(yella_ptr_vector_at(vec, 0), "two");
    assert_string_equal(yella_ptr_vector_at(vec, 1), "four");
    yella_pop_back_ptr_vector(vec);
    assert_int_equal(yella_ptr_vector_size(vec), 1);
    assert_string_equal(yella_ptr_vector_at(vec, 0), "two");
    yella_pop_front_ptr_vector(vec);
    assert_int_equal(yella_ptr_vector_size(vec), 0);
    assert_null(yella_ptr_vector_at(vec, 0));
    assert_null(yella_ptr_vector_at(vec, 1));
    assert_null(yella_ptr_vector_at(vec, 2));
    assert_null(yella_ptr_vector_at(vec, 3));
    assert_null(yella_ptr_vector_at(vec, 4));
    yella_destroy_ptr_vector(vec);
}

static int* int_ptr(int i)
{
    int* result;

    result = malloc(sizeof(int));
    *result = i;
    return result;
}

static void big(void** arg)
{
    yella_ptr_vector* vec;
    int i;
    int* ip;

    vec = yella_create_ptr_vector();
    for (i = 0; i < 1000000; i++)
        yella_push_back_ptr_vector(vec, int_ptr(i));
    assert_int_equal(yella_ptr_vector_size(vec), 1000000);
    for (i = 0; i < 1000000; i++)
    {
        ip = yella_ptr_vector_at(vec, i);
        assert_non_null(ip);
        assert_int_equal(*ip, i);
    }
    yella_pop_front_ptr_vector(vec);
    assert_int_equal(yella_ptr_vector_size(vec), 999999);
    for (i = 0; i < 999999; i++)
    {
        ip = yella_ptr_vector_at(vec, i);
        assert_non_null(ip);
        assert_int_equal(*ip, i + 1);
    }
    yella_destroy_ptr_vector(vec);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(simple),
        cmocka_unit_test(erase),
        cmocka_unit_test(big)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
