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
#include "common/uds.h"
#include "common/uds_util.h"
#include <unicode/ustring.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

static void simple(void** arg)
{
    yella_ptr_vector* vec;

    vec = yella_create_uds_ptr_vector();
    assert_non_null(vec);
    assert_int_equal(yella_ptr_vector_size(vec), 0);
    yella_push_back_ptr_vector(vec, udsnew(u"two"));
    assert_int_equal(yella_ptr_vector_size(vec), 1);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 0), u"two") == 0);
    yella_push_front_ptr_vector(vec, udsnew(u"one"));
    assert_int_equal(yella_ptr_vector_size(vec), 2);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 0), u"one") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 1), u"two") == 0);
    yella_push_back_ptr_vector(vec, udsnew(u"three"));
    assert_int_equal(yella_ptr_vector_size(vec), 3);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 0), u"one") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 1), u"two") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 2), u"three") == 0);
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

    vec = yella_create_uds_ptr_vector();
    yella_push_back_ptr_vector(vec, udsnew(u"one"));
    yella_push_back_ptr_vector(vec, udsnew(u"two"));
    yella_push_back_ptr_vector(vec, udsnew(u"three"));
    yella_push_back_ptr_vector(vec, udsnew(u"four"));
    yella_push_back_ptr_vector(vec, udsnew(u"five"));
    assert_int_equal(yella_ptr_vector_size(vec), 5);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 0), u"one") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 1), u"two") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 2), u"three") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 3), u"four") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 4), u"five") == 0);
    yella_erase_ptr_vector_at(vec, 2);
    assert_int_equal(yella_ptr_vector_size(vec), 4);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 0), u"one") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 1), u"two") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 2), u"four") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 3), u"five") == 0);
    yella_erase_ptr_vector_at(vec, 0);
    assert_int_equal(yella_ptr_vector_size(vec), 3);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 0), u"two") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 1), u"four") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 2), u"five") == 0);
    yella_erase_ptr_vector_at(vec, 2);
    assert_int_equal(yella_ptr_vector_size(vec), 2);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 0), u"two") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 1), u"four") == 0);
    yella_pop_back_ptr_vector(vec);
    assert_int_equal(yella_ptr_vector_size(vec), 1);
    assert_true(u_strcmp(yella_ptr_vector_at(vec, 0), u"two") == 0);
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

struct test_data
{
    int val;
};

static void* test_copier(void* p, void* udata)
{
    struct test_data* d;

    d = (struct test_data*)udata;
    d->val = -1;
    return udsdup((uds)p);
}

static void copier(void** arg)
{
    yella_ptr_vector* vec;
    yella_ptr_vector* vec2;
    struct test_data tdd;
    uds p;

    tdd.val = 1;
    vec = yella_create_uds_ptr_vector();
    yella_set_ptr_vector_copier(vec, test_copier, &tdd);
    p = udsnew(u"my dog has fleas");
    yella_push_back_ptr_vector(vec, p);
    vec2 = yella_copy_ptr_vector(vec);
    assert_int_equal(yella_ptr_vector_size(vec2), 1);
    assert_ptr_not_equal(p, yella_ptr_vector_at(vec2, 0));
    assert_ptr_not_equal(yella_ptr_vector_at(vec, 0), yella_ptr_vector_at(vec2, 0));
    assert_true(u_strcmp((UChar*)yella_ptr_vector_at(vec2, 0), p) == 0);
    yella_destroy_ptr_vector(vec);
    yella_destroy_ptr_vector(vec2);
    assert_int_equal(tdd.val, -1);
}

static void test_destructor(void* p, void* udata)
{
    struct test_data* d;

    d = (struct test_data*)udata;
    d->val = -1;
    free(p);
}

static void destructor(void** arg)
{
    yella_ptr_vector* vec;
    struct test_data tdd;
    int* p;

    tdd.val = 1;
    vec = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(vec, test_destructor, &tdd);
    p = malloc(sizeof(int));
    yella_push_back_ptr_vector(vec, p);
    yella_destroy_ptr_vector(vec);
    assert_int_equal(tdd.val, -1);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(simple),
        cmocka_unit_test(erase),
        cmocka_unit_test(big),
        cmocka_unit_test(destructor),
        cmocka_unit_test(copier)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
