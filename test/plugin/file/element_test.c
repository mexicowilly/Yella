#include "plugin/file/element.h"
#include "common/file.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

static void compare(void** arg)
{
    element* elem1;
    element* elem2;
    attribute* attr;
    int rc;

    elem1 = create_element(u"funky");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem1, attr);
    elem2 = create_element(u"monkey");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem2, attr);
    rc = compare_element_attributes(elem1, elem2);
    assert_int_equal(rc, 0);
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_SHA256;
    attr->value.byte_array.mem = malloc(5);
    memcpy(attr->value.byte_array.mem, "hello", 5);
    attr->value.byte_array.sz = 5;
    add_element_attribute(elem1, attr);
    rc = compare_element_attributes(elem1, elem2);
    assert_int_not_equal(rc, 0);
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_SHA256;
    attr->value.byte_array.mem = malloc(5);
    memcpy(attr->value.byte_array.mem, "hello", 5);
    attr->value.byte_array.sz = 5;
    add_element_attribute(elem2, attr);
    rc = compare_element_attributes(elem1, elem2);
    assert_int_equal(rc, 0);
    destroy_element(elem2);
    elem2 = create_element(u"iguana");
    rc = compare_element_attributes(elem1, elem2);
    assert_int_not_equal(rc, 0);
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_SYMBOLIC_LINK;
    add_element_attribute(elem2, attr);
    rc = compare_element_attributes(elem1, elem2);
    assert_int_not_equal(rc, 0);
    destroy_element(elem2);
    destroy_element(elem1);
}

void diff(void** arg)
{
    element* elem1;
    element* elem2;
    element* elem3;
    attribute* attr;

    elem1 = create_element(u"funky");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem1, attr);
    elem2 = create_element(u"funky");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_SYMBOLIC_LINK;
    add_element_attribute(elem2, attr);
    diff_elements(elem1, elem2);
    elem3 = create_element(u"chunky");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem3, attr);
    assert_int_equal(compare_element_attributes(elem1, elem3), 0);
    destroy_element(elem3);
    destroy_element(elem2);
    destroy_element(elem1);
    elem1 = create_element(u"funky");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem1, attr);
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_SHA256;
    attr->value.byte_array.mem = malloc(5);
    memcpy(attr->value.byte_array.mem, "hello", 5);
    attr->value.byte_array.sz = 5;
    add_element_attribute(elem1, attr);
    elem2 = create_element(u"funky");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem2, attr);
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_SHA256;
    attr->value.byte_array.mem = malloc(7);
    memcpy(attr->value.byte_array.mem, "goodbye", 7);
    attr->value.byte_array.sz = 7;
    add_element_attribute(elem2, attr);
    diff_elements(elem1, elem2);
    elem3 = create_element(u"funky");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_SHA256;
    attr->value.byte_array.mem = malloc(5);
    memcpy(attr->value.byte_array.mem, "hello", 5);
    attr->value.byte_array.sz = 5;
    add_element_attribute(elem3, attr);
    assert_int_equal(compare_element_attributes(elem1, elem3), 0);
    destroy_element(elem3);
    destroy_element(elem2);
    destroy_element(elem1);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(compare),
        cmocka_unit_test(diff)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
