#include "plugin/file/attribute.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>
#include <plugin/file/attribute.h>

static void compare(void** arg)
{
    attribute one;
    attribute two;
    int rc;

    one.type = ATTR_TYPE_FILE_TYPE;
    one.value.int_value = 707;
    two.type = ATTR_TYPE_FILE_TYPE;
    two.value.int_value = 707;
    assert_int_equal(compare_attributes(&one, &two), 0);
    two.value.int_value = 708;
    assert_true(compare_attributes(&one, &two) < 0);
    one.type = ATTR_TYPE_SHA256;
    one.value.byte_array.sz = 5;
    one.value.byte_array.mem = (uint8_t*)"hello";
    assert_int_not_equal(compare_attributes(&one, &two), 0);
    two.type = ATTR_TYPE_SHA256;
    two.value.byte_array.sz = 6;
    two.value.byte_array.mem = (uint8_t*)"h3llo1";
    assert_int_not_equal(compare_attributes(&one, &two), 0);
    two.value.byte_array.sz = 5;
    two.value.byte_array.mem = (uint8_t*)"h3llo";
    assert_int_not_equal(compare_attributes(&one, &two), 0);
    two.value.byte_array.mem = (uint8_t*)"hello";
    assert_int_equal(compare_attributes(&one, &two), 0);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(compare)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
