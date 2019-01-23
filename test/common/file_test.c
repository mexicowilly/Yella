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
#include "common/text_util.h"
#include <unicode/ustring.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

typedef struct blob
{
    size_t read_count;
    uint8_t* bytes;
    size_t sz;
} blob;

static void function_to_apply(const uint8_t* const bytes, size_t sz, void* udata)
{
    blob* bl;

    bl = udata;
    bl->bytes = realloc(bl->bytes, bl->sz + sz);
    memcpy(bl->bytes + bl->sz, bytes, sz);
    bl->sz += sz;
    ++bl->read_count;
}

static void apply_function(void** arg)
{
    blob bl;
    uint8_t* src;
    size_t src_sz;
    size_t i;
    FILE* f;

    memset(&bl, 0, sizeof(bl));
    src_sz = 65535;
    src = malloc(src_sz);
    for (i = 0; i < src_sz; i++)
        src[i] = i % 255;
    f = fopen("apply_function_data", "w");
    fwrite(src, src_sz, 1, f);
    fclose(f);
    assert_int_equal(yella_apply_function_to_file_contents(u"apply_function_data", function_to_apply, &bl), YELLA_NO_ERROR);
    assert_int_equal(bl.sz, src_sz);
    assert_memory_equal(src, bl.bytes, src_sz);
    free(src);
    free(bl.bytes);
    remove("apply_function_data");
    printf("%zu reads were performed\n", bl.read_count);
}

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

static void contents(void** arg)
{
    yella_rc yrc;
    FILE* f;
    uint8_t* content;

    f = fopen("contents_data", "w");
    assert_non_null(f);
    fwrite("hello", 1, 5, f);
    fclose(f);
    yrc = yella_file_contents(u"contents_data", &content);
    remove("contents_data");
    assert_int_equal(YELLA_NO_ERROR, yrc);
    assert_memory_equal(content, "hello", 5);
    free(content);
}

static void create_dir(void** arg)
{
    yella_rc yrc;

    remove("create_dir_test_dir");
    yrc = yella_create_directory(u"create_dir_test_dir");
    assert_int_equal(YELLA_NO_ERROR, yrc);
    yrc = yella_create_directory(u"create_dir_test_dir");
    assert_int_equal(YELLA_ALREADY_EXISTS, yrc);
    remove("create_dir_test_dir");
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

static void ensure_dir(void** arg)
{
    yella_rc yrc;

    yella_remove_all(u"ensure_dir_test_dir");
    yrc = yella_ensure_dir_exists(u"ensure_dir_test_dir/my/dog/has/fleas");
    assert_int_equal(YELLA_NO_ERROR, yrc);
    yrc = yella_ensure_dir_exists(u"ensure_dir_test_dir/my/dog/has/fleas");
    assert_int_equal(YELLA_NO_ERROR, yrc);
    yella_remove_all(u"ensure_dir_test_dir");
}

static void exists(void** arg)
{
    UChar* cwd;
    bool ex;

    cwd = yella_getcwd();
    assert_non_null(cwd);
    ex = yella_file_exists(cwd);
    free(cwd);
    assert_true(ex);
    ex = yella_file_exists(u"doggy-doggy-wonderdog");
    assert_false(ex);
}

static void file_type(void** arg)
{
    yella_file_type tp;
    UChar* cwd;
    yella_rc yrc;
    FILE* f;

    cwd = yella_getcwd();
    assert_non_null(cwd);
    yrc = yella_get_file_type(cwd, &tp);
    assert_int_equal(YELLA_NO_ERROR, yrc);
    assert_int_equal(YELLA_FILE_TYPE_DIRECTORY, tp);
    f = fopen("file_type_regular", "w");
    assert_non_null(f);
    fwrite(cwd, sizeof(UChar), u_strlen(cwd), f);
    fclose(f);
    free(cwd);
    yrc = yella_get_file_type(u"file_type_regular", &tp);
    assert_int_equal(YELLA_NO_ERROR, yrc);
    assert_int_equal(YELLA_FILE_TYPE_REGULAR, tp);
    remove("file_type_regular");
    yrc = yella_get_file_type(u"doggy-doggy-wonderdog", &tp);
    assert_int_equal(YELLA_DOES_NOT_EXIST, yrc);
}

static void getcwd(void** arg)
{
    UChar* cwd;
    char* utf8;

    cwd = yella_getcwd();
    assert_non_null(cwd);
    utf8 = yella_to_utf8(cwd);
    free(cwd);
    printf("cwd = '%s'\n", utf8);
    free(utf8);
}

static void size(void** arg)
{
    yella_rc yrc;
    FILE* f;
    size_t sz;

    f = fopen("size_data", "w");
    assert_non_null(f);
    fwrite("hello", 1, 5, f);
    fclose(f);
    yrc = yella_file_size(u"size_data", &sz);
    remove("size_data");
    assert_int_equal(YELLA_NO_ERROR, yrc);
    assert_int_equal(5, sz);
    yrc = yella_file_size(u"doggy-doggy-wangledog", &sz);
    assert_int_equal(YELLA_DOES_NOT_EXIST, yrc);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(apply_function),
        cmocka_unit_test(base_name),
        cmocka_unit_test(contents),
        cmocka_unit_test(create_dir),
        cmocka_unit_test(dir_name),
        cmocka_unit_test(ensure_dir),
        cmocka_unit_test(exists),
        cmocka_unit_test(file_type),
        cmocka_unit_test(getcwd),
        cmocka_unit_test(size)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
